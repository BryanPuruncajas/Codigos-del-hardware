/**
 * @file BaseCommunicator.h
 * @author David Saldana
 * @brief Handles the creation of communication stations and the p2p management
 * @version 0.1
 * @date 2024-01-28
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef BLIMPSWARM_BASECOMMUNICATOR_H
#define BLIMPSWARM_BASECOMMUNICATOR_H

#include <cstdint>
#include "LowLevelComm.h"
#include "util/DataTypes.h"
#include "util/ParamManager.h"
#include <Preferences.h>

class BaseCommunicator {
public:
    /**
     * @brief Construct a new Base Communicator object
     * 
     * @param comm The type of communication station to create
     */
    explicit BaseCommunicator(LowLevelComm* comm); // Constructor declaration

    /**
     * @brief Sets the main station parameter to the current station
     * 
     */
    void setMainBaseStation();

    /**
     * @brief Adds a ping station to the list of stations to send pings to.
     * 
     * @param mac_addr The mac address of the ping station to be added to memory
     */
    void addPingStation(const uint8_t mac_addr[6]);

    /**
     * @brief Sends a ping message to each station in the list
     * 
     */
    void pingStations();

    /**
     * @brief Sends measurements to the base station
     * 
     * @param measurements The data to be sent to the base station
     * @return true Data was successfully sent
     * @return false Data was not sent
     */
    bool sendMeasurements(ReceivedData* measurements);

    /**
     * @brief Checks for new data and reads it if new data was received.
     * 
     * @return true Data was successfully received
     * @return false Data was not received
     */
    bool readNewMessages();
    
    /**
     * @brief Takes the data from readNewMessages() and returns it to be used.
     * 
     * @return ControlInput Returns the commands received from the base station
     */
    ControlInput receiveMsgCmd();

    /**
     * @brief Reads the latest message and checks if it is a new message.
     * 
     * @return true The message was a new message
     * @return false The message isn't a new message
     */
    bool isNewMsgCmd();

private:
    LowLevelComm* comm; // Object that manager the low level communication
    uint8_t main_station_mac[6];  // Main station mac

    static const int MAX_PING_STATIONS = 5; // Maximum number of ping stations
    uint8_t pStations[MAX_PING_STATIONS][6]; // Array to store MAC addresses of ping stations
    int numPingStations = 0; // Number of ping stations currently added
    Preferences preferences;

    unsigned long previousMillis = 0; // Stores the last time a message was sent
    const long interval = 333; // Interval at which to run the sender (milliseconds) //TODO make SSD

    // Messages
    ControlInput* msgCmd;
    bool newMsgCmd = false;
    ReceivedData* msgMeasure;
    bool newMeasure = false;
    //updateParam(const uint8_t* data, unsigned int length); //TODO
};


#endif //BLIMPSWARM_BASECOMMUNICATOR_H
