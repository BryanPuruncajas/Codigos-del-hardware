#include "tests/TestRunners.h"
#include "app/AppConfig.h"
#include "app/ControlCommon.h"
#include "app/SensorMap.h"
#include "app/Telemetry.h"
#include <Arduino.h>
#include <math.h>
#include <stdint.h>

// ============================================================================
// P05 v6  -  CONTROL SIMULTANEO Z + YAW
//
// QUE CAMBIO RESPECTO A v5
// ------------------------
// La arquitectura de v5 era correcta y se conserva entera:
//
//     ALTITUDE PID ---> zDemand  (empuje vertical deseado)
//     YAW PID      ---> yawCmd   (mando normalizado -1..+1)
//                          |
//                          v
//                     +---------+
//                     |  MIXER  |  1) inclinacion segun el mando de yaw
//                     +---------+  2) eficiencias reales etaZ, etaYaw
//                          |       3) power = max(pAlt, pYaw)
//                          v
//                    motores + servos
//
// Lo que cambia en v6 son cuatro cosas concretas:
//
//  1) TODA LA MATEMATICA VIVE EN ControlCommon.h, compartida con P03 y P04.
//     Antes cada test tenia su propia copia con unidades distintas y las
//     ganancias no transferian entre pruebas.
//
//  2) LA ZONA MUERTA DEL YAW ES SOBRE EL ERROR, EN GRADOS, Y CONFIGURABLE.
//     v5 activaba el yaw con `|yawCmd| > 0.02`. Como yawCmd = kp * error, el
//     deadband REAL era 0.02/kp radianes: con kp = 0.08 el yaw no hacia
//     absolutamente nada hasta 14.3 grados de error, en silencio, y coincidia
//     casi exactamente con la banda de exito, asi que el yaw "lograba" el
//     objetivo justo donde dejaba de corregir. Ahora pides los grados que
//     quieres y son esos.
//
//  3) LA POTENCIA DE YAW SE INTERPOLA EN TODO EL RANGO [min, max]:
//         power = min + |yawCmd| * (max - min)
//     v5 hacia clamp(|yawCmd| * max, min, max). Con min y max cercanos (7% y
//     8.5%, por ejemplo) la potencia quedaba pegada al piso en todo el rango
//     util y el yaw se comportaba como un rele on/off.
//
//  4) ki DE YAW ES CONFIGURABLE desde la estacion de tierra. En v5 estaba
//     clavado en 0.35 dentro de getConfig() e ignoraba lo que mandaras, asi
//     que tocarlo exigia recompilar y bajar el blimp.
//
//
// LIMITACION FISICA QUE NO SE PUEDE ELIMINAR EN SOFTWARE
// ------------------------------------------------------
// Mientras el vehiculo hace yaw, el empuje vectorizado conserva componente
// vertical positiva (etaZ > 0 siempre): girar SIEMPRE empuja hacia arriba un
// poco. El lazo de altura solo puede compensarlo bajando su demanda hasta 0.
// Si el yaw exige mas empuje del que la altura quiere, el blimp subira algo
// durante el giro. Se minimiza con autoridad alta y con el maxPower de yaw lo
// mas bajo posible que aun gire.
//
// Esto era cierto con el P0025 (asimetria 35/85 de montaje: el giro no era
// una cupla pura, habia fuerza lateral neta y el blimp derivaba). Con el
// Tower Pro la geometria por ahora es simetrica (SERVO1_Z_DEG=SERVO2_Z_DEG=90
// en ControlCommon.h) -- falta confirmar en vuelo si sigue habiendo asimetria
// de montaje o no.
//
//
// SLOTS DE ControlInput  (los 9 huecos utiles estan ocupados)
// -----------------------------------------------------------
//   FZ    : referencia de altura, metros
//   TZ    : referencia de yaw, radianes
//   FX    : alt Kp
//   AUX0  : alt Ki
//   TX    : alt Kd
//   AUX1  : ALT_PACK (min, max, slew, banda de exito)
//   AUX2  : yaw Kp
//   AUX3  : yaw Kd
//   AUX4  : YAW_PACK (min, max, deadband, autoridad, ki)
// ============================================================================

namespace {

using namespace ControlCommon;

constexpr uint32_t ALT_STABLE_REQUIRED_MS = 2000U;
constexpr uint32_t YAW_LOCK_REQUIRED_MS   = 500U;

constexpr float YAW_REACQUIRE_MARGIN_DEG = 3.0f;

constexpr uint32_t DEBUG_PERIOD_MS = 500U;


struct Config {
    AltGains  altGains;
    AltLimits altLimits;
    YawGains  yawGains;
    YawLimits yawLimits;
    PackStatus altPack;
    PackStatus yawPack;
};


// Las fases son SOLO REPORTE. Nunca apagan, limitan ni escalan un lazo.
// En v4 la fase ALTITUDE_ACQUIRE reducia la autoridad de yaw al 35%, con lo
// que el yaw estaba efectivamente desactivado mientras el blimp buscaba
// altura, y no salia de esa fase hasta 2 s continuos dentro de la ventana.
enum class Phase : uint8_t {
    ALTITUDE_ACQUIRE = 0,
    YAW_ACQUIRE      = 1,
    HOLD             = 2,
};


struct State {
    bool initialized = false;

