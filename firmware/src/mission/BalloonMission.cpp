#include "mission/BalloonMission.h"
#include "app/AppConfig.h"
#include "app/SensorMap.h"
#include "app/Telemetry.h"
#include <Arduino.h>
#include <math.h>

namespace {

// ============================================================================
// CALIBRACION FISICA VALIDADA
// ============================================================================
// Valores obtenidos durante P00/P02/P03/P07.
constexpr float SERVO1_Z_DEG       = 35.0f;
constexpr float SERVO2_Z_DEG       = 85.0f;
constexpr float SERVO1_FORWARD_DEG = 120.0f;
constexpr float SERVO2_FORWARD_DEG = 0.0f;
constexpr float SERVO1_BACK_DEG    = 0.0f;
constexpr float SERVO2_BACK_DEG    = 120.0f;
constexpr float SERVO1_RIGHT_DEG   = 120.0f;
constexpr float SERVO2_RIGHT_DEG   = 120.0f;
constexpr float SERVO1_LEFT_DEG    = 0.0f;
constexpr float SERVO2_LEFT_DEG    = 0.0f;

// ============================================================================
// VISION VALIDADA EN P06/P07
// ============================================================================
constexpr float IMAGE_CENTER_X = 120.0f;
constexpr float DEFAULT_VIS_DEADBAND_PX = 15.0f;  // 105..135 px

// ============================================================================
// POTENCIAS DE MOVIMIENTO DE MISION
// ============================================================================
// Avance y escape se conservan como en las pruebas validadas.
constexpr float MOVE_POWER = 0.20f;

// P08/P09 conservaron busqueda al 20% durante su validacion.
constexpr float SINGLE_TARGET_SEARCH_POWER = 0.20f;

// P10/P11 usan por defecto giro maximo de 10%; puede cambiarse desde consola
// mediante --vis-max-power.
constexpr float DEFAULT_MULTI_TARGET_TURN_POWER = 0.10f;

// ============================================================================
// PARAMETROS DE VISITA / MAQUINA DE ESTADOS
// ============================================================================
constexpr float VISIT_SCORE_THRESHOLD = 60.0f;
constexpr int CLOSE_FRAMES_REQUIRED = 3;
constexpr int LOST_FRAMES_REQUIRED = 8;
constexpr unsigned long ESCAPE_MS = 1800;

// Nicla no entrega ID de cada globo. Despues de una visita se exige girar
// al menos 45 grados adicionales antes de aceptar una nueva deteccion.
constexpr float MIN_NEW_TARGET_YAW_RAD = 45.0f * PI / 180.0f;

// ============================================================================
// SUPERVISOR DE ALTURA P10/P11
// ============================================================================
// Se conservan EXACTAMENTE los umbrales de la version larga ya probada.
constexpr float ALTITUDE_ENTER_LOW_M  = 0.12f;  // faltan >12 cm
constexpr float ALTITUDE_ENTER_HIGH_M = 0.18f;  // sobra >18 cm
constexpr float ALTITUDE_EXIT_BAND_M  = 0.07f;  // retorna a +/-7 cm

// ============================================================================
// PID DE ALTURA TUNEABLE DESDE run_test.py
// ============================================================================
// P10/P11 reciben:
//   PARAM_FZ   = altura objetivo
//   PARAM_FX   = alt Kp
//   PARAM_TX   = alt Kd
//   PARAM_TZ   = alt Ki
//   PARAM_AUX0 = alt max power [0..1]
//   PARAM_AUX1 = alt slew [potencia/s]
constexpr float DEFAULT_ALT_KP = 0.30f;
constexpr float DEFAULT_ALT_KI = 0.025f;
constexpr float DEFAULT_ALT_KD = 0.18f;
constexpr float DEFAULT_ALT_MAX_POWER = 0.12f;
constexpr float DEFAULT_ALT_SLEW_PER_SEC = 0.18f;

constexpr float ALT_I_POWER_MAX = 0.06f;
constexpr float ALT_I_ACTIVE_ERROR_M = 0.35f;
constexpr float VZ_FILTER_ALPHA = 0.82f;
constexpr float HEIGHT_ERROR_DEADBAND_M = 0.025f;

struct AltitudeConfig {
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
};

AltitudeControllerState altitudeCtrl;

// ============================================================================
// PD VISUAL TUNEABLE DESDE run_test.py
// ============================================================================
// P08/P09 reciben:
//   AUX0 = vis Kp
//   AUX1 = vis Kd
//   AUX2 = vis min power
//   AUX3 = vis max power
//   AUX4 = deadband px
//
// P10/P11 necesitan simultaneamente altura + vision, asi que reciben:
//   AUX2 = vis Kp
//   AUX3 = vis Kd
//   AUX4 = vis max power
// En P10/P11 la potencia minima y deadband permanecen en defaults por falta
// de campos libres en ControlInput.params[].
constexpr float DEFAULT_VIS_KP = 0.10f;
constexpr float DEFAULT_VIS_KD = 0.015f;
constexpr float DEFAULT_VIS_MIN_POWER = 0.06f;
constexpr float DEFAULT_VIS_MAX_POWER = 0.10f;
constexpr float VIS_D_FILTER_ALPHA = 0.80f;

struct VisualConfig {
    float kp;
    float kd;
    float minPower;
    float maxPower;
    float deadbandPx;
};

struct VisualControllerState {
    bool initialized = false;
    float lastErrorNorm = 0.0f;
    float filteredDerivative = 0.0f;
    unsigned long lastUs = 0;
};

VisualControllerState visualCtrl;

// ============================================================================
// HELPERS GENERALES
// ============================================================================
float wrapPi(float angle) {
    while (angle > PI)  angle -= 2.0f * PI;
    while (angle < -PI) angle += 2.0f * PI;
    return angle;
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

void applyPhysicalCommand(AppContext& ctx,
                          float servo1Deg,
                          float servo2Deg,
                          float motorPower) {
    motorPower = constrain(motorPower, 0.0f, 1.0f);

    const int servo1Us = degreesToPulseUs(servo1Deg);
    const int servo2Us = degreesToPulseUs(servo2Deg);

    if (ctx.robot->actuatorsAreArmed()) {
        ctx.robot->commandMotorPowerTest(motorPower, motorPower,
                                         servo1Us, servo2Us);
    }

    // F3 refleja exactamente lo ordenado por la mision.
    ctx.robot->servo_old1 = servo1Deg;
    ctx.robot->servo_old2 = servo2Deg;
    ctx.robot->motor_power1 = motorPower;
    ctx.robot->motor_power2 = motorPower;
}

// ============================================================================
// CONFIGURACION PID ALTURA
// ============================================================================
AltitudeConfig getAltitudeConfig(const AppContext& ctx) {
    const float kp = ctx.command.params[AppConfig::PARAM_FX];
    const float kd = ctx.command.params[AppConfig::PARAM_TX];
    const float ki = ctx.command.params[AppConfig::PARAM_TZ];
    const float maxPower = ctx.command.params[AppConfig::PARAM_AUX0];
    const float slew = ctx.command.params[AppConfig::PARAM_AUX1];

    const bool valid =
        isfinite(kp) && kp >= 0.0f &&
        isfinite(ki) && ki >= 0.0f &&
        isfinite(kd) && kd >= 0.0f &&
        isfinite(maxPower) && maxPower > 0.0f && maxPower <= 1.0f &&
        isfinite(slew) && slew > 0.0f;

    if (valid) {
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
        DEFAULT_ALT_SLEW_PER_SEC
    };
}

void resetAltitudeController(float reference, float rawVz) {
    altitudeCtrl.initialized = true;
    altitudeCtrl.filteredVz = isfinite(rawVz) ? rawVz : 0.0f;
    altitudeCtrl.integralPower = 0.0f;
    altitudeCtrl.lastPower = 0.0f;
    altitudeCtrl.lastReference = reference;
    altitudeCtrl.lastUs = micros();
}

float calculateAltitudePower(float reference,
                             float height,
                             float rawVz,
                             const AltitudeConfig& cfg) {
    if (!isfinite(reference) || !isfinite(height) || !isfinite(rawVz)) {
        altitudeCtrl.lastPower = 0.0f;
        return 0.0f;
    }

    if (!altitudeCtrl.initialized ||
        !isfinite(altitudeCtrl.lastReference) ||
        fabsf(reference - altitudeCtrl.lastReference) > 0.05f) {
        resetAltitudeController(reference, rawVz);
    }

    const unsigned long nowUs = micros();
    float dt = (nowUs - altitudeCtrl.lastUs) * 1.0e-6f;
    altitudeCtrl.lastUs = nowUs;

    if (!isfinite(dt) || dt <= 0.0f || dt > 0.20f) {
        dt = 0.01f;
    }

    altitudeCtrl.lastReference = reference;

    altitudeCtrl.filteredVz =
        VZ_FILTER_ALPHA * altitudeCtrl.filteredVz +
        (1.0f - VZ_FILTER_ALPHA) * rawVz;

    const float error = reference - height;

    float pError = error;
    if (fabsf(pError) < HEIGHT_ERROR_DEADBAND_M) {
        pError = 0.0f;
    }

    // Integral solo cerca de la referencia y limitado a aporte positivo.
    if (fabsf(error) <= ALT_I_ACTIVE_ERROR_M) {
        altitudeCtrl.integralPower += cfg.ki * error * dt;
        const float iLimit = min(ALT_I_POWER_MAX, cfg.maxPower);
        altitudeCtrl.integralPower = constrain(altitudeCtrl.integralPower,
                                               0.0f,
                                               iLimit);
    }

    const float pTerm = cfg.kp * pError;
    const float dTerm = -cfg.kd * altitudeCtrl.filteredVz;

    float requestedPower = pTerm + altitudeCtrl.integralPower + dTerm;

    // Solo se ha validado empuje vertical +Z. No ordenamos potencia negativa.
    requestedPower = constrain(requestedPower, 0.0f, cfg.maxPower);

    // Rampa para evitar saltos bruscos de potencia.
    const float maxDelta = cfg.slewPerSec * dt;
    float power = constrain(requestedPower,
                            altitudeCtrl.lastPower - maxDelta,
                            altitudeCtrl.lastPower + maxDelta);
    power = constrain(power, 0.0f, cfg.maxPower);

    altitudeCtrl.lastPower = power;
    return power;
}

// ============================================================================
// CONFIGURACION PD VISUAL
// ============================================================================
VisualConfig getVisualConfig(const AppContext& ctx, int targetCount) {
    // P10/P11: AUX2/AUX3/AUX4 se reservan para vision.
    if (targetCount >= 2) {
        const float kp = ctx.command.params[AppConfig::PARAM_AUX2];
        const float kd = ctx.command.params[AppConfig::PARAM_AUX3];
        const float maxPower = ctx.command.params[AppConfig::PARAM_AUX4];

        const bool valid =
            isfinite(kp) && kp >= 0.0f &&
            isfinite(kd) && kd >= 0.0f &&
            isfinite(maxPower) && maxPower > 0.0f && maxPower <= 1.0f;

        if (valid) {
            return {
                kp,
                kd,
                DEFAULT_VIS_MIN_POWER,
                constrain(maxPower, DEFAULT_VIS_MIN_POWER, 1.0f),
                DEFAULT_VIS_DEADBAND_PX
            };
        }

        return {
            DEFAULT_VIS_KP,
            DEFAULT_VIS_KD,
            DEFAULT_VIS_MIN_POWER,
            DEFAULT_MULTI_TARGET_TURN_POWER,
            DEFAULT_VIS_DEADBAND_PX
        };
    }

    // P08/P09: todos los parametros visuales estan disponibles.
    const float kp = ctx.command.params[AppConfig::PARAM_AUX0];
    const float kd = ctx.command.params[AppConfig::PARAM_AUX1];
    const float minPower = ctx.command.params[AppConfig::PARAM_AUX2];
    const float maxPower = ctx.command.params[AppConfig::PARAM_AUX3];
    const float deadbandPx = ctx.command.params[AppConfig::PARAM_AUX4];

    const bool valid =
        isfinite(kp) && kp >= 0.0f &&
        isfinite(kd) && kd >= 0.0f &&
        isfinite(minPower) && minPower >= 0.0f &&
        isfinite(maxPower) && maxPower > 0.0f && maxPower <= 1.0f &&
        minPower <= maxPower &&
        isfinite(deadbandPx) && deadbandPx >= 1.0f && deadbandPx <= 100.0f;

    if (valid) {
        return {
            kp,
            kd,
            minPower,
            maxPower,
            deadbandPx
        };
    }

    // Si no hay configuracion valida, conserva comportamiento seguro.
    return {
        DEFAULT_VIS_KP,
        DEFAULT_VIS_KD,
        DEFAULT_VIS_MIN_POWER,
        SINGLE_TARGET_SEARCH_POWER,
        DEFAULT_VIS_DEADBAND_PX
    };
}

void resetVisualController(float errorNorm = 0.0f) {
    visualCtrl.initialized = true;
    visualCtrl.lastErrorNorm = errorNorm;
    visualCtrl.filteredDerivative = 0.0f;
    visualCtrl.lastUs = micros();
}

float calculateVisualTurnPower(float errorNorm,
                               const VisualConfig& cfg) {
    if (!isfinite(errorNorm)) {
        resetVisualController();
        return 0.0f;
    }

    if (!visualCtrl.initialized) {
        resetVisualController(errorNorm);
    }

    const unsigned long nowUs = micros();
    float dt = (nowUs - visualCtrl.lastUs) * 1.0e-6f;
    visualCtrl.lastUs = nowUs;

    if (!isfinite(dt) || dt <= 0.0f || dt > 0.20f) {
        dt = 0.01f;
    }

    const float derivative =
        (errorNorm - visualCtrl.lastErrorNorm) / dt;
    visualCtrl.lastErrorNorm = errorNorm;

    visualCtrl.filteredDerivative =
        VIS_D_FILTER_ALPHA * visualCtrl.filteredDerivative +
        (1.0f - VIS_D_FILTER_ALPHA) * derivative;

    // radialRate > 0 significa que el error se esta alejando del centro.
    // radialRate < 0 significa que ya se esta acercando al centro y el D frena.
    const float direction = (errorNorm >= 0.0f) ? 1.0f : -1.0f;
    const float radialRate = visualCtrl.filteredDerivative * direction;

    float effort =
        cfg.kp * fabsf(errorNorm) +
        cfg.kd * radialRate;

    effort = constrain(effort, 0.0f, cfg.maxPower);

    if (effort > 0.0f) {
        effort = max(cfg.minPower, effort);
    }

    return effort;
}

} // namespace

// ============================================================================
// BALLOON MISSION - MISMA MAQUINA DE ESTADOS DE LA VERSION LARGA VALIDADA
// ============================================================================
void BalloonMission::configure(int targetCount, bool stopAfterVisit) {
    targetCount_ = targetCount;
    stopAfterVisit_ = stopAfterVisit;
}

void BalloonMission::reset(AppContext& ctx) {
    visited_ = 0;
    closeFrames_ = 0;
    lostFrames_ = 0;
    searchInitialized_ = false;
    missionStartMs_ = millis();

    const float currentHeight = ctx.sensors[SensorMap::ALTITUDE];
    const float requestedHeight = ctx.command.params[AppConfig::PARAM_FZ];

    // P10/P11 reciben altura objetivo mediante --height -> PARAM_FZ.
    // P08/P09 conservan la altura actual solo como dato diagnostico.
    if (targetCount_ >= 2 &&
        isfinite(requestedHeight) &&
        requestedHeight > 0.05f) {
        searchHeight_ = requestedHeight;
    } else {
        searchHeight_ = currentHeight;
    }

    initialHeight_ = currentHeight;
    yawRef_ = ctx.sensors[SensorMap::YAW];

    altitudeCorrectionActive_ = false;
    altitudePauseStartMs_ = 0;

    resetAltitudeController(
        searchHeight_,
        ctx.sensors[SensorMap::VERTICAL_VELOCITY]);
    resetVisualController();

    enter(ctx, SEARCH);
}

bool BalloonMission::detected(const AppContext& ctx) const {
    const int flag = (int)ctx.sensors[SensorMap::NICLA_FLAG];
    return ((flag & 0x40) != 0) && ((flag & 0x03) != 0);
}

float BalloonMission::visitScore(const AppContext& ctx) const {
    return ctx.sensors[SensorMap::NICLA_W];
}

void BalloonMission::enter(AppContext& ctx, State s) {
    state_ = s;
    stateStartMs_ = millis();

    if (s == SEARCH) {
        closeFrames_ = 0;
        lostFrames_ = 0;
        lastSearchYaw_ = ctx.sensors[SensorMap::YAW];
        searchYawAccum_ = 0.0f;
        searchInitialized_ = true;
        resetVisualController();
    }
    else if (s == APPROACH) {
        resetVisualController();
    }
    else if (s == VISIT_CONFIRM) {
        closeFrames_ = 0;
        resetVisualController();
    }
    else if (s == WAIT_TARGET_LOST) {
        lostFrames_ = 0;
        resetVisualController();
    }
}

void BalloonMission::update(AppContext& ctx) {
    if (!ctx.robot) return;

    const float yaw = ctx.sensors[SensorMap::YAW];
    const float x = ctx.sensors[SensorMap::NICLA_X];
    const bool see = detected(ctx);
    const float score = visitScore(ctx);

    // Esta mision usa comandos fisicos directos ya validados.
    // No se usa el mixer heredado para evitar conflictos con los nuevos
    // controladores de esta capa.
    ctx.robot->PDterms.yawEn = false;
    ctx.robot->PDterms.zEn = false;

    const AltitudeConfig altitudeCfg = getAltitudeConfig(ctx);
    const VisualConfig visualCfg = getVisualConfig(ctx, targetCount_);

    // Seguro por defecto.
    float servo1Deg = SERVO1_Z_DEG;
    float servo2Deg = SERVO2_Z_DEG;
    float motorPower = 0.0f;
    float diagnosticFx = 0.0f;

    // Para SEARCH/WAIT_TARGET_LOST:
    // P08/P09 conservan 20%; P10/P11 toman --vis-max-power.
    const float searchTurnPower =
        (targetCount_ >= 2)
            ? visualCfg.maxPower
            : SINGLE_TARGET_SEARCH_POWER;

    // ========================================================================
    // SUPERVISOR DE ALTURA - SOLO P10/P11
    // ========================================================================
    // Conserva los mismos umbrales y la misma prioridad de la version larga:
    // cuando la altura se aleja demasiado, pausa temporalmente la mision.
    // Dentro de esa pausa, la subida ya no es ON/OFF: usa PID tuneable.
    // Si esta alto, sigue usando descenso pasivo porque aun no se ha validado
    // empuje activo hacia -Z.
    const float height = ctx.sensors[SensorMap::ALTITUDE];
    const float verticalVelocity =
        ctx.sensors[SensorMap::VERTICAL_VELOCITY];

    const bool altitudeControlEnabled =
        targetCount_ >= 2 &&
        isfinite(searchHeight_) &&
        isfinite(height) &&
        isfinite(verticalVelocity);

    const float heightError = searchHeight_ - height;

    if (state_ != DONE && altitudeControlEnabled) {
        const unsigned long nowMs = millis();

        if (!altitudeCorrectionActive_) {
            if (heightError > ALTITUDE_ENTER_LOW_M ||
                heightError < -ALTITUDE_ENTER_HIGH_M) {
                altitudeCorrectionActive_ = true;
                altitudePauseStartMs_ = nowMs;

                // Comienza la correccion desde cero para no heredar potencia
                // de una correccion anterior.
                resetAltitudeController(searchHeight_, verticalVelocity);
            }
        }
        else if (fabsf(heightError) <= ALTITUDE_EXIT_BAND_M) {
            // Congela los temporizadores del estado mientras la mision estuvo
            // pausada corrigiendo altura. Esto es importante en ESCAPE.
            if (altitudePauseStartMs_ != 0) {
                stateStartMs_ += nowMs - altitudePauseStartMs_;
            }

            altitudeCorrectionActive_ = false;
            altitudePauseStartMs_ = 0;
            resetAltitudeController(searchHeight_, verticalVelocity);
        }

        if (altitudeCorrectionActive_) {
            float altitudePower = 0.0f;

            if (heightError > ALTITUDE_EXIT_BAND_M) {
                // Estamos bajos: vector +Z validado + PID.
                altitudePower = calculateAltitudePower(
                    searchHeight_,
                    height,
                    verticalVelocity,
                    altitudeCfg);
            }
            else {
                // Estamos altos: descenso pasivo.
                altitudePower = 0.0f;
                resetAltitudeController(searchHeight_, verticalVelocity);
            }

            applyPhysicalCommand(ctx,
                                 SERVO1_Z_DEG,
                                 SERVO2_Z_DEG,
                                 altitudePower);

            // En F4, fx_cmd contiene error de altura durante esta correccion.
            Telemetry::sendControl(ctx,
                                   yawRef_,
                                   searchHeight_,
                                   heightError);

            Telemetry::sendMission(ctx,
                                   (int)state_,
                                   visited_,
                                   targetCount_,
                                   searchHeight_,
                                   score,
                                   (millis() - missionStartMs_) / 1000.0f);
            return;
        }
    }

    // ========================================================================
    // MAQUINA DE ESTADOS - LOGICA CONSERVADA DE LA VERSION PROBADA
    // ========================================================================
    switch (state_) {

    case SEARCH: {
        // Busca girando siempre a la derecha.
        servo1Deg = SERVO1_RIGHT_DEG;
        servo2Deg = SERVO2_RIGHT_DEG;
        motorPower = searchTurnPower;

        // Para P10/P11, despues de una visita no aceptamos inmediatamente
        // cualquier blob que reaparezca. Primero exigimos 45 deg adicionales.
        if (searchInitialized_ && isfinite(yaw)) {
            const float dyaw = wrapPi(yaw - lastSearchYaw_);
            searchYawAccum_ += fabsf(dyaw);
            lastSearchYaw_ = yaw;
        }

        const bool newTargetSeparationOk =
            (visited_ == 0) ||
            (searchYawAccum_ >= MIN_NEW_TARGET_YAW_RAD);

        if (see && newTargetSeparationOk) {
            yawRef_ = yaw;
            enter(ctx, APPROACH);
        }
        break;
    }

    case APPROACH:
        if (!see || !isfinite(x)) {
            // Si se pierde el globo, vuelve a buscar.
            enter(ctx, SEARCH);
            break;
        }
        else {
            const float visualErrorPx = x - IMAGE_CENTER_X;
            const float visualErrorNorm =
                constrain(visualErrorPx / IMAGE_CENTER_X,
                          -1.0f,
                          1.0f);

            diagnosticFx = visualErrorNorm;

            if (fabsf(visualErrorPx) > visualCfg.deadbandPx) {
                // Fuera de la banda central: correccion visual PD.
                motorPower = calculateVisualTurnPower(
                    visualErrorNorm,
                    visualCfg);

                if (visualErrorPx < 0.0f) {
                    // Globo a la izquierda -> giro izquierda.
                    servo1Deg = SERVO1_LEFT_DEG;
                    servo2Deg = SERVO2_LEFT_DEG;
                }
                else {
                    // Globo a la derecha -> giro derecha.
                    servo1Deg = SERVO1_RIGHT_DEG;
                    servo2Deg = SERVO2_RIGHT_DEG;
                }
            }
            else {
                // Ya esta centrado: reset del D y avance hacia el globo.
                resetVisualController(visualErrorNorm);

                servo1Deg = SERVO1_FORWARD_DEG;
                servo2Deg = SERVO2_FORWARD_DEG;
                motorPower = MOVE_POWER;

                // Solo intentamos confirmar visita si esta centrado y cerca.
                if (score >= VISIT_SCORE_THRESHOLD) {
                    enter(ctx, VISIT_CONFIRM);
                }
            }
        }
        break;

    case VISIT_CONFIRM: {
        // Frenamos durante la confirmacion para no seguir empujando al globo.
        servo1Deg = SERVO1_Z_DEG;
        servo2Deg = SERVO2_Z_DEG;
        motorPower = 0.0f;

        const bool centered =
            see &&
            isfinite(x) &&
            fabsf(x - IMAGE_CENTER_X) <= visualCfg.deadbandPx;

        if (centered && score >= VISIT_SCORE_THRESHOLD) {
            closeFrames_++;
        }
        else {
            closeFrames_ = 0;

            if (!see) {
                enter(ctx, SEARCH);
            }
            else {
                enter(ctx, APPROACH);
            }
        }

        if (closeFrames_ >= CLOSE_FRAMES_REQUIRED) {
            visited_++;

            // P08 termina justo al confirmar visita.
            // P09/P10/P11 ejecutan ESCAPE antes de decidir si terminan.
            if (stopAfterVisit_) {
                enter(ctx, DONE);
            }
            else {
                enter(ctx, ESCAPE);
            }
        }
        break;
    }

    case ESCAPE:
        // Retroceso corto tras visitar.
        servo1Deg = SERVO1_BACK_DEG;
        servo2Deg = SERVO2_BACK_DEG;
        motorPower = MOVE_POWER;

        if (millis() - stateStartMs_ >= ESCAPE_MS) {
            enter(ctx, WAIT_TARGET_LOST);
        }
        break;

    case WAIT_TARGET_LOST:
        // Gira a la derecha hasta perder obligatoriamente el globo anterior.
        servo1Deg = SERVO1_RIGHT_DEG;
        servo2Deg = SERVO2_RIGHT_DEG;
        motorPower = searchTurnPower;

        if (!see) {
            lostFrames_++;
        }
        else {
            lostFrames_ = 0;
        }

        if (lostFrames_ >= LOST_FRAMES_REQUIRED) {
            // P09: targetCount=1 -> termina.
            // P10/P11: si faltan globos -> vuelve a SEARCH.
            if (visited_ >= targetCount_) {
                enter(ctx, DONE);
            }
            else {
                enter(ctx, SEARCH);
            }
        }
        break;

    case DONE:
        servo1Deg = SERVO1_Z_DEG;
        servo2Deg = SERVO2_Z_DEG;
        motorPower = 0.0f;
        break;
    }

    applyPhysicalCommand(ctx,
                         servo1Deg,
                         servo2Deg,
                         motorPower);

    // P08-P11 se desarman automaticamente al llegar a DONE.
    if (state_ == DONE && ctx.robot->actuatorsAreArmed()) {
        ctx.robot->setActuatorsArmed(false);
    }

    // F4:
    //   height_ref = altura objetivo de mision
    //   fx_cmd     = error visual normalizado durante APPROACH
    Telemetry::sendControl(ctx,
                           yawRef_,
                           searchHeight_,
                           diagnosticFx);

    Telemetry::sendMission(ctx,
                           (int)state_,
                           visited_,
                           targetCount_,
                           searchHeight_,
                           score,
                           (millis() - missionStartMs_) / 1000.0f);
}