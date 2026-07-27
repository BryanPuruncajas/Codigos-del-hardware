/**
 * @file BLMotor.h
 * @author David Saldana
 * @brief Actuation of brushless motors for a XIAO ESP32S3
 * @version 0.1
 * @date 2024-02-09
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef BLIMPSWARM_BLMOTOR_H
#define BLIMPSWARM_BLMOTOR_H

#include "Actuator.h"
#include <ESP32Servo.h>

class BLMotor : public Actuator {
public:
    /**
     * @brief Construct a new BLMotor object
     * 
     * @param minVal Minimum value that can be used to actuate
     * @param maxVal Maximum value that can be used to actuate
     * @param offsetVal Offset value for correction of error
     * @param pinVal Pin used for the component
     * @param periodHertz The frequency at which the component is actuated
     */
    BLMotor(int minVal, int maxVal, int offsetVal, int pinVal, int periodHertz);

    BLMotor(int pinVal);

    /**
     * @brief Calibrates the ESCs to properly interpret values received
     * 
     */
    void calibrate();

    /**
     * @brief Arms the motors to prevent calibration during flight
     * 
     */
    void arm();

    /**
     * @brief Actuates the value given to the brushless motor
     * 
     * @param value The value to actuate
     */
    void act(float value);

private:
    Servo thrust;
    int period_hertz;
    int min_thrust = 0;  // Force in grams  //TODO convert this into variables that can be loaded from the flash memory
    int max_thrust = 35;  // Force in grams  //TODO convert this into variables that can be loaded from the flash memory

    // Constants to convert PWM to Force. Equation: Force = a * PWM + b
    const float pwm_a = 0.042337045;  //TODO convert this into variables that can be loaded from the flash memory
    const float pwm_b = -46.58244237; //TODO convert this into variables that can be loaded from the flash memory
};


#endif //BLIMPSWARM_BLMOTOR_H
