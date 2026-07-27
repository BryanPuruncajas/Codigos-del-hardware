/**
 * @file Barometer390.h
 * @author David Saldana
 * @brief Manages the values received from a BMP390(Barometer) sensor
 * @version 0.1
 * @date 2024-07-14
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#ifndef Barometer_h
#define Barometer_h

#include "SensorInterface.h"
#include <Adafruit_BMP3XX.h>

class Barometer : public SensorInterface {
private:
    float altitude; // Stores the calculated altitude
    float pressure; // Stores the latest pressure reading
    float temperature; // Stores the latest temperature reading
    Adafruit_BMP3XX bme;
    bool baroInitialized = false;
    unsigned long startTime;
    float velocityZ;
    float groundLevel = 0.0;
    // Other private members as necessary...

public:
    /**
     * @brief Construct and initializes new Barometer object.
     * 
     */
    Barometer();

    /**
     * @brief Initializes the BMP390 and sets the parameter values.
     * 
     */
    void startup() override;

    /**
     * @brief Updates the altitude and other values received from the BMP390. 
     * 
     * @return true Values were successfully updated.
     * @return false Vakues were not successfully updated.
     */
    bool update() override;

    /**
     * @brief Reads the stored values and returns them inside a float array.
     * 
     * @param count The number of values to be read.
     * @return float* Returns a float array containing the values that were read.
     */
    float* readValues(int& count) override;

    /**
     * @brief Get the parameters for the barometers.
     * 
     */
    void getPreferences();
    // Additional Barometer-specific methods...
};


#endif