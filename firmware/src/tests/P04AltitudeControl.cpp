#include "tests/TestRunners.h"
#include "app/AppConfig.h"
#include "app/SensorMap.h"
#include "app/Telemetry.h"
#include <Arduino.h>
#include <math.h>

namespace {

// Vector vertical fisicamente validado.
constexpr float SERVO1_Z_DEG = 35.0f;
constexpr float SERVO2_Z_DEG = 85.0f;

// Valores por defecto. Se pueden sobreescribir desde run_test.py:
// --kp, --ki, --kd, --max-power, --slew
constexpr float DEFAULT_ALT_KP = 0.30f;
constexpr float DEFAULT_ALT_KI = 0.025f;
constexpr float DEFAULT_ALT_KD = 0.18f;
constexpr float DEFAULT_ALT_MAX_POWER = 0.12f;
constexpr float DEFAULT_POWER_SLEW_PER_SEC = 0.18f;

// El integral nunca aporta mas de 6% por si solo.
constexpr float ALT_I_POWER_MAX = 0.06f;
constexpr float ALT_I_ACTIVE_ERROR_M = 0.35f;
constexpr float VZ_FILTER_ALPHA = 0.82f;
constexpr float HEIGHT_ERROR_DEADBAND_M = 0.025f;

struct ControllerConfig {
    float kp;
    float ki;
    float kd;
    float maxPower;
    float slewPerSec;
};

struct AltitudeControllerState {
    bool initialized = false;
    float filteredVz = 0.0f;
    float integralPower = 0.0f;
    float lastPower = 0.0f;
    float lastReference = NAN;
    unsigned long lastUs = 0;
    unsigned long lastModeEnteredMs = 0;
};

AltitudeControllerState ctrl;

ControllerConfig getConfig(const AppContext& ctx) {
    const float kp = ctx.command.params[AppConfig::PARAM_AUX0];
    const float ki = ctx.command.params[AppConfig::PARAM_AUX1];
    const float kd = ctx.command.params[AppConfig::PARAM_AUX2];
    const float maxPower = ctx.command.params[AppConfig::PARAM_AUX3];
    const float slew = ctx.command.params[AppConfig::PARAM_AUX4];

    // Durante el paquete ARM los AUX llegan en cero. En ese caso usamos defaults.
    // maxPower y slew actuan como marcadores de que la configuracion fue enviada.
    const bool consoleConfigValid =
        isfinite(kp) && kp >= 0.0f &&
        isfinite(ki) && ki >= 0.0f &&
        isfinite(kd) && kd >= 0.0f &&
        isfinite(maxPower) && maxPower > 0.0f && maxPower <= 1.0f &&
        isfinite(slew) && slew > 0.0f;

    if (consoleConfigValid) {
        return {
            kp,
            ki,
            kd,
            constrain(maxPower, 0.01f, 1.0f),
            constrain(slew, 0.01f, 2.0f)
        };
    }

    return {
        DEFAULT_ALT_KP,
        DEFAULT_ALT_KI,
        DEFAULT_ALT_KD,
        DEFAULT_ALT_MAX_POWER,
        DEFAULT_POWER_SLEW_PER_SEC
    };
}

int degreesToPulseUs(float deg) {
    const float clipped = constrain(deg,
                                    AppConfig::P0025_MIN_DEG,
                                    AppConfig::P0025_MAX_DEG);
    const float spanDeg = AppConfig::P0025_MAX_DEG - AppConfig::P0025_MIN_DEG;
    const float spanUs = (float)(AppConfig::P0025_MAX_US - AppConfig::P0025_MIN_US);

    return (int)lroundf(AppConfig::P0025_MIN_US +
                        (clipped - AppConfig::P0025_MIN_DEG) * spanUs / spanDeg);
}

void resetController(float reference, float rawVz) {
    ctrl.initialized = true;
    ctrl.filteredVz = isfinite(rawVz) ? rawVz : 0.0f;
    ctrl.integralPower = 0.0f;
    ctrl.lastPower = 0.0f;
    ctrl.lastReference = reference;
    ctrl.lastUs = micros();
}

float calculateAltitudePower(float reference,
                             float height,
                             float rawVz,
                             const ControllerConfig& cfg) {
    if (!isfinite(reference) || !isfinite(height) || !isfinite(rawVz)) {
        ctrl.lastPower = 0.0f;
        return 0.0f;
    }

    if (!ctrl.initialized || !isfinite(ctrl.lastReference) ||
        fabsf(reference - ctrl.lastReference) > 0.05f) {
        resetController(reference, rawVz);
    }

    const unsigned long nowUs = micros();
    float dt = (nowUs - ctrl.lastUs) * 1.0e-6f;
    ctrl.lastUs = nowUs;

    if (!isfinite(dt) || dt <= 0.0f || dt > 0.20f) {
        dt = 0.01f;
    }

    ctrl.lastReference = reference;

    ctrl.filteredVz = VZ_FILTER_ALPHA * ctrl.filteredVz +
                      (1.0f - VZ_FILTER_ALPHA) * rawVz;

    const float error = reference - height;
    float pError = error;
    if (fabsf(pError) < HEIGHT_ERROR_DEADBAND_M) {
        pError = 0.0f;
    }

    if (fabsf(error) <= ALT_I_ACTIVE_ERROR_M) {
        ctrl.integralPower += cfg.ki * error * dt;
        const float iLimit = min(ALT_I_POWER_MAX, cfg.maxPower);
        ctrl.integralPower = constrain(ctrl.integralPower, 0.0f, iLimit);
    }

    const float pTerm = cfg.kp * pError;
    const float dTerm = -cfg.kd * ctrl.filteredVz;
    float requestedPower = pTerm + ctrl.integralPower + dTerm;

    // Solo empuje vertical +Z validado.
    requestedPower = constrain(requestedPower, 0.0f, cfg.maxPower);

    const float maxDelta = cfg.slewPerSec * dt;
    float power = constrain(requestedPower,
                            ctrl.lastPower - maxDelta,
                            ctrl.lastPower + maxDelta);
    power = constrain(power, 0.0f, cfg.maxPower);
    ctrl.lastPower = power;

    return power;
}

} // namespace

