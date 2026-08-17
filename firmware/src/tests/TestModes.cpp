#include "tests/TestModes.h"
#include "tests/TestRunners.h"
#include "app/AppConfig.h"
#include "app/SensorMap.h"
#include "app/Telemetry.h"
#include "mission/BalloonMission.h"

namespace {
BalloonMission mission;
void neutral(AppContext& ctx){
    if(!ctx.robot) return;
    float c[5]={0,0,0,0,0};
    if(ctx.robot->actuatorsAreArmed()) ctx.robot->control(ctx.sensors,c,5);
}
}

namespace TestModes {
void onModeEnter(AppContext& ctx,int newMode){
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
    if(newMode==AppConfig::P08_ONE_BALLOON){ mission.configure(1,true); mission.reset(ctx); }
    if(newMode==AppConfig::P09_VISIT_ESCAPE){ mission.configure(1,false); mission.reset(ctx); }
    if(newMode==AppConfig::P10_TWO_BALLOONS){ mission.configure(2,false); mission.reset(ctx); }
    if(newMode==AppConfig::P11_FOUR_BALLOONS){ mission.configure(4,false); mission.reset(ctx); }
}

void reset(AppContext& ctx){ onModeEnter(ctx,ctx.mode); }

void run(AppContext& ctx){
    Telemetry::sendCore(ctx);
    Telemetry::sendNicla(ctx);
    Telemetry::sendActuators(ctx);

    switch(ctx.mode){
      case AppConfig::SAFE_STOP: neutral(ctx); break;
      case AppConfig::P00_MOTOR_POWER_TEST: TestRunners::p00MotorPower(ctx); break;
      case AppConfig::P00_SERVO_CALIBRATION: TestRunners::p00ServoCalibration(ctx); break;
      case AppConfig::P01_SENSOR_INTEGRATION: TestRunners::p01Sensors(ctx); break;
      case AppConfig::P02_MANUAL_CONTROL: TestRunners::p02Manual(ctx); break;
      case AppConfig::P03_YAW_CONTROL: TestRunners::p03Yaw(ctx); break;
      case AppConfig::P04_ALTITUDE_CONTROL: TestRunners::p04Altitude(ctx); break;
      case AppConfig::P05_YAW_ALTITUDE: TestRunners::p05YawAltitude(ctx); break;
      case AppConfig::P06_NICLA_RAW: TestRunners::p06NiclaRaw(ctx); break;
      case AppConfig::P07_VISUAL_CENTER: TestRunners::p07VisualCenter(ctx); break;
      case AppConfig::P08_ONE_BALLOON:
      case AppConfig::P09_VISIT_ESCAPE:
      case AppConfig::P10_TWO_BALLOONS:
      case AppConfig::P11_FOUR_BALLOONS: mission.update(ctx); break;
      default: neutral(ctx); break;
    }
}
}