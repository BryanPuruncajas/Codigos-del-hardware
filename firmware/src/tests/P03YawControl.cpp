#include "tests/TestRunners.h"
#include "app/AppConfig.h"
#include "app/ControlCommon.h"
#include "app/SensorMap.h"
#include "app/Telemetry.h"
#include <Arduino.h>
#include <math.h>
#include <stdint.h>

// ============================================================================
// P03 v2  -  YAW AISLADO, MISMA ARQUITECTURA QUE P05
//
// QUE CAMBIO Y POR QUE
// --------------------
// El P03 anterior era otra cosa completamente distinta a P05:
//
//   - la salida del PD estaba en FRACCION DE POTENCIA, no en mando normalizado
//   - los servos saltaban al EXTREMO COMPLETO (0/0 o 120/120): bang-bang puro,
//     sin blend proporcional ni autoridad configurable
//   - no tenia termino integral
//   - la zona muerta actuaba distinto
//
// Resultado: las ganancias que sintonizabas en P03 NO se podian copiar a P05.
// Peor, los valores tipicos de P03 (kp ~ 0.12) metidos en P05 v5 dejaban el
// yaw practicamente muerto, porque en P05 kp esta en "mando por radian" y
// necesita valores del orden de 2.0.
//
// Ahora P03 llama exactamente a las mismas funciones de ControlCommon que P05.
// Mismas unidades, mismo deadband, mismo filtro de velocidad, mismo mapeo de
// potencia, mismo blend de servos. Lo que sintonices aqui vale tal cual alla.
//
//
// POTENCIA BASE  (--yaw-base-power)
// ---------------------------------
// P03 no controla altura, pero puedes darle un empuje vertical CONSTANTE para
// que el ensayo ocurra cerca del punto de operacion real en vez de a empuje
// cero. Ese valor entra en el mixer en el mismo lugar donde P05 pone la
// demanda del PID de altura:
//
//     power = max(basePower / etaZ, pYaw)
//
// Usa aqui la potencia de hover que midas con P00M. Con 0 el blimp queda a la
// deriva vertical y solo se mueve para girar.
//
//
// SLOTS DE ControlInput
// ---------------------
//   TZ    : referencia de yaw en radianes
//   AUX0  : yaw Kp   (mando por radian)
//   AUX1  : yaw Kd   (mando por rad/s)
//   AUX2  : YAW_PACK (min, max, deadband, autoridad, ki)
//   AUX3  : potencia base, fraccion 0..1
//   AUX4  : reservado
// ============================================================================

namespace {

using namespace ControlCommon;

constexpr uint32_t DEBUG_PERIOD_MS = 500U;

// Techo de potencia del test. El yaw nunca deberia pedir mas que su propio
// maxPower, pero dejamos un tope duro por seguridad.
constexpr float P03_ABSOLUTE_MAX_POWER = 0.30f;


struct Config {
    YawGains  gains;
    YawLimits limits;
    float     basePower;
    PackStatus packStatus;
};


struct State {
    bool initialized = false;
    unsigned long modeEnteredMs = 0U;
    unsigned long lastUs = 0U;

    YawState yaw;

    float lastPower  = 0.0f;
    float lastServo1 = SERVO1_Z_DEG;
    float lastServo2 = SERVO2_Z_DEG;

