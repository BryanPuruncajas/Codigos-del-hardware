#include "mission/BalloonMission.h"
#include "app/AppConfig.h"
#include "app/ControlCommon.h"
#include "app/SensorMap.h"
#include "app/Telemetry.h"
#include <Arduino.h>
#include <math.h>

namespace {
// GEOMETRIA COMPARTIDA
// --------------------
// Este archivo tenia su propia copia de las posiciones de servo, con
// SERVO2_Z_DEG = 85. Ese valor quedo OBSOLETO el 23/08: la gondola izquierda
// esta desalineada 10 grados y su vertical real es 95. Con 85 el vehiculo
// giraba solo, una vuelta completa cada 13 segundos.
//
// Al tomar las constantes de ControlCommon.h, cualquier recalibracion futura
// de la geometria se aplica a TODOS los tests a la vez, en vez de tener que
// acordarse de tocar cinco archivos.
using ControlCommon::SERVO1_Z_DEG;
using ControlCommon::SERVO2_Z_DEG;
using ControlCommon::SERVO1_LEFT_DEG;
using ControlCommon::SERVO2_LEFT_DEG;
using ControlCommon::SERVO1_RIGHT_DEG;
using ControlCommon::SERVO2_RIGHT_DEG;
using ControlCommon::SERVO1_FORWARD_DEG;
using ControlCommon::SERVO2_FORWARD_DEG;
using ControlCommon::SERVO1_BACK_DEG;
using ControlCommon::SERVO2_BACK_DEG;


// ============================================================================
// CALIBRACION FISICA VALIDADA
// ============================================================================
// Valores obtenidos durante P00/P02/P03/P07.

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
// UMBRAL DE VISITA
// ----------------
// Se compara contra NICLA_W. OJO: el significado de ese campo CAMBIO el
// 23/08. Antes la Nicla mandaba int(10*val), o sea la CONFIANZA de deteccion
// multiplicada por 10: no eran pixeles, siempre salia cuadrado y llegaba a
// 259 en un frame de 240. Ahora manda un diametro equivalente en pixeles
// derivado del numero de celdas activas, que si escala con 1/distancia.
//
// Medido el 23/08 con el globo a la distancia de visitado: nicla_w = 62 px.
// El valor 60 queda en rango por casualidad, pero conviene revalidarlo:
// coloca el globo donde quieras que cuente como visitado, corre p06 y lee
// nicla_w.
//
// RECALIBRADO EL 24/08 con evidencia de vuelo real, no de mano quieta:
// la calibracion p06 sostenia el globo INMOVIL con la mano justo en la
// posicion de captura (ahi si dio mediana ~60). Pero en varios vuelos
// reales con centrado ya funcionando bien (deadband 30px, ver
// blimp_20260824_152908.csv), el score maximo que el blimp alcanza
// acercandose en vuelo -- con la inercia real del avance, no a mano --
// ronda 40-60 y casi nunca lo supera:
//   blimp_20260824_152908.csv: pico 45, bien centrado (x=121-133)
//   otros vuelos: picos de 39, 43, 60 (este ultimo justo antes de chocar)
// 60 pedia el maximo teorico que un acercamiento volado casi nunca toca.
// Bajado con margen bajo el pico tipico observado.
constexpr float VISIT_SCORE_THRESHOLD = 38.0f;

// SIGNO DE LA CONVERSION PIXEL -> YAW
// -----------------------------------
// Medido en vuelo el 23/08 (P07): la convencion de yaw del BNO085 va en
// sentido OPUESTO a la coordenada x de la imagen. Con signo positivo el
// vehiculo giraba ALEJANDOSE del objetivo (correlacion -0.67 entre error en
// pixeles y sentido de giro). Con -1 la correlacion pasa a +0.86 y la
// deteccion sube del 58% al 91%, porque el globo deja de escaparse del
// encuadre.
constexpr float VISION_YAW_SIGN = -1.0f;

// Campo de vision horizontal de la camara, en grados. Convierte pixeles a
// grados de giro. Medido el 23/08 comparando el centrado con 70 y 90: 70 da
// menor sobrepico (rango de nicla_x 142 px contra 186) y mejor error
// mediano (22 px contra 25).
constexpr float VISION_FOV_DEG = 70.0f;

// PERIODO DE ACTUALIZACION DE LA REFERENCIA VISUAL
// -----------------------------------------------
// Antes se detectaba "muestra nueva" comparando si nicla_x habia cambiado.
// Eso falla: nicla_x es un ENTERO y la Nicla repite valores, asi que un globo
// estacionado en el mismo pixel dejaba de generar correcciones aunque siguiera
// descentrado. El vehiculo se quedaba clavado con error permanente.
//
// Ahora la referencia se recalcula por TIEMPO, al ritmo real de la camara
// (~10 Hz medido, y el paper de MochiSwarm reporta lo mismo para la Nicla).
constexpr unsigned long VISION_UPDATE_MS = 100;

// AVANCE HACIA EL GLOBO
// ---------------------
// Antes esto inclinaba los servos a una FRACCION FIJA del recorrido hacia
// la posicion horizontal (0.50, elegida a mano) y compensaba la sustentacion
// perdida subiendo potencia. Reemplazado por
// ControlCommon::computeThrustAllocation(), que calcula el angulo exacto a
// partir de MOVE_POWER y altitudeDemand (ver la rama de avance en APPROACH,
// mas abajo) siguiendo la asignacion analitica de MochiSwarm (Xu et al.
// 2025, ecs. 4,5,9-11) en vez de una fraccion fija.
constexpr int CLOSE_FRAMES_REQUIRED = 3;
constexpr int LOST_FRAMES_REQUIRED = 8;
constexpr unsigned long ESCAPE_MS = 1800;

// Nicla no entrega ID de cada globo. Despues de una visita se exige girar
// al menos 45 grados adicionales antes de aceptar una nueva deteccion.
constexpr float MIN_NEW_TARGET_YAW_RAD = 45.0f * PI / 180.0f;

// SUAVIZADO DE LA REFERENCIA VISUAL DE YAW
// -----------------------------------------
// Detectado el 24/08 en blimp_20260824_102105.csv: nicla_x cruzaba todo el
// cuadro varias veces antes de perder el globo (197 -> 50 -> 77 -> 203 px)
// en vez de asentarse. La cascada de yaw (DEF_YAW_KP/KD) ya esta validada
// en P03/P05/P07 y es compartida por esos tests, asi que no se toca aca.
//
// El salto viene de ANTES de la cascada: cerca del globo, el mismo
// movimiento fisico del blimp se traduce en un desplazamiento de pixeles
// mucho mayor (efecto de campo cercano), asi que missionYawRef podia saltar
// de golpe entre dos muestras de 100ms y mandar al blimp a corregir de mas.
//
// En vez de saltar directo al offset calculado en cada muestra nueva, la
// referencia se mueve una FRACCION del camino hacia el objetivo (filtro
// exponencial sobre el propio setpoint, no sobre el error de control).
// Con 1.0 el comportamiento es identico al de antes (sin filtrar).
constexpr float VISION_REF_SMOOTH = 0.5f;

// PRIORIDAD DE YAW SOBRE ALTURA DURANTE EL CENTRADO EN APPROACH
// ---------------------------------------------------------------
// Idea planteada el 24/08: una vez que el blimp YA encontro el objetivo y
// esta en APPROACH centrando, tiene mas sentido terminar de centrarse bien
// que exprimir toda la autoridad de altura -- las correcciones de altura
// fuertes viajan montadas sobre el mismo tilt del yaw (mixOutputs hace
// power = max(pAlt, pYaw)), asi que un pAlt grande gira igual de fuerte
// aunque el yaw no lo haya pedido. Eso es lo que se vio como "vueltas
// fuertes por ratos" en blimp_20260824_103833.csv, coincidiendo siempre
// con motor alto por causa de altura, no de vision.
//
// Mientras esta centrando (SOLO en esta rama de APPROACH), la demanda de
// altura que entra al mixer se topa a una fraccion de su maximo. Fuera de
// aca -- SEARCH, WAIT_TARGET_LOST, avance ya centrado -- la altura sigue
// con autoridad completa, porque ahi no hay un centrado fino que proteger.
constexpr float APPROACH_ALT_POWER_FRACTION = 0.5f;

// MISMO PROBLEMA EN SEARCH/WAIT_TARGET_LOST
// -------------------------------------------
// Confirmado el 24/08: "gira muy rapido, no le da chance a la camara de ver
// bien el globo" durante SEARCH (sin target a la vista, girando ciego).
// La causa es identica a la de APPROACH_ALT_POWER_FRACTION: aca abajo,
// motorPower = max(searchTurnPower, altitudeDemand). Si la altura pide mas
// que --vis-max-power (bastante comun, la zona intermedia entre bandas del
// supervisor de emergencia no esta topada a nada), gana ese numero mas
// grande y el giro sale mas rapido de lo que se configuro, sin que
// --vis-max-power tenga ninguna influencia real.
//
// El supervisor de emergencia (altitudeCorrectionActive_, mas arriba) sigue
// intacto para desviaciones grandes: ese SI apunta los servos verticales y
// no gira nada. Esto de aca es solo la zona intermedia donde SEARCH sigue
// girando pero con algo de ayuda de altura.
constexpr float SEARCH_ALT_POWER_FRACTION = 0.5f;

// ============================================================================
// SUPERVISOR DE ALTURA P08-P11
// ============================================================================
// Recupera antes de que el error crezca demasiado. Ademas de la banda de
// posicion, se usa la velocidad vertical para anticipar una caida rapida.
constexpr float ALTITUDE_ENTER_LOW_M  = 0.05f;  // entra si faltan >5 cm
constexpr float ALTITUDE_ENTER_HIGH_M = 0.18f;  // arriba: descenso pasivo
constexpr float ALTITUDE_EXIT_BAND_M  = 0.07f;  // retorna a +/-7 cm

// Disparo predictivo: si esta cerca del SP pero ya cae rapido, el supervisor
// toma prioridad antes de que el blimp acumule demasiada velocidad hacia abajo.
constexpr float FALL_SPEED_TRIGGER_MPS = -0.12f;

// Para soltar la prioridad de altura, la caida debe estar practicamente frenada.
constexpr float FALL_SPEED_RELEASE_MPS = -0.05f;

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
// Subido de 0.12: con la potencia de hover medida en ~11% ese techo dejaba
// un solo punto de margen para subir.
constexpr float DEFAULT_ALT_MAX_POWER = 0.16f;
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

// Estado del lazo de altura, compartido con P04/P05 via ControlCommon.
ControlCommon::AltState missionAlt;

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
// Estado de la cascada de yaw. La mision ya no usa un PD de vision propio:
// la camara produce una REFERENCIA de yaw y esta cascada la sigue, cerrando
// sobre el giroscopio a 250 Hz. Son las mismas ganancias que P03/P05/P07.
ControlCommon::YawState missionYaw;
float missionYawRef = 0.0f;
bool  missionYawRefValid = false;
float missionLastNiclaX = -1.0f;
unsigned long missionYawLastUs = 0;
unsigned long missionLastVisionMs = 0;

void resetMissionYaw(float yaw) {
    ControlCommon::resetYawState(missionYaw, yaw);
    missionYawRef = isfinite(yaw) ? yaw : 0.0f;
    missionYawRefValid = false;
    missionLastNiclaX = -1.0f;
    missionYawLastUs = micros();
    missionLastVisionMs = 0;
}

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
                          float motor1Power,
                          float motor2Power) {
    motor1Power = constrain(motor1Power, 0.0f, 1.0f);
    motor2Power = constrain(motor2Power, 0.0f, 1.0f);

    const int servo1Us = degreesToPulseUs(servo1Deg);
    const int servo2Us = degreesToPulseUs(servo2Deg);

    if (ctx.robot->actuatorsAreArmed()) {
        ctx.robot->commandMotorPowerTest(motor1Power, motor2Power,
                                         servo1Us, servo2Us);
    }

    // F3 refleja exactamente lo ordenado por la mision.
    ctx.robot->servo_old1 = servo1Deg;
    ctx.robot->servo_old2 = servo2Deg;
    ctx.robot->motor_power1 = motor1Power;
    ctx.robot->motor_power2 = motor2Power;
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

    // El PID real de altura vive ahora en ControlCommon::AltState. Sin este
    // reset se heredaba el integral y la potencia de una correccion anterior
    // o de otro intento de mision, y el lazo arrancaba con un sesgo invisible.
    ControlCommon::resetAltState(missionAlt, rawVz);
}

