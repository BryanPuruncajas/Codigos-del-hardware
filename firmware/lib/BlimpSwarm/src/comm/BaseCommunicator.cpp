/**
 * @file BaseCommunicator.cpp
 * @author David Saldana
 * @brief Implementation of BaseCommunicator.h
 * @version 0.1
 * @date 2024-01-28
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "BaseCommunicator.h"

uint8_t receivedData[MAX_DATA_SIZE];
int receivedDataLength;

BaseCommunicator::BaseCommunicator(LowLevelComm* comm) : comm(comm) {
    comm->init();
}

void BaseCommunicator::setMainBaseStation(){
    preferences.begin("params", true);
    
    if (preferences.getBytesLength("GroundMac") == 6) {
        Serial.print("Main Base Set: ");
        uint8_t mac_addr[6];
        preferences.getBytes("GroundMac", mac_addr, 6);
        memcpy(this->main_station_mac, mac_addr, 6); // Copy the MAC address
        for (int i = 0; i < 6; ++i) {
            if (i > 0) {
                Serial.print(":");
            }
            // Print each byte in hexadecimal format
            if (this->main_station_mac[i] < 16) {
                Serial.print("0"); // Print a leading zero for values less than 0x10
            }
            Serial.print(this->main_station_mac[i], HEX);
        }
        Serial.println();
        comm->addPeer(this->main_station_mac);
        this->addPingStation(this->main_station_mac);  // The main station is also a ping station.
    } else {
        Serial.println("Main Base Not Set.");
    }
    preferences.end(); 
}


void BaseCommunicator::addPingStation(const uint8_t mac_addr[6]) {
    if (numPingStations < MAX_PING_STATIONS) {
        memcpy(pStations[numPingStations], mac_addr, 6); // Add new ping station MAC address
        numPingStations++; // Increment the count of ping stations
    } else {
        //TODO: Handle the case where the ping stations array is full
        // This could be logging an error message or ignoring the add request
    }
}

void BaseCommunicator::pingStations() {
    for (int i = 0; i < numPingStations; ++i) {
        // Construct ping message
        // Use comm.sendData(pingStations[i], pingMessage, pingMessageLength);
        //TODO: define a short message for pinging.
        // TODO: send ping
    }

}


bool BaseCommunicator::sendMeasurements(ReceivedData* measurements){
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        // Save the last time you executed the code
        previousMillis = currentMillis;

        // Place the code you want to run at a rate of 5 times per second here
        comm->sendData(this->main_station_mac,(uint8_t *) measurements, sizeof(ReceivedData));
        return true;
    }
    return false;  //fixme ??
}



bool BaseCommunicator::readNewMessages(){
    // Check for new messages
    if (comm->newData()){
        // Check if the message is a command
        comm->receiveData(receivedData, receivedDataLength);

        //TODO different types of messages
        
        if (receivedDataLength == sizeof(ControlInput)){
            msgCmd = (ControlInput *) receivedData;
            newMsgCmd = true;
        }else if(receivedDataLength== sizeof(ControlInput)){
            //TODO this type of message
        }


        return true;
    }
    return false;

}


ControlInput BaseCommunicator::receiveMsgCmd(){
    newMsgCmd = false;
    return *msgCmd;
}

bool BaseCommunicator::isNewMsgCmd(){
    readNewMessages();
    return this->newMsgCmd;
}