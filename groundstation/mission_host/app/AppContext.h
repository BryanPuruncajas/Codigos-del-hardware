#pragma once
#include "Arduino.h"
#include "app/AppConfig.h"
struct NiclaFeedback {
 enum {SEARCH,CONFIRM,FULL,PARTIAL,MISSING,AMBIG,LOST};
 static constexpr uint16_t N03_PROTOCOL=0x4e03;
 uint32_t generation=1;uint16_t protocol=N03_PROTOCOL,state=FULL,sample=0;
 bool fresh=true,resetPending=false;
};
struct Suite {
 struct Barometer {uint32_t ageMs=0;bool healthy=false;uint32_t sampleTimeUs=0;} barometer;
 NiclaFeedback fb;unsigned resets=0;
 const NiclaFeedback& niclaFeedback() const {return fb;}
 Barometer verticalDiagnostics() const {return barometer;}
 void resetNiclaTarget(){++resets;fb.fresh=false;fb.resetPending=true;}
};
struct RobotMock {
 Suite sensorsuite;bool armed=false;unsigned writes=0;
 struct {bool yawEn=true,zEn=true,rollEn=false,pitchEn=false,rotateEn=false;} PDterms;
 float z_integral=0,yaw_integral=0,yawrate_integral=0;
 bool calibration=false;
 void setServoCalibrationSelection(int x){calibration=x!=0;}
 bool servoCalibrationIsEnabled(){return calibration;}
 void control(float*,float*,int){++writes;}
 float servo_old1=0,servo_old2=0,motor_power1=0,motor_power2=0;
 bool actuatorsAreArmed(){return armed;}
 void setActuatorsArmed(bool x){armed=x;}
 void commandMotorPowerTest(float,float,int,int){++writes;}
};
#include "mission/FlowHeight.h"

struct AppContext {
    FlowHeight flowHeight;
 RobotMock* robot=nullptr;
 float sensors[32]={};struct {float params[16]={};} command;
 int mode=21;
 unsigned long modeEnteredMs=0;
 bool resetRequested=false;
};
