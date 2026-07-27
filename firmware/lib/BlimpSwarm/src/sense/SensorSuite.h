/**
 * @file SensorSuite.h
 * @author David Saldana
 * @brief Suite of software used to collect values from the onboard sensors
 * @version 0.1
 * @date 2024-01-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#ifndef SensorSuite_h
#define SensorSuite_h


#include "SensorInterface.h"
#include "Barometer390.h"
#include "BNO85.h"
#include "BatterySensor.h"
#include "GY_US42V2.h"

class SensorSuite : public SensorInterface {
public:
    // SensorSuite();
    // virtual ~SensorSuite();
    
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

private:
    Barometer barometer;
    BNO85 bnoSensor;
    WeightedBatterySensor batterySensor;
    GY_US42V2 ultrasonicSensor; 

    float sensorValues[16]; // Adjust based on actual data size needed
    int valueCount; // Keep track of the total number of values stored
};

#endif
