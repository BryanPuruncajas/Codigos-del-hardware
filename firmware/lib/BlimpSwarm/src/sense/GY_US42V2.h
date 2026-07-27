/*
  Project: GY-US42V2 Ultrasonic Distance Sensor Interface
  Description: This code interfaces with the GY-US42V2 ultrasonic distance sensor using the I2C protocol,
               reads the distance, and prints it to the serial monitor.
  Author: Diego Salazar
  Date: 2023-10-24
*/

#ifndef GY_US42V2_H  // Include guard to prevent multiple declarations
#define GY_US42V2_H

#include <Arduino.h>  // Arduino core library
#include <Wire.h>     // I2C library

// GY_US42V2 class declaration
class GY_US42V2 {
public:
    GY_US42V2(uint8_t address = 0x70);  // Constructor with default I2C address
    uint16_t readDistance();  // Method to read distance from sensor
    uint16_t getMinimumDistance();

private:
    const uint8_t sensorAddress;  // I2C address of the sensor
    static const uint8_t RANGE_COMMAND = 0x51;  // Command to start a ranging cycle in cm
    static const uint8_t READ_BYTES = 2;  // Number of bytes to read from the sensor
    uint16_t distanceBuffer[3];
    uint8_t bufferIndex;
};

#endif // GY_US42V2_H  // End of include guard
