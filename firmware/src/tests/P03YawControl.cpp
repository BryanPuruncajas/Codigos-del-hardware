#include "tests/TestRunners.h"
#include "app/AppConfig.h"
#include "app/ControlCommon.h"
#include "app/SupportYaw.h"
#include "app/VisualSupport.h"
#include "app/VisualBearing.h"
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
//   AUX4  : 0 legacy; 1 constant support/heading; 2 visual support/heading
//   FX    : AUX4=2 only: 1 preparing while held, 0 released; no mode reset
//   TX    : AUX4=2 only: optional forward demand0..0.01, gated by vision
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
    bool      supportHold;
    bool      visualHold;
    bool      preparing;
    PackStatus packStatus;
};


struct State {
    bool initialized = false;
    unsigned long modeEnteredMs = 0U;
    unsigned long lastUs = 0U;

    YawState yaw;
    SupportYaw::Mixer supportMixer;
    bool supportHold = false;
    bool visualHold = false;
    VisualSupport::Loop visual;
    VisualBearing bearing;
    uint32_t bearingGeneration = 0;

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
    const float selector=ctx.command.params[AppConfig::PARAM_AUX4];
    cfg.supportHold = selector==1.0f || selector==2.0f;
    cfg.visualHold = selector==2.0f;
    cfg.preparing = cfg.visualHold && ctx.command.params[AppConfig::PARAM_FX]>.5f;

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
                            AppConfig::P0025_MIN_DEG, AppConfig::P0025_MAX_DEG,
                            AppConfig::P0025_MIN_US, AppConfig::P0025_MAX_US);
}


