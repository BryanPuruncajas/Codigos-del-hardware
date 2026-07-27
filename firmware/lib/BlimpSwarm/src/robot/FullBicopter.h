/**
 * @file FullBicopter.h
 * @author Edward Jeff
 * @brief Robot class that includes all aspect of robot including motors, servos, IMU, and Nicla Vision
 * @version 0.1
 * @date 2024-03-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef BLIMPSWARM_FULLBICOPTER_H
#define BLIMPSWARM_FULLBICOPTER_H



#include "RawBicopter.h"
#include "sense/NiclaSuite.h"
#include <Arduino.h>




class FullBicopter : public RawBicopter {
public:
    /**
     * @brief Construct a new Full Bicopter object.
     * Initializes the actuator, sensors, and the Nicla Vision.
     * 
     */
    FullBicopter();

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
    
    // Sensor interface
    NiclaSuite sensorsuite;

    // List of the variables that need persistant storage
    float z_integral = 0;
    float yaw_integral = 0;
    float yawrate_integral = 0;
    float servo_old1 = 0;
    float servo_old2 = 0;
    

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
};


#endif //BLIMPSWARM_FULLBICOPTER_H
