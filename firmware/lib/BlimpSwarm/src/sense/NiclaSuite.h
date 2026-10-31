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
    const NiclaFeedback& niclaFeedback() const { return nicla.feedback(); }
    void resetNiclaTarget() { nicla.resetTarget(); }
    VerticalDiagnostics verticalDiagnostics() const { return sensorSuite.verticalDiagnostics(); }
    void publishFlowVertical(const VerticalDiagnostics& d){
        sensorSuite.publishFlowVertical(d);
        sensorValues[0]=0;sensorValues[1]=d.healthy?d.filteredHeight:0.f;
        sensorValues[2]=d.healthy?d.filteredVelocity:0.f;
    }

private:
    Nicla nicla;
    SensorSuite sensorSuite;
    float sensorValues[21]; // 11 SensorSuite + 10 Nicla Vision
    int valueCount; // Keep track of the total number of values stored
};

#endif
