#pragma once
#include "app/AppContext.h"
namespace TestRunners {
void p00MotorPower(AppContext& ctx);
void p00ServoCalibration(AppContext& ctx);
void p01Sensors(AppContext& ctx);
void p02Manual(AppContext& ctx);
void p03Yaw(AppContext& ctx);
void p04Altitude(AppContext& ctx);
void p05YawAltitude(AppContext& ctx);
void p06NiclaRaw(AppContext& ctx);
void p07VisualCenter(AppContext& ctx);
}