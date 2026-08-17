#include "RawBicopter.h"
#include <Arduino.h>

#define SERVO1 D0
#define SERVO2 D1
#define THRUST1 D9
#define THRUST2 D10

RawBicopter::RawBicopter(){}

void RawBicopter::startup(){
    servo1=new AServo(SERVO1);
    servo2=new AServo(SERVO2);
    motor1=new BLMotor(1100,2000,0,THRUST1,55);
    motor2=new BLMotor(1100,2000,0,THRUST2,58);
    led=new LED(LED_BUILTIN);

    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);

    getPreferences();
    motor1->setPowerLimit(motorPowerLimit);
    motor2->setPowerLimit(motorPowerLimit);

    // Sigue existiendo SAFE BOOT: nada se arma automaticamente al encender.
    setActuatorsArmed(false);
}

int RawBicopter::sense(float sensors[MAX_SENSORS]){ return 0; }

void RawBicopter::actuate(const float actuators[],int size){
    if(!actuatorsArmed) return;
    servo1->act(actuators[2]);
    servo2->act(actuators[3]);
    motor1->act(constrain(actuators[0],0.0f,motorPowerLimit));
    motor2->act(constrain(actuators[1],0.0f,motorPowerLimit));
    led->act(actuators[4]);
}

void RawBicopter::control(float sensors[MAX_SENSORS],float controls[],int size){
    if(actuatorsArmed) actuate(controls,size);
}

void RawBicopter::setActuatorsArmed(bool armedState){
    if(armedState){
        if(actuatorsArmed) return;

        // Salimos del modo servo-only, pero no movemos los servos de su ultima posicion.
        servoCalibrationSelection=0;
        servo1->enable();
        servo2->enable();
        motor1->enable();
        motor2->enable();
        motor1->setPowerLimit(motorPowerLimit);
        motor2->setPowerLimit(motorPowerLimit);

        // SECUENCIA EFECTIVA DEL FIRMWARE VIEJO QUE YA FUNCIONABA CON ESTOS ESC.
        // IMPORTANTE: en el codigo original, (i - 1000) / 1000 era division
        // ENTERA, por lo que el resultado era 0 durante todo el bucle.
        // Es decir, el ESC recibia throttle minimo (~1100 us) durante ~3.7 s;
        // NO recibia una rampa 5% -> 50%. Reproducimos ese comportamiento.
        motor1->act(0.0f);
        motor2->act(0.0f);
        delay(10);

        for(int i=1050;i<1500;i++){
            motor1->act(0.0f);
            motor2->act(0.0f);
            delay(6);
        }

        motor1->act(0.0f);
        motor2->act(0.0f);
        delay(1000);

        actuatorsArmed=true;
    } else {
        if(motor1) motor1->disable();
        if(motor2) motor2->disable();
        if(servo1) servo1->disable();
        if(servo2) servo2->disable();
        actuatorsArmed=false;
        servoCalibrationSelection=0;
    }
}

void RawBicopter::setServoCalibrationSelection(int selection){
    selection=constrain(selection,0,3);

    // P00 nunca deja PWM de brushless activo.
    if(motor1) motor1->disable();
    if(motor2) motor2->disable();
    actuatorsArmed=false;

    if(selection==servoCalibrationSelection) return;

    // Muy importante: el servo NO seleccionado queda detach, por lo que no recibe
    // ningun comando cuando se prueba el otro.
    if(servo1) servo1->disable();
    if(servo2) servo2->disable();

    if(selection==1 || selection==3) servo1->enable();
    if(selection==2 || selection==3) servo2->enable();
    servoCalibrationSelection=selection;
}

void RawBicopter::setServoCalibrationEnabled(bool enabledState){
    setServoCalibrationSelection(enabledState ? 3 : 0);
}

void RawBicopter::commandServoCalibrationUs(int servo1Us,int servo2Us){
    if(actuatorsArmed || servoCalibrationSelection==0) return;

    if(servoCalibrationSelection==1 || servoCalibrationSelection==3)
        servo1->actMicroseconds(constrain(servo1Us,900,2100));

    if(servoCalibrationSelection==2 || servoCalibrationSelection==3)
        servo2->actMicroseconds(constrain(servo2Us,900,2100));
}

void RawBicopter::commandMotorPowerTest(float motor1Power,float motor2Power,int servo1Us,int servo2Us){
    if(!actuatorsArmed || servoCalibrationSelection!=0) return;

    servo1Us=constrain(servo1Us,900,2100);
    servo2Us=constrain(servo2Us,900,2100);
    motor1Power=constrain(motor1Power,0.0f,motorPowerLimit);
    motor2Power=constrain(motor2Power,0.0f,motorPowerLimit);

    servo1->actMicroseconds(servo1Us);
    servo2->actMicroseconds(servo2Us);
    motor1->act(motor1Power);
    motor2->act(motor2Power);
}

void RawBicopter::calibrate(){
    Serial.println("[SAFE] calibracion ESC automatica bloqueada.");
}

void RawBicopter::arm(){ setActuatorsArmed(true); }

void RawBicopter::getPreferences(){
    Preferences p;
    p.begin("params",true);

    PDterms.servoBeta=p.getFloat("servoBeta",90);
    PDterms.servoRange=p.getFloat("servoRange",260);
    PDterms.botZlim=p.getFloat("botZlim",0.001);
    PDterms.servo2Mirror=p.getBool("servo2Mirror",true);
    PDterms.yawInvert=p.getFloat("yawInvert",1.0);
    PDterms.servo1Trim=p.getFloat("servo1Trim",0.0);
    PDterms.servo2Trim=p.getFloat("servo2Trim",0.0);
    PDterms.swapFxTz=p.getBool("swapFxTz",false);
    PDterms.pitchOffset=p.getFloat("pitchOffset",0);
    PDterms.pitchInvert=p.getFloat("pitchInvert",1);
    PDterms.servo_move_min=p.getFloat("servo_move_min",2);

    // P00M debe reproducir la potencia del firmware heredado.
    // Ignoramos cualquier motorPowerLimit antiguo que haya quedado persistido
    // en NVS durante pruebas previas al 50%.
    motorPowerLimit=1.00f;
    if(motor1) motor1->setPowerLimit(motorPowerLimit);
    if(motor2) motor2->setPowerLimit(motorPowerLimit);

    servoDiff=2*PI-PDterms.servoRange*PI/180.0f;
    p.end();
}

float RawBicopter::adjustAngle(float angle){
    while(angle < -servoDiff/2-PDterms.servoBeta*PI/180.0f) angle+=2*PI;
    while(angle > 2*PI-servoDiff/2-PDterms.servoBeta*PI/180.0f) angle-=2*PI;
    return angle;
}

float RawBicopter::clamp(float val,float minVal,float maxVal){
    return std::max(minVal,std::min(maxVal,val));
}