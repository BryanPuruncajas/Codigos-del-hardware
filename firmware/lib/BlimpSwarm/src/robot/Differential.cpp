/**
 * @file Differential.cpp
 * @author Edward Jeff
 * @brief Implementation of Differential.h
 * @version 0.1
 * @date 2024-03-01
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "Differential.h"

Differential::Differential() {}

void Differential::startup() {
    RawBicopter::startup(); // Initializes the servos and motors
    sensorsuite.startup();  // Initializes the Nicla and the other sensors
    float senses[MAX_SENSORS];
    Differential::sense(senses);
}

int Differential::sense(float sensors[MAX_SENSORS]) {
    sensorsuite.update();
    int numSenses = 0;
    //(temperature, altitude, velocityAltitude, roll, pitch, yaw, rollrate, pitchrate, yawrate, batteryVoltage)

    float *sensorsValues = sensorsuite.readValues(numSenses);
    for (int i = 0; i < numSenses; i++) {
        sensors[i] = sensorsValues[i];
    }
    return numSenses;
}

// Controls [Ready, Fx, height/Fz, Tz, Tx]
void Differential::control(float sensors[MAX_SENSORS], float controls[], int size) {
    float outputs[5];

    // When control[0] == 0, the robot stops its motors and sets servos to facing vertically upward
    if (controls[0] == 0) {
        outputs[0] = 0;
        outputs[1] = 0;
        // Checking for full rotations and adjusting t1 and t2
        float t1 = adjustAngle(PI / 2);
        float t2 = adjustAngle(PI / 2);

        // Converting values to a more stable form
        // float servoBottom = 90.0f - PDterms.servoRange/2.0f; // bottom limit of servo in degrees
        // float servoTop = 90.0f + PDterms.servoRange/2.0f; // top limit of servo in degrees
        outputs[2] = controls[5] * PDterms.reelSpeed * 90 + 90; // 180.0f - clamp((t2) * 180.0f / PI + PDterms.servoBeta, 0.0f, PDterms.servoRange) * 180.0f / PDterms.servoRange;
        outputs[3] = 180.0f - clamp((t2) * 180.0f / PI + PDterms.servoBeta, 0.0f, PDterms.servoRange) * 180.0f / PDterms.servoRange;
        outputs[4] = 0;

        return Differential::actuate(outputs, size);
    }

    float feedbackControls[5];
    Differential::addFeedback(sensors, controls, feedbackControls);

    Differential::getOutputs(sensors, feedbackControls, outputs);
    outputs[2] = controls[5] * PDterms.reelSpeed * 90 + 90;
    RawBicopter::actuate(outputs, size);
}

void Differential::getPreferences() {
    // calls the getPreferences of the superclass object to reduce the number of getPreferences calls
    RawBicopter::getPreferences();
    sensorsuite.getPreferences();
    // Implementation for reading values from non-volatile storage (NVS)
    // must manually enter keys and default values for every variable.
    Preferences preferences;           // initialize the preferences
    preferences.begin("params", true); // true means read-only

    // value = preferences.getInt("value", default_value); //(value is an int) (default_value is manually set)

    // enables feedback on each vector
    PDterms.zEn = preferences.getBool("zEn", false);
    PDterms.yawEn = preferences.getBool("yawEn", false);
    PDterms.rollEn = preferences.getBool("rollEn", false);
    PDterms.rotateEn = preferences.getBool("rotateEn", false);
    PDterms.pitchEn = preferences.getBool("pitchEn", false);

    // PID terms
    PDterms.kpyaw = preferences.getFloat("kpyaw", 0.1);
    PDterms.kppyaw = preferences.getFloat("kppyaw", 0.1);
    PDterms.kdyaw = preferences.getFloat("kdyaw", 0.1); // same thing as if I said kpyawrate
    PDterms.kddyaw = preferences.getFloat("kddyaw", 0.1);
    PDterms.kiyaw = preferences.getFloat("kiyaw", 0);
    PDterms.kiyawrate = preferences.getFloat("kiyawrate", 0);

    PDterms.kpz = preferences.getFloat("kpz", 0.7);
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
    PDterms.reelSpeed = preferences.getFloat("reelSpeed", 0);

    // radius of the blimp
    PDterms.lx = preferences.getFloat("lx", .15);

    preferences.end();
}

void Differential::addFeedback(float sensors[MAX_SENSORS], float controls[], float feedbackControls[]) {

    float fx = controls[1]; // Fx
    float fz = controls[2]; // Fz/height
    float tx = controls[3]; // tx
    float tz = controls[4]; // tz

    int dt = 4000;

    // Z feedback
    if (PDterms.zEn) {
        // Integral in Z
        z_integral += (controls[2] - (sensors[1])) * ((float)dt) / 1000000.0f * PDterms.kiz;
        z_integral = clamp(z_integral, PDterms.z_int_low, PDterms.z_int_high);
        // Serial.println("z feedback");
        // PID in z
        fz = (controls[2] - (sensors[1])) * PDterms.kpz - (sensors[2]) * PDterms.kdz + (z_integral);
    }

    // Yaw feedback (Cascading control format)
    //      this means that there is a separate PID for both absolute yaw and yawrate which are combined
    //      what this basically means is that the D term for the absolute yaw is replaced with the yawrate PI
    //          the D term in yaw is equivalent to the P term in yawrate
    if (PDterms.yawEn) {
        float kpyaw_max_increase = 0.04f; // Maximum increase in kpyaw
        float kdyaw_max_increase = 0.04f; // Maximum increase in kdyaw (D-term adjustment)

        // Calculate dynamic scaling based on fx
        float scaling_factor = 1.0f - clamp(fabs(fx), 0.0f, 1.0f); // When fx approaches 0, scaling_factor approaches 1

        // Adjust P-term dynamically
        float dynamic_kpyaw = PDterms.kpyaw + scaling_factor * kpyaw_max_increase;

        // Adjust D-term dynamically
        float dynamic_kdyaw = PDterms.kdyaw + scaling_factor * kdyaw_max_increase;

        // Absolute yaw error
        float e_yaw = controls[4] - sensors[5]; // Tz - Yaw
        e_yaw = atan2(sin(e_yaw), cos(e_yaw));  // Normalize yaw error
        e_yaw = clamp(e_yaw, -PI / 5, PI / 5);

        // Integral term for absolute yaw (remains the same)
        yaw_integral += e_yaw * ((float)dt) / 1000000.0f * PDterms.kiyaw;
        yaw_integral = clamp(yaw_integral, -PI / 4, PI / 4);

        // Find desired yaw rate from absolute yaw feedback
        float yaw_desired_rate = (e_yaw + yaw_integral);

        // Yaw rate error
        float e_yawrate = yaw_desired_rate * dynamic_kpyaw - sensors[8]; // YawRate feedback

        // Final result of the cascading PID controller with dynamic P and D terms
        tz = yaw_desired_rate * PDterms.kppyaw + e_yawrate * dynamic_kdyaw // Dynamic D-term adjustment
             - sensors[8] * PDterms.kddyaw                                 // Derivative feedback remains based on sensors
             + yawrate_integral;
    }

    // Pitch feedback
    if (PDterms.pitchEn) {
        // Serial.println("roll feedback");
        fx = fx - sensors[3] * PDterms.kppitch - sensors[6] * PDterms.kdpitch; // Pitch - PitchRate
    }
    // Roll feedback
    if (PDterms.rollEn) {
        // Serial.println("roll feedback");
        tx =
            tx - constrain(sensors[4] * PDterms.kproll - sensors[7] * PDterms.kdroll, -fz * PDterms.lx * .9, fz * PDterms.lx * .9); // Roll - RollRate
    }
    // Roll and pitch rotation state feedback
    if (PDterms.rotateEn) {
        // Serial.println("rotate feedback");
        float cosp = cos(sensors[4]); // Pitch
        float sinp = sin(sensors[4]); // Pitch
        float cosr = cos(sensors[3]); // Roll
        float ifx = controls[1];      // Fx
        fx = ifx * cosp + fz * sinp;
        fz = (-1 * ifx * sinp + fz * cosp) / cosr;
    }
    feedbackControls[0] = fx;
    feedbackControls[1] = fz;
    feedbackControls[2] = tx;
    feedbackControls[3] = tz;
    feedbackControls[4] = 1;
}

void Differential::getOutputs(float sensors[MAX_SENSORS], float controls[], float out[]) {
    // Assuming PDterms, kf1, kf2, servo1offset, servo2offset, and feedbackPD.pitch are defined elsewhere
    float theta_ema = 0.0f; // Exponential moving average of theta
    float alpha = 0.95f;    // Smaller values will smooth more, larger values will be more responsive

    float l = PDterms.lx;

    float fx = clamp(controls[0], -0.6, 0.6);
    float fz = controls[1];
    // if (fx > abs(fz)){
    fz = clamp(fz, -1, 1);
    // } else {
    //     fz = clamp(fz, 0.001, 2);
    // }
    float tauz = clamp(controls[3], -.1, .1);

    // differential control part
    float F_mag = sq(fx) + sq(fz);
    float theta = atan2(fz, fx);
    float f1 = 0;
    float f2 = 0;

    // // Variables for handling large, rapid changes in theta
    // static float previous_theta = theta;
    // static float last_theta_time = millis(); // Time when the last valid theta change occurred

    // // Threshold and duration for large rapid theta changes
    // float theta_threshold = PI / 8;     // 45 degrees threshold
    // float theta_change_duration = 500; // 1 second

    // // If the change in theta is too large and happens in a short time, ignore it
    // if (fabs(theta - previous_theta) > theta_threshold)
    // {
    //     if (millis() - last_theta_time < theta_change_duration)
    //     {
    //         // Ignore the large, rapid change
    //         theta = previous_theta;
    //     }
    //     else
    //     {
    //         // Accept the change after the duration
    //         last_theta_time = millis();
    //     }
    // }

    // // Store previous theta for the next iteration
    // previous_theta = theta;

    if (F_mag != 0) {

        if (fabs(fx) == 0.0) {
            fx = 10 * fabs(tauz);
            theta = atan2(fz, fabs(tauz * 10));
        }

        if (fabs(tauz / fx) > 0.1) {
            fx = 0.2;
            float scaled_tauz = tauz * fabs(fx);
            tauz = scaled_tauz;
            theta = atan2(fz, fx);
        }

        if (fx < 0) {
            // fz = -0.1 * fx;
            // theta = atan2(fz, fx);
            theta = PI - 0.01;
        }

        // else
        float term1 = tauz / (l * cos(theta));
        float term2 = sqrt(F_mag);
        f1 = 0.5 * (term1 + term2);
        f2 = 0.5 * (-term1 + term2);
        // }
        // else
        // {
        // theta = atan2(fz, fabs(tauz / -.1));
        // float term1 = tauz / (2 * l);
        // float term2 = sqrt(F_mag);
        // f1 = 0.5 * (term1 + term2);
        // f2 = 0.5 * (-term1 + term2);
        // }
    }

    // Adds the pitch to the servo to accomodate for swinging behavior
    if (PDterms.pitchEn) {
        theta += sensors[3] * PDterms.pitchInvert + PDterms.pitchOffset / 180 * PI; // Pitch
    }

    // Smoothing theta using exponential rolling average
    theta_ema = alpha * theta + (1 - alpha) * theta_ema;
    theta = theta_ema; // Use smoothed theta for the rest of the calculations
    // Checking for full rotations and adjusting t1 and t2
    // theta = adjustAngle(theta);

    // Converting values to a more stable form
    // float servoBottom = 90.0f - PDterms.servoRange/2.0f; // bottom limit of servo in degrees
    // float servoTop = 90.0f + PDterms.servoRange/2.0f; // top limit of servo in degrees
    out[3] = 180.0f - clamp((theta) * 180.0f / PI + PDterms.servoBeta, 0.0f, PDterms.servoRange) * 180.0f / PDterms.servoRange;
    out[0] = clamp(f1, 0.025, 1); // Cap f1 at a minimum of 0.025
    out[1] = clamp(f2, 0.025, 1); // Cap f2 at a minimum of 0.025

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