#include "tests/TestRunners.h"
#include "app/AppConfig.h"
#include "app/SensorMap.h"
#include "app/Telemetry.h"
#include <Arduino.h>
#include <math.h>
#include <stdint.h>

namespace {

// ============================================================================
// P05 v3 - ALTITUDE ACQUIRE -> YAW ACQUIRE -> HOLD
//
// Objetivo de esta prueba:
//   1) adquirir y estabilizar Z primero;
//   2) sin apagar el control de Z, adquirir yaw;
//   3) mantener Z y re-adquirir yaw solo si realmente se aleja.
//
// IMPORTANTE:
// - Un solo bloque (mixer) escribe servos/motores.
// - Altitud y yaw calculan demandas; NO escriben actuadores por separado.
// - No se controla roll en P05. Roll se deja para una prueba dedicada.
// ============================================================================

// Geometria fisica ya validada en P00/P02.
constexpr float SERVO1_Z_DEG     = 35.0f;
constexpr float SERVO2_Z_DEG     = 85.0f;
constexpr float SERVO1_RIGHT_DEG = 120.0f;
constexpr float SERVO2_RIGHT_DEG = 120.0f;
constexpr float SERVO1_LEFT_DEG  = 0.0f;
constexpr float SERVO2_LEFT_DEG  = 0.0f;

// Defaults seguros. run_test.py puede reemplazar los parametros de control.
constexpr float DEFAULT_ALT_KP = 0.30f;
constexpr float DEFAULT_ALT_KI = 0.025f;
constexpr float DEFAULT_ALT_KD = 0.01f;
constexpr float DEFAULT_ALT_MIN = 0.00f;
constexpr float DEFAULT_ALT_MAX = 0.15f;
constexpr float DEFAULT_ALT_SLEW = 0.18f;
constexpr float DEFAULT_ALT_SUCCESS_M = 0.10f;   // +/-10 cm

constexpr float DEFAULT_YAW_KP = 0.10f;
constexpr float DEFAULT_YAW_KD = 0.03f;
constexpr float DEFAULT_YAW_MIN = 0.07f;
constexpr float DEFAULT_YAW_MAX = 0.085f;
constexpr float DEFAULT_YAW_SUCCESS_DEG = 10.0f;
constexpr float DEFAULT_YAW_SERVO_AUTHORITY = 0.30f; // 30% hacia vector yaw extremo

// Condiciones de transicion.
constexpr uint32_t ALT_STABLE_REQUIRED_MS = 2000U;
constexpr float ALT_STABLE_VZ_MAX = 0.10f; // m/s
constexpr uint32_t YAW_LOCK_REQUIRED_MS = 500U;
constexpr float YAW_REACQUIRE_MARGIN_DEG = 3.0f;

// Control.
constexpr float ALT_DEADBAND = 0.025f;
constexpr float ALT_INTEGRAL_ZONE_M = 0.35f;
constexpr float VZ_FILTER_ALPHA = 0.82f;
constexpr float YAW_RATE_FILTER_ALPHA = 0.80f;
constexpr uint32_t DEBUG_PERIOD_MS = 500U;

// Float IEEE754 representa exactamente enteros hasta 2^24.
constexpr uint32_t PACK_MARKER = (1UL << 23);

struct Config {
    float akp;
    float aki;
    float akd;
    float amin;
    float amax;
    float aslew;
    float altSuccessM;

    float ykp;
    float ykd;
    float ymin;
    float ymax;
    float yawSuccessRad;
    float yawServoAuthority;
};

enum class Phase : uint8_t {
    ALTITUDE_ACQUIRE = 0,
    YAW_ACQUIRE = 1,
    HOLD = 2,
};

struct State {
    bool initialized = false;
    unsigned long modeEnteredMs = 0U;
    unsigned long lastUs = 0U;

    float filteredVz = 0.0f;
    float altIntegral = 0.0f;
    float altLastPower = 0.0f;

    float lastYaw = 0.0f;
    float filteredYawVel = 0.0f;

    Phase phase = Phase::ALTITUDE_ACQUIRE;

    bool altitudeStableTiming = false;
    uint32_t altitudeStableSinceMs = 0U;

    bool yawLockTiming = false;
    uint32_t yawLockSinceMs = 0U;

