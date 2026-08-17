#include "BLMotor.h"
#include <Arduino.h>

BLMotor::BLMotor(int minVal,int maxVal,int offsetVal,int pinVal,int periodHertz)
: Actuator(minVal,maxVal,offsetVal,pinVal), period_hertz(periodHertz) {
    // IMPORTANTE PARA ESTOS ESC:
    // El firmware heredado mantenia PWM presente desde startup. Dejarlos en
    // INPUT/detach hace que algunos ESC arranquen pitando por "no signal" y
    // luego no acepten correctamente potencia aunque el software diga ARMED.
    //
    // Conservamos el safe boot de forma logica: PWM SI existe, pero queda
    // clavado en throttle minimo hasta un ARM explicito.
    pinMode(this->pin,OUTPUT);
    thrust.attach(this->pin,this->min,this->max);
    thrust.setPeriodHertz(this->period_hertz);
    thrust.writeMicroseconds(this->min);
    enabled=false;
}

BLMotor::BLMotor(int pinVal)
: Actuator(1100,2000,0,pinVal), period_hertz(50) {
    pinMode(this->pin,OUTPUT);
    thrust.attach(this->pin,this->min,this->max);
    thrust.setPeriodHertz(this->period_hertz);
    thrust.writeMicroseconds(this->min);
    enabled=false;
}

void BLMotor::setPowerLimit(float limit){
    powerLimit=constrain(limit,0.0f,1.0f);
}

void BLMotor::enable(){
    if(enabled) return;

    // El PWM ya esta conectado desde el constructor. Antes de habilitar
    // comandos distintos de cero reafirmamos throttle minimo.
    thrust.writeMicroseconds(this->min);
    enabled=true;
}

void BLMotor::disable(){
    // NO detach: el ESC debe seguir viendo una senal valida de throttle bajo.
    // La seguridad se mantiene porque act() ignora cualquier potencia mientras
    // enabled == false.
    thrust.writeMicroseconds(this->min);
    enabled=false;
}

void BLMotor::act(float value){
    if(!enabled) return;

    value=constrain(value,0.0f,powerLimit);

    // Conversion EXACTA heredada: value 0..1 -> fuerza 0..35 g -> PWM.
    // Con las constantes originales: 0% ~= 1100 us y 100% ~= 1927 us.
    float force=value*(max_thrust-min_thrust)+min_thrust;
    int pwm=(int)((force-pwm_b)/pwm_a);
    pwm=constrain(pwm,this->min,this->max);
    thrust.writeMicroseconds(pwm);
}

void BLMotor::arm(){
    enable();
    act(0.0f);
    delay(1000);
}

void BLMotor::calibrate(){
    Serial.println("[SAFE] calibrate() no se ejecuta automaticamente.");
}