/**
 * @file ParamManager.h
 * @author David Saldana
 * @brief Manages the parsing and adjustments of the parameters saved on the esp32 flash memory.
 * @version 0.1
 * @date 2024-02-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#ifndef BLIMPSWARM_PARAMMANAGER_H
#define BLIMPSWARM_PARAMMANAGER_H


#include <Preferences.h>
#include <Arduino.h>
#include <float.h>
#include "util/DataTypes.h"

// Other robot types would be included here

class ParamManager {
public:
    /**
     * @brief Parses incoming parameter commands and sets the parameters.
     * 
     * @param data The incoming data received.
     * @param data_len The length of the received data.
     */
    void parseAndSetPreference(const uint8_t* data, int data_len);

    /**
     * @brief Set the ground mac parameter of the robot.
     * 
     * @param mac_addr The mac address for the robot.
     */
    void setGroundMac(const uint8_t mac_addr[6]);

private:
    Preferences preferences;

    /**
     * @brief Converts incoming data into a string to be printed and used.
     * 
     * @param data The incoming data.
     * @param index The index at which to start reading the data.
     * @param length The length of the data.
     * @return String The string conversion of data.
     */
    String readString(const uint8_t *data, int &index, uint8_t length);
};



#endif //BLIMPSWARM_PARAMMANAGER_H
