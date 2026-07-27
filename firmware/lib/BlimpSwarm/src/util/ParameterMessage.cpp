/**
 * @file ParameterMessage.cpp
 * @author David Saldana
 * @brief Legacy file for handling the P2P communications
 * @version 0.1
 * @date 2024-07-15
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include <Arduino.h>
#include "DataTypes.h"

#define MAX_MESSAGE_SIZE 64 // Adjust based on your expected maximum message size

// Datatype identifiers
#define MessageType_Parameter 0x00

// Utility function to add data to the buffer
int addToBuffer(uint8_t* buffer, int offset, const uint8_t* data, int data_len) {
    if (offset + data_len > MAX_MESSAGE_SIZE) return offset; // No space left, handle this properly
    memcpy(buffer + offset, data, data_len);
    return offset + data_len; // New offset after adding data
}

// Example function to create an int message
int createMessageInt(uint8_t* buffer, const char* key, int value) {
    int offset = 0;
    buffer[offset++] = MessageType_Parameter; // Message type
    buffer[offset++] = DataType_Int; // Data type
    int keyLen = strlen(key);
    buffer[offset++] = keyLen; // Key length
    offset = addToBuffer(buffer, offset, (const uint8_t*)key, keyLen); // Key
    offset = addToBuffer(buffer, offset, (const uint8_t*)&value, sizeof(value)); // Value
    return offset; // Total message length
}

// Example function to create a float message 
int createMessageFloat(uint8_t* buffer, const char* key, float value) {
    int offset = 0;
    buffer[offset++] = MessageType_Parameter; // Message type
    buffer[offset++] = DataType_Float; // Data type
    int keyLen = strlen(key);
    buffer[offset++] = keyLen; // Key length
    offset = addToBuffer(buffer, offset, (const uint8_t*)key, keyLen); // Key
    offset = addToBuffer(buffer, offset, (const uint8_t*)&value, sizeof(value)); // Value
    return offset; // Total message length
}

// Example function to create a string message
int createMessageString(uint8_t* buffer, const char* key, const char* value) {
    int offset = 0;
    buffer[offset++] = MessageType_Parameter; // Message type
    buffer[offset++] = DataType_String; // Data type
    int keyLen = strlen(key);
    buffer[offset++] = keyLen; // Key length
    offset = addToBuffer(buffer, offset, (const uint8_t*)key, keyLen); // Key
    int valueLen = strlen(value);
    offset = addToBuffer(buffer, offset, (const uint8_t*)value, valueLen); // Value
    return offset; // Total message length
}

// void setup() {
//     Serial.begin(115200);
//     uint8_t messageBuffer[MAX_MESSAGE_SIZE]; // Pre-allocated buffer for message creation

//     // Creating an int message
//     int messageLength = createMessageInt(messageBuffer, "temp", 25);
//     // Send or process the message...
//     // For demonstration, printing the message bytes
//     for (int i = 0; i < messageLength; ++i) {
//         Serial.print(messageBuffer[i], HEX);
//         Serial.print(" ");
//     }
//     Serial.println();

//     // Repeat for float and string messages as needed
// }

// void loop() {
//   // Empty loop
// }