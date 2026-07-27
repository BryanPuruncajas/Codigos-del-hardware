/**
 * @file SensorInterface.h
 * @author David Saldana
 * @brief Interface for all sensors used by the robots
 * @version 0.1
 * @date 2024-02-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#ifndef BLIMPSWARM_SENSOR_H
#define BLIMPSWARM_SENSOR_H

#include <Preferences.h>

class SensorInterface {
public:
    /**
     * @brief Destroy the sensor object.
     * 
     */
    virtual ~SensorInterface() {}

    /**
     * @brief Initializes sensors in case they are unable to run on startup (Example: I2C devices).
     * 
     */
    virtual void startup() = 0;

    /**
     * @brief Updates the sensor values.
     * 
     * @return true The values were successfully updated.
     * @return false The values were not successfully updated.
     */
    virtual bool update() = 0;

    /**
     * @brief Reads all the sensor values and returns them in a float array. 
     *        Takes the number of values as the input.
     * 
     * @param count The number of values to be read into
     * @return float* Pointer to a float array containing the values
     */
    virtual float* readValues(int& count) = 0;

    /**
     * @brief Gets ground station parameters for the sensor
     * 
     */
    virtual void getPreferences();
};


#endif //BLIMPSWARM_SENSOR_H
