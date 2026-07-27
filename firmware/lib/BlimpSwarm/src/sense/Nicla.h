/**
 * @file Nicla.h
 * @author Edward Jeff
 * @brief Manages the sensor values from the Nicla Vision through a Ibus connection.
 *        Uses UART protocol to communicate.
 * @version 0.1
 * @date 2024-03-12
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef BLIMPSWARM_NICLA_H
#define BLIMPSWARM_NICLA_H

#include "SensorInterface.h"
#include <cstddef>
#include <IBusBM.h>
#include <Arduino.h>

#define BALL_DET 0x80
#define GOAL_DET 0x81

class Nicla : public SensorInterface {
public:
    IBusBM IBus;

    /**
     * @brief Construct a new Nicla object.
     * 
     */
    Nicla();

    /**
     * @brief Initializes the Nicla Vision connection.
     *        Utilizes HardwareSerial to communicate with the Nicla Vision through the onboard UART pin.
     * 
     */
    void startup() override;

    /**
     * @brief Initializes the Nicla Vision connection.
     *        Utilizes HardwareSerial to communicate with the Nicla Vision through the onboard UART pin.
     * 
     * @param setMode The mode to initialize the Nicla Vision in.
     */
    void startup(uint8_t setMode);

    /**
     * @copydoc SensorInterface::update()
     */
    bool update() override;

    /**
     * @copydoc SensorInterface::update()
     * @brief Also adjusts the mode that the Nicla Vision needs to switch to.
     * 
     * @param setMode The desired mode for the Nicla Vision
     */
    bool update(uint8_t setMode);

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
    float value[10]; // Array to store the values returned by Nicla Vision
    uint8_t mode = GOAL_DET; // Default mode the Nicla Vision will start at
    unsigned long sendTime;
};


#endif