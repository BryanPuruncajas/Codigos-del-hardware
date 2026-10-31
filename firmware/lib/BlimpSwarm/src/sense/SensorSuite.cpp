/**
 * @file SensorSuite.cpp
 * @author David Saldana
 * @brief Implementation of SensorSuite.h
 * @version 0.1
 * @date 2024-01-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "SensorSuite.h"

// SensorSuite::SensorSuite() {}

// SensorSuite::~SensorSuite() {
//     // Clean-up code if needed, e.g., delete sensor instances if owned
// }

void SensorSuite::startup() {
    SensorSuite::getPreferences();
    bnoSensor.startup();
    flowVertical=VerticalDiagnostics{};
    flowVertical.ageMs=UINT32_MAX;
    batterySensor.startup();
    for (int i = 0; i < 11; i++) {
        sensorValues[i] = 0;
    }
}


bool SensorSuite::update() {
    bool updated = false;
    int tempCount = 0, offset = 0;

    // No pressure or temperature acquisition. Slots 0..2 come from Flowdeck.
    offset = 3; 
    // Repeat the pattern for bnoSensor and batterySensor
    { // Service the IMU independently of the radio range sample.
        if (bnoSensor.update()) {
            float* bnoValues = bnoSensor.readValues(tempCount);
            for(int i = 0; i < tempCount; ++i) {
                sensorValues[offset + i] = bnoValues[i];
            }
            offset += tempCount;
            updated = true;
        }
    }
    offset = 10;
    if (batterySensor.update()) {
        float* batteryValues = batterySensor.readValues(tempCount);
        for(int i = 0; i < tempCount; ++i) {
            sensorValues[offset + i] = batteryValues[i];
        }
        offset += tempCount;
        updated = true;
    }
    // Sin ultrasonico: SensorSuite termina en indice 10 (11 valores).
    // La Nicla se concatena inmediatamente a partir del indice 11.
    offset = 11;

    valueCount = offset; // 11 valores base
    return updated;
}

float* SensorSuite::readValues(int& count) {
    count = valueCount; // Return the count of valid sensor values
    return sensorValues; // Return the array of sensor values
}



void SensorSuite::getPreferences() {
    bnoSensor.getPreferences();
    batterySensor.getPreferences();
    return;

}
