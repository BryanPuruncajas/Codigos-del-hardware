/*
  Project: GY-US42V2 Ultrasonic Distance Sensor Interface
  Description: This code interfaces with the GY-US42V2 ultrasonic distance sensor using the I2C protocol,
               reads the distance, and prints it to the serial monitor.
  Author: Diego Salazar
  Date: 2023-10-24
*/

#include "GY_US42V2.h"  // Include the header file

// Constructor implementation
GY_US42V2::GY_US42V2(uint8_t address) : sensorAddress(address) {
    Wire.begin();  // Initialize the I2C bus
    delay(100);
    for (int i = 0; i < 3; ++i) {
        distanceBuffer[i] = 720;  // Initialize buffer with an invalid distance value
    }
}

// Method to read distance from sensor
uint16_t GY_US42V2::readDistance() {
    Wire.beginTransmission(sensorAddress);  // Begin transmission to sensor
    Wire.write(RANGE_COMMAND);  // Send command to start a ranging cycle in cm
    Wire.endTransmission();  // End transmission
    //delay(100);  // Wait for ranging to complete

    Wire.requestFrom(sensorAddress, READ_BYTES);  // Request 2 bytes of data from sensor
    if (Wire.available() == READ_BYTES) {  // Check if data is available
        uint16_t distance = Wire.read() << 8 | Wire.read();  // Combine the two bytes into a 16-bit number
        distanceBuffer[bufferIndex] = distance;  // Store distance in buffer
        bufferIndex = (bufferIndex + 1) % 3;  // Move to next position in circular buffer

        distance = getMinimumDistance();
        return distance;  // Return the distance value
    } else {
        return 0xFFFF;  // Return 0xFFFF if data is not available to indicate an error
    }
}


// Method to get the minimum distance from the last three readings
uint16_t GY_US42V2::getMinimumDistance() {
  uint16_t minDistance = distanceBuffer[0];
  for (int i = 1; i < 3; ++i) {
    if (distanceBuffer[i] < minDistance) {
      minDistance = distanceBuffer[i];
    }
  }
  return minDistance;
}