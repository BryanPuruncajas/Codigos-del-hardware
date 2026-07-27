/**
 * @file BNO85.cpp
 * @author David Saldana
 * @brief Implementation of BNO85.h
 * @version 0.1
 * @date 2024-07-15
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "BNO85.h"

BNO85::BNO85() {
    // Constructor logic if necessary
    restartLength = 5000000;
    bnoOn = false;
}

void BNO85::startup() {
    // if (bnoOn) return;
    BNO85::getPreferences();
    if (bnoOn){
        return;
    }
    startTime = micros();
    Serial.println("BNO Initialization!");
    Wire.begin(D4, D5); 
    delay(100);
    // Wire.setClock(400000);
    int tempcount = 0;
    bool connect = false;
    if (myIMU.begin(0x4A, Wire)) {
        tempcount++;
        Serial.print("  connecting! ");
        connect = myIMU.isConnected();
        Serial.println(connect);
        
        Serial.print("  ");
        
        delay(50 + tempcount * 50);
        
    } else  {
        Serial.println("Ooops, no BNO085 detected ... Check your wiring or I2C ADDR!");
        startTime = micros();
        return;
    }
    Wire.setClock(400000); //Increase I2C data rate to 400kHz
    Serial.print("  BNO started: ");
    connect = myIMU.isConnected();
    Serial.println(connect);
    myIMU.softReset();
    myIMU.modeOn();
    bnoOn = true;
    startTime = micros();
    for (int i = 0; i < 6; i++) {
        sensorValues[i] = 0.0f;
    }
    // if (myIMU.wasReset()) {
    //     setReports();
    // }
}

// Here is where you define the sensor outputs you want to receive
void BNO85::setReports() {
    Serial.println("Setting desired reports");
    if (myIMU.enableGyroIntegratedRotationVector() == true) {
        Serial.println(F("Gryo Integrated Rotation vector enabled"));
        Serial.println(F("Output in form i, j, k, real, gyroX, gyroY, gyroZ"));
    } else {
        Serial.println("Could not enable gyro integrated rotation vector");
    }
    // Serial.println("  Setting desired reports");
    // if (myIMU.enableRotationVector() == true) {
    //     Serial.println(F("    Rotation vector enabled"));
    //     Serial.println(F("      Output in form roll, pitch, yaw in radians"));
    // } else {
    //     Serial.println("    Could not enable rotation vector");
    // }
    // if (myIMU.enableGyro() == true) {
    //     Serial.println(F("    Gyro enabled"));
    //     Serial.println(F("      Output in form x, y, z, in radians per second"));
    // } else {
    //     Serial.println("    Could not enable gyro");
    // }
    while(myIMU.wasReset()) {
        delay(10);
    }
    
}

// Conversion from quaternion to roll, pitch, yaw
void quaternionToEuler(float qW, float qX, float qY, float qZ, float& roll, float& pitch, float& yaw) {
    // Roll (x-axis rotation)
    double sinr_cosp = +2.0 * (qW * qX + qY * qZ);
    double cosr_cosp = +1.0 - 2.0 * (qX * qX + qY * qY);
    roll = atan2(sinr_cosp, cosr_cosp);

    // Pitch (y-axis rotation)
    double sinp = +2.0 * (qW * qY - qZ * qX);
    if (fabs(sinp) >= 1)
        pitch = copysign(M_PI / 2, sinp); // Use 90 degrees if out of range
    else
        pitch = asin(sinp);

    // Yaw (z-axis rotation)
    double siny_cosp = +2.0 * (qW * qZ + qX * qY);
    double cosy_cosp = +1.0 - 2.0 * (qY * qY + qZ * qZ);
    yaw = atan2(siny_cosp, cosy_cosp);
}


bool BNO85::update() {
    if (micros() - startTime > restartLength) { // every 3 seconds try to reconnect.
        BNO85::startup();
        restartLength = restartLength + 500000;
    }
    if (!bnoOn) {
        // Initialize sensorValues to 0 if the sensor is not on
        for (int i = 0; i < 6; i++) {
            sensorValues[i] = 0.0f;
        }
        return false;
    }

    if (myIMU.wasReset()) {
        Serial.println("Sensor was reset");
        setReports();
    }

    bool bevent = false;
    // The loop now only checks for one type of sensor event
    while (myIMU.getSensorEvent()) {
        bevent = true;
        startTime = micros();

        // Only checking for gyro integrated rotation vector
        if (myIMU.getSensorEventID() == SENSOR_REPORTID_GYRO_INTEGRATED_ROTATION_VECTOR) {
            float qW = myIMU.getGyroIntegratedRVReal();
            float qX = myIMU.getGyroIntegratedRVI();
            float qY = myIMU.getGyroIntegratedRVJ();
            float qZ = myIMU.getGyroIntegratedRVK();

            // Convert quaternion to Euler angles
            quaternionToEuler(qW, qX, qY, qZ, sensorValues[0], sensorValues[1], sensorValues[2]);

            // Normalize angles between -PI and PI
            for (int i = 0; i < 3; i++) {
                while (sensorValues[i] > M_PI) sensorValues[i] -= 2 * M_PI;
                while (sensorValues[i] < -M_PI) sensorValues[i] += 2 * M_PI;
            }

            // Get angular velocity directly from the sensor
            sensorValues[3] = sensorValues[3] * pitchgamma + myIMU.getGyroIntegratedRVangVelX() * (1-pitchgamma);
            sensorValues[4] = sensorValues[4] * rollgamma + myIMU.getGyroIntegratedRVangVelY() * (1-rollgamma);
            sensorValues[5] = sensorValues[5] * yawgamma + myIMU.getGyroIntegratedRVangVelZ() * (1-yawgamma);

            break; // Since we are only looking for this event, break after handling
        }
    }

    // This example assumes you have a way to return or use sensorValues
    return bevent; // Indicate successful update
}

float* BNO85::readValues(int& count) {
    count = 6; // Indicate we are returning 6 values
    return sensorValues; // Return the pointer to the sensor values
}

void BNO85::getPreferences(){

    // Implementation for reading values from non-volatile storage (NVS)
    // must manually enter keys and default values for every variable.
    Preferences preferences; //initialize the preferences 
    preferences.begin("params", true); //true means read-only

    
    yawgamma = constrain(preferences.getFloat("yawrate_gamma", 0), 0, 1);
    rollgamma = constrain(preferences.getFloat("rollrate_gamma", 0), 0, 1);
    pitchgamma = constrain(preferences.getFloat("pitchrate_gamma", 0), 0, 1);
    preferences.end();
    return;
}