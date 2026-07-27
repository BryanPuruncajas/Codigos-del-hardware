/**
 * @file LLC_ESPNow.h
 * @author David Saldana
 * @brief Sets up async functions for communication and represents a communication station
 * @version 0.1
 * @date 2024-01-28
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef ESPNOWCOMM_H
#define ESPNOWCOMM_H


#include "LowLevelComm.h"
#include "WiFi.h"
#include <esp_now.h> // This is the arduino library for ESP-NOW
#include "util/ParamManager.h"

/**
 * @brief Async callback function that handles the data received from fellow peers
 * 
 * @param mac_addr The mac address that sent the information
 * @param data The data received from the address
 * @param data_len The length of the buffer
 */
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len);

/**
 * @brief Async callback function that handles the data sent to fellow peers
 * 
 * @param mac_addr The mac address to receive the information
 * @param status Status indicating whether the information was successfully sent
 */
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);

class LLC_ESPNow : public LowLevelComm {
public:

    /**
     * @brief Construct a new llc espnow object
     * 
     */
    LLC_ESPNow();

    /**
     * @copydoc LowLevelComm::init()
     * 
     */
    void init() override;

    /**
     * @copydoc LowLevelComm::sendData()
     */
    void sendData(const uint8_t mac_addr[6], const uint8_t* data, unsigned int length) override;

    /**
     * @copydoc LowLevelComm::receiveData()
     */
    void receiveData(uint8_t receivedData[MAX_DATA_SIZE], int& length) override;

    /**
     * @copydoc LowLevelComm::newData()
     */
    bool newData() override;

    /**
     * @copydoc LowLevelComm::addPeer()
     */
    void addPeer(const uint8_t *peerAddr) override;

    /**
     * @copydoc LowLevelComm::removePeer()
     */
    void removePeer(const uint8_t *peerAddr) override;

private:
    const int MAC_ADDRESS_SIZE = 6;
    esp_now_peer_info_t peerInfo;
};






#endif // ESPNOWCOMM_H