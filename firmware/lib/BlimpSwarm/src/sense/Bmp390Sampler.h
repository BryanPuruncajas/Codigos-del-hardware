#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <bmp3.h>
#include <math.h>
#include "VerticalProfile.h"

// Uses the Bosch driver already bundled with the installed Adafruit library.
// Configure ONCE in normal mode; never restart a conversion in update().
class Bmp390Sampler {
    struct Bus {TwoWire* wire;uint8_t address;} bus_;
    bmp3_dev device_{};
    bool ready_=false;
    static int8_t readRegisters(uint8_t reg,uint8_t* data,uint32_t len,void* context){
        Bus& b=*static_cast<Bus*>(context);
        if(len>32)return -1;
        b.wire->beginTransmission(b.address);b.wire->write(reg);
        if(b.wire->endTransmission(false)!=0)return -1;
        if(uint32_t(b.wire->requestFrom(b.address,uint8_t(len)))!=len)return -1;
        for(uint32_t i=0;i<len;++i)data[i]=uint8_t(b.wire->read());
        return 0;
    }
    static int8_t writeRegisters(uint8_t reg,const uint8_t* data,uint32_t len,void* context){
        Bus& b=*static_cast<Bus*>(context);
        if(len>31)return -1;
        b.wire->beginTransmission(b.address);b.wire->write(reg);
        if(b.wire->write(data,len)!=len){b.wire->endTransmission();return -1;}
        return b.wire->endTransmission()==0?0:-1;
    }
    static void waitUs(uint32_t us,void*){delayMicroseconds(us);}
public:
    explicit Bmp390Sampler(TwoWire& wire=Wire):bus_{&wire,0x77}{}
    bool begin(uint8_t address=0x77){
        ready_=false;bus_.address=address;bus_.wire->begin();device_=bmp3_dev{};
        device_.intf=BMP3_I2C_INTF;device_.intf_ptr=&bus_;
        device_.read=readRegisters;device_.write=writeRegisters;device_.delay_us=waitUs;
        if(bmp3_init(&device_)!=BMP3_OK || device_.chip_id!=BMP390_CHIP_ID)return false;
        device_.settings.press_en=BMP3_ENABLE;device_.settings.temp_en=BMP3_ENABLE;
        auto& f=device_.settings.odr_filter;
        f.press_os=BMP3_OVERSAMPLING_8X;f.temp_os=BMP3_NO_OVERSAMPLING;
        f.iir_filter=BMP3_IIR_FILTER_COEFF_1;f.odr=BMP3_ODR_25_HZ;
        const uint32_t mask=BMP3_SEL_PRESS_EN|BMP3_SEL_TEMP_EN|BMP3_SEL_PRESS_OS|
            BMP3_SEL_TEMP_OS|BMP3_SEL_IIR_FILTER|BMP3_SEL_ODR;
        if(bmp3_set_sensor_settings(mask,&device_)!=BMP3_OK)return false;
        device_.settings.op_mode=BMP3_MODE_NORMAL;
        ready_=bmp3_set_op_mode(&device_)==BMP3_OK;
        return ready_;
    }
    bool read(float& pressurePa,float& temperatureC){
        if(!ready_)return false;
        if(bmp3_get_status(&device_)!=BMP3_OK ||
           !device_.status.sensor.drdy_press || !device_.status.sensor.drdy_temp)return false;
        bmp3_data data{};
        if(bmp3_get_sensor_data(BMP3_PRESS|BMP3_TEMP,&data,&device_)!=BMP3_OK ||
           !isfinite(data.pressure)||data.pressure<=0||!isfinite(data.temperature))return false;
        pressurePa=float(data.pressure);temperatureC=float(data.temperature);
        return true;
    }
};
