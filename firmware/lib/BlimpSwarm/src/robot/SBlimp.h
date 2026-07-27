/**
 * @file SBlimp.h
 * @author David Saldana
 * @brief Robot that includes the actuation of four different motors
 * @version 0.1
 * @date 2024-01-20
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef BLIMPSWARM_SBLIMP_H
#define BLIMPSWARM_SBLIMP_H


#include "Robot.h"
#include <Arduino.h>
#include "act/BLMotor.h"
#include "act/LED.h"
#include <Preferences.h>

class SBlimp : public Robot {
public:
    /**
     * @brief Construct a new SBlimp object
     * 
     */
    SBlimp();

    /**
     * @copydoc Robot::startup()
     * 
     */
    void startup() override;

    /**
     * @copydoc Robot::sense()
     * 
     * @brief SBlimp does not include any sensors
     * 
     */
    int sense(float sensors[MAX_SENSORS]) override;

    /**
     * @copydoc Robot::actuate()
     * 
     */
    void actuate(const float actuators[], int size) override;

    /**
     * @copydoc Robot::control()
     * 
     * @brief Directly converts array of higher-level control commands into actuation
     * 
     */
    void control(float sensors[MAX_SENSORS], float controls[], int size ) override;

    /**
     * @copydoc Robot::calibrate
     * 
     */
    void calibrate() override;

    /**
     * @copydoc Robot::arm()
     * 
     */
    void arm() override;

    /**
     * @copydoc Robot::getPreferences()
     * 
     */
    void getPreferences() override;

private:
    BLMotor* motor1;
    BLMotor* motor2;
    BLMotor* motor3;
    BLMotor* motor4;
    LED* led;
};


#endif //BLIMPSWARM_SBLIMP_H
