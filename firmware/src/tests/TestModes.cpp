#include "tests/TestModes.h"
#include "tests/TestRunners.h"
#include "app/AppConfig.h"
#include "app/SensorMap.h"
#include "app/Telemetry.h"
#include "mission/BalloonMission.h"
#include "app/FlowSafety.h"

namespace {
BalloonMission mission;
int pausedMode=-1;
uint32_t pausedAt=0;
bool resuming=false;
uint16_t lastManualAck=0;
bool autonomous(int mode){return mode>=AppConfig::P08_ONE_BALLOON && mode<=AppConfig::P11_INFINITY;}
void neutral(AppContext& ctx){
    if(!ctx.robot) return;
    float c[5]={0,0,0,0,0};
    if(ctx.robot->actuatorsAreArmed()) ctx.robot->control(ctx.sensors,c,5);
}
}

namespace TestModes {
void onModeEnter(AppContext& ctx,int newMode){
    const bool resume=resuming && ctx.mode==AppConfig::P02_MANUAL_CONTROL && pausedMode==newMode;
    resuming=false;
    if(newMode==AppConfig::P02_MANUAL_CONTROL && autonomous(ctx.mode)){
        pausedMode=ctx.mode;pausedAt=millis();
    }else if(newMode!=AppConfig::P02_MANUAL_CONTROL && !resume){pausedMode=-1;lastManualAck=0;}
    ctx.mode=newMode;
    ctx.modeEnteredMs=millis();
    ctx.resetRequested=false;
    if(!ctx.robot) return;

    ctx.robot->z_integral=0;
    ctx.robot->yaw_integral=0;
    ctx.robot->yawrate_integral=0;
    ctx.robot->PDterms.yawEn=false;
    ctx.robot->PDterms.zEn=false;
    ctx.robot->PDterms.rollEn=false;
    ctx.robot->PDterms.pitchEn=false;
    ctx.robot->PDterms.rotateEn=false;

    // Al entrar a P00 NO centramos ambos servos. El selector recibido decide
    // cual se conecta y mueve; el otro permanece detach.
    if(newMode==AppConfig::P00_SERVO_CALIBRATION){
        ctx.robot->setServoCalibrationSelection(0);
    } else if(ctx.robot->servoCalibrationIsEnabled()) {
        ctx.robot->setServoCalibrationSelection(0);
    }

    if(newMode==AppConfig::P03_YAW_CONTROL) ctx.robot->PDterms.yawEn=true;
    if(newMode==AppConfig::P04_ALTITUDE_CONTROL) ctx.robot->PDterms.zEn=true;
    if(newMode==AppConfig::P05_YAW_ALTITUDE){
        ctx.robot->PDterms.yawEn=true;
        ctx.robot->PDterms.zEn=true;
    }
    if(newMode==AppConfig::P07_VISUAL_CENTER) ctx.robot->PDterms.yawEn=true;
    if(resume){mission.resumeFromManual(ctx,uint32_t(millis()-pausedAt));pausedMode=-1;}
    if(!resume && newMode==AppConfig::P08_ONE_BALLOON){ mission.configure(1,true); mission.reset(ctx); }
    if(!resume && newMode==AppConfig::P09_VISIT_ESCAPE){ mission.configure(1,false); mission.reset(ctx); }
    if(!resume && newMode==AppConfig::P10_TWO_BALLOONS){ mission.configure(2,false); mission.reset(ctx); }
    if(!resume && newMode==AppConfig::P11_FOUR_BALLOONS){ mission.configure(4,false); mission.reset(ctx); }
    if(!resume && newMode==AppConfig::P11_INFINITY){ mission.configure(4,false,2); mission.reset(ctx); }
    if(newMode==AppConfig::P12_VISION_BENCH){
        ctx.robot->setActuatorsArmed(false);
        mission.configure(5,false,2);
        mission.reset(ctx);
    }
}

void reset(AppContext& ctx){ onModeEnter(ctx,ctx.mode); }

bool resumePausedMission(AppContext& ctx,int mode){
    if(!ctx.robot || ctx.mode!=AppConfig::P02_MANUAL_CONTROL || !autonomous(mode) || pausedMode!=mode ||
       mission.state()==BalloonMission::DONE || !ctx.flowHeight.valid || !ctx.robot->actuatorsAreArmed())return false;
    resuming=true;onModeEnter(ctx,mode);return true;
}

bool completePausedWaypoint(AppContext& ctx){
    const float value=ctx.command.params[AppConfig::PARAM_AUX1];
    if(!ctx.robot || ctx.mode!=AppConfig::P02_MANUAL_CONTROL || !autonomous(pausedMode) ||
       !isfinite(value) || value<1 || value>65535 || value!=floorf(value))return false;
    const uint16_t id=uint16_t(value),step=uint16_t(id-lastManualAck);
    if(lastManualAck && (step==0 || step>=0x8000u))return step==0;
    if(!mission.manualWaypointClear(ctx))return false;
    lastManualAck=id;return true;
}

void run(AppContext& ctx){
    const bool missingRange=flowControlRequired(ctx.mode) && !ctx.flowHeight.valid;
    if(missingRange && ctx.robot && ctx.robot->actuatorsAreArmed())ctx.robot->setActuatorsArmed(false);
    Telemetry::sendCore(ctx);
    Telemetry::sendNicla(ctx);
    Telemetry::sendNiclaStatus(ctx);
    Telemetry::sendActuators(ctx);
    Telemetry::sendFlow(ctx);
    Telemetry::sendFlowControl(ctx);

    if(missingRange)return;
    switch(ctx.mode){
      case AppConfig::SAFE_STOP: neutral(ctx); break;
      case AppConfig::P00_MOTOR_POWER_TEST: TestRunners::p00MotorPower(ctx); break;
      case AppConfig::P00_SERVO_CALIBRATION: TestRunners::p00ServoCalibration(ctx); break;
      case AppConfig::P01_SENSOR_INTEGRATION: TestRunners::p01Sensors(ctx); break;
      case AppConfig::P02_MANUAL_CONTROL:
        TestRunners::p02Manual(ctx);
        if(autonomous(pausedMode))mission.reportPaused(ctx,pausedAt);
        break;
      case AppConfig::P03_YAW_CONTROL: TestRunners::p03Yaw(ctx); break;
      case AppConfig::P04_ALTITUDE_CONTROL: TestRunners::p04Altitude(ctx); break;
      case AppConfig::P05_YAW_ALTITUDE: TestRunners::p05YawAltitude(ctx); break;
      case AppConfig::P06_NICLA_RAW: TestRunners::p06NiclaRaw(ctx); break;
      case AppConfig::P07_VISUAL_CENTER: TestRunners::p07VisualCenter(ctx); break;
      case AppConfig::P08_ONE_BALLOON:
      case AppConfig::P09_VISIT_ESCAPE:
      case AppConfig::P10_TWO_BALLOONS:
      case AppConfig::P11_FOUR_BALLOONS:
      case AppConfig::P11_INFINITY:
      case AppConfig::P12_VISION_BENCH: mission.update(ctx); break;
      default: neutral(ctx); break;
    }
}
}
