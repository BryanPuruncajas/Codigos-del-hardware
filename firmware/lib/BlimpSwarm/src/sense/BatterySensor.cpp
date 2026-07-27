/**
 * @file BatterySensor.cpp
 * @author Edward Jeff
 * @brief Implementation of BatterySensor.h
 * @version 0.1
 * @date 2024-02-28
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "BatterySensor.h"

BatterySensor::BatterySensor(int pin, float referenceVoltage) : pin(pin), referenceVoltage(referenceVoltage) {
}

void BatterySensor::startup() {
    BatterySensor::getPreferences();
    pinMode(pin, INPUT);
}
bool BatterySensor::update() {
    int analogValue = analogRead(pin); // Read the analog value (0 to 1023)
    value[0] = (analogValue * referenceVoltage) / 1023.0; // Convert to voltage
    return true;
}

float* BatterySensor::readValues(int& count) {
    count = 1; // BatterySensor only has one value
    return value;
}

void BatterySensor::getPreferences(){
    return;
}

WeightedBatterySensor::WeightedBatterySensor( float gamma)
:  filteredValue(0.0), gamma(gamma), valueCount(1) {
}

WeightedBatterySensor::~WeightedBatterySensor() {
    // Clean-up code if needed
}
void WeightedBatterySensor::startup(int pin, float referenceVoltage) {
    getPreferences();
    sensor = new BatterySensor(pin, referenceVoltage);
    sensor->startup();
}

void WeightedBatterySensor::startup() {
    sensor = new BatterySensor(D8, 1.7);
    sensor->startup();
}


bool WeightedBatterySensor::update() {
    if (sensor->update()) {
        int count;
        float* newValue = sensor->readValues(count);
        filteredValue = gamma * filteredValue + (1 - gamma) * newValue[0];
        return true; // Value updated
    }
    return false; // No update performed
}

float* WeightedBatterySensor::readValues(int& count) {
    count = valueCount;
    return &filteredValue;
}
void WeightedBatterySensor::getPreferences(){
    return;

}