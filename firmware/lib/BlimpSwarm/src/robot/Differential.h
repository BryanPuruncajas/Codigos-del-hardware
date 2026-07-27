/**
 * @file Differential.h
 * @author Thong Vu
 * @brief Robot class for the Differential robot
 * @version 0.1
 * @date 2024-09-26
 * 
 * @copyright Copyright (c) 2024
 * 
 */


#pragma once

#include "RawBicopter.h"
#include "sense/NiclaSuite.h"
#include <Arduino.h>


class Differential : public RawBicopter {
private:
    /**
     * @brief Applies controllers based on the values retrieved from the sensors.
     * 
     * @param sensors Float array of values received from sensors
     * @param controls Float array of values received from controller input
     * @param feedbackControls The resulting control array after input is added
     */
    void addFeedback(float sensors[MAX_SENSORS], float controls[], float feedbackControls[]);

    /**
     * @brief Controls the outputs after feedback to send them to the actuators
     * 
     * @param sensors Float array of values received from sensors
     * @param controls Float array of values received from controller input
     * @param outputs The resulting control array after controlled
     */
    void getOutputs(float sensors[MAX_SENSORS], float controls[], float outputs[]);

public:
    // Sensor interface
    NiclaSuite sensorsuite;

    // List of the variables that need persistant storage
    float z_integral = 0;
    float yaw_integral = 0;
    float yawrate_integral = 0;
    float servo_old1 = 0;
    float servo_old2 = 0;
    /**
     * @brief Construct a new Differential object.
     * Initializes the actuator, sensors, and the Nicla Vision.
     * 
     */
    Differential();

    /**
     * @copydoc RawBicopter::sense()
     * 
     * @brief Reads all the values from the sensors on the main board. Includes the BNO and BMP.
     */
    int sense(float sensors[MAX_SENSORS]) override;

    /**
     * @copydoc RawBicopter::control()
     * 
     * @brief Takes the raw commands, adds feedback to those commands,
     * and then calculates the values to actuate.
     */
    void control(float sensors[MAX_SENSORS], float controls[], int size) override;

    /**
     * @copydoc RawBicopter::getPreferences()
     * 
     */
    void getPreferences() override;

    /**
     * @copydoc RawBicopter::startup()
     * 
     */
    void startup() override;
};

