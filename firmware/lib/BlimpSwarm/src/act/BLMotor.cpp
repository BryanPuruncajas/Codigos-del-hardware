/**
 * @file BLMotor.cpp
 * @author David Saldana
 * @brief Implementation of BLMotor.h
 * @version 0.1
 * @date 2024-02-09
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "BLMotor.h"
#include <Arduino.h>



BLMotor::BLMotor(int minVal, int maxVal, int offsetVal, int pinVal, int periodHertz)
        : Actuator(minVal, maxVal, offsetVal, pinVal) {
    // Additional initialization specific to SpecificActuator
    this->period_hertz = periodHertz;
    pinMode(this->pin, OUTPUT);

    this->thrust.attach(this->pin, this->min, this->max);
    this->thrust.setPeriodHertz(this->period_hertz);
}

BLMotor::BLMotor(int pinVal)
        : Actuator(1100, 2000, 0, pinVal) {
    // Additional initialization specific to SpecificActuator
    pinMode(pinVal, OUTPUT);
    this->thrust.attach(this->pin, this->min, this->max);
    this->thrust.setPeriodHertz(50);
}


void BLMotor::act(float value){
    value = constrain(value, 0, 1);

    // Force to PWM
    float force = value * (max_thrust - min_thrust) + min_thrust;
    int pwm = (int) ((force - pwm_b) / pwm_a);
    this->thrust.writeMicroseconds(pwm);
}



void BLMotor::calibrate(){

    delay(1000);
    Serial.println("Calibrating ESCs....");
    thrust.writeMicroseconds(this->max);
    delay(8000);

    // Back to minimum value
    thrust.writeMicroseconds(this->min);
    delay(8000);
    thrust.writeMicroseconds(0);
    delay(1000);
    Serial.println("Calibration completed");
}


void BLMotor::arm(){
// ESC arming sequence for BLHeli S
    thrust.writeMicroseconds(this->min);
    delay(10);

    // Sweep up
    for (int i = 1050; i < 1500; i++)
    {
        thrust.writeMicroseconds(i);
        delay(6);
    }

    thrust.writeMicroseconds(1000);
    delay(1000);
}
