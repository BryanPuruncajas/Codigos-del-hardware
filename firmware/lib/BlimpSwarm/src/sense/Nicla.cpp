/**
 * @file Nicla.cpp
 * @author Edward Jeff
 * @brief Implementation of Nicla.h
 * @version 0.1
 * @date 2024-03-12
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "Nicla.h"

HardwareSerial MySerial0(0);
Nicla::Nicla() {
    for (int i = 0; i < 10; i++) {
        value[i] = 0;
    }
}

void Nicla::startup() {
    Nicla::getPreferences();
    Serial.println("Starting IBUS Init");
    MySerial0.begin(115200, SERIAL_8N1, -1, -1);
    IBus.begin(MySerial0, IBUSBM_NOTIMER);
    IBus.loop();
    MySerial0.write(mode); 
    sendTime = micros();
}

void Nicla::startup(uint8_t setMode) {
    Serial.println("Starting IBUS Init");
    MySerial0.begin(115200, SERIAL_8N1, -1, -1);
    IBus.begin(MySerial0, IBUSBM_NOTIMER);
    IBus.loop();
    mode = setMode; 
    MySerial0.write(mode); 
    sendTime = micros();
}

//data = [flag, x_roi, y_roi, w_roi, h_roi, x_value, y_value, w_value, h_value, dis]
bool Nicla::update() { 
    IBus.loop();

    for (int i = 0; i < 10; i++){
        value[i] = (float)IBus.readChannel(i);
    }
    return true;
}

//data = [flag, x_roi, y_roi, w_roi, h_roi, x_value, y_value, w_value, h_value, dis]
bool Nicla::update(uint8_t setMode) {
    MySerial0.write(setMode); 

    return Nicla::update();
    
}


float* Nicla::readValues(int& count) {
    count = 10; 
    return value;
}

void Nicla::getPreferences(){
    return;
}