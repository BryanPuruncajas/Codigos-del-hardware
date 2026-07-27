/**
 * @file AServo.cpp
 * @author David Saldana
 * @brief Implementation of Aservo.h
 * @version 0.1
 * @date 2024-02-09
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "AServo.h"



AServo::AServo(int minVal, int maxVal, int offsetVal, int pinVal, int periodHertz): Actuator(minVal, maxVal, offsetVal, pinVal) {
    // Additional initialization specific to servos
    this->period_hertz = periodHertz;
    pinMode(this->pin, OUTPUT);
    servo.attach(this->pin, this->min, this->max);
    servo.setPeriodHertz(this->period_hertz);
}

AServo::AServo(int pinVal): Actuator(550, 2450, 0, pinVal) {
    // Additional initialization specific to servos
    pinMode(this->pin, OUTPUT);
    servo.attach(this->pin, this->min, this->max);
    servo.setPeriodHertz(50); // Standard 50hz servo
}


void AServo::act(float value){
    //TODO the input should be force. We should use the calibration parameters
    //TODO the thrust is related to angular velocity
    int angle = constrain(value, 0, 180) ; // cant handle values between PI and 2PI
    servo.write((int) angle);
//    int val = constrain(value, 0, 1);
//    this->thrust.writeMicroseconds((int)((val) * (this->max - this->min) + this->min));
}