    uint32_t lastDebugMs = 0U;
} ctrl;

float wrapPi(float a) {
    while (a > PI)  a -= 2.0f * PI;
    while (a < -PI) a += 2.0f * PI;
    return a;
}

float lerpFloat(float a, float b, float t) {
    t = constrain(t, 0.0f, 1.0f);
    return a + (b - a) * t;
}

bool elapsedMs(uint32_t now, uint32_t since, uint32_t duration) {
    return (uint32_t)(now - since) >= duration;
}

const char* phaseName(Phase p) {
    switch (p) {
        case Phase::ALTITUDE_ACQUIRE: return "ALTITUDE_ACQUIRE";
        case Phase::YAW_ACQUIRE:      return "YAW_ACQUIRE";
        case Phase::HOLD:             return "HOLD";
        default:                      return "?";
    }
}

void enterPhase(Phase next, uint32_t nowMs) {
    if (ctrl.phase == next) return;

    ctrl.phase = next;
    ctrl.altitudeStableTiming = false;
    ctrl.yawLockTiming = false;

    if (next == Phase::YAW_ACQUIRE) {
        // Conservamos integral de altura: ya aprendio parte del empuje de sostener Z.
        ctrl.filteredYawVel = 0.0f;
    }

    Serial.printf("[P05] phase -> %s\n", phaseName(next));
    (void)nowMs;
}

// AUX1 / paquete ALTURA:
// bits  0.. 6 : alt min power, % entero 0..100
// bits  7..13 : alt max power, % entero 0..100
// bits 14..17 : slew / 0.02 (1..15 -> 0.02..0.30 /s)
// bits 18..22 : banda de exito de altura, cm 1..31
// bit      23 : marker
void decodeAltitudePack(float raw,
                        float& amin,
                        float& amax,
                        float& aslew,
                        float& successM) {
    amin = DEFAULT_ALT_MIN;
    amax = DEFAULT_ALT_MAX;
    aslew = DEFAULT_ALT_SLEW;
    successM = DEFAULT_ALT_SUCCESS_M;

    if (!isfinite(raw) || raw <= 1.0f) return;

    const uint32_t packed = (uint32_t)lroundf(raw);
    if ((packed & PACK_MARKER) == 0U) return;

    const uint32_t payload = packed & (~PACK_MARKER);
    const uint32_t minPct = payload & 0x7FU;
    const uint32_t maxPct = (payload >> 7) & 0x7FU;
    const uint32_t slewUnits = (payload >> 14) & 0x0FU;
    const uint32_t successCm = (payload >> 18) & 0x1FU;

    amin = constrain((float)minPct / 100.0f, 0.0f, 1.0f);
    amax = constrain((float)maxPct / 100.0f, 0.001f, 1.0f);
    aslew = constrain((float)max((uint32_t)1U, slewUnits) * 0.02f,
                      0.02f,
                      0.30f);
    successM = constrain((float)max((uint32_t)1U, successCm) / 100.0f,
                         0.01f,
                         0.31f);
}

// AUX4 / paquete YAW:
// bits  0.. 6 : yaw min power en decimas de % (70 = 7.0%)
// bits  7..13 : yaw max power en decimas de % (85 = 8.5%)
// bits 14..18 : banda de exito yaw en pasos de 0.5 deg
// bits 19..22 : autoridad servo en pasos de 10% (0..100%)
// bit      23 : marker
void decodeYawPack(float raw,
                   float& ymin,
                   float& ymax,
                   float& successRad,
                   float& servoAuthority) {
    ymin = DEFAULT_YAW_MIN;
    ymax = DEFAULT_YAW_MAX;
    successRad = DEFAULT_YAW_SUCCESS_DEG * PI / 180.0f;
    servoAuthority = DEFAULT_YAW_SERVO_AUTHORITY;

    if (!isfinite(raw) || raw <= 1.0f) return;

    const uint32_t packed = (uint32_t)lroundf(raw);
    if ((packed & PACK_MARKER) == 0U) return;

    const uint32_t payload = packed & (~PACK_MARKER);
    const uint32_t minTenthPct = payload & 0x7FU;
    const uint32_t maxTenthPct = (payload >> 7) & 0x7FU;
    const uint32_t successHalfDeg = (payload >> 14) & 0x1FU;
    const uint32_t authority10Pct = (payload >> 19) & 0x0FU;

    ymin = constrain((float)minTenthPct / 1000.0f, 0.0f, 0.127f);
    ymax = constrain((float)maxTenthPct / 1000.0f, 0.001f, 0.127f);
    successRad = constrain((float)max((uint32_t)1U, successHalfDeg) * 0.5f,
                           0.5f,
                           15.5f) * PI / 180.0f;
    servoAuthority = constrain((float)authority10Pct / 10.0f, 0.0f, 1.0f);
}

Config getConfig(const AppContext& ctx) {
    const float akp = ctx.command.params[AppConfig::PARAM_FX];
    const float aki = ctx.command.params[AppConfig::PARAM_AUX0];
    const float akd = ctx.command.params[AppConfig::PARAM_TX];
    const float ykp = ctx.command.params[AppConfig::PARAM_AUX2];
    const float ykd = ctx.command.params[AppConfig::PARAM_AUX3];

    float amin, amax, aslew, altSuccessM;
    decodeAltitudePack(ctx.command.params[AppConfig::PARAM_AUX1],
                       amin, amax, aslew, altSuccessM);

    float ymin, ymax, yawSuccessRad, yawServoAuthority;
    decodeYawPack(ctx.command.params[AppConfig::PARAM_AUX4],
                  ymin, ymax, yawSuccessRad, yawServoAuthority);

    const bool valid =
        isfinite(akp) && akp >= 0.0f &&
        isfinite(aki) && aki >= 0.0f &&
        isfinite(akd) && akd >= 0.0f &&
        isfinite(amin) && amin >= 0.0f &&
        isfinite(amax) && amax > 0.0f && amin <= amax &&
        isfinite(aslew) && aslew > 0.0f &&
        isfinite(altSuccessM) && altSuccessM > 0.0f &&
        isfinite(ykp) && ykp >= 0.0f &&
        isfinite(ykd) && ykd >= 0.0f &&
        isfinite(ymin) && ymin >= 0.0f &&
        isfinite(ymax) && ymax > 0.0f && ymin <= ymax &&
        isfinite(yawSuccessRad) && yawSuccessRad > 0.0f &&
        isfinite(yawServoAuthority) && yawServoAuthority >= 0.0f &&
        yawServoAuthority <= 1.0f;

    if (valid) {
        return {
            akp, aki, akd, amin, amax, aslew, altSuccessM,
            ykp, ykd, ymin, ymax, yawSuccessRad, yawServoAuthority
        };
    }

    return {
        DEFAULT_ALT_KP, DEFAULT_ALT_KI, DEFAULT_ALT_KD,
        DEFAULT_ALT_MIN, DEFAULT_ALT_MAX, DEFAULT_ALT_SLEW,
        DEFAULT_ALT_SUCCESS_M,
        DEFAULT_YAW_KP, DEFAULT_YAW_KD,
        DEFAULT_YAW_MIN, DEFAULT_YAW_MAX,
        DEFAULT_YAW_SUCCESS_DEG * PI / 180.0f,
        DEFAULT_YAW_SERVO_AUTHORITY
    };
}

int pulse(float deg) {
    const float c = constrain(deg,
                              AppConfig::P0025_MIN_DEG,
                              AppConfig::P0025_MAX_DEG);

    return (int)lroundf(
        AppConfig::P0025_MIN_US +
        (c - AppConfig::P0025_MIN_DEG) *
        (AppConfig::P0025_MAX_US - AppConfig::P0025_MIN_US) /
        (AppConfig::P0025_MAX_DEG - AppConfig::P0025_MIN_DEG)
    );
}

void resetController(float yaw, float vz) {
    ctrl = State{};
    ctrl.initialized = true;
    ctrl.lastUs = micros();
    ctrl.filteredVz = isfinite(vz) ? vz : 0.0f;
    ctrl.lastYaw = isfinite(yaw) ? yaw : 0.0f;
    ctrl.phase = Phase::ALTITUDE_ACQUIRE;
    ctrl.lastDebugMs = millis();

    Serial.println("[P05] phase -> ALTITUDE_ACQUIRE");
}

float getDt() {
    const unsigned long now = micros();
    float dt = (now - ctrl.lastUs) * 1e-6f;
    ctrl.lastUs = now;

    if (!isfinite(dt) || dt <= 0.0f || dt > 0.20f) dt = 0.01f;
    return dt;
}

float computeYawPower(const Config& cfg,
                      float yawError,
                      float yawVelocity) {
    const float dir = yawError > 0.0f ? 1.0f : -1.0f;
    const float towardTargetRate = yawVelocity * dir;

    float effort = cfg.ykp * fabsf(yawError) -
                   cfg.ykd * towardTargetRate;

    effort = constrain(effort, 0.0f, cfg.ymax);
    if (effort <= 0.0f) return 0.0f;

    return constrain(max(cfg.ymin, effort), 0.0f, cfg.ymax);
}

float computeAltitudePower(const Config& cfg,
                           float heightError,
                           float filteredVz,
                           float dt) {
    const float pError =
        fabsf(heightError) < ALT_DEADBAND ? 0.0f : heightError;

    // Integral aprende el empuje de sostenimiento. Al estar por encima del SP,
    // el error negativo tambien descarga la integral.
    if (fabsf(heightError) < ALT_INTEGRAL_ZONE_M) {
        ctrl.altIntegral += cfg.aki * heightError * dt;
        ctrl.altIntegral = constrain(ctrl.altIntegral, 0.0f, cfg.amax);
    }

    float requested = cfg.akp * pError +
                      ctrl.altIntegral -
                      cfg.akd * filteredVz;

    // Solo se ha validado empuje activo en +Z; descenso es pasivo.
    requested = constrain(requested, 0.0f, cfg.amax);

    // Piso de potencia configurable: si el PID pide empuje positivo, no manda
    // una potencia tan baja que fisicamente no produzca efecto.
    if (requested > 0.0f && requested < cfg.amin) {
        requested = cfg.amin;
    }

    const float maxDelta = cfg.aslew * dt;
    float power = constrain(requested,
                            ctrl.altLastPower - maxDelta,
                            ctrl.altLastPower + maxDelta);

    power = constrain(power, 0.0f, cfg.amax);
    ctrl.altLastPower = power;
    return power;
}

void updateAltitudeStableState(const Config& cfg,
                               float heightError,
                               float filteredVz,
                               bool valid,
                               uint32_t nowMs) {
    const bool stable = valid &&
                        fabsf(heightError) <= cfg.altSuccessM &&
                        fabsf(filteredVz) <= ALT_STABLE_VZ_MAX;

    if (!stable) {
        ctrl.altitudeStableTiming = false;
        return;
    }

    if (!ctrl.altitudeStableTiming) {
        ctrl.altitudeStableTiming = true;
        ctrl.altitudeStableSinceMs = nowMs;
        return;
    }

    if (elapsedMs(nowMs,
                  ctrl.altitudeStableSinceMs,
                  ALT_STABLE_REQUIRED_MS)) {
        enterPhase(Phase::YAW_ACQUIRE, nowMs);
    }
}

void updateYawLockState(const Config& cfg,
                        float absYawError,
                        bool valid,
                        uint32_t nowMs) {
    if (!valid || absYawError > cfg.yawSuccessRad) {
        ctrl.yawLockTiming = false;
        return;
    }

    if (!ctrl.yawLockTiming) {
        ctrl.yawLockTiming = true;
        ctrl.yawLockSinceMs = nowMs;
        return;
    }

    if (elapsedMs(nowMs,
                  ctrl.yawLockSinceMs,
                  YAW_LOCK_REQUIRED_MS)) {
        enterPhase(Phase::HOLD, nowMs);
    }
}

} // namespace

