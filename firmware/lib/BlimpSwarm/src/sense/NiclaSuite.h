/**
 * @file NiclaSuite.h
 * @author David Saldana
 * @brief Suite of software for all the sensors including the Nicla Vision
 * @version 0.1
 * @date 2024-01-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#ifndef NiclaSuite_h
#define NiclaSuite_h

#include "SensorSuite.h"
#include "Nicla.h"


class NiclaSuite : public SensorInterface {
public:
    /**
     * @copydoc SensorInterface::startup()
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
     * @brief Changes the Nicla Vision's mode to the desired mode
     * 
     * @param setMode The desired mode to change the Nicla Vision to.
     */
    void changeNiclaMode(uint8_t setMode);

private:
    Nicla nicla;
    SensorSuite sensorSuite;
    float sensorValues[22]; // Adjust based on actual data size needed (21 + 1 for ultrasonicSensor distance)
    int valueCount; // Keep track of the total number of values stored
};

#endif
