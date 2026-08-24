#include "tests/TestRunners.h"
#include "app/AppConfig.h"
#include "app/ControlCommon.h"
#include "app/SensorMap.h"
#include "app/Telemetry.h"
#include <Arduino.h>
#include <math.h>
#include <stdint.h>
 
// ============================================================================
// P04 v2  -  ALTURA AISLADA, MISMO LAZO QUE P05
//
// QUE CAMBIO Y POR QUE
// --------------------
// El P04 anterior ya tenia la misma FORMA que el lazo de altura de P05
// (kp*error + I - kd*vz, mismo deadband, misma zona integral, mismo filtro
// de vz), pero se comportaba distinto en tres puntos que arruinaban la
// transferencia de ganancias:
//
//  1) EL TOPE DEL INTEGRAL ESTABA CLAVADO EN 6%  (ALT_I_POWER_MAX = 0.06).
//     P05 lo tiene en maxPower. Si tu potencia de hover es, por ejemplo, 9%,
//     en P04 el integral se saturaba en 6% y el blimp se quedaba colgado
//     debajo del setpoint hasta que kp*error completara la diferencia.
//     Eso da un error permanente de (hover - 0.06)/kp que NO es culpa de la
//     sintonia: es el clamp. Ahora el tope es maxPower en los dos tests.
//
//  2) NO TENIA ANTI-WINDUP. El integral cargaba incluso con la salida
//     saturada y contra el limite de slew, asi que el transitorio mentia
//     respecto de lo que hara P05.
//
//  3) NO TENIA PISO DE POTENCIA (amin). P05 si. Ahora los dos lo tienen y es
//     configurable. RECOMENDACION: dejalo en 0 mientras sintonizas, porque un
//     piso alto se traga el termino proporcional y convierte el lazo en un
//     rele de dos estados que parece estable pero no esta sintonizado.
//
// Como los servos se quedan siempre en el vector vertical (35/85), etaZ = 1 y
// la demanda de empuje es identica a la potencia de motor. Por eso este test
// es el banco limpio para sacar kp/ki/kd: sin yaw, sin mixer, sin acoplamiento.
//
//
// SLOTS DE ControlInput
// ---------------------
//   FZ    : referencia de altura en metros
//   AUX0  : alt Kp
//   AUX1  : alt Ki
//   AUX2  : alt Kd
//   AUX3  : ALT_PACK (min, max, slew, banda de exito)
//   AUX4  : reservado
// ============================================================================
 
namespace {
 
using namespace ControlCommon;
 
constexpr uint32_t DEBUG_PERIOD_MS = 500U;
 
 
struct Config {
    AltGains  gains;
    AltLimits limits;
    PackStatus packStatus;
};
 
 
struct State {
    bool initialized = false;
    unsigned long modeEnteredMs = 0U;
    unsigned long lastUs = 0U;
 
    AltState alt;
 
    float lastReference = NAN;
 
    uint32_t lastDebugMs = 0U;
 