    unsigned long modeEnteredMs = 0U;
    unsigned long lastUs = 0U;

    AltState alt;
    YawState yaw;

    float lastServo1 = SERVO1_Z_DEG;
    float lastServo2 = SERVO2_Z_DEG;

    Phase phase = Phase::ALTITUDE_ACQUIRE;

    bool     altStableTiming = false;
    uint32_t altStableSinceMs = 0U;

    bool     yawLockTiming = false;
    uint32_t yawLockSinceMs = 0U;

    uint32_t lastDebugMs = 0U;
};

State ctrl;


const char* phaseName(Phase p) {
    switch (p) {
        case Phase::ALTITUDE_ACQUIRE: return "ALT_ACQ";
        case Phase::YAW_ACQUIRE:      return "YAW_ACQ";
        case Phase::HOLD:             return "HOLD";
        default:                      return "?";
    }
}


Config getConfig(const AppContext& ctx) {

    Config cfg;

    // ---------------------------- ALTURA ----------------------------

    cfg.altGains.kp = pickPositive(
        ctx.command.params[AppConfig::PARAM_FX], DEF_ALT_KP, 0.0f, 5.0f);

    cfg.altGains.ki = pickNonNegative(
        ctx.command.params[AppConfig::PARAM_AUX0], DEF_ALT_KI, 0.0f, 1.0f);

    cfg.altGains.kd = pickNonNegative(
        ctx.command.params[AppConfig::PARAM_TX], DEF_ALT_KD, 0.0f, 1.0f);

    cfg.altPack = decodeAltPack(
        ctx.command.params[AppConfig::PARAM_AUX1], cfg.altLimits);

    // ----------------------------- YAW ------------------------------

    cfg.yawGains.kp = pickPositive(
        ctx.command.params[AppConfig::PARAM_AUX2], DEF_YAW_KP, 0.0f, 20.0f);

    cfg.yawGains.kd = pickNonNegative(
        ctx.command.params[AppConfig::PARAM_AUX3], DEF_YAW_KD, 0.0f, 10.0f);

    cfg.yawPack = decodeYawPack(
        ctx.command.params[AppConfig::PARAM_AUX4], cfg.yawLimits);

    cfg.yawGains.ki = cfg.yawLimits.ki;

    return cfg;
}


void dumpConfig(const Config& cfg) {

    Serial.printf(
        "[P05] cfg ALT kp=%.3f ki=%.4f kd=%.3f min=%.3f max=%.3f "
        "slew=%.3f band=%.0fcm  pack=%s\n",
        cfg.altGains.kp, cfg.altGains.ki, cfg.altGains.kd,
        cfg.altLimits.minPower, cfg.altLimits.maxPower,
        cfg.altLimits.slew, cfg.altLimits.successM * 100.0f,
        packStatusName(cfg.altPack));

    Serial.printf(
        "[P05] cfg YAW kp=%.3f ki=%.3f kd=%.3f min=%.4f max=%.4f "
        "db=%.1fdeg auth=%.3f  pack=%s\n",
        cfg.yawGains.kp, cfg.yawGains.ki, cfg.yawGains.kd,
        cfg.yawLimits.minPower, cfg.yawLimits.maxPower,
        cfg.yawLimits.deadbandRad * RAD2DEG,
        cfg.yawLimits.authority,
        packStatusName(cfg.yawPack));

    if (cfg.altPack == PackStatus::LEGACY_V1 || cfg.yawPack == PackStatus::LEGACY_V1) {
        Serial.println("[P05] AVISO: run_test.py desactualizado. "
                       "Actualiza la estacion de tierra o volaras con defaults.");
    }
}


int pulse(float deg) {
    return degreesToPulseUs(deg,
                            AppConfig::SERVO_ANGLE_MIN_DEG, AppConfig::SERVO_ANGLE_MAX_DEG,
                            AppConfig::SERVO_PULSE_MIN_US, AppConfig::SERVO_PULSE_MAX_US);
}


void resetController(const Config& cfg, float yaw, float vz) {

    ctrl = State{};

    ctrl.initialized = true;
    ctrl.lastUs      = micros();
    ctrl.lastServo1  = SERVO1_Z_DEG;
    ctrl.lastServo2  = SERVO2_Z_DEG;
    ctrl.phase       = Phase::ALTITUDE_ACQUIRE;
    ctrl.lastDebugMs = millis();

    resetAltState(ctrl.alt, vz);
    resetYawState(ctrl.yaw, yaw);

    Serial.println("[P05] reset -> ALT_ACQ");
    dumpConfig(cfg);
}


float getDt() {

    const unsigned long now = micros();
    float dt = (now - ctrl.lastUs) * 1.0e-6f;
    ctrl.lastUs = now;

    if (!isfinite(dt) || dt <= 0.0f || dt > 0.20f) {
        dt = 0.01f;
    }

    return dt;
}


void enterPhase(Phase next) {

    if (ctrl.phase == next) return;

    ctrl.phase = next;
    ctrl.altStableTiming = false;
    ctrl.yawLockTiming = false;

    Serial.printf("[P05] phase -> %s\n", phaseName(next));
}


void updateAltStable(const Config& cfg, float heightError, bool valid, uint32_t nowMs) {

    const bool stable = valid && (fabsf(heightError) <= cfg.altLimits.successM);

    if (!stable) {
        ctrl.altStableTiming = false;
        return;
    }

    if (!ctrl.altStableTiming) {
        ctrl.altStableTiming = true;
        ctrl.altStableSinceMs = nowMs;
        return;
    }

    if (elapsedMs(nowMs, ctrl.altStableSinceMs, ALT_STABLE_REQUIRED_MS)) {
        enterPhase(Phase::YAW_ACQUIRE);
    }
}


void updateYawLock(const Config& cfg, float absYawError, bool valid, uint32_t nowMs) {

    // La banda de exito del yaw es la zona muerta: es donde el control deja
    // de actuar, asi que es el criterio honesto de "llegue".
    if (!valid || absYawError > cfg.yawLimits.deadbandRad) {
        ctrl.yawLockTiming = false;
        return;
    }

    if (!ctrl.yawLockTiming) {
        ctrl.yawLockTiming = true;
        ctrl.yawLockSinceMs = nowMs;
        return;
    }

    if (elapsedMs(nowMs, ctrl.yawLockSinceMs, YAW_LOCK_REQUIRED_MS)) {
        enterPhase(Phase::HOLD);
    }
}

} // namespace


