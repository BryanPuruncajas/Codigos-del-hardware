/**
 * @file RawBicopter.cpp
 * @author Edward Jeff
 * @brief Implementation of RawBicopter.h
 * @version 0.1
 * @date 2024-01-20
 * 
 * @copyright Copyright (c) 2024
 * 
 */


#include "RawBicopter.h"
#include <Arduino.h>


#define SERVO1 D0
#define SERVO2 D1
#define THRUST1 D9
#define THRUST2 D10
#define BATT A2


RawBicopter::RawBicopter(){
    
}

void RawBicopter::startup() {
    servo1 = new AServo(SERVO1);
    servo2 = new AServo(SERVO2);
    motor1 = new BLMotor(1100, 2000, 0, THRUST1, 55);
    motor2 = new BLMotor(1100, 2000, 0, THRUST2, 58);
    led = new LED(LED_BUILTIN);


    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);

    Preferences preferences;
    
    preferences.begin("params", false); //true means read-only
    if (preferences.getBool("calibrate", false)){
        //calibrate brushless motors simultaneously
        calibrate();
        preferences.putBool("calibrate", false);
    } else {
        // Arm brushless motors simulataneously
        arm();
    }
    preferences.end();
}

int RawBicopter::sense(float sensors[MAX_SENSORS]) {
    return 0;
}

void RawBicopter::actuate(const float actuators[], int size) {
    servo1->act(actuators[2]);
    servo2->act(actuators[3]);
    motor1->act(actuators[0]);
    motor2->act(actuators[1]);
    led->act(actuators[4]);
}


void RawBicopter::control(float sensors[MAX_SENSORS], float controls[], int size) {
    RawBicopter::actuate(controls, size);
}

void RawBicopter::getPreferences() {
    Preferences preferences;
    preferences.begin("params", true); // Initializes the preferences in read-only mode   

    // A-matrix adjustments for the servo
    PDterms.servoBeta = preferences.getFloat("servoBeta", 90);
    PDterms.servoRange = preferences.getFloat("servoRange", 260);
    PDterms.botZlim = preferences.getFloat("botZlim", 0.001);
    PDterms.pitchOffset = preferences.getFloat("pitchOffset", 0);
    PDterms.pitchInvert = preferences.getFloat("pitchInvert", 1);
    PDterms.servo_move_min = preferences.getFloat("servo_move_min", 2); // degrees

    servoDiff = 2*PI - PDterms.servoRange * PI/180;// calculating the servo dead zone

    preferences.end();
}

void RawBicopter::calibrate(){
    delay(1000);
    Serial.println("Calibrating ESCs....");
    // ESC arming sequence for BLHeli S
    motor1->act(1);
    motor2->act(1);
    delay(8000);

    // Back to minimum value
    motor1->act(0);
    motor2->act(0);
    delay(8000);
    //motor1->act(-1); //FIXME is this necessary? If so, we can have a if (value== -1) then writeMicroseconds(0) in motor
    delay(1000);
    Serial.println("Calibration completed");
}

void RawBicopter::arm(){
    // ESC arming sequence for BLHeli S
    motor1->act(0);
    motor2->act(0);
    delay(10);

    // Sweep up
    for (int i = 1050; i < 1500; i++)
    {
        motor1->act((i-1000)/1000);
        motor2->act((i-1000)/1000);
        delay(6);
    }

    // Back to minimum value
    motor1->act(0);
    motor2->act(0);
    delay(1000);
}

// adjusts the servo deadzone to be in the correct place
float RawBicopter::adjustAngle(float angle) {
  while (angle <  - servoDiff / 2 - PDterms.servoBeta * PI/180.0f ) angle += 2 * PI;
  while (angle > 2 * PI - servoDiff / 2 - PDterms.servoBeta * PI/180.0f ) angle -= 2 * PI;
  return angle;
}

float RawBicopter::clamp(float val, float minVal, float maxVal) {
    return std::max(minVal, std::min(maxVal, val));
}

