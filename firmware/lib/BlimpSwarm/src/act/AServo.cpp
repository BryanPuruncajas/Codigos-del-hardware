#include "AServo.h"
#include <Arduino.h>
AServo::AServo(int minVal,int maxVal,int offsetVal,int pinVal,int periodHertz)
:Actuator(minVal,maxVal,offsetVal,pinVal),period_hertz(periodHertz){ pinMode(this->pin,INPUT); }
// Margen de seguridad ±50us sobre AppConfig::SERVO_PULSE_MIN_US/MAX_US
// (500..2400, datasheet del Tower Pro). Si ese rango cambia, este clamp
// tiene que seguir siendo IGUAL o MAS ANCHO, nunca mas angosto -- sino
// vuelve a recortar el pulso antes de llegar al servo (ver el bug real
// encontrado el 13/09 en RawBicopter.cpp, un clamp distinto que quedo
// hardcodeado en el rango viejo del P0025 y anulaba cualquier cambio aca).
AServo::AServo(int pinVal):Actuator(450,2450,0,pinVal),period_hertz(50){ pinMode(this->pin,INPUT); }
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
