/**
 * @file Barometer.cpp
 * @author David Saldana
 * @brief Implementation of Barometer.h
 * @version 0.1
 * @date 2024-07-14
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "Barometer.h"

BarometerOld::BarometerOld(){
    
    //pass
    
}
void BarometerOld::init(){
    /* Initialise the sensor */
    int countTries = 0;

    // Attempts to initialize I2C communication between the BMP and the main board.
    baroOn = bme.begin_I2C();

    // Tries to start the connection 10 times.
    while (!baroOn) {
        delay(100);
        if (countTries > 10) {
            Serial.println(F("Could not find a valid BMP390 sensor, check wiring or "
                            "try a different address!"));
                            
            break;
        }
        countTries += 1;
        baroOn = bme.begin_I2C();
    }

    // Sets the parameters to desired values.
    bme.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
    bme.setPressureOversampling(BMP3_OVERSAMPLING_16X);
    bme.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_7);
    bme.setOutputDataRate(BMP3_ODR_100_HZ);
    delay(50);
    if (baroOn){
        estimatedZ = bme.readAltitude(1013.25);
        oldZ = estimatedZ;
        float groundZ = getEstimatedZ();
        delay(30);
        updateBarometer();
        int tempcount = 0;
        while (abs(groundZ - getEstimatedZ()) > .005 || groundZ == getEstimatedZ()){
        // Serial.print("Ground Cal...");
        // Serial.println(groundZ - getEstimatedZ());
        if (tempcount >30){
            break;
        }
        tempcount += 1;
        groundZ = getEstimatedZ();
        delay(10);

        updateBarometer();
        }
        Serial.print("Ground Cal Done: ");
        Serial.println(groundZ - getEstimatedZ());
    } else {
        estimatedZ = 0;
        oldZ = 0;
    }
    
    
    
}

bool BarometerOld::updateBarometer(){
    if (baroOn){
        if ((micros() - tStart) > (BMP390_SAMPLERATE_DELAY_MS * 1000)) {
            oldZ = estimatedZ;
            estimatedZ = bme.readAltitude(1013.25);
            dtBaro = micros() - tStart;
            tStart = micros();
            return true;
        }
    }
    return false;
}


float BarometerOld::getEstimatedZ(){
    return estimatedZ;


}
float BarometerOld::getVelocityZ(){
    // Micro seconds to seconds
    return 1000000 * (estimatedZ - oldZ) / dtBaro;
}