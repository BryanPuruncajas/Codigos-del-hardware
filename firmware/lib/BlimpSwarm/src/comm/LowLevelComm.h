/**
 * @file LowLevelComm.h
 * @author David Saldana
 * @brief Virtual class for setting up low level communication protocols
 * @version 0.1
 * @date 2024-01-22
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef BLIMPSWARM_LOWLEVELCOMM_H
#define BLIMPSWARM_LOWLEVELCOMM_H
#include <Arduino.h>

#define MAX_DATA_SIZE 100


class LowLevelComm {
public:
    /**
     * @brief Initializes the async functions and starts up the board for communication
     * 
     */
    virtual void init() = 0;
    
    /**
     * @brief Function that sends data to the given mac address
     * 
     * @param mac_addr The mac address to receive the information
     * @param data The data to be sent
     * @param length The buffer length of the data
     */
    virtual void sendData(const uint8_t mac_addr[6], const uint8_t* data, unsigned int length) = 0;

    /**
     * @brief Function that copies the data received from the async function into a specified buffer
     * 
     * @param receivedData The buffer to store the intercepted data
     * @param length The length of the buffer
     */
    virtual void receiveData(uint8_t receivedData[MAX_DATA_SIZE], int& length) = 0;

    /**
     * @brief Function to check whether there was new data that was received
     * 
     * @return true New data was received
     * @return false New data was not received
     */
    virtual bool newData() = 0;

    /**
     * @brief Function that adds a peer to the list of communication peers
     * 
     * @param peerAddr A list of addresses that are connected with this board
     */
    virtual void addPeer(const uint8_t *peerAddr) = 0;

    /**
     * @brief Function that removes a peer from the list of communication peers
     * 
     * @param peerAddr A list of addresses that are connected with this board
     */
    virtual void removePeer(const uint8_t* peerAddr) = 0;
};




#endif //BLIMPSWARM_LOWLEVELCOMM_H
