#include "tests/TestRunners.h"
#include "app/AppConfig.h"
#include "app/ControlCommon.h"
#include "app/SensorMap.h"
#include "app/Telemetry.h"
#include <Arduino.h>
#include <math.h>
#include <stdint.h>

// ============================================================================
// P07 v2  -  CENTRADO VISUAL SOBRE LA CASCADA DE YAW
//
// QUE CAMBIO Y POR QUE
// --------------------
// La version anterior tenia su PROPIO PD de vision que mandaba potencia
// directamente a los motores:
//
//     error_px -> PD propio (vis_kp, vis_kd) -> potencia + servos al extremo
//
// Tres problemas, todos medidos en vuelo el 23/08:
//
//  1) EL DERIVATIVO SE CALCULABA SOBRE nicla_x. La Nicla reporta a ~10 FPS y
//     la deteccion era valida solo el 66% del tiempo. Derivar esa senal da
//     basura, y con kd alto la basura va directo a los actuadores.
//
//  2) UNIDADES PROPIAS QUE NO TRANSFIEREN. vis_kp iba de error normalizado a
//     potencia, sin relacion con la cascada de yaw. Habia que sintonizar dos
//     lazos de yaw distintos y solo uno estaba bien ajustado.
//
//  3) SIN AUTORIDAD UTIL. Con vis_kp = 0.10 y 40 px de error el PD pedia 3.3%,
//     por debajo del piso de 6%, asi que quedaba como rele. Medido: oscilaba
//     +/-60 px sin converger.
//
// AHORA, siguiendo el patron de NiclaSimpleServo::servoing del repo de
// referencia:
//
//     error_px -> offset angular -> REFERENCIA de yaw -> cascada -> mixer
//
// La vision ya no manda potencia: dice HACIA DONDE mirar. El lazo de yaw ya
// sintonizado (ykp/ykd/yki sobre el giroscopio a 250 Hz) se encarga de llegar.
// En el repo de referencia:
//
//     des_yaw = (tracking_x / max_x) - 0.5
//     robot_to_goal = yaw + des_yaw       // referencia absoluta
//
// VENTAJA CLAVE
// -------------
// La referencia solo se actualiza con deteccion NUEVA. Entre detecciones queda
// congelada y el yaw sigue apuntando al ultimo lugar conocido, con el
// giroscopio cerrando el lazo a 250 Hz. Que la Nicla pierda el objetivo un
// tercio del tiempo deja de importar tanto.
//
//
// SLOTS DE ControlInput
// ---------------------
//   AUX0  : yaw Kp   (lazo externo de la cascada, rad/s por radian)
//   AUX1  : yaw Kd   (lazo interno, mando por rad/s)
//   AUX2  : YAW_PACK (min, max, deadband, autoridad, ki)
//   AUX3  : campo de vision horizontal en grados
//   AUX4  : zona muerta visual en pixeles
// ============================================================================

namespace {

using namespace ControlCommon;

// GEOMETRIA DE LA CAMARA
// La Nicla corre en HQVGA: 240 x 160. Centro horizontal en 120.
constexpr float IMAGE_WIDTH_PX = 240.0f;
constexpr float IMAGE_CENTER_X = IMAGE_WIDTH_PX * 0.5f;

// Campo de vision horizontal por defecto. Convierte pixeles a grados:
//
//     offset_grados = (x - centro) / ancho * FOV
//
// Con FOV amplio (IOCTL_SET_FOV_WIDE) ronda los 90 grados. Si el centrado se
// pasa o se queda corto de forma SISTEMATICA, este es el numero a ajustar:
// es una propiedad de la LENTE, no una ganancia de control.
constexpr float DEFAULT_FOV_DEG = 90.0f;

// Zona muerta visual. Medido el 23/08 con el globo inmovil: nicla_x tiene
// ~5 px de ruido. 20 px deja margen sin perder precision util.
constexpr float DEFAULT_DEADBAND_PX = 20.0f;

// SIGNO DE LA CONVERSION PIXEL -> YAW
// -----------------------------------
// Relaciona "objetivo a la derecha en la imagen" con el sentido de giro que
// acerca el vehiculo a el.
//
// Medido en vuelo el 23/08: con VISION_YAW_SIGN = +1 el blimp giraba al reves
// (correlacion -0.674 entre error en pixeles y sentido de servo, 5 de 6 casos
// incoherentes). La convencion de yaw del BNO085 va en sentido opuesto al de
// la coordenada x de la imagen, igual que ya pasaba con angVelZ.
//
// Si algun dia se cambia el IMU, la orientacion de la camara o la conversion
// a Euler, hay que volver a medirlo: correlacionar fx_cmd (error en px) con
// el signo de (servo1 - SERVO1_Z_DEG) sobre un vuelo con el objetivo bien
// detectado.
constexpr float VISION_YAW_SIGN = -1.0f;

// Sin deteccion durante este tiempo se descarta la referencia y el vehiculo
// deja de girar, en vez de perseguir un dato viejo.
constexpr uint32_t TARGET_TIMEOUT_MS = 1500U;

constexpr uint32_t DEBUG_PERIOD_MS = 500U;


struct Config {
    YawGains   yawGains;
    YawLimits  yawLimits;
    float      fovDeg;
    float      deadbandPx;
    PackStatus yawPack;
};


struct State {
    bool initialized = false;
    unsigned long modeEnteredMs = 0U;
    unsigned long lastUs = 0U;