namespace TestRunners {

void p05YawAltitude(AppContext& ctx) {
    const float yawRef = ctx.command.params[AppConfig::PARAM_TZ];
    const float heightRef = ctx.command.params[AppConfig::PARAM_FZ];

    const float yaw = ctx.sensors[SensorMap::YAW];
    const float height = ctx.sensors[SensorMap::ALTITUDE];
    const float vz = ctx.sensors[SensorMap::VERTICAL_VELOCITY];

    const Config cfg = getConfig(ctx);

    if (!ctrl.initialized || ctrl.modeEnteredMs != ctx.modeEnteredMs) {
        resetController(yaw, vz);
        ctrl.modeEnteredMs = ctx.modeEnteredMs;
    }

    const float dt = getDt();
    const uint32_t nowMs = millis();

    // --------------------------- Yaw rate filtrado ---------------------------
    float yawVel = 0.0f;
    if (isfinite(yaw)) {
        const float rawYawVel = wrapPi(yaw - ctrl.lastYaw) / dt;
        ctrl.lastYaw = yaw;

        ctrl.filteredYawVel =
            YAW_RATE_FILTER_ALPHA * ctrl.filteredYawVel +
            (1.0f - YAW_RATE_FILTER_ALPHA) * rawYawVel;

        yawVel = ctrl.filteredYawVel;
    }

    // ---------------------------- Vz filtrada -------------------------------
    const float safeVz = isfinite(vz) ? vz : 0.0f;
    ctrl.filteredVz =
        VZ_FILTER_ALPHA * ctrl.filteredVz +
        (1.0f - VZ_FILTER_ALPHA) * safeVz;

    const bool yawValid = isfinite(yawRef) && isfinite(yaw);
    const bool heightValid =
        isfinite(heightRef) && isfinite(height) && isfinite(vz);

    const float yawError = yawValid ? wrapPi(yawRef - yaw) : 0.0f;
    const float absYawError = fabsf(yawError);
    const float heightError = heightValid ? (heightRef - height) : 0.0f;

    // El control Z se calcula en TODAS las fases despues de arrancar P05.
    float altPower = 0.0f;
    if (heightValid) {
        altPower = computeAltitudePower(cfg,
                                        heightError,
                                        ctrl.filteredVz,
                                        dt);
    }

    // -------------------------- Maquina de estados --------------------------
    if (ctrl.phase == Phase::ALTITUDE_ACQUIRE) {
        // En esta fase yaw no tiene ninguna autoridad.
        updateAltitudeStableState(cfg,
                                  heightError,
                                  ctrl.filteredVz,
                                  heightValid,
                                  nowMs);
    }
    else if (ctrl.phase == Phase::YAW_ACQUIRE) {
        // Altitud sigue activa; yaw se superpone mediante el mixer.
        updateYawLockState(cfg,
                           absYawError,
                           yawValid,
                           nowMs);
    }
    else if (ctrl.phase == Phase::HOLD) {
        // Histeresis sencilla: una vez adquirido yaw, no vuelve a corregir por
        // ruido pequeno. Re-adquiere solo si sale Success + 3 grados.
        const float reacquireRad =
            cfg.yawSuccessRad + YAW_REACQUIRE_MARGIN_DEG * PI / 180.0f;

        if (yawValid && absYawError > reacquireRad) {
            enterPhase(Phase::YAW_ACQUIRE, nowMs);
        }
    }

    // ------------------------------ Mixer -----------------------------------
    float servo1Deg = SERVO1_Z_DEG;
    float servo2Deg = SERVO2_Z_DEG;
    float yawPower = 0.0f;
    float outputPower = altPower;

    const bool yawNeedsCorrection =
        ctrl.phase == Phase::YAW_ACQUIRE &&
        yawValid &&
        absYawError > cfg.yawSuccessRad;

    if (yawNeedsCorrection) {
        yawPower = computeYawPower(cfg, yawError, yawVel);

        // La demanda de yaw no reemplaza Z. La potencia comun final es al menos
        // la que necesita altura y al menos la necesaria para producir yaw.
        outputPower = max(altPower, yawPower);

        // En lugar de saltar a 0/0 o 120/120, interpolamos desde el vector Z.
        // Esto conserva buena parte de la componente vertical mientras gira.
        const float normalizedYaw =
            (cfg.ymax > 0.0f) ? constrain(yawPower / cfg.ymax, 0.0f, 1.0f) : 0.0f;
        const float blend = cfg.yawServoAuthority * normalizedYaw;

        if (yawError > 0.0f) {
            // Aumentar yaw -> izquierda / antihorario.
            servo1Deg = lerpFloat(SERVO1_Z_DEG, SERVO1_LEFT_DEG, blend);
            servo2Deg = lerpFloat(SERVO2_Z_DEG, SERVO2_LEFT_DEG, blend);
        } else {
            // Disminuir yaw -> derecha / horario.
            servo1Deg = lerpFloat(SERVO1_Z_DEG, SERVO1_RIGHT_DEG, blend);
            servo2Deg = lerpFloat(SERVO2_Z_DEG, SERVO2_RIGHT_DEG, blend);
        }
    }

    outputPower = constrain(outputPower, 0.0f, cfg.amax);

    if (ctx.robot->actuatorsAreArmed()) {
        ctx.robot->commandMotorPowerTest(outputPower,
                                         outputPower,
                                         pulse(servo1Deg),
                                         pulse(servo2Deg));
    }

    ctx.robot->servo_old1 = servo1Deg;
    ctx.robot->servo_old2 = servo2Deg;
    ctx.robot->motor_power1 = outputPower;
    ctx.robot->motor_power2 = outputPower;

    // Debug util para descubrir potencia real de sostenimiento.
    if (elapsedMs(nowMs, ctrl.lastDebugMs, DEBUG_PERIOD_MS)) {
        ctrl.lastDebugMs = nowMs;
        Serial.printf(
            "[P05] phase=%s zErr=%.3f vz=%.3f altP=%.3f "
            "yawErrDeg=%.2f yawP=%.3f out=%.3f S1=%.1f S2=%.1f\n",
            phaseName(ctrl.phase),
            heightError,
            ctrl.filteredVz,
            altPower,
            yawError * 180.0f / PI,
            yawPower,
            outputPower,
            servo1Deg,
            servo2Deg
        );
    }

    // Conservamos F4: yawRef, heightRef, error de altura.
    Telemetry::sendControl(ctx, yawRef, heightRef, heightError);
}

} // namespace TestRunners