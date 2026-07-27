/**
 * @file FullBicopter.cpp
 * @author Edward Jeff
 * @brief Implementation of FullBicopter.h
 * @version 0.1
 * @date 2024-03-01
 * 
 * @copyright Copyright (c) 2024
 * 
 */


#include "FullBicopter.h"


FullBicopter::FullBicopter(){
    
}

void FullBicopter::startup() {
    RawBicopter::startup(); // Initializes the servos and motors
    sensorsuite.startup(); // Initializes the Nicla and the other sensors
    float senses[MAX_SENSORS];
    FullBicopter::sense(senses);
}


int FullBicopter::sense(float sensors[MAX_SENSORS]) {
    sensorsuite.update();
    int numSenses = 0;
    //(temperature, altitude, velocityAltitude, roll, pitch, yaw, rollrate, pitchrate, yawrate, batteryVoltage)

    float* sensorsValues = sensorsuite.readValues(numSenses);
    for (int i = 0; i < numSenses; i++) {
        sensors[i] = sensorsValues[i];
    }
    return numSenses; 
}

// Controls [Ready, Fx, height/Fz, Tz, Tx]
void FullBicopter::control(float sensors[MAX_SENSORS], float controls[], int size) {
    float outputs[5];

    // When control[0] == 0, the robot stops its motors and sets servos to facing vertically upward
    if (controls[0] == 0 ) {
        outputs[0] = 0;
        outputs[1] = 0;
        // Checking for full rotations and adjusting t1 and t2
        float t1 = adjustAngle(PI/2);
        float t2 = adjustAngle(PI/2);

        // Converting values to a more stable form
        // float servoBottom = 90.0f - PDterms.servoRange/2.0f; // bottom limit of servo in degrees
        // float servoTop = 90.0f + PDterms.servoRange/2.0f; // top limit of servo in degrees
        outputs[2] = clamp((t1 ) * 180.0f / PI  + PDterms.servoBeta , 0.0f,  PDterms.servoRange ) * 180.0f / PDterms.servoRange;
        outputs[3] = 180.0f -clamp((t2) * 180.0f / PI  + PDterms.servoBeta, 0.0f,  PDterms.servoRange ) * 180.0f / PDterms.servoRange;
        outputs[4] = 0;
        
        return FullBicopter::actuate(outputs, size);
    }
    
    float feedbackControls[5];
    FullBicopter::addFeedback(sensors, controls, feedbackControls);
    
    outputs[4] = 1;
    FullBicopter::getOutputs(sensors, feedbackControls,  outputs);
    RawBicopter::actuate(outputs, size);
}

void FullBicopter::getPreferences() {
    //calls the getPreferences of the superclass object to reduce the number of getPreferences calls
    RawBicopter::getPreferences();
    sensorsuite.getPreferences();
    // Implementation for reading values from non-volatile storage (NVS)
    // must manually enter keys and default values for every variable.
    Preferences preferences; //initialize the preferences 
    preferences.begin("params", true); //true means read-only

    //value = preferences.getInt("value", default_value); //(value is an int) (default_value is manually set)
    
    // enables feedback on each vector
    PDterms.zEn = preferences.getBool("zEn", false);
    PDterms.yawEn = preferences.getBool("yawEn", false);
    PDterms.rollEn = preferences.getBool("rollEn", false);
    PDterms.rotateEn = preferences.getBool("rotateEn", false);
    PDterms.pitchEn = preferences.getBool("pitchEn", false);

    // PID terms
    PDterms.kpyaw = preferences.getFloat("kpyaw", 0.1);
    PDterms.kppyaw = preferences.getFloat("kppyaw", 0.1);
    PDterms.kdyaw = preferences.getFloat("kdyaw", 0.1);// same thing as if I said kpyawrate
    PDterms.kddyaw = preferences.getFloat("kddyaw", 0.1);
    PDterms.kiyaw = preferences.getFloat("kiyaw", 0);
    PDterms.kiyawrate = preferences.getFloat("kiyawrate", 0);

    PDterms.kpz = preferences.getFloat("kpz", 0.5);
    PDterms.kdz = preferences.getFloat("kdz", 0.5);
    PDterms.kiz = preferences.getFloat("kiz", 0);

    PDterms.kproll = preferences.getFloat("kproll", 0);
    PDterms.kdroll = preferences.getFloat("kdroll", 0);
    PDterms.kppitch = preferences.getFloat("kppitch", 0);
    PDterms.kdpitch = preferences.getFloat("kdpitch", 0);

    // Range terms for the integrals
    PDterms.z_int_low = preferences.getFloat("z_int_low", 0);
    PDterms.z_int_high = preferences.getFloat("z_int_high", 0);
    PDterms.yawRateIntRange = preferences.getFloat("yawRateIntRange", 0);

    // radius of the blimp
    PDterms.lx = preferences.getFloat("lx", .15);

    preferences.end();
}

