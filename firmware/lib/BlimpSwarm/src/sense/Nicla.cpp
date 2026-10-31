/**
 * @file Nicla.cpp
 * @author Edward Jeff
 * @brief Implementation of Nicla.h
 * @version 0.1
 * @date 2024-03-12
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "Nicla.h"

HardwareSerial MySerial0(0);
Nicla::Nicla() {
    for (int i = 0; i < 10; i++) {
        value[i] = 0;
    }
}

void Nicla::startup() {
    Nicla::getPreferences();
    Serial.println("Starting IBUS Init");
    MySerial0.begin(115200, SERIAL_8N1, D7, D6);
    IBus.begin(MySerial0, IBUSBM_NOTIMER);
    resetReception();
    IBus.loop();
    MySerial0.write(mode); 
    sendTime = micros();
}

void Nicla::startup(uint8_t setMode) {
    Serial.println("Starting IBUS Init");
    MySerial0.begin(115200, SERIAL_8N1, D7, D6);
    IBus.begin(MySerial0, IBUSBM_NOTIMER);
    resetReception();
    IBus.loop();
    mode = setMode; 
    MySerial0.write(mode); 
    sendTime = micros();
}

//data = [flag, x_roi, y_roi, w_roi, h_roi, x_value, y_value, w_value, h_value, dis]
bool Nicla::update() { 
    IBus.loop();
    const uint32_t now = millis();
    const uint8_t count = IBus.cnt_rec; // advances only after checksum validation
    const bool received = count != lastRxCount;
    if (received) {
        lastRxCount = count;
        lastRxMs = now;
        rxSeen = true;
        const uint16_t protocol = IBus.readChannel(12);
        const uint16_t sample = IBus.readChannel(10);
        const bool n03 = protocol == NiclaFeedback::N03_PROTOCOL;
        if (!n03 || !sampleSeen || protocol != feedback_.protocol || sample != feedback_.sample) {
            ++feedback_.generation;
            lastSampleMs = now;
            sampleSeen = true;
        }
        feedback_.sample = sample;
        feedback_.protocol = protocol;
        feedback_.state = n03 ? IBus.readChannel(11) : NiclaFeedback::SEARCH;
        const uint16_t ack = IBus.readChannel(13);
        if (feedback_.resetPending && ack != feedback_.resetAck) feedback_.resetPending = false;
        feedback_.resetAck = ack;
        for (int i = 0; i < 10; i++) {
            value[i] = (float)IBus.readChannel(i);
        }
    }
    feedback_.fresh = rxSeen && sampleSeen && !feedback_.resetPending &&
        (uint32_t)(now - lastRxMs) < RX_TIMEOUT_MS &&
        (uint32_t)(now - lastSampleMs) < RX_TIMEOUT_MS;
    if (!feedback_.fresh) {
        invalidateValues();
    }
    // NiclaSuite copies values on every update, including invalidation.
    static uint32_t diagnosticMs = 0;
    static uint32_t totalFrames = 0;
    static uint8_t diagnosticCount = 0;
    totalFrames += (uint8_t)(count - diagnosticCount);
    diagnosticCount = count;
    if ((uint32_t)(now - diagnosticMs) >= 500) {
        diagnosticMs = now;
        Serial.printf("[NICLA_RX] frames=%lu fresh=%u flag=%.0f x=%.0f y=%.0f w=%.0f h=%.0f sample=%u state=%u protocol=%u reset_pending=%u\n",
            (unsigned long)totalFrames,
            (unsigned)feedback_.fresh,
            value[0], value[5], value[6], value[7], value[8],
            feedback_.sample, feedback_.state, feedback_.protocol, (unsigned)feedback_.resetPending);
    }
    return received;
}

void Nicla::invalidateValues() {
    for (int i = 0; i < 9; i++) value[i] = 0;
    value[9] = 9999; // existing protocol sentinel; never a measured distance
}

void Nicla::resetReception() {
    lastRxCount = IBus.cnt_rec;
    lastRxMs = 0;
    rxSeen = false;
    sampleSeen = false;
    feedback_ = NiclaFeedback{};
    invalidateValues();
}

void Nicla::resetTarget() {
    // One-byte N03 command, ESP D6/TX -> Nicla RX. No effect on legacy mode.
    if (feedback_.protocol == NiclaFeedback::N03_PROTOCOL) {
        MySerial0.write((uint8_t)'R');
        feedback_.resetPending = true;
    }
    feedback_.fresh = false;
    invalidateValues();
}

//data = [flag, x_roi, y_roi, w_roi, h_roi, x_value, y_value, w_value, h_value, dis]
bool Nicla::update(uint8_t setMode) {
    MySerial0.write(setMode); 

    return Nicla::update();
    
}


float* Nicla::readValues(int& count) {
    count = 10; 
    return value;
}

void Nicla::getPreferences(){
    return;
}
