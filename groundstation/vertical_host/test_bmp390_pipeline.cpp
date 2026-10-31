#include "Arduino.h"
#include "Wire.h"
#include "sense/Barometer390.h"
#include <assert.h>
#include <stdio.h>
uint32_t fakeUs=0,lastSensorUs=0;
SerialMock Serial;TwoWire Wire;
bool busOk=true,modeOk=true;
float fakePressure=101325,fakeTemperature=24;
unsigned settingsCalls=0,modeCalls=0,reads=0;
extern "C" {
int8_t bmp3_init(bmp3_dev* d){d->chip_id=BMP390_CHIP_ID;return busOk?BMP3_OK:BMP3_E_COMM_FAIL;}
int8_t bmp3_set_sensor_settings(uint32_t,bmp3_dev* d){
 ++settingsCalls;
 assert(d->settings.odr_filter.press_os==BMP3_OVERSAMPLING_8X);
 assert(d->settings.odr_filter.temp_os==BMP3_NO_OVERSAMPLING);
 assert(d->settings.odr_filter.odr==BMP3_ODR_25_HZ);
 assert(d->settings.odr_filter.iir_filter==BMP3_IIR_FILTER_COEFF_1);
 return BMP3_OK;
}
int8_t bmp3_set_op_mode(bmp3_dev* d){
 ++modeCalls;assert(d->settings.op_mode==BMP3_MODE_NORMAL);lastSensorUs=fakeUs;
 return modeOk?BMP3_OK:BMP3_E_COMM_FAIL;
}
int8_t bmp3_get_status(bmp3_dev* d){
 d->status.sensor.drdy_press=d->status.sensor.drdy_temp=uint32_t(fakeUs-lastSensorUs)>=40000;
 return busOk?BMP3_OK:BMP3_E_COMM_FAIL;
}
int8_t bmp3_get_sensor_data(uint8_t,bmp3_data* d,bmp3_dev*){
 ++reads;lastSensorUs=fakeUs;d->pressure=fakePressure;d->temperature=fakeTemperature;
 return busOk?BMP3_OK:BMP3_E_COMM_FAIL;
}
}
int main(){
 Bmp390Sampler s;assert(s.begin());float p=7,t=8;
 assert(!s.read(p,t) && p==7 && t==8);
 fakeUs=40000;assert(s.read(p,t) && p==101325 && reads==1);
 assert(!s.read(p,t) && reads==1); // no duplicated sample, no conversion restart
 fakeUs+=40000;fakePressure=NAN;assert(!s.read(p,t) && p==101325);
 assert(settingsCalls==1 && modeCalls==1);
 busOk=false;assert(!s.read(p,t));busOk=true;modeOk=false;
 Bmp390Sampler bad;assert(!bad.begin() && !bad.read(p,t));modeOk=true;
 fakeUs=0;lastSensorUs=0;fakePressure=101325;reads=settingsCalls=modeCalls=0;
 Barometer b;b.startup();auto d=b.diagnostics();assert(d.healthy);
 assert(settingsCalls==1 && modeCalls==1);
 int n=0;float* values=b.readValues(n);assert(n==4 && fabsf(values[2])<.00001f);
 const unsigned previousReads=reads;fakeUs+=20000;assert(b.update());
 const uint32_t sample=b.diagnostics().sampleTimeUs;
 assert(reads==previousReads+1 && !b.update());
 fakeUs+=20000;assert(!b.update() && b.diagnostics().sampleTimeUs==sample);
 fakeUs+=20000;assert(b.update());assert(b.diagnostics().samplePeriodUs==40000);
 assert(settingsCalls==1 && modeCalls==1);
 busOk=false;fakeUs+=350000;assert(!b.update());
 assert(!b.diagnostics().healthy && isnan(b.readValues(n)[2]));
 busOk=true;fakeUs+=200000;assert(b.update());
 assert(b.diagnostics().healthy && b.readValues(n)[3]==0); // no gap derivative spike
 puts("PASS BMP390 normal-mode/data-ready acquisition, one configuration, timestamps, I2C failure, stale invalidation, gap reset");
}
