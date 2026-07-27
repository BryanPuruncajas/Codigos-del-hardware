/**
 * @file BatterySensor.h
 * @author Edward Jeff
 * @brief Manages the sensor values from the onboard battery connected through a pin.
 * @version 0.1
 * @date 2024-02-28
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#ifndef BLIMPSWARM_BATTERYSENSOR_H
#define BLIMPSWARM_BATTERYSENSOR_H

#include "SensorInterface.h"
#include <Arduino.h>

class BatterySensor : public SensorInterface {
public:
    /**
     * @brief Construct a new Battery Sensor object.
     * 
     * @param pin The pin associated with reading the resistor connected to the battery.
     * @param referenceVoltage Voltage to use as a reference for calculating the actual voltage.
     */
    BatterySensor(int pin, float referenceVoltage = 5.0);

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
    float value[1]; // Array to store the battery voltage
    int pin; // Analog pin where the battery is connected
    float referenceVoltage; // Reference voltage for analogRead, typically 5V or 3.3V
};


class WeightedBatterySensor : public SensorInterface {
public:
    /**
     * @brief Construct a new Weighted Battery Sensor object.
     * 
     * @param gamma Value used to adjust the sensor's output.
     */
    WeightedBatterySensor(float gamma = 0.999);

    /**
     * @brief Destroy the Weighted Battery Sensor object.
     * 
     */
    ~WeightedBatterySensor();

    /**
     * @brief Initializes the battery.
     * 
     * @param pin The pin associated with reading the resistor connected to the battery.
     * @param referenceVoltage Voltage to use as a reference for calculating the actual voltage.
     */
    void startup(int pin, float referenceVoltage);

    /**
     * @brief Initializes the battery. Default pin and referenceVoltage are D8 and 1.7 respectively.
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
    BatterySensor* sensor; // Assumes ownership of a BatterySensor
    float filteredValue; // Stores the exponentially weighted average
    float gamma; // Weighting factor for the filter. Larger gamma (between 0,1) means more preference to old values
    int valueCount; // To comply with the readValues method signature
};




#endif //BLIMPSWARM_BATTERYSENSOR_H
