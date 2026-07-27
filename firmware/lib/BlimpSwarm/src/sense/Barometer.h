/**
 * @file Barometer.h
 * @author David Saldana
 * @brief Legacy File.
 *        Manages the values from the BMP3XX(barometer) models. 
 * @version 0.1
 * @date 2024-07-14
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include <Adafruit_BMP3XX.h>
class BarometerOld
{
private:
    unsigned long tStart = micros();
    unsigned long dtBaro = 0;
    bool baroOn = false;
    Adafruit_BMP3XX bme;
    uint16_t BMP390_SAMPLERATE_DELAY_MS = 10; //how often to read data from the board
    float estimatedZ = 0;
    float oldZ = 0;
public:
    /**
     * @brief Construct a new Barometer Old object.
     * 
     */
    BarometerOld();

    /**
     * @brief Attempts to initialize the BMP with desired values. Sets the ground to current height.
     * 
     */
    void init();

    /**
     * @brief Get the value of estimatedZ
     * 
     * @return float Value representing the estimated height of the robot.
     */
    float getEstimatedZ();

    /**
     * @brief Gets the value of the estimated velocity of the robot.
     * 
     * @return float Value representing the velocity of the robot.
     */
    float getVelocityZ();

    /**
     * @brief Reads the latest altitude and associated values from the BMP.
     * 
     * @return true Values were successfully read.
     * @return false Values were not successfully read.
     */
    bool updateBarometer();
};