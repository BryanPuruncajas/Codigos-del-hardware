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
void p00ServoCalibration(AppContext& ctx){
    // AUX0 = angulo S1 0..120
    // AUX1 = angulo S2 0..120
    // AUX2 = selector: 1=S1, 2=S2, 3=ambos
    const float s1Deg=constrain(ctx.command.params[AppConfig::PARAM_AUX0],
                                AppConfig::P0025_MIN_DEG,AppConfig::P0025_MAX_DEG);
    const float s2Deg=constrain(ctx.command.params[AppConfig::PARAM_AUX1],
                                AppConfig::P0025_MIN_DEG,AppConfig::P0025_MAX_DEG);
    const int selection=constrain((int)lroundf(ctx.command.params[AppConfig::PARAM_AUX2]),1,3);

    const int s1Us=degreesToPulseUs(s1Deg);
    const int s2Us=degreesToPulseUs(s2Deg);

    ctx.robot->setServoCalibrationSelection(selection);
    ctx.robot->commandServoCalibrationUs(s1Us,s2Us);

    ctx.robot->servo_old1=s1Deg;
    ctx.robot->servo_old2=s2Deg;
    ctx.robot->motor_power1=0.0f;
    ctx.robot->motor_power2=0.0f;
    Telemetry::sendControl(ctx,(float)s1Us,(float)s2Us,(float)selection);
}
}