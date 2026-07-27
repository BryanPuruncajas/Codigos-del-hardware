/**
 * @file ParamManager.cpp
 * @author David Saldana
 * @brief Implementation of ParamManager.h
 * @version 0.1
 * @date 2024-02-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "ParamManager.h"

String ParamManager::readString(const uint8_t *data, int &index, uint8_t length) {
    String result = String((char*)&data[index], length);
    index += length; // Advance index past the read string
    return result;
}

void ParamManager::parseAndSetPreference(const uint8_t* data, int data_len) {
    preferences.begin("params", false); 
    
    int index = 1; // First value is in index 1, since first byte is for determining the message type
    uint8_t datatype = data[index++];
    uint8_t keyLength = data[index++];
    String key = String((char*)&data[index], keyLength);
    index += keyLength;

    if (datatype == DataType_Int) {
        if (data_len >= index + sizeof(int)) {
            int newValue = *(reinterpret_cast<const int*>(&data[index]));
            int currentValue = preferences.getInt(key.c_str(), INT_MIN);
            if (currentValue != newValue) {
                preferences.putInt(key.c_str(), newValue);
            }
        }
    } else if (datatype == DataType_Float) {
        if (data_len >= index + sizeof(float)) {
            float newValue = *(reinterpret_cast<const float*>(&data[index]));
            float currentValue = preferences.getFloat(key.c_str(), FLT_MIN);
            if (currentValue != newValue) {
                preferences.putFloat(key.c_str(), newValue);
            }
        }
    } else if (datatype == DataType_String) {
        String newValue = String((char*)&data[index], data_len - index);
        String currentValue = preferences.getString(key.c_str(), "");
        if (currentValue != newValue) {
            preferences.putString(key.c_str(), newValue);
        }
    } else if (datatype == DataType_Bool) {
        if (data_len >= index + sizeof(bool)) {
            bool newValue = *(reinterpret_cast<const bool*>(&data[index]));
            bool currentValue = preferences.getBool(key.c_str(), 0);
            if (currentValue != newValue) {
                preferences.putBool(key.c_str(), newValue);
            }
        }
    }

    preferences.end();
}
void ParamManager::setGroundMac(const uint8_t mac_addr[6]) {
    preferences.begin("params", false);
    preferences.putBytes("GroundMac", mac_addr, 6);
    preferences.end(); 
}