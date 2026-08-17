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

constexpr float DEFAULT_YAW_KP = 0.12f;
constexpr float DEFAULT_YAW_KD = 0.035f;
constexpr float DEFAULT_YAW_MIN_POWER = 0.06f;
constexpr float DEFAULT_YAW_MAX_POWER = 0.12f;
constexpr float DEFAULT_YAW_DEADBAND_RAD = 5.0f * PI / 180.0f;
constexpr float YAW_RATE_FILTER_ALPHA = 0.80f;

struct YawConfig {
    float kp;
    float kd;
    float minPower;
    float maxPower;
    float deadbandRad;
};

struct YawState {
    bool initialized = false;
    float lastYaw = 0.0f;
    float filteredYawVelocity = 0.0f;
    unsigned long lastUs = 0;
    unsigned long modeEnteredMs = 0;
};

YawState ctrl;

float wrapPi(float angle) {
    while (angle > PI) angle -= 2.0f * PI;
    while (angle < -PI) angle += 2.0f * PI;
    return angle;
}

YawConfig getConfig(const AppContext& ctx) {
    const float kp = ctx.command.params[AppConfig::PARAM_AUX0];
    const float kd = ctx.command.params[AppConfig::PARAM_AUX1];
    const float minPower = ctx.command.params[AppConfig::PARAM_AUX2];
    const float maxPower = ctx.command.params[AppConfig::PARAM_AUX3];
    const float deadband = ctx.command.params[AppConfig::PARAM_AUX4];

    const bool valid = isfinite(kp) && kp >= 0.0f &&
                       isfinite(kd) && kd >= 0.0f &&
                       isfinite(minPower) && minPower >= 0.0f &&
                       isfinite(maxPower) && maxPower > 0.0f && maxPower <= 1.0f &&
                       minPower <= maxPower &&
                       isfinite(deadband) && deadband > 0.0f && deadband < PI;
    if (valid) return {kp, kd, minPower, maxPower, deadband};
    return {DEFAULT_YAW_KP, DEFAULT_YAW_KD, DEFAULT_YAW_MIN_POWER,
            DEFAULT_YAW_MAX_POWER, DEFAULT_YAW_DEADBAND_RAD};
}

int degreesToPulseUs(float deg) {
    const float clipped = constrain(deg, AppConfig::P0025_MIN_DEG, AppConfig::P0025_MAX_DEG);
    const float spanDeg = AppConfig::P0025_MAX_DEG - AppConfig::P0025_MIN_DEG;
    const float spanUs = (float)(AppConfig::P0025_MAX_US - AppConfig::P0025_MIN_US);
    return (int)lroundf(AppConfig::P0025_MIN_US +
                        (clipped - AppConfig::P0025_MIN_DEG) * spanUs / spanDeg);
}

void resetYawController(float yawNow) {
    ctrl.initialized = isfinite(yawNow);
    ctrl.lastYaw = isfinite(yawNow) ? yawNow : 0.0f;
    ctrl.filteredYawVelocity = 0.0f;
    ctrl.lastUs = micros();
}

float updateYawVelocity(float yawNow) {
    if (!ctrl.initialized || !isfinite(yawNow)) {
        resetYawController(yawNow);
        return 0.0f;
    }
    const unsigned long nowUs = micros();
    float dt = (nowUs - ctrl.lastUs) * 1.0e-6f;
    ctrl.lastUs = nowUs;
    if (!isfinite(dt) || dt <= 0.0f || dt > 0.20f) dt = 0.01f;

    const float rawVelocity = wrapPi(yawNow - ctrl.lastYaw) / dt;
    ctrl.lastYaw = yawNow;
    ctrl.filteredYawVelocity = YAW_RATE_FILTER_ALPHA * ctrl.filteredYawVelocity +
                               (1.0f - YAW_RATE_FILTER_ALPHA) * rawVelocity;
    return ctrl.filteredYawVelocity;
}
}

namespace TestRunners {
void p03Yaw(AppContext& ctx) {
    const float yawRef = ctx.command.params[AppConfig::PARAM_TZ];
    const float yawNow = ctx.sensors[SensorMap::YAW];
    const YawConfig cfg = getConfig(ctx);

    if (!ctrl.initialized || ctrl.modeEnteredMs != ctx.modeEnteredMs) {
        resetYawController(yawNow);
        ctrl.modeEnteredMs = ctx.modeEnteredMs;
    }

    const float yawVelocity = updateYawVelocity(yawNow);
    const float yawError = (isfinite(yawRef) && isfinite(yawNow)) ? wrapPi(yawRef - yawNow) : 0.0f;

    float servo1Deg = SERVO1_Z_DEG;
    float servo2Deg = SERVO2_Z_DEG;
    float motorPower = 0.0f;

    if (isfinite(yawRef) && isfinite(yawNow) && fabsf(yawError) > cfg.deadbandRad) {
        const float direction = yawError > 0.0f ? 1.0f : -1.0f;
        const float velocityTowardTarget = yawVelocity * direction;
        float effort = cfg.kp * fabsf(yawError) - cfg.kd * velocityTowardTarget;
        effort = constrain(effort, 0.0f, cfg.maxPower);
        if (effort > 0.0f) motorPower = max(cfg.minPower, effort);

        if (yawError > 0.0f) {
            servo1Deg = SERVO1_LEFT_DEG;
            servo2Deg = SERVO2_LEFT_DEG;
        } else {
            servo1Deg = SERVO1_RIGHT_DEG;
            servo2Deg = SERVO2_RIGHT_DEG;
        }
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

    Telemetry::sendControl(ctx, yawRef, 0.0f, yawError);
}
}