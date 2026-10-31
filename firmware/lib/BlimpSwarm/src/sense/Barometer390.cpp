/**
 * @file Barometer390.cpp
 * @author David Saldana
 * @brief Implementation of Barometer390.h
 * @version 0.1
 * @date 2024-07-14
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "Barometer390.h"

namespace {
// Same pressure->altitude equation as the installed Adafruit library, but
// consume the pressure already obtained by one checked performReading().
float pressureAltitude(float pressurePa) {
    return 44330.0f * (1.0f - powf((pressurePa / 100.0f) / 1013.25f, 0.1903f));
}
}

Barometer::Barometer() {
    // Initialization code, similar to what you have in initBarometer()
    baroInitialized = false;
}

void Barometer::startup() {
    Barometer::getPreferences();
    if (baroInitialized){
        return;
    }
    
    velocityZ = 0;
    temperature = 0;
    pressure = 0;
    altitude = 0;
    int retryCount = 0;
    const int maxRetries = 5;
    const int initialDelay = 50; // Initial delay in milliseconds
    Serial.println("Initializing Barometer!");
    // Attempt to initialize the barometer
    while (!baroInitialized && retryCount < maxRetries) {
        if (sampler.begin(0x77) || sampler.begin(0x76)) {
            Serial.println("  Barometer Connected!");
            baroInitialized = true;
            break;
        } else { // Failed to initialize, increase retry count and delay
            retryCount++;
            Serial.print(F("  Attempt "));
            Serial.print(retryCount);
            Serial.println(F(" failed, retrying..."));
            delay(initialDelay * retryCount); // Increase delay with each retry
        }
    } 

    if (!baroInitialized) {
        Serial.println(F("  Could not find a valid BMP390 sensor, check wiring or try a different address!"));
        return; // Exit if unable to initialize
    }

    Serial.println("  BMP390 normal25Hz, pressure8x, temperature1x, IIR1; data-ready samples only.");

    groundLevel = 0.0;
    int calibrationReadings = 10;
    int discardReadings = 5; // Number of initial readings to discard
    for (int i = 0; i < discardReadings + calibrationReadings; i++) {
        const uint32_t waitStart=micros();bool received=false;
        while(uint32_t(micros()-waitStart)<250000){
            if(sampler.read(pressure,temperature)){lastGoodUs=micros();received=true;break;}
            delay(2);
        }
        if (!received) {
            Serial.println(F("  Failed to perform reading for calibration."));
            baroInitialized = false;
            return;
        }
        if (!isfinite(pressure) || pressure <= 0 || !isfinite(temperature)) {
            baroInitialized = false;
            return;
        }
        if (i >= discardReadings) { // Start accumulating after discarding initial readings
            groundLevel += pressureAltitude(pressure);
        }
        delay(100); // Short delay between readings
    }
    startTime = micros();
    velocityZ = 0;
    groundLevel /= calibrationReadings; // Average of the good readings
    altitude = groundLevel;
    estimate.reset(0.0f, lastGoodUs);
    samplePeriodUs=0;
    Serial.print(F("  Ground level calibrated to: "));
    Serial.print(groundLevel);
    Serial.println(" meters above sea level.");
}



bool Barometer::update() {
    if (uint32_t(micros() - startTime) < VerticalProfile::POLL_US) {
        return false;
    }
    if (!baroInitialized) {
        return false;
    }
    startTime = micros(); // rate-limit failed reads too
    if (!sampler.read(pressure,temperature)) return false;
    const uint32_t now = micros();
    const float relative = VerticalProfile::HEIGHT_SCALE*(pressureAltitude(pressure) - groundLevel);
    if (!estimate.update(relative, now)) return false;
    samplePeriodUs=uint32_t(now-lastGoodUs);lastGoodUs = now;
    altitude = groundLevel + estimate.height;
    velocityZ = estimate.velocity;
    return true; // Data was successfully updated
}

// returns [pressure, temperature, altitude]
float* Barometer::readValues(int& count) {
    static float values[4]; // Static to ensure it persists after the method returns
    values[0] = pressure;
    values[1] = temperature;
    const bool valid = diagnostics().healthy;
    values[2] = valid ? estimate.height : NAN;
    values[3] = valid ? velocityZ : NAN;
    count = 4; // Indicate that we're returning 3 values
    return values;
}

VerticalDiagnostics Barometer::diagnostics() const {
    const uint32_t age = estimate.initialized ? uint32_t(micros() - lastGoodUs) / 1000 : UINT32_MAX;
    return {estimate.rawHeight, estimate.height, estimate.rawVelocity,
            estimate.velocity, age, baroInitialized && estimate.initialized && age < VerticalProfile::STALE_MS,
            lastGoodUs,samplePeriodUs,estimate.windowSpanUs/1000};
}

// TODO: Allow for parameters to be used to change barometer
void Barometer::getPreferences(){
    return;
}
