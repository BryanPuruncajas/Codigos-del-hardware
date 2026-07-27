/**
 * @file AServo.h
 * @author David Saldana
 * @brief Actuation of servo motors for a XIAO ESP32S3
 * @version 0.1
 * @date 2024-02-09
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef BLIMPSWARM_ASERVO_H
#define BLIMPSWARM_ASERVO_H

#include "Actuator.h"
#include <ESP32Servo.h> // Primary library used for the actuation of servos for all esp32 chips

class AServo : public Actuator{
public:
    /**
     * @brief Construct a new AServo object
     * 
     * @param minVal Minimum value that can be used to actuate
     * @param maxVal Maximum value that can be used to actuate
     * @param offsetVal Offset value for correction of error
     * @param pinVal Pin used for the component
     * @param periodHertz The frequency at which the component is actuated
     */
    AServo(int minVal, int maxVal, int offsetVal, int pinVal, int periodHertz);

    AServo(int pinVal);

    /**
     * @brief Actuates the value given to the servo motor
     * 
     * @param value The value to actuate
     */
    void act(float value);

private:
    Servo servo;
    int period_hertz;
};


#endif //BLIMPSWARM_ASERVO_H
