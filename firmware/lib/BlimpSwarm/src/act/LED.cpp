/**
 * @file LED.cpp
 * @author David Saldana
 * @brief Implementation of LED.h
 * @version 0.1
 * @date 2024-02-09
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "LED.h"
#include <Arduino.h>

LED::LED(int pinVal): Actuator(pinVal) {
    pinMode(this->pin, OUTPUT);
}

void LED::act(float value){
    int led = (value <= 0)? LOW: HIGH;
    digitalWrite(this->pin, led);
}