void resetController(const Config& cfg, float yaw) {

    ctrl = State{};

    ctrl.initialized = true;
    ctrl.lastUs      = micros();
    ctrl.lastServo1  = SERVO1_Z_DEG;
    ctrl.lastServo2  = SERVO2_Z_DEG;
    ctrl.lastDebugMs = millis();

    resetYawState(ctrl.yaw, yaw);
    ctrl.supportHold = cfg.supportHold;
    ctrl.visualHold = cfg.visualHold;
    ctrl.supportMixer.reset(cfg.visualHold);
    ctrl.visual.reset(cfg.basePower,millis());

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

    float yawRef = ctx.command.params[AppConfig::PARAM_TZ];
    const float yaw    = ctx.sensors[SensorMap::YAW];

    const Config cfg = getConfig(ctx);

    if (!ctrl.initialized || ctrl.modeEnteredMs != ctx.modeEnteredMs || ctrl.supportHold != cfg.supportHold || ctrl.visualHold != cfg.visualHold) {
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

    float yawError = yawValid ? wrapPi(yawRef - yaw) : 0.0f;

    if (cfg.supportHold) {
        // Forward requires explicit TX opt-in. Relative barometer is used only
        // during bounded visual recovery after a centred height anchor exists.
        bool valid = yawValid && isfinite(measuredRate);
        float verticalDemand=cfg.basePower, yawCommand=0,forwardRequest=0;
        // Normal control keeps positive thrust, but must allow the predictive
        // height law to request less than the former14% floor while braking.
        const float motorFloor=cfg.visualHold ? fminf(cfg.limits.maxPower,
            fmaxf(VisualSupport::MIN_RUNNING_POWER,cfg.limits.minPower)) : 0.f;
        if(cfg.visualHold && ctx.robot->actuatorsAreArmed()){
            const auto& feedback=ctx.robot->sensorsuite.niclaFeedback();
            VisualSupport::Observation o;
            o.fresh=feedback.fresh && !feedback.resetPending && feedback.protocol==NiclaFeedback::N03_PROTOCOL;
            o.confirmed=(feedback.state==NiclaFeedback::FULL || feedback.state==NiclaFeedback::PARTIAL) &&
                (ctx.sensors[SensorMap::NICLA_FLAG]==65 || ctx.sensors[SensorMap::NICLA_FLAG]==66);
            o.full=feedback.state==NiclaFeedback::FULL;o.generation=feedback.generation;
            o.x=ctx.sensors[SensorMap::NICLA_X_VALUE];o.y=ctx.sensors[SensorMap::NICLA_Y_VALUE];
            o.w=ctx.sensors[SensorMap::NICLA_W_VALUE];o.h=ctx.sensors[SensorMap::NICLA_H_VALUE];
            const auto barometer=ctx.robot->sensorsuite.verticalDiagnostics();
            ctrl.visual.update(o,nowMs,cfg.basePower,cfg.limits.maxPower,cfg.preparing,
                motorFloor*ctrl.supportMixer.etaZ,
                ctx.sensors[SensorMap::ALTITUDE],ctx.sensors[SensorMap::VERTICAL_VELOCITY],
                barometer.healthy && barometer.ageMs<=250,ctx.sensors[SensorMap::PITCH]);
            verticalDemand=ctrl.visual.demand;
            valid=valid && !ctrl.visual.failed;
            if(valid && o.valid() &&
                (!ctrl.bearing.ready || o.generation!=ctrl.bearingGeneration)){
                ctrl.bearing.update(yaw,o.x,nowMs);
                ctrl.bearingGeneration=o.generation;
            }
            if(ctrl.bearing.ready){
                yawRef=wrapPi(ctrl.bearing.reference+ctrl.visual.recoveryYawOffset(nowMs));
                yawError=wrapPi(yawRef-yaw);
            }
            if(valid && !cfg.preparing)
                forwardRequest=ctrl.visual.forwardRequest(o,nowMs,ctx.command.params[AppConfig::PARAM_TX],yawError);
        }
        if (!valid) {
            if(cfg.visualHold)ctrl.visual.stop();
            ctrl.supportMixer.power = 0;
            ctrl.yaw.integral = 0;
            if (ctx.robot->actuatorsAreArmed()) {
                ctx.robot->commandMotorPowerTest(0,0,
                    pulse(ctrl.supportMixer.servo1),pulse(ctrl.supportMixer.servo2));
                ctx.robot->setActuatorsArmed(false);
            }
        } else if (ctx.robot->actuatorsAreArmed()) {
            // Stabilize as soon as thrust starts. While held, do not learn an
            // integral from the operator's restraint. Preserve controller and
            // mixer state at release; proportional/rate correction is continuous.
            yawCommand=SupportYaw::command(cfg.gains,cfg.limits,ctrl.yaw,
                yawError,yawRate,dt,!cfg.preparing && (!cfg.visualHold || ctrl.visual.phase==VisualSupport::RUN));
            const float ceiling=cfg.visualHold ? fminf(VisualSupport::MAX_POWER,cfg.limits.maxPower) :
                fminf(P03_ABSOLUTE_MAX_POWER,fmaxf(cfg.basePower,cfg.limits.maxPower));
            // Visual descent must NOT be overridden by a yaw minimum-power
            // request: steer using available vertical thrust, no yaw boost.
            ctrl.supportMixer.update(verticalDemand,yawCommand,cfg.limits.authority,
                cfg.visualHold?0.f:cfg.limits.maxPower,ceiling,dt,
                motorFloor,
                0.f,forwardRequest,cfg.visualHold);
            ctx.robot->commandMotorPowerTest(ctrl.supportMixer.power,ctrl.supportMixer.power,
                pulse(ctrl.supportMixer.servo1),pulse(ctrl.supportMixer.servo2));
        } else {
            ctrl.supportMixer.reset(cfg.visualHold);
            ctrl.yaw.integral = 0;
        }
        if(cfg.visualHold){
            const int phase=ctrl.visual.failed ? VisualSupport::FAULT :
                (ctx.robot->actuatorsAreArmed()?ctrl.visual.phase:VisualSupport::DISARMED);
            Telemetry::sendVisualSupport(ctx,ctrl.visual.lastY,
                ctrl.visual.seen?(80.f-ctrl.visual.lastY)/80.f:0.f,
                ctrl.visual.height.trim,ctrl.visual.demand,yawCommand,phase);
            Telemetry::sendVisualBearing(ctx,ctrl.bearing.lastX,yawRef,yawError,
                ctx.robot->actuatorsAreArmed() && ctrl.bearing.ready && !ctrl.visual.failed,
                VisualBearing::VERSION,forwardRequest);
        }
        ctx.robot->servo_old1=ctrl.supportMixer.servo1;
        ctx.robot->servo_old2=ctrl.supportMixer.servo2;
        ctx.robot->motor_power1=ctrl.supportMixer.power;
        ctx.robot->motor_power2=ctrl.supportMixer.power;
        if (elapsedMs(nowMs,ctrl.lastDebugMs,DEBUG_PERIOD_MS)) {
            ctrl.lastDebugMs=nowMs;
            Serial.printf("[P03_SUPPORT] error_deg=%.2f rate=%.3f I=%.3f power=%.3f etaZ=%.3f S1=%.1f S2=%.1f\n",
                yawError*RAD2DEG,yawRate,ctrl.yaw.integral,ctrl.supportMixer.power,
                ctrl.supportMixer.etaZ,ctrl.supportMixer.servo1,ctrl.supportMixer.servo2);
        }
        Telemetry::sendControl(ctx,yawRef,
            cfg.visualHold && ctrl.visual.altitudeReady?ctrl.visual.altitudeReference:0.0f,yawError);
        return;
    }

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

    servo1Deg = constrain(servo1Deg, AppConfig::P0025_MIN_DEG, AppConfig::P0025_MAX_DEG);
    servo2Deg = constrain(servo2Deg, AppConfig::P0025_MIN_DEG, AppConfig::P0025_MAX_DEG);

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
