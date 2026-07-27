/**
 * @file CustomBicopter.cpp
 * @author Edward Jeff
 * @brief Implementation of CustomBicopter.h
 * @version 0.1
 * @date 2024-03-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include "CustomBicopter.h"


CustomBicopter::CustomBicopter(): FullBicopter() {
    
}


// Controls [Ready, Fx, height/Fz, Tz, Tx]
void CustomBicopter::control(float sensors[MAX_SENSORS], float controls[], int size) {
    float outputs[5];
    // When control[0] == 0, the robot stops its motors and sets servos to 90 degrees
    if (controls[0] == 0) {
        outputs[0] = 0;
        outputs[1] = 0;
        outputs[2] = 90 + PDterms.servoBeta;
        outputs[3] = 90 - PDterms.servoBeta;
        outputs[4] = 0;
        
        return FullBicopter::actuate(outputs, size);
    }
    
    float feedbackControls[5];
    CustomBicopter::addFeedback(sensors, controls, feedbackControls);
    
    outputs[4] = 1; //led control
    CustomBicopter::getOutputs(sensors, feedbackControls,  outputs);
    

    FullBicopter::actuate(outputs, size);
}

void CustomBicopter::getPreferences() {
    //calls the getPreferences of the superclass object to reduce the number of getPreferences calls
    FullBicopter::getPreferences();
    // Implementation for reading values from non-volatile storage (NVS)
    // must manually enter keys and default values for every variable.
    Preferences preferences; //initialize the preferences 
    preferences.begin("params", true); //true means read-only

    // examples
    // PDterms.zEn = preferences.getBool("zEn", false);
    // PDterms.kiyawrate = preferences.getFloat("kiyawrate", 0);
    // value = preferences.getInt("value", default_value); //(value is an int) (default_value is manually set)

    // insert your parameters that you want to add from ground station here.

    preferences.end();
}



//adds sensor feedback into the control values
void CustomBicopter::addFeedback(float sensors[MAX_SENSORS], float controls[], float feedbackControls[]) {
    
    float fx = controls[1]; // Fx
    float fz = controls[2]; // Fz/height
    float tx = controls[3]; // tx
    float tz = controls[4]; // tz
    float temperature = sensors[0];
    float altitude = sensors[1];
    float altitudeVelocity = sensors[2];
    float pitch = sensors[3];
    float roll = sensors[4];
    float yaw = sensors[5];
    float pitchrate = sensors[6];
    float rollrate = sensors[7];
    float yawrate = sensors[8];
    float battery = sensors[10];
    
    //add feedback to your code.
    
    //switching indexes as controls[0] is the ready command which is no longer needed. 
    feedbackControls[0] = fx;
    feedbackControls[1] = fz;
    feedbackControls[2] = tx;
    feedbackControls[3] = tz;
    feedbackControls[4] = 1;
}


void CustomBicopter::getOutputs(float sensors[MAX_SENSORS], float controls[], float out[]) {

    float fx = controls[0]; // Fx
    float fz = controls[1]; // Fz/height
    float tx = controls[2]; // tx
    float tz = controls[3]; // tz


    float f1 = 0;
    float f2 = 0;
    float t1 = 0;
    float t2 = 0;

    out[2] = constrain((t1 ) , 0.0f,  PDterms.servoRange ) * 180.0f / PDterms.servoRange;
    out[3] = 180.0f -constrain((t2) , 0.0f,  PDterms.servoRange ) * 180.0f / PDterms.servoRange;
    out[0] = constrain(f1, 0, 1);
    out[1] = constrain(f2, 0, 1);

}