    // Solo informativo: cuanto lleva dentro de la banda de exito.
    bool     inBand = false;
    uint32_t inBandSinceMs = 0U;
};
 
State ctrl;
 
 
Config getConfig(const AppContext& ctx) {
 
    Config cfg;
 
    cfg.gains.kp = pickPositive(
        ctx.command.params[AppConfig::PARAM_AUX0], DEF_ALT_KP, 0.0f, 5.0f);
 
    cfg.gains.ki = pickNonNegative(
        ctx.command.params[AppConfig::PARAM_AUX1], DEF_ALT_KI, 0.0f, 1.0f);
 
    cfg.gains.kd = pickNonNegative(
        ctx.command.params[AppConfig::PARAM_AUX2], DEF_ALT_KD, 0.0f, 1.0f);
 
    cfg.packStatus = decodeAltPack(
        ctx.command.params[AppConfig::PARAM_AUX3], cfg.limits);
 
    return cfg;
}
 
 
void dumpConfig(const Config& cfg) {
 
    Serial.printf(
        "[P04] cfg ALT kp=%.3f ki=%.4f kd=%.3f min=%.3f max=%.3f "
        "slew=%.3f band=%.0fcm  pack=%s\n",
        cfg.gains.kp, cfg.gains.ki, cfg.gains.kd,
        cfg.limits.minPower, cfg.limits.maxPower,
        cfg.limits.slew,
        cfg.limits.successM * 100.0f,
        packStatusName(cfg.packStatus));
 
    if (cfg.packStatus == PackStatus::LEGACY_V1) {
        Serial.println("[P04] AVISO: run_test.py desactualizado. "
                       "Actualiza la estacion de tierra o volaras con defaults.");
    }
}
 
 
int pulse(float deg) {
    return degreesToPulseUs(deg,
                            AppConfig::P0025_MIN_DEG, AppConfig::P0025_MAX_DEG,
                            AppConfig::P0025_MIN_US, AppConfig::P0025_MAX_US);
}
 
 
void resetController(const Config& cfg, float reference, float vz) {
 
    ctrl = State{};
 
    ctrl.initialized   = true;
    ctrl.lastUs        = micros();
    ctrl.lastReference = reference;
    ctrl.lastDebugMs   = millis();
 
    resetAltState(ctrl.alt, vz);
 
    Serial.println("[P04] reset");
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
 
} // namespace
 
 
namespace TestRunners {
 
void p04Altitude(AppContext& ctx) {
 
    const float heightRef = ctx.command.params[AppConfig::PARAM_FZ];
    const float height    = ctx.sensors[SensorMap::ALTITUDE];
    const float rawVz     = ctx.sensors[SensorMap::VERTICAL_VELOCITY];
 
    const Config cfg = getConfig(ctx);
 
    const bool newMode = (!ctrl.initialized || ctrl.modeEnteredMs != ctx.modeEnteredMs);
 
    // Un cambio de referencia en caliente tambien reinicia el integral: si no,
    // el escalon arranca con la carga del setpoint anterior y el transitorio
    // que midas no sirve para sintonizar.
    const bool newReference =
        ctrl.initialized &&
        isfinite(heightRef) && isfinite(ctrl.lastReference) &&
        fabsf(heightRef - ctrl.lastReference) > 0.05f;
 
    if (newMode || newReference) {
        resetController(cfg, heightRef, rawVz);
        ctrl.modeEnteredMs = ctx.modeEnteredMs;
    }
 
    ctrl.lastReference = heightRef;
 
    const float dt    = getDt();
    const uint32_t nowMs = millis();
 
    const float filteredVz = updateVz(ctrl.alt, rawVz);
 
    const bool valid = isfinite(heightRef) && isfinite(height) && isfinite(rawVz);
 
    const float heightError = valid ? (heightRef - height) : 0.0f;
 
    // ------------------------------------------------------------------
    // LAZO DE ALTURA
    //
    // Servos siempre en el vector vertical, asi que etaZ = 1 y la demanda
    // de empuje ES la potencia de motor. No hay mixer que enmascare nada.
    // ------------------------------------------------------------------
 
    float demand = 0.0f;
 
    if (valid) {
        demand = computeAltDemand(cfg.gains, cfg.limits, ctrl.alt, heightError, dt);
    }
 
    float power = rateLimit(demand, ctrl.alt.lastPower, cfg.limits.slew, dt);
    power = constrain(power, 0.0f, cfg.limits.maxPower);
 
    // ------------------------------------------------------------------
    // ACTUADORES
    // ------------------------------------------------------------------
 
    const int servo1Us = pulse(SERVO1_Z_DEG);
    const int servo2Us = pulse(SERVO2_Z_DEG);
 
    if (ctx.robot->actuatorsAreArmed()) {
        ctrl.alt.lastPower = power;
        ctx.robot->commandMotorPowerTest(power, power, servo1Us, servo2Us);
    } else {
        ctrl.alt.lastPower = 0.0f;
        power = 0.0f;
    }
 
    ctx.robot->servo_old1   = SERVO1_Z_DEG;
    ctx.robot->servo_old2   = SERVO2_Z_DEG;
    ctx.robot->motor_power1 = power;
    ctx.robot->motor_power2 = power;
 
    // ------------------------------------------------------------------
    // BANDA DE EXITO  (solo informativa, no limita nada)
    // ------------------------------------------------------------------
 
    const bool nowInBand = valid && (fabsf(heightError) <= cfg.limits.successM);
 
    if (!nowInBand) {
        ctrl.inBand = false;
    } else if (!ctrl.inBand) {
        ctrl.inBand = true;
        ctrl.inBandSinceMs = nowMs;
    }
 
    // ------------------------------------------------------------------
    // DEBUG
    //
    // Mira `I`: cuando se estabiliza, ESA es tu potencia de hover. Es el
    // numero que necesitas para elegir amin/amax y para --yaw-base-power
    // de P03.
    // ------------------------------------------------------------------
 
    if (elapsedMs(nowMs, ctrl.lastDebugMs, DEBUG_PERIOD_MS)) {
 
        ctrl.lastDebugMs = nowMs;
 
        const uint32_t bandMs = ctrl.inBand ? (nowMs - ctrl.inBandSinceMs) : 0U;
 
        Serial.printf(
            "[P04] zRef=%.2f z=%.2f zErr=%.3f vz=%.3f | "
            "P=%.3f I=%.3f D=%.3f | dem=%.3f out=%.3f | band=%lums\n",
            heightRef,
            height,
            heightError,
            filteredVz,
            cfg.gains.kp * ((fabsf(heightError) < ALT_DEADBAND_M) ? 0.0f : heightError),
            ctrl.alt.integral,
            -cfg.gains.kd * filteredVz,
            demand,
            power,
            (unsigned long)bandMs);
    }
 
    Telemetry::sendControl(ctx, 0.0f, heightRef, heightError);
}
 
} // namespace TestRunners
 