void FullBicopter::addFeedback(float sensors[MAX_SENSORS], float controls[], float feedbackControls[]) {
    
    float fx = -controls[1]; // Fx
    float fz = controls[2]; // Fz/height
    float tx = controls[3]; // tx
    float tz = controls[4]; // tz
    
    int dt = 4000;
    
    // Z feedback
    if (PDterms.zEn) {
        // Integral in Z
        z_integral += (controls[2] - (sensors[1])) * ((float)dt)/1000000.0f * PDterms.kiz;
        z_integral = clamp(z_integral, PDterms.z_int_low, PDterms.z_int_high);
        // Serial.println("z feedback");
        // PID in z
        fz = (controls[2] - (sensors[1]))*PDterms.kpz 
                      - (sensors[2])*PDterms.kdz + (z_integral); 
    }

    // Yaw feedback (Cascading control format) 
    //      this means that there is a separate PID for both absolute yaw and yawrate which are combined
    //      what this basically means is that the D term for the absolute yaw is replaced with the yawrate PI
    //          the D term in yaw is equivalent to the P term in yawrate
    if (PDterms.yawEn) {
        // Serial.print("yaw feedback: ");
        // initial absolute error for the yaw
        float e_yaw = controls[4] - sensors[5]; // Tz - Yaw
        e_yaw = atan2(sin(e_yaw), cos(e_yaw));
        e_yaw = clamp(e_yaw, -PI/5, PI/5);

        // integral term for the absolute yaw
        yaw_integral += e_yaw * ((float)dt)/1000000.0f * PDterms.kiyaw;
        yaw_integral = clamp(yaw_integral, -PI/4, PI/4);
        
        // finding desired yaw rate from the absolute yaw feedback
        float yaw_desired_rate = (e_yaw  + yaw_integral);

        // getting the error in the yawrate
        float e_yawrate = yaw_desired_rate * PDterms.kpyaw - sensors[8]; // YawRate

        // integral term for the yawrate
        yawrate_integral += e_yawrate * ((float)dt)/1000000.0f * PDterms.kiyawrate;
        yawrate_integral = clamp(yawrate_integral, - PDterms.yawRateIntRange, PDterms.yawRateIntRange);

        // final result of the cascading PID controller in yaw
        tz = yaw_desired_rate * PDterms.kppyaw  + e_yawrate*PDterms.kdyaw - sensors[8] * PDterms.kddyaw  + yawrate_integral;
        
        // Serial.println(tz);
    }

    // Pitch feedback
    if (PDterms.pitchEn) { 
        // Serial.println("roll feedback");
        fx = fx - sensors[3]*PDterms.kppitch - sensors[6] * PDterms.kdpitch; // Pitch - PitchRate
    }
    // Roll feedback
    if (PDterms.rollEn) { 
        // Serial.println("roll feedback");
        tx = tx - constrain(sensors[4]*PDterms.kproll - sensors[7] * PDterms.kdroll, -fz*PDterms.lx*.9, fz*PDterms.lx*.9); // Roll - RollRate
    }
    // Roll and pitch rotation state feedback
    if (PDterms.rotateEn) {
        // Serial.println("rotate feedback");
        float cosp = cos(sensors[4]); // Pitch
        float sinp = sin(sensors[4]); // Pitch
        float cosr = cos(sensors[3]); // Roll
        float ifx = controls[1]; // Fx
        fx = ifx*cosp + fz*sinp;
        fz = (-1*ifx*sinp + fz* cosp)/cosr;
    }
    feedbackControls[0] = fx;
    feedbackControls[1] = fz;
    feedbackControls[2] = tx;
    feedbackControls[3] = tz;
    feedbackControls[4] = 1;
    
    
}

