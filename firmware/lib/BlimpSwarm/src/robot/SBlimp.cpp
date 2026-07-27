/**
 * @file SBlimp.cpp
 * @author David Saldana
 * @brief Impementation of SBlimp.h
 * @version 0.1
 * @date 2024-01-20
 * 
 * @copyright Copyright (c) 2024
 * 
 */


#include "SBlimp.h"


#define SERVO1 D0
#define SERVO2 D1
#define THRUST1 D9
#define THRUST2 D10
#define BATT A2


SBlimp::SBlimp(){

}

void SBlimp::startup() {

    motor1 = new BLMotor(1100, 2000, 0, THRUST1, 50);
    motor2 = new BLMotor(1100, 2000, 0, THRUST2, 50);
    motor3 = new BLMotor(1100, 2000, 0, SERVO1, 55);
    motor4 = new BLMotor(1100, 2000, 0, SERVO2, 58);
    // On board LED light
    led = new LED(LED_BUILTIN);


    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);

    Preferences preferences;
    
    preferences.begin("params", false); //true means read-only
    if (preferences.getBool("calibrate", false)){
        //calibrate brushless motors
        calibrate();
        preferences.putBool("calibrate", false);
    }
    else {
        // Arm brushless motors
        arm();
    }
    preferences.end(); //true means read-only
}

int SBlimp::sense(float sensors[MAX_SENSORS]) {
    // Implementation for sensing - fill the sensors array
    // Return the number of sensors used
    return 0; // Placeholder return value
}

void SBlimp::actuate(const float actuators[], int size) {
    motor1->act(actuators[0]);
    motor2->act(actuators[1]);
    motor3->act(actuators[2]);
    motor4->act(actuators[3]);
    led->act(actuators[4]);
}


void SBlimp::control(float sensors[MAX_SENSORS], float controls[], int size) {
    SBlimp::actuate(controls, size);
}

void SBlimp::getPreferences() {
    
    // Implementation for reading values from non-volatile storage (NVS)
    // must manually enter keys and default values for every variable.
    Preferences preferences; //initialize the preferences 
    preferences.begin("params", true); //true means read-only

    //value = preferences.getInt("value", default_value); //(value is an int) (default_value is manually set)
    

    preferences.end();
}

void SBlimp::calibrate(){

    delay(1000);
    Serial.println("Calibrating ESCs....");
    // ESC arming sequence for BLHeli S
    motor1->act(1);
    motor2->act(1);
    motor3->act(1);
    motor4->act(1);
    delay(8000);

    // Back to minimum value
    motor1->act(0);
    motor2->act(0);
    motor3->act(0);
    motor4->act(0);
    delay(8000);
    //motor1->act(-1); //FIXME is this necessary? If so, we can have a if (value== -1) then writeMicroseconds(0) in motor
    delay(1000);
    Serial.println("Calibration completed");
}

void SBlimp::arm(){
    // ESC arming sequence for BLHeli S
    motor1->act(0);
    motor2->act(0);
    motor3->act(0);
    motor4->act(0);
    delay(10);

    // Sweep up
    for (int i = 1050; i < 1500; i++)
    {
        motor1->act((i-1000)/1000);
        motor2->act((i-1000)/1000);
        motor3->act((i-1000)/1000);
        motor4->act((i-1000)/1000);
        delay(6);
    }

    // Back to minimum value
    motor1->act(0);
    motor2->act(0);
    motor3->act(0);
    motor4->act(0);
    delay(1000);
}
