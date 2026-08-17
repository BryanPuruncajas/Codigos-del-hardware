#include "AServo.h"
#include <Arduino.h>
AServo::AServo(int minVal,int maxVal,int offsetVal,int pinVal,int periodHertz)
:Actuator(minVal,maxVal,offsetVal,pinVal),period_hertz(periodHertz){ pinMode(this->pin,INPUT); }
AServo::AServo(int pinVal):Actuator(550,2450,0,pinVal),period_hertz(50){ pinMode(this->pin,INPUT); }
void AServo::enable(){
    if(enabled) return;
    pinMode(this->pin,OUTPUT);
    servo.attach(this->pin,this->min,this->max);
    servo.setPeriodHertz(period_hertz);
    enabled=true;
}
void AServo::disable(){ if(enabled) servo.detach(); pinMode(this->pin,INPUT); enabled=false; }
void AServo::act(float value){ if(!enabled) return; servo.write((int)constrain(value,0.0f,180.0f)); }
void AServo::actMicroseconds(int pulseUs){ if(!enabled) return; servo.writeMicroseconds(constrain(pulseUs,this->min,this->max)); }