float calculateAltitudePower(float reference,
                             float height,
                             float rawVz,
                             const AltitudeConfig& cfg) {
    // ------------------------------------------------------------------
    // MIGRADO A ControlCommon::computeAltDemand EL 23/08
    //
    // La mision tenia su PROPIO PID de altura, con las dos limitaciones que
    // ya habiamos corregido en ControlCommon.h pero que aqui nunca se
    // aplicaron:
    //
    //  1) ALT_I_POWER_MAX = 0.06. El integral topado en 6% cuando la
    //     potencia de hover medida es ~11%: no podia llegar ni a la mitad
    //     de lo necesario para sostenerse. Ese fue el mismo fallo que dejo
    //     al blimp sin despegar durante cuatro corridas de P04.
    //
    //  2) ALT_I_ACTIVE_ERROR_M estrecho. El integral se congelaba justo
    //     cuando el error era grande, o sea cuando mas falta hacia.
    //
    // Ahora usa el mismo lazo que P04 y P05: tope del integral en maxPower,
    // zona de integracion de 1.5 m y anti-windup por integracion condicional.
    // Las ganancias que sintonices en P04 valen aqui tal cual.
    // ------------------------------------------------------------------
    if (!isfinite(reference) || !isfinite(height) || !isfinite(rawVz)) {
        missionAlt.lastPower = 0.0f;
        return 0.0f;
    }

    const unsigned long nowUs = micros();
    float dt = (nowUs - altitudeCtrl.lastUs) * 1.0e-6f;
    altitudeCtrl.lastUs = nowUs;
    if (!isfinite(dt) || dt <= 0.0f || dt > 0.20f) {
        dt = 0.01f;
    }

    ControlCommon::updateVz(missionAlt, rawVz);

    ControlCommon::AltGains g;
    g.kp = cfg.kp;
    g.ki = cfg.ki;
    g.kd = cfg.kd;

    ControlCommon::AltLimits lim = ControlCommon::defaultAltLimits();
    lim.minPower = 0.0f;
    lim.maxPower = cfg.maxPower;
    lim.slew     = cfg.slewPerSec;

    const float demand = ControlCommon::computeAltDemand(
        g, lim, missionAlt, reference - height, dt);

    float power = ControlCommon::rateLimit(demand, missionAlt.lastPower,
                                           lim.slew, dt);
    power = constrain(power, 0.0f, lim.maxPower);
    missionAlt.lastPower = power;
    return power;
}