namespace TestRunners {

void p04Altitude(AppContext& ctx) {
    const float heightRef = ctx.command.params[AppConfig::PARAM_FZ];
    const float heightNow = ctx.sensors[SensorMap::ALTITUDE];
    const float verticalVelocity = ctx.sensors[SensorMap::VERTICAL_VELOCITY];
    const ControllerConfig cfg = getConfig(ctx);

    if (!ctrl.initialized || ctrl.lastModeEnteredMs != ctx.modeEnteredMs) {
        resetController(heightRef, verticalVelocity);
        ctrl.lastModeEnteredMs = ctx.modeEnteredMs;
    }

    const float motorPower = calculateAltitudePower(heightRef,
                                                    heightNow,
                                                    verticalVelocity,
                                                    cfg);

    const int servo1Us = degreesToPulseUs(SERVO1_Z_DEG);
    const int servo2Us = degreesToPulseUs(SERVO2_Z_DEG);

    if (ctx.robot->actuatorsAreArmed()) {
        ctx.robot->commandMotorPowerTest(motorPower, motorPower,
                                         servo1Us, servo2Us);
    }

    ctx.robot->servo_old1 = SERVO1_Z_DEG;
    ctx.robot->servo_old2 = SERVO2_Z_DEG;
    ctx.robot->motor_power1 = motorPower;
    ctx.robot->motor_power2 = motorPower;

    const float heightError = (isfinite(heightRef) && isfinite(heightNow))
        ? (heightRef - heightNow)
        : 0.0f;
    Telemetry::sendControl(ctx, 0.0f, heightRef, heightError);
}

} // namespace TestRunners