#ifndef BLIMPSWARM_ASERVO_H
#define BLIMPSWARM_ASERVO_H
#include "Actuator.h"
#include <ESP32Servo.h>
class AServo : public Actuator {
public:
    AServo(int minVal,int maxVal,int offsetVal,int pinVal,int periodHertz);
    AServo(int pinVal);
    void act(float value);
    void actMicroseconds(int pulseUs);
    void enable();
    void disable();
    bool isEnabled() const { return enabled; }
private:
    Servo servo;
    int period_hertz=50;
    bool enabled=false;
};
#endif
