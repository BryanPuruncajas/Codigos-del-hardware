/**
 * @file BNO85.h
 * @author David Saldana
 * @brief Manages the sensor values from the BNO85
 * @version 0.1
 * @date 2024-01-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#ifndef BNO85_h
#define BNO85_h

#include <Wire.h>
#include "SensorInterface.h"
#include "SparkFun_BNO08x_Arduino_Library.h"
#include <math.h>

class BNO85 : public SensorInterface {
public:
    /**
     * @brief Construct a new BNO85 object.
     * 
     */
    BNO85();

    /**
     * @brief Initializes the BNO85 object.
     * 
     */
    void startup() override;

    /**
     * @copydoc SensorInterface::update()
     */
    bool update() override;

    /**
     * @copydoc SensorInterface::readValues()
     */
    float* readValues(int& count) override;

    /**
     * @copydoc SensorInterface::getPreferences()
     * 
     */
    void getPreferences();

    /**
     * @brief Set the outputs that the user wants to receive.
     * 
     */
    void setReports();
    
    unsigned long startTime;
    // Additional methods as necessary

private:
    BNO08x myIMU;
    float pitchgamma = 0;
    float rollgamma = 0;
    float yawgamma = 0;
    bool bnoOn = false;
    float sensorValues[6]; // Array to store roll, pitch, yaw, rollRate, pitchRate, yawRate
    unsigned long restartLength = 500000;
    
    // Define sensor data structure here if needed
};

#endif