// ============================================================================
// CONFIGURACION PD VISUAL
// ============================================================================
VisualConfig getVisualConfig(const AppContext& ctx, int targetCount) {
    // MAPA DE SLOTS UNIFICADO para P08..P11.
    //
    // Antes habia DOS mapas distintos: P08/P09 leian vision de AUX0..AUX4 y
    // P10/P11 de AUX2..AUX4, porque los dos primeros huecos los ocupaba la
    // configuracion de altura. Eso obligaba a que la altura solo funcionara
    // con targetCount >= 2, y hacia que el mismo flag significara cosas
    // distintas segun la prueba.
    //
    // Ahora es uno solo:
    //   AUX0 = alt max power   AUX1 = alt slew
    //   AUX2 = vis min power   AUX3 = vis max power   AUX4 = vis deadband px
    //
    // kp y kd de vision ya no se usan: desde el 23/08 el centrado corre sobre
    // la cascada de yaw con las ganancias de ControlCommon.h, las mismas de
    // P03/P05/P07.
    (void)targetCount;

    const float minPower   = ctx.command.params[AppConfig::PARAM_AUX2];
    const float maxPower   = ctx.command.params[AppConfig::PARAM_AUX3];
    const float deadbandPx = ctx.command.params[AppConfig::PARAM_AUX4];

    const bool valid =
        isfinite(minPower) && minPower >= 0.0f &&
        isfinite(maxPower) && maxPower > 0.0f && maxPower <= 1.0f &&
        minPower <= maxPower &&
        isfinite(deadbandPx) && deadbandPx >= 1.0f && deadbandPx <= 100.0f;

    if (valid) {
        return {DEFAULT_VIS_KP, DEFAULT_VIS_KD,
                minPower, maxPower, deadbandPx};
    }

    return {DEFAULT_VIS_KP, DEFAULT_VIS_KD, DEFAULT_VIS_MIN_POWER,
            DEFAULT_VIS_MAX_POWER, DEFAULT_VIS_DEADBAND_PX};
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

    // TODAS las misiones P08..P11 reciben la altura objetivo mediante
    // --height -> PARAM_FZ.
    //
    // Antes esto exigia targetCount_ >= 2, asi que P08 y P09 (targetCount_=1)
    // caian al else y fijaban searchHeight_ a la altura ACTUAL. Si la mision
    // arrancaba desde el piso, el supervisor veia error ~0 y nunca ordenaba
    // el despegue: el blimp se quedaba en el suelo buscando globos.
    if (isfinite(requestedHeight) &&
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
    resetMissionYaw(ctx.sensors[SensorMap::YAW]);

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

    const float yawNow = ctx.sensors[SensorMap::YAW];

    if (s == SEARCH) {
        closeFrames_ = 0;
        lostFrames_ = 0;
        lastSearchYaw_ = yawNow;
        searchYawAccum_ = 0.0f;
        searchInitialized_ = true;
        resetMissionYaw(yawNow);
    }
    else if (s == APPROACH) {
        resetMissionYaw(yawNow);
    }
    else if (s == VISIT_CONFIRM) {
        closeFrames_ = 0;
        resetMissionYaw(yawNow);
    }
    else if (s == WAIT_TARGET_LOST) {
        lostFrames_ = 0;
        resetMissionYaw(yawNow);
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

    // Solo el avance en APPROACH usa potencias independientes por motor
    // (ver ControlCommon::computeThrustAllocation). El resto de los estados
    // siguen mandando la misma potencia a los dos motores a traves de
    // motorPower, como siempre.
    float motor1Power = 0.0f;
    float motor2Power = 0.0f;
    bool independentMotors = false;

    // Para SEARCH/WAIT_TARGET_LOST:
    // P08/P09 conservan 20%; P10/P11 toman --vis-max-power.
    const float searchTurnPower = visualCfg.maxPower;

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

    // El supervisor de altura corre en TODAS las misiones. Antes exigia
    // targetCount_ >= 2, asi que P08 y P09 volaban a la deriva vertical.
    // ------------------------------------------------------------------
    // DEMANDA DE ALTURA CONTINUA
    //
    // Antes la altura solo actuaba cuando el error pasaba de 10 cm, y en ese
    // caso PAUSABA la mision. Durante el centrado el mixer recibia demanda
    // vertical CERO, asi que el blimp caia mientras giraba: para cuando el
    // supervisor reaccionaba, ya habia perdido el globo de vista.
    //
    // Ahora la altura se calcula SIEMPRE y entra al mixer junto al yaw, que
    // es como lo hace P05 y como lo describe el paper de MochiSwarm en las
    // ecuaciones (9)-(10): las dos realimentaciones se FUSIONAN en una sola
    // fuerza deseada, no se alternan.
    float altitudeDemand = 0.0f;
    if (isfinite(searchHeight_) && isfinite(height) && isfinite(verticalVelocity)) {
        altitudeDemand = calculateAltitudePower(
            searchHeight_, height, verticalVelocity, altitudeCfg);
    }

    const bool altitudeControlEnabled =
        isfinite(searchHeight_) &&
        isfinite(height) &&
        isfinite(verticalVelocity);

    const float heightError = searchHeight_ - height;

    if (state_ != DONE && altitudeControlEnabled) {
        const unsigned long nowMs = millis();

        // ------------------------------------------------------------
        // RECUPERACION ANTICIPADA POR VELOCIDAD VERTICAL
        //
        // Antes solo se reaccionaba al superar el error de altura. En los
        // logs el blimp llegaba al umbral ya cayendo a ~0.25 m/s, por lo que
        // seguia perdiendo decenas de centimetros incluso con potencia maxima.
        // Ahora, si esta cerca del SP y ya cae rapido, altura toma prioridad.
        // ------------------------------------------------------------
        const bool fallingFastNearSetpoint =
            (heightError > -0.03f) &&
            (verticalVelocity < FALL_SPEED_TRIGGER_MPS);

        if (!altitudeCorrectionActive_) {
            if (heightError > ALTITUDE_ENTER_LOW_M ||
                fallingFastNearSetpoint ||
                heightError < -ALTITUDE_ENTER_HIGH_M) {

                altitudeCorrectionActive_ = true;
                altitudePauseStartMs_ = nowMs;

                // NO se resetea missionAlt aqui: conserva el integral de hover
                // aprendido durante toda la mision.
            }
        }
        else {
            // Para liberar la prioridad no basta con volver a la banda: la
            // velocidad de caida tambien debe estar practicamente controlada.
            const bool heightRecovered =
                fabsf(heightError) <= ALTITUDE_EXIT_BAND_M;

            const bool fallArrested =
                verticalVelocity >= FALL_SPEED_RELEASE_MPS;

            if (heightRecovered && fallArrested) {
                if (altitudePauseStartMs_ != 0) {
                    stateStartMs_ += nowMs - altitudePauseStartMs_;
                }

                altitudeCorrectionActive_ = false;
                altitudePauseStartMs_ = 0;
            }
        }

        if (altitudeCorrectionActive_) {
            // CONFIRMADO CON LOGS EL 24/08 (blimp_20260824_083554.csv):
            //
            // altitudeDemand YA incluye el termino D (-kd*filteredVz), que
            // anticipa la caida correctamente. Replay offline del PID sobre
            // el log real mostro que a h=1.86 (26 cm arriba del SP=1.60),
            // cayendo a -0.10 m/s, el PID ya pedia 13% de motor. El motor
            // real seguia en 0.000 hasta h=1.57 (1.6 s despues), porque este
            // bloque descartaba altitudeDemand con un "else { = 0.0f }"
            // cada vez que heightError > 0 y no se cumplia
            // fallingFastNearSetpoint (que solo dispara a <3 cm del SP).
            //
            // El PID ya sabe cuando NO hace falta empuje: si estas arriba y
            // quieto, kp*error domina y demand sale ~0 solo. Si estas arriba
            // pero cayendo fuerte, el termino D lo compensa y demand sube
            // solo, ANTES de cruzar el SP. No hace falta esta segunda
            // decision heuristica encima: confiamos en altitudeDemand
            // siempre que la correccion esta activa.
            const float altitudePower = altitudeDemand;

            applyPhysicalCommand(ctx,
                                 SERVO1_Z_DEG,
                                 SERVO2_Z_DEG,
                                 altitudePower,
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
        // Topado igual que en APPROACH: la altura puede ayudar un poco
        // mientras gira ciego, pero no puede acelerar el giro por encima
        // de lo que --vis-max-power definio.
        {
            const float altitudeDemandForSearch = fminf(
                altitudeDemand,
                altitudeCfg.maxPower * SEARCH_ALT_POWER_FRACTION);
            motorPower = (searchTurnPower > altitudeDemandForSearch)
                             ? searchTurnPower : altitudeDemandForSearch;
        }

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
                // ------------------------------------------------------------
                // CENTRADO VISUAL SOBRE LA CASCADA DE YAW
                //
                // Antes aqui habia un PD de vision propio que mandaba potencia
                // directa y ponia los servos al extremo. Dos problemas, ambos
                // medidos en vuelo el 23/08 sobre P07, que tenia el mismo
                // codigo:
                //
                //  1) EL SIGNO ESTABA INVERTIDO. El mapeo "error < 0 -> giro
                //     izquierda" hacia que el vehiculo se alejara del globo.
                //     Correlacion -0.67 entre error y sentido de giro.
                //
                //  2) EL DERIVATIVO SE CALCULABA SOBRE nicla_x, que llega a
                //     10 FPS. Derivar esa senal da basura.
                //
                // Ahora la vision produce una REFERENCIA de yaw y la cascada
                // la sigue con el giroscopio a 250 Hz. La referencia solo se
                // actualiza con deteccion NUEVA: entre medias el vehiculo
                // sigue apuntando al ultimo lugar conocido, asi que perder el
                // globo un instante ya no rompe el seguimiento.
                // ------------------------------------------------------------
                const unsigned long nowUs = micros();
                float dtYaw = (nowUs - missionYawLastUs) * 1.0e-6f;
                missionYawLastUs = nowUs;
                if (!isfinite(dtYaw) || dtYaw <= 0.0f || dtYaw > 0.20f) {
                    dtYaw = 0.01f;
                }

                const float measuredRate = ctx.sensors[SensorMap::YAW_RATE];
                const float yawRate = ControlCommon::updateYawRate(
                    missionYaw, yaw, measuredRate, dtYaw);

                // Solo recalcula la referencia con muestra nueva: la Nicla
                // repite el ultimo valor entre reportes.
                const unsigned long nowMs = millis();
                if (!missionYawRefValid ||
                    (nowMs - missionLastVisionMs) >= VISION_UPDATE_MS) {
                    missionLastVisionMs = nowMs;
                    missionLastNiclaX = x;
                    const float offsetRad = VISION_YAW_SIGN *
                        (visualErrorPx / (2.0f * IMAGE_CENTER_X)) *
                        VISION_FOV_DEG * ControlCommon::DEG2RAD;
                    const float rawTargetYaw =
                        ControlCommon::wrapPi(yaw + offsetRad);

                    if (!missionYawRefValid) {
                        // Primera muestra: sin referencia previa que
                        // suavizar, vamos directo al objetivo.
                        missionYawRef = rawTargetYaw;
                    } else {
                        // Filtro exponencial SOBRE EL SETPOINT: la
                        // referencia se mueve VISION_REF_SMOOTH del camino
                        // hacia el nuevo objetivo, no salta de golpe. Esto
                        // amortigua el efecto de campo cercano sin tocar
                        // las ganancias de la cascada (kp/kd), que siguen
                        // siendo las mismas validadas en P03/P05/P07.
                        const float refErr = ControlCommon::wrapPi(
                            rawTargetYaw - missionYawRef);
                        missionYawRef = ControlCommon::wrapPi(
                            missionYawRef + VISION_REF_SMOOTH * refErr);
                    }

                    missionYawRefValid = true;
                    yawRef_ = missionYawRef;
                }

                ControlCommon::YawGains yg;
                yg.kp = ControlCommon::DEF_YAW_KP;
                yg.kd = ControlCommon::DEF_YAW_KD;

                ControlCommon::YawLimits yl = ControlCommon::defaultYawLimits();
                yl.maxPower = visualCfg.maxPower;
                yl.minPower = visualCfg.minPower;
                yg.ki = yl.ki;

                const float yawErr =
                    ControlCommon::wrapPi(missionYawRef - yaw);

                const float yawCmd = ControlCommon::computeYawCommand(
                    yg, yl, missionYaw, yawErr, yawRate, dtYaw);

                // El mixer recibe la demanda de altura Y el mando de yaw.
                // power = max(pAlt, pYaw): ninguno de los dos lazos deja al
                // otro sin actuador, y el blimp deja de caer mientras centra.
                //
                // Mientras centra, topamos pAlt a una fraccion del maximo
                // (ver APPROACH_ALT_POWER_FRACTION): sigue sosteniendo
                // altura, pero ya no le impone al giro toda la fuerza de
                // una correccion de altura grande. Fuera de este bloque
                // (SEARCH, avance ya centrado) la altura sigue sin tope
                // extra.
                const float altitudeDemandForCenter = fminf(
                    altitudeDemand,
                    altitudeCfg.maxPower * APPROACH_ALT_POWER_FRACTION);

                float etaZ = 1.0f, etaYaw = 0.0f, blend = 0.0f;
                const float ceiling =
                    (altitudeCfg.maxPower > yl.maxPower)
                        ? altitudeCfg.maxPower : yl.maxPower;
                ControlCommon::mixOutputs(altitudeDemandForCenter, yawCmd, yl, ceiling,
                                          servo1Deg, servo2Deg, motorPower,
                                          etaZ, etaYaw, blend);
            }
            else {
                // Ya esta centrado: suelta la referencia de yaw y avanza.
                resetMissionYaw(yaw);

                // Asignacion analitica (ver ControlCommon::computeThrustAllocation):
                // calcula el angulo EXACTO de cada servo para el avance
                // (MOVE_POWER) y la altura (altitudeDemand) pedidos, en vez
                // de inclinar a una fraccion fija y despues inflar la
                // potencia para compensar la sustentacion perdida.
                ControlCommon::computeThrustAllocation(
                    MOVE_POWER, altitudeDemand, altitudeCfg.maxPower,
                    servo1Deg, servo2Deg, motor1Power, motor2Power);
                independentMotors = true;

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
        {
            const float altitudeDemandForSearch = fminf(
                altitudeDemand,
                altitudeCfg.maxPower * SEARCH_ALT_POWER_FRACTION);
            motorPower = (searchTurnPower > altitudeDemandForSearch)
                             ? searchTurnPower : altitudeDemandForSearch;
        }

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

    if (!independentMotors) {
        motor1Power = motorPower;
        motor2Power = motorPower;
    }
    applyPhysicalCommand(ctx,
                         servo1Deg,
                         servo2Deg,
                         motor1Power,
                         motor2Power);

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