void FullBicopter::getOutputs(float sensors[MAX_SENSORS], float controls[], float out[]) {
    // Assuming PDterms, kf1, kf2, servo1offset, servo2offset, and feedbackPD.pitch are defined elsewhere

    float l = PDterms.lx;

    float fx = clamp(controls[0], -1, 1);
    float fz = controls[1];
    // if (fx > abs(fz)){
        fz = clamp(fz, PDterms.botZlim, 2);
    // } else {
    //     fz = clamp(fz, 0.001, 2);
    // }
    float taux = clamp(controls[2], -l + 0.01f, l - 0.01f);
    float tauz = clamp(controls[3], -.1, .1);

    // Inverse A-Matrix calculations
    float term1 = l * l * fx * fx + l * l * fz * fz + taux * taux + tauz * tauz;
    float term2 = 2 * fz * l * taux - 2 * fx * l * tauz;
    float term3 = sqrt(term1 + term2);
    float term4 = sqrt(term1 - term2);
    float f1 = term3 / (2 * l);
    float f2 = term4 / (2 * l);
    float t1 = atan2((fz * l - taux) / term3, (fx * l + tauz) / term3);
    float t2 = atan2((fz * l + taux) / term4, (fx * l - tauz) / term4);
    
    // Adds the pitch to the servo to accomodate for swinging behavior
    if (PDterms.pitchEn) {
        t1 += sensors[3] * PDterms.pitchInvert + PDterms.pitchOffset/180 * PI; // Pitch
        t2 += sensors[3] * PDterms.pitchInvert + PDterms.pitchOffset/180 * PI; // Pitch
    }

    // Checking for full rotations and adjusting t1 and t2
    t1 = adjustAngle(t1);
    t2 = adjustAngle(t2);

    // Converting values to a more stable form
    // float servoBottom = 90.0f - PDterms.servoRange/2.0f; // bottom limit of servo in degrees
    // float servoTop = 90.0f + PDterms.servoRange/2.0f; // top limit of servo in degrees
    out[2] = clamp((t1 ) * 180.0f / PI  + PDterms.servoBeta , 0.0f,  PDterms.servoRange ) * 180.0f / PDterms.servoRange;
    out[3] = 180.0f -clamp((t2) * 180.0f / PI  + PDterms.servoBeta, 0.0f,  PDterms.servoRange ) * 180.0f / PDterms.servoRange;
    out[0] = clamp(f1, 0, 1);
    out[1] = clamp(f2, 0, 1);
    if (abs(out[2] - servo_old1) < PDterms.servo_move_min) {
        out[2] = servo_old1;
    } else {
        servo_old1 = out[2];
    }
    
    if (abs(out[3] - servo_old2) < PDterms.servo_move_min) {
        out[3] = servo_old2;
    } else {
        servo_old2 = out[3];
    }
    // // Adjust servo positions if motor speeds are too low
    // if (out[0] < 0.02f) {
    //     out[2] = 90;
    // }
    // if (out[1] < 0.02f) {
    //     out[3] = 90;
    // }
}