    uint32_t lastDebugMs = 0U;
};

State ctrl;


Config getConfig(const AppContext& ctx) {

    Config cfg;

    cfg.gains.kp = pickPositive(
        ctx.command.params[AppConfig::PARAM_AUX0], DEF_YAW_KP, 0.0f, 20.0f);

    cfg.gains.kd = pickNonNegative(
        ctx.command.params[AppConfig::PARAM_AUX1], DEF_YAW_KD, 0.0f, 10.0f);

    cfg.packStatus = decodeYawPack(
        ctx.command.params[AppConfig::PARAM_AUX2], cfg.limits);

    cfg.gains.ki = cfg.limits.ki;

    cfg.basePower = pickNonNegative(
        ctx.command.params[AppConfig::PARAM_AUX3], 0.0f, 0.0f, P03_ABSOLUTE_MAX_POWER);

    return cfg;
}


void dumpConfig(const Config& cfg) {

    Serial.printf(
        "[P03] cfg YAW kp=%.3f ki=%.3f kd=%.3f min=%.4f max=%.4f "
        "db=%.1fdeg auth=%.3f base=%.4f  pack=%s\n",
        cfg.gains.kp, cfg.gains.ki, cfg.gains.kd,
        cfg.limits.minPower, cfg.limits.maxPower,
        cfg.limits.deadbandRad * RAD2DEG,
        cfg.limits.authority,
        cfg.basePower,
        packStatusName(cfg.packStatus));

    if (cfg.packStatus == PackStatus::LEGACY_V1) {
        Serial.println("[P03] AVISO: run_test.py desactualizado. "
                       "Actualiza la estacion de tierra o volaras con defaults.");
    }
}


int pulse(float deg) {
    return degreesToPulseUs(deg,
                            AppConfig::SERVO_ANGLE_MIN_DEG, AppConfig::SERVO_ANGLE_MAX_DEG,
                            AppConfig::SERVO_PULSE_MIN_US, AppConfig::SERVO_PULSE_MAX_US);
}


void resetController(const Config& cfg, float yaw) {

    ctrl = State{};

    ctrl.initialized = true;
    ctrl.lastUs      = micros();
    ctrl.lastServo1  = SERVO1_Z_DEG;
    ctrl.lastServo2  = SERVO2_Z_DEG;
    ctrl.lastDebugMs = millis();

    resetYawState(ctrl.yaw, yaw);

    Serial.println("[P03] reset");
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

void p03Yaw(AppContext& ctx) {

    const float yawRef = ctx.command.params[AppConfig::PARAM_TZ];
    const float yaw    = ctx.sensors[SensorMap::YAW];

    const Config cfg = getConfig(ctx);

    if (!ctrl.initialized || ctrl.modeEnteredMs != ctx.modeEnteredMs) {
        resetController(cfg, yaw);
        ctrl.modeEnteredMs = ctx.modeEnteredMs;
    }

    const float dt    = getDt();
    const uint32_t nowMs = millis();

    // Velocidad de yaw del GIROSCOPIO (negada dentro de updateYawRate segun
    // YAW_RATE_SIGN). Si el sensor no la entrega, la funcion cae sola a
    // derivar el angulo.
    const float measuredRate = ctx.sensors[SensorMap::YAW_RATE];
    const float yawRate = updateYawRate(ctrl.yaw, yaw, measuredRate, dt);

    const bool yawValid = isfinite(yawRef) && isfinite(yaw);

    const float yawError = yawValid ? wrapPi(yawRef - yaw) : 0.0f;

    // ------------------------------------------------------------------
    // LAZO DE YAW
    // ------------------------------------------------------------------

    float yawCmd = 0.0f;

    if (yawValid) {
        yawCmd = computeYawCommand(cfg.gains, cfg.limits, ctrl.yaw,
                                   yawError, yawRate, dt);
    }

    // ------------------------------------------------------------------
    // MIXER
    //
    // Identico al de P05, con la potencia base ocupando el lugar que alla
    // ocupa la demanda del PID de altura.
    // ------------------------------------------------------------------

    float servo1Deg = SERVO1_Z_DEG;
    float servo2Deg = SERVO2_Z_DEG;
    float power     = 0.0f;
    float etaZ      = 1.0f;
    float etaYaw    = 0.0f;
    float blend     = 0.0f;

    const float powerCeiling = fmaxf(cfg.limits.maxPower, cfg.basePower);

    mixOutputs(cfg.basePower, yawCmd, cfg.limits, powerCeiling,
               servo1Deg, servo2Deg, power, etaZ, etaYaw, blend);

    // ------------------------------------------------------------------
    // LIMITES DE VELOCIDAD DE ACTUADORES
    // ------------------------------------------------------------------

    power = rateLimit(power, ctrl.lastPower, DEF_ALT_SLEW, dt);
    power = constrain(power, 0.0f, powerCeiling);

    servo1Deg = rateLimit(servo1Deg, ctrl.lastServo1, SERVO_SLEW_DEG_PER_S, dt);
    servo2Deg = rateLimit(servo2Deg, ctrl.lastServo2, SERVO_SLEW_DEG_PER_S, dt);

    servo1Deg = constrain(servo1Deg, AppConfig::SERVO_ANGLE_MIN_DEG, AppConfig::SERVO_ANGLE_MAX_DEG);
    servo2Deg = constrain(servo2Deg, AppConfig::SERVO_ANGLE_MIN_DEG, AppConfig::SERVO_ANGLE_MAX_DEG);

    ctrl.lastServo1 = servo1Deg;
    ctrl.lastServo2 = servo2Deg;

    // ------------------------------------------------------------------
    // ACTUADORES
    // ------------------------------------------------------------------

    if (ctx.robot->actuatorsAreArmed()) {
        ctrl.lastPower = power;
        ctx.robot->commandMotorPowerTest(power, power,
                                         pulse(servo1Deg), pulse(servo2Deg));
    } else {
        ctrl.lastPower = 0.0f;
    }

    ctx.robot->servo_old1   = servo1Deg;
    ctx.robot->servo_old2   = servo2Deg;
    ctx.robot->motor_power1 = power;
    ctx.robot->motor_power2 = power;

    // ------------------------------------------------------------------
    // DEBUG
    // ------------------------------------------------------------------

    if (elapsedMs(nowMs, ctrl.lastDebugMs, DEBUG_PERIOD_MS)) {

        ctrl.lastDebugMs = nowMs;

        Serial.printf(
            "[P03] yawErr=%.1f yawRate=%.2f yawCmd=%.3f yawI=%.3f | "
            "blend=%.2f etaZ=%.2f etaY=%.2f | P=%.3f Fz=%.3f S1=%.1f S2=%.1f\n",
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

    Telemetry::sendControl(ctx, yawRef, 0.0f, yawError);
}

} // namespace TestRunners