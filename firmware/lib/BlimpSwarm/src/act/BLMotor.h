#ifndef BLIMPSWARM_BLMOTOR_H
#define BLIMPSWARM_BLMOTOR_H
#include "Actuator.h"
#include <ESP32Servo.h>

class BLMotor : public Actuator {
public:
    BLMotor(int minVal, int maxVal, int offsetVal, int pinVal, int periodHertz);
    BLMotor(int pinVal);
    void calibrate();
    void arm();
    void act(float value);
    void enable();
    void disable();
    bool isEnabled() const { return enabled; }
    void setPowerLimit(float limit);
    float getPowerLimit() const { return powerLimit; }
private:
    Servo thrust;
    int period_hertz=50;
    bool enabled=false;
    float powerLimit=1.00f;
    int min_thrust=0;
    int max_thrust=35;
    const float pwm_a=0.042337045f;
    const float pwm_b=-46.58244237f;
};
#endif