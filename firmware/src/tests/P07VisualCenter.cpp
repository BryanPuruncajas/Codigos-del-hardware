#include "tests/TestRunners.h"
#include "app/AppConfig.h"
#include "app/SensorMap.h"
#include "app/Telemetry.h"
#include <Arduino.h>
#include <math.h>

namespace {
constexpr float SERVO1_Z_DEG     = 35.0f;
constexpr float SERVO2_Z_DEG     = 85.0f;
constexpr float SERVO1_RIGHT_DEG = 120.0f;
constexpr float SERVO2_RIGHT_DEG = 120.0f;
constexpr float SERVO1_LEFT_DEG  = 0.0f;
constexpr float SERVO2_LEFT_DEG  = 0.0f;
constexpr float IMAGE_CENTER_X = 120.0f;

constexpr float DEFAULT_VIS_KP = 0.10f;
constexpr float DEFAULT_VIS_KD = 0.015f;
constexpr float DEFAULT_VIS_MIN_POWER = 0.06f;
constexpr float DEFAULT_VIS_MAX_POWER = 0.10f;
constexpr float DEFAULT_VIS_DEADBAND_PX = 15.0f;
constexpr float DERIV_FILTER_ALPHA = 0.80f;

struct VisualConfig {
    float kp;
    float kd;
    float minPower;
    float maxPower;
    float deadbandPx;
};
struct VisualState {
    bool initialized = false;
    float lastError = 0.0f;
    float filteredDerivative = 0.0f;
    unsigned long lastUs = 0;
    unsigned long modeEnteredMs = 0;
};
VisualState ctrl;

bool detected(const AppContext& ctx) {
    const int flag = (int)ctx.sensors[SensorMap::NICLA_FLAG];
    return ((flag & 0x40) != 0) && ((flag & 0x03) != 0);
}

VisualConfig getConfig(const AppContext& ctx) {
    const float kp = ctx.command.params[AppConfig::PARAM_AUX0];
    const float kd = ctx.command.params[AppConfig::PARAM_AUX1];
    const float minPower = ctx.command.params[AppConfig::PARAM_AUX2];
    const float maxPower = ctx.command.params[AppConfig::PARAM_AUX3];
    const float deadbandPx = ctx.command.params[AppConfig::PARAM_AUX4];
    const bool valid = isfinite(kp) && kp >= 0.0f &&
                       isfinite(kd) && kd >= 0.0f &&
                       isfinite(minPower) && minPower >= 0.0f &&
                       isfinite(maxPower) && maxPower > 0.0f && maxPower <= 1.0f &&
                       minPower <= maxPower &&
                       isfinite(deadbandPx) && deadbandPx >= 1.0f && deadbandPx <= 100.0f;
    if (valid) return {kp, kd, minPower, maxPower, deadbandPx};
    return {DEFAULT_VIS_KP, DEFAULT_VIS_KD, DEFAULT_VIS_MIN_POWER,
            DEFAULT_VIS_MAX_POWER, DEFAULT_VIS_DEADBAND_PX};
}

int degreesToPulseUs(float deg) {
    const float clipped = constrain(deg, AppConfig::P0025_MIN_DEG, AppConfig::P0025_MAX_DEG);
    const float spanDeg = AppConfig::P0025_MAX_DEG - AppConfig::P0025_MIN_DEG;
    const float spanUs = (float)(AppConfig::P0025_MAX_US - AppConfig::P0025_MIN_US);
    return (int)lroundf(AppConfig::P0025_MIN_US +
                        (clipped - AppConfig::P0025_MIN_DEG) * spanUs / spanDeg);
}

void resetVisual(float errorNorm = 0.0f) {
    ctrl.initialized = true;
    ctrl.lastError = errorNorm;
    ctrl.filteredDerivative = 0.0f;
    ctrl.lastUs = micros();
}

float visualEffort(float errorNorm, const VisualConfig& cfg) {
    if (!ctrl.initialized) resetVisual(errorNorm);
    const unsigned long nowUs = micros();
    float dt = (nowUs - ctrl.lastUs) * 1.0e-6f;
    ctrl.lastUs = nowUs;
    if (!isfinite(dt) || dt <= 0.0f || dt > 0.20f) dt = 0.01f;

    const float deriv = (errorNorm - ctrl.lastError) / dt;
    ctrl.lastError = errorNorm;
    ctrl.filteredDerivative = DERIV_FILTER_ALPHA * ctrl.filteredDerivative +
                              (1.0f - DERIV_FILTER_ALPHA) * deriv;

    const float direction = errorNorm >= 0.0f ? 1.0f : -1.0f;
    const float radialRate = ctrl.filteredDerivative * direction;
    float effort = cfg.kp * fabsf(errorNorm) + cfg.kd * radialRate;
    effort = constrain(effort, 0.0f, cfg.maxPower);
    if (effort > 0.0f) effort = max(cfg.minPower, effort);
    return effort;
}
}

namespace TestRunners {
void p07VisualCenter(AppContext& ctx) {
    const VisualConfig cfg = getConfig(ctx);
    if (ctrl.modeEnteredMs != ctx.modeEnteredMs) {
        resetVisual();
        ctrl.modeEnteredMs = ctx.modeEnteredMs;
    }

    const bool hasTarget = detected(ctx);
    const float x = ctx.sensors[SensorMap::NICLA_X];
    float servo1Deg = SERVO1_Z_DEG;
    float servo2Deg = SERVO2_Z_DEG;
    float motorPower = 0.0f;
    float errorNorm = 0.0f;

    if (hasTarget && isfinite(x)) {
        const float errorPx = x - IMAGE_CENTER_X;
        errorNorm = constrain(errorPx / IMAGE_CENTER_X, -1.0f, 1.0f);
        if (fabsf(errorPx) > cfg.deadbandPx) {
            motorPower = visualEffort(errorNorm, cfg);
            if (errorPx < 0.0f) {
                servo1Deg = SERVO1_LEFT_DEG;
                servo2Deg = SERVO2_LEFT_DEG;
            } else {
                servo1Deg = SERVO1_RIGHT_DEG;
                servo2Deg = SERVO2_RIGHT_DEG;
            }
        } else {
            resetVisual(errorNorm);
        }
    } else {
        resetVisual();
    }

    if (ctx.robot->actuatorsAreArmed()) {
        ctx.robot->commandMotorPowerTest(motorPower, motorPower,
                                         degreesToPulseUs(servo1Deg),
                                         degreesToPulseUs(servo2Deg));
    }
    ctx.robot->servo_old1 = servo1Deg;
    ctx.robot->servo_old2 = servo2Deg;
    ctx.robot->motor_power1 = motorPower;
    ctx.robot->motor_power2 = motorPower;

    Telemetry::sendControl(ctx, ctx.sensors[SensorMap::YAW], 0.0f, errorNorm);
}
}