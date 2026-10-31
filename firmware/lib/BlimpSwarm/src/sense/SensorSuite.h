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
#include "VerticalDiagnostics.h"
#include "BNO85.h"
#include "BatterySensor.h"

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
    VerticalDiagnostics verticalDiagnostics() const { return flowVertical; }
    void publishFlowVertical(const VerticalDiagnostics& d){
        flowVertical=d;
        sensorValues[0]=0; // temperature is unavailable: BMP390 is not sampled
        sensorValues[1]=d.healthy?d.filteredHeight:0.f;
        sensorValues[2]=d.healthy?d.filteredVelocity:0.f;
    }

private:
    VerticalDiagnostics flowVertical{};
    BNO85 bnoSensor;
    WeightedBatterySensor batterySensor;
    // Sensors: IMU + battery. Vertical data is injected from Flowdeck only.
    // No hay sensor ultrasonico instalado en esta version de la tesis.
    float sensorValues[11]{};
    int valueCount=11; // Keep track of the total number of values stored
};

#endif