namespace TestRunners {

void p05YawAltitude(AppContext& ctx) {

    const float yawRef    = ctx.command.params[AppConfig::PARAM_TZ];
    const float heightRef = ctx.command.params[AppConfig::PARAM_FZ];

    const float yaw    = ctx.sensors[SensorMap::YAW];
    const float height = ctx.sensors[SensorMap::ALTITUDE];
    const float rawVz  = ctx.sensors[SensorMap::VERTICAL_VELOCITY];

    const Config cfg = getConfig(ctx);

    if (!ctrl.initialized || ctrl.modeEnteredMs != ctx.modeEnteredMs) {
        resetController(cfg, yaw, rawVz);
        ctrl.modeEnteredMs = ctx.modeEnteredMs;
    }

    const float dt = getDt();
    const uint32_t nowMs = millis();

    const float measuredRate = ctx.sensors[SensorMap::YAW_RATE];
    const float yawRate    = updateYawRate(ctrl.yaw, yaw, measuredRate, dt);
    const float filteredVz = updateVz(ctrl.alt, rawVz);

    const bool yawValid    = isfinite(yawRef) && isfinite(yaw);
    const bool heightValid = isfinite(heightRef) && isfinite(height) && isfinite(rawVz);

    const float yawError    = yawValid ? wrapPi(yawRef - yaw) : 0.0f;
    const float absYawError = fabsf(yawError);
    const float heightError = heightValid ? (heightRef - height) : 0.0f;

    // ------------------------------------------------------------------
    // LAZO DE ALTURA  -  SIEMPRE ACTIVO
    //
    // Truco util para caracterizar: con una referencia muy negativa
    // (--height -2) el error queda fuera de la zona integral, el integral
    // se congela en 0 y zDemand se satura en 0. P05 se convierte en un
    // ensayo de YAW PURO con la arquitectura real que vas a volar.
    // ------------------------------------------------------------------

    float zDemand = 0.0f;

    if (heightValid) {
        zDemand = computeAltDemand(cfg.altGains, cfg.altLimits, ctrl.alt, heightError, dt);
    }

    // ------------------------------------------------------------------
    // LAZO DE YAW  -  SIEMPRE ACTIVO, SIEMPRE CON AUTORIDAD COMPLETA
    //
    // Truco simetrico: poniendo --yaw-deg igual a tu rumbo actual el error
    // cae dentro de la zona muerta, el yaw no actua y P05 se convierte en
    // un ensayo de ALTURA PURA.
    // ------------------------------------------------------------------

    float yawCmd = 0.0f;

    if (yawValid) {
        yawCmd = computeYawCommand(cfg.yawGains, cfg.yawLimits, ctrl.yaw,
                                   yawError, yawRate, dt);
    }

    // ------------------------------------------------------------------
    // MAQUINA DE ESTADOS  (solo reporte)
    // ------------------------------------------------------------------

    if (ctrl.phase == Phase::ALTITUDE_ACQUIRE) {
        updateAltStable(cfg, heightError, heightValid, nowMs);
    }
    else if (ctrl.phase == Phase::YAW_ACQUIRE) {
        updateYawLock(cfg, absYawError, yawValid, nowMs);
    }
    else if (ctrl.phase == Phase::HOLD) {
        const float reacquireRad =
            cfg.yawLimits.deadbandRad + YAW_REACQUIRE_MARGIN_DEG * DEG2RAD;

        if (yawValid && absYawError > reacquireRad) {
            enterPhase(Phase::YAW_ACQUIRE);
        }
    }

    // ------------------------------------------------------------------
    // MIXER
    // ------------------------------------------------------------------

    float servo1Deg = SERVO1_Z_DEG;
    float servo2Deg = SERVO2_Z_DEG;
    float power     = 0.0f;
    float etaZ      = 1.0f;
    float etaYaw    = 0.0f;
    float blend     = 0.0f;

    mixOutputs(zDemand, yawCmd, cfg.yawLimits, cfg.altLimits.maxPower,
               servo1Deg, servo2Deg, power, etaZ, etaYaw, blend);

    // ------------------------------------------------------------------
    // LIMITES DE VELOCIDAD DE ACTUADORES
    //
    // El slew se aplica a la salida FINAL, no dentro del PID. Asi tambien se
    // protege al ESC del escalon que introduce el piso de potencia de yaw, y
    // el integrador de altura no se carga contra el slew.
    // ------------------------------------------------------------------

    power = rateLimit(power, ctrl.alt.lastPower, cfg.altLimits.slew, dt);
    power = constrain(power, 0.0f, cfg.altLimits.maxPower);

    servo1Deg = rateLimit(servo1Deg, ctrl.lastServo1, SERVO_SLEW_DEG_PER_S, dt);
    servo2Deg = rateLimit(servo2Deg, ctrl.lastServo2, SERVO_SLEW_DEG_PER_S, dt);

    servo1Deg = constrain(servo1Deg, AppConfig::SERVO_ANGLE_MIN_DEG, AppConfig::SERVO_ANGLE_MAX_DEG);
    servo2Deg = constrain(servo2Deg, AppConfig::SERVO_ANGLE_MIN_DEG, AppConfig::SERVO_ANGLE_MAX_DEG);

    ctrl.lastServo1 = servo1Deg;
    ctrl.lastServo2 = servo2Deg;

    // ------------------------------------------------------------------
    // ACTUADORES  -  UNICO BLOQUE QUE ESCRIBE
    // ------------------------------------------------------------------

    if (ctx.robot->actuatorsAreArmed()) {
        ctrl.alt.lastPower = power;
        ctx.robot->commandMotorPowerTest(power, power,
                                         pulse(servo1Deg), pulse(servo2Deg));
    } else {
        // Sin armar: mantener el estado interno coherente para que al armar
        // no haya un salto de potencia ni de servo.
        ctrl.alt.lastPower = 0.0f;
    }

    ctx.robot->servo_old1   = servo1Deg;
    ctx.robot->servo_old2   = servo2Deg;
    ctx.robot->motor_power1 = power;
    ctx.robot->motor_power2 = power;

    // ------------------------------------------------------------------
    // DEBUG
    //
    // `altI` estabilizado = potencia de hover.
    // `Fz` = power * etaZ = empuje vertical realmente entregado.
    // ------------------------------------------------------------------

    if (elapsedMs(nowMs, ctrl.lastDebugMs, DEBUG_PERIOD_MS)) {

        ctrl.lastDebugMs = nowMs;

        Serial.printf(
            "[P05] %s "
            "zErr=%.3f vz=%.3f altI=%.3f zDem=%.3f | "
            "yawErr=%.1f yawRate=%.2f yawCmd=%.3f yawI=%.3f | "
            "blend=%.2f etaZ=%.2f etaY=%.2f | "
            "P=%.3f Fz=%.3f S1=%.1f S2=%.1f\n",
            phaseName(ctrl.phase),
            heightError,
            filteredVz,
            ctrl.alt.integral,
            zDemand,
            yawError * RAD2DEG,
            yawRate,
            yawCmd,
            ctrl.yaw.integral,
            blend,
            etaZ,
            etaYaw,
            power,
            power * etaZ,
            servo1Deg,
            servo2Deg);
    }

    Telemetry::sendControl(ctx, yawRef, heightRef, heightError);
}

} // namespace TestRunners