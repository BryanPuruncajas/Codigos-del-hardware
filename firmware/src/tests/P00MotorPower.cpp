#include "tests/TestRunners.h"
#include "app/AppConfig.h"
#include "app/Telemetry.h"
#include <Arduino.h>

namespace {
int degreesToPulseUs(float deg){
    const float clipped=constrain(deg,AppConfig::P0025_MIN_DEG,AppConfig::P0025_MAX_DEG);
    const float spanDeg=AppConfig::P0025_MAX_DEG-AppConfig::P0025_MIN_DEG;
    const float spanUs=(float)(AppConfig::P0025_MAX_US-AppConfig::P0025_MIN_US);
    return (int)lroundf(AppConfig::P0025_MIN_US+
                       (clipped-AppConfig::P0025_MIN_DEG)*spanUs/spanDeg);
}
}

namespace TestRunners {
void p00MotorPower(AppContext& ctx){
    // AUX0 = angulo S1 [0..120]
    // AUX1 = angulo S2 [0..120]
    // AUX2 = potencia M1 [0..1]  (misma escala que firmware viejo)
    // AUX3 = potencia M2 [0..1]
    const float s1Deg=constrain(ctx.command.params[AppConfig::PARAM_AUX0],
                                AppConfig::P0025_MIN_DEG,AppConfig::P0025_MAX_DEG);
    const float s2Deg=constrain(ctx.command.params[AppConfig::PARAM_AUX1],
                                AppConfig::P0025_MIN_DEG,AppConfig::P0025_MAX_DEG);
    const float m1=constrain(ctx.command.params[AppConfig::PARAM_AUX2],
                             0.0f,AppConfig::ABSOLUTE_MOTOR_POWER_LIMIT);
    const float m2=constrain(ctx.command.params[AppConfig::PARAM_AUX3],
                             0.0f,AppConfig::ABSOLUTE_MOTOR_POWER_LIMIT);

    const int s1Us=degreesToPulseUs(s1Deg);
    const int s2Us=degreesToPulseUs(s2Deg);
    const float appliedM1=constrain(m1,0.0f,ctx.robot->getMotorPowerLimit());
    const float appliedM2=constrain(m2,0.0f,ctx.robot->getMotorPowerLimit());

    ctx.robot->commandMotorPowerTest(appliedM1,appliedM2,s1Us,s2Us);

    ctx.robot->servo_old1=s1Deg;
    ctx.robot->servo_old2=s2Deg;
    // Telemetria = potencia REAL aplicada, no solo la solicitada.
    ctx.robot->motor_power1=appliedM1;
    ctx.robot->motor_power2=appliedM2;
    Telemetry::sendControl(ctx,(float)s1Us,(float)s2Us,max(appliedM1,appliedM2));
}
}