    YawState yaw;

    float lastServo1 = SERVO1_Z_DEG;
    float lastServo2 = SERVO2_Z_DEG;
    float lastPower  = 0.0f;

    // Referencia de yaw derivada de la vision. Se actualiza SOLO con
    // deteccion nueva; entre medias el lazo sigue apuntando aqui.
    float yawRef = 0.0f;
    bool  yawRefValid = false;
    uint32_t lastDetectionMs = 0U;

    float lastNiclaX = -1.0f;

    uint32_t lastDebugMs = 0U;
};

State ctrl;


// La Nicla marca deteccion valida con el bit 0x40 (modo globo) mas alguno de
// los bits bajos alternando. Se exige ademas ancho > 0: el flag puede venir
// activo con coordenadas vacias.
bool detected(const AppContext& ctx) {
    const int flag = (int)ctx.sensors[SensorMap::NICLA_FLAG];
    const float w = ctx.sensors[SensorMap::NICLA_W];
    return ((flag & 0x40) != 0) && ((flag & 0x03) != 0) &&
           isfinite(w) && (w > 0.0f);
}


Config getConfig(const AppContext& ctx) {

    Config cfg;

    cfg.yawGains.kp = pickPositive(
        ctx.command.params[AppConfig::PARAM_AUX0], DEF_YAW_KP, 0.0f, 20.0f);

    cfg.yawGains.kd = pickNonNegative(
        ctx.command.params[AppConfig::PARAM_AUX1], DEF_YAW_KD, 0.0f, 10.0f);

    cfg.yawPack = decodeYawPack(
        ctx.command.params[AppConfig::PARAM_AUX2], cfg.yawLimits);

    cfg.yawGains.ki = cfg.yawLimits.ki;

    cfg.fovDeg = pickPositive(
        ctx.command.params[AppConfig::PARAM_AUX3], DEFAULT_FOV_DEG, 20.0f, 180.0f);

    cfg.deadbandPx = pickPositive(
        ctx.command.params[AppConfig::PARAM_AUX4], DEFAULT_DEADBAND_PX, 1.0f, 100.0f);

    return cfg;
}


void dumpConfig(const Config& cfg) {

    Serial.printf(
        "[P07] cfg CASCADA kp=%.3f ki=%.2f kd=%.3f min=%.4f max=%.4f "
        "db=%.1fdeg auth=%.2f  pack=%s\n",
        cfg.yawGains.kp, cfg.yawGains.ki, cfg.yawGains.kd,
        cfg.yawLimits.minPower, cfg.yawLimits.maxPower,
        cfg.yawLimits.deadbandRad * RAD2DEG,
        cfg.yawLimits.authority,
        packStatusName(cfg.yawPack));

    Serial.printf(
        "[P07] cfg VISION fov=%.0fdeg deadband=%.0fpx (%.3f deg por pixel)\n",
        cfg.fovDeg, cfg.deadbandPx, cfg.fovDeg / IMAGE_WIDTH_PX);

    if (cfg.yawPack == PackStatus::LEGACY_V1) {
        Serial.println("[P07] AVISO: run_test.py desactualizado.");
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
    ctrl.yawRef      = isfinite(yaw) ? yaw : 0.0f;
    ctrl.yawRefValid = false;

    resetYawState(ctrl.yaw, yaw);

    Serial.println("[P07] reset");
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

void p07VisualCenter(AppContext& ctx) {

    const float yaw = ctx.sensors[SensorMap::YAW];

    const Config cfg = getConfig(ctx);

    if (!ctrl.initialized || ctrl.modeEnteredMs != ctx.modeEnteredMs) {
        resetController(cfg, yaw);
        ctrl.modeEnteredMs = ctx.modeEnteredMs;
    }

    const float dt = getDt();
    const uint32_t nowMs = millis();

    const float measuredRate = ctx.sensors[SensorMap::YAW_RATE];
    const float yawRate = updateYawRate(ctrl.yaw, yaw, measuredRate, dt);

    // ------------------------------------------------------------------
    // VISION -> REFERENCIA DE YAW
    //
    // Solo se recalcula con deteccion NUEVA (nicla_x cambio de valor).
    // Entre detecciones la referencia queda congelada y el lazo sigue
    // apuntando al ultimo lugar conocido, cerrando sobre el giroscopio a
    // 250 Hz. Por eso ya no importa tanto que la Nicla reporte a 10 FPS.
    // ------------------------------------------------------------------

    const float niclaX = ctx.sensors[SensorMap::NICLA_X];
    const bool hasTarget = detected(ctx) && isfinite(niclaX) && isfinite(yaw);

    float errorPx = 0.0f;
    bool freshDetection = false;

    if (hasTarget) {

        errorPx = niclaX - IMAGE_CENTER_X;
        ctrl.lastDetectionMs = nowMs;

        // La Nicla repite el ultimo valor entre reportes: solo cuenta si
        // cambio de verdad.
        if (fabsf(niclaX - ctrl.lastNiclaX) > 0.5f || !ctrl.yawRefValid) {

            freshDetection = true;
            ctrl.lastNiclaX = niclaX;

            if (fabsf(errorPx) > cfg.deadbandPx) {
                // Pixeles -> grados -> referencia ABSOLUTA de yaw.
                // x positivo = objetivo a la derecha en la imagen.
                const float offsetRad = VISION_YAW_SIGN *
                    (errorPx / IMAGE_WIDTH_PX) * cfg.fovDeg * DEG2RAD;

                ctrl.yawRef = wrapPi(yaw + offsetRad);
            } else if (!ctrl.yawRefValid) {
                ctrl.yawRef = yaw;
            }

            ctrl.yawRefValid = true;
        }
    }

    // Objetivo perdido demasiado tiempo: soltar la referencia en vez de
    // seguir girando hacia un dato viejo.
    if (ctrl.yawRefValid &&
        elapsedMs(nowMs, ctrl.lastDetectionMs, TARGET_TIMEOUT_MS)) {
        ctrl.yawRefValid = false;
        ctrl.lastNiclaX = -1.0f;
        resetYawState(ctrl.yaw, yaw);
    }

    // ------------------------------------------------------------------
    // CASCADA DE YAW  -  las mismas ganancias que P03 y P05
    // ------------------------------------------------------------------

    float yawError = 0.0f;
    float yawCmd = 0.0f;

    if (ctrl.yawRefValid) {
        yawError = wrapPi(ctrl.yawRef - yaw);
        yawCmd = computeYawCommand(cfg.yawGains, cfg.yawLimits, ctrl.yaw,
                                   yawError, yawRate, dt);
    }

    // ------------------------------------------------------------------
    // MIXER  -  sin demanda de altura: P07 no controla Z
    // ------------------------------------------------------------------

    float servo1Deg = SERVO1_Z_DEG;
    float servo2Deg = SERVO2_Z_DEG;
    float power     = 0.0f;
    float etaZ      = 1.0f;
    float etaYaw    = 0.0f;
    float blend     = 0.0f;

    mixOutputs(0.0f, yawCmd, cfg.yawLimits, cfg.yawLimits.maxPower,
               servo1Deg, servo2Deg, power, etaZ, etaYaw, blend);

    // ------------------------------------------------------------------
    // LIMITES DE VELOCIDAD DE ACTUADORES
    // ------------------------------------------------------------------

    power = rateLimit(power, ctrl.lastPower, DEF_ALT_SLEW, dt);
    power = constrain(power, 0.0f, cfg.yawLimits.maxPower);

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
        power = 0.0f;
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
            "[P07] %s errPx=%+6.1f yawRef=%+7.1f yaw=%+7.1f yawErr=%+6.1f "
            "cmd=%+.3f | blend=%+.2f P=%.3f %s\n",
            ctrl.yawRefValid ? "TRK" : "---",
            errorPx,
            ctrl.yawRef * RAD2DEG,
            yaw * RAD2DEG,
            yawError * RAD2DEG,
            yawCmd,
            blend,
            power,
            freshDetection ? "<-nueva" : "");
    }

    Telemetry::sendControl(ctx, ctrl.yawRef, 0.0f, errorPx);
}

} // namespace TestRunners