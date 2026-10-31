#include "mission/BalloonMission.h"
#include "mission/MissionHeightOptions.h"
#include "sense/VerticalProfile.h"
#include "mission/BalloonCourse.h"
#include "app/AppConfig.h"
#include "app/ControlCommon.h"
#include "app/SensorMap.h"
#include "app/Telemetry.h"
#include <Arduino.h>
#include <math.h>

namespace {
struct MissionVertical {
    float height=0,vz=0;bool healthy=false,independent=false,rangeVelocity=false;uint32_t sampleUs=0;
};
MissionVertical verticalObservation(const AppContext& ctx){
    MissionVertical v;
    const auto& f=ctx.flowHeight;
    v.height=f.height;v.vz=f.vz;v.healthy=f.valid;
    v.sampleUs=f.sampleTimeUs;v.independent=true;
    v.rangeVelocity=f.source==FlowHeight::FLOW_RANGE && f.valid;
    return v;
}
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


// ============================================================================
// CALIBRACION FISICA VALIDADA
// ============================================================================
// Valores obtenidos durante P00/P02/P03/P07.

// ============================================================================
// VISION VALIDADA EN P06/P07
// ============================================================================
constexpr float IMAGE_CENTER_X = 120.0f;
constexpr float DEFAULT_VIS_DEADBAND_PX = 15.0f;  // 105..135 px
// One-waypoint profile: brake rotation earlier; authority/pins unchanged.
constexpr float N03_APPROACH_YAW_KP = .35f;
constexpr uint32_t N03_APPROACH_TIMEOUT_MS = 180000; // host120s plus preparation margin
constexpr uint32_t N03_RECOVERY_MS=15000;

// ============================================================================
// POTENCIAS DE MOVIMIENTO DE MISION
// ============================================================================
// Avance y escape comparten esta potencia. Bajado de 0.20 a pedido del
// usuario (04/09): en vuelo se sentia demasiado fuerte tanto yendo hacia el
// globo como retrocediendo despues de visitarlo.
constexpr float MOVE_POWER = 0.15f;

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
// Se compara contra NICLA_W_VALUE, el tamano de la deteccion enviado en F2.
// OJO: el significado de ese campo CAMBIO el
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
// New detector reports bounding-box width, not the historical equivalent diameter.
// User-selected provisional reference 2026-10-28: about 80 cm camera-to-surface,
// about 120 px wide. Latest manual trial: initial 80 cm 121..134 px;
// return 80 cm 93..101 px. Size is orientation/segmentation dependent,
// so 120 px is an approximate operational reference, not a ranging model.
// This is estimated arrival, NOT a physical contact measurement.
// Define zero explicitly to disable automatic arrival.
#ifndef N03_VISIT_WIDTH_PX
#define N03_VISIT_WIDTH_PX 120
#endif

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
constexpr unsigned long TURN_SETTLE_MS = 300;
constexpr unsigned long TURN_TIMEOUT_MS = 20000;
constexpr float TURN_TOLERANCE_RAD = 5.0f * PI / 180.0f;
constexpr float TURN_RATE_TOLERANCE = 0.10f;

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
// aca -- SEARCH, giro al siguiente, avance ya centrado -- la altura sigue
// con autoridad completa, porque ahi no hay un centrado fino que proteger.
constexpr float APPROACH_ALT_POWER_FRACTION = 0.5f;

// MISMO PROBLEMA EN SEARCH
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
// Subido de 0.05 a 0.10 (04/09): en blimp_20260904_143155.csv, varios saltos
// APPROACH->SEARCH coincidian con la altura apenas 6-7cm bajo el setpoint --
// dentro de lo que ya se ve como variacion normal del lazo (ver P11, score
// nunca llegaba a VISIT_SCORE_THRESHOLD porque el supervisor tomaba el
// control justo al acercarse y el globo se salia de cuadro mientras subia
// derecho). Con 10cm el supervisor sigue entrando ante caidas reales, pero
// deja de interrumpir el centrado por ruido normal de +-6-7cm.
constexpr float ALTITUDE_ENTER_LOW_M  = 0.10f;  // entra si faltan >10 cm
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

void resetAltitudeController(float reference, float rawVz, float maxPower) {
    altitudeCtrl.initialized = true;
    altitudeCtrl.filteredVz = isfinite(rawVz) ? rawVz : 0.0f;
    altitudeCtrl.integralPower = 0.0f;
    altitudeCtrl.lastPower = 0.0f;
    altitudeCtrl.lastReference = reference;
    altitudeCtrl.lastUs = micros();

    // El PID real de altura vive ahora en ControlCommon::AltState. Sin este
    // reset se heredaba el integral y la potencia de una correccion anterior
    // o de otro intento de mision, y el lazo arrancaba con un sesgo invisible.
    ControlCommon::resetAltState(missionAlt, rawVz, maxPower);
}

float getN03PowerCeiling(const AppContext& ctx) {
    return fminf(AdaptiveLift::HARD_MAX,getAltitudeConfig(ctx).maxPower);
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
    const float deadbandPx = MissionHeightOptions::decode(ctx.command.params[AppConfig::PARAM_AUX4]).deadband;

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
void BalloonMission::configure(int targetCount, bool stopAfterVisit, int track) {
    // Four-waypoint course closes at 1: five confirmed contacts, including start.
    targetCount_ = targetCount == 4 ? 5 : targetCount;
    track_ = track == 2 ? 2 : 1;
    stopAfterVisit_ = stopAfterVisit;
}

void BalloonMission::reset(AppContext& ctx) {
    // Entering/resetting a mission is not the same as requesting another
    // waypoint. Keep an already confirmed, fresh N03 target in front of us.
    // Otherwise a valid PARTIAL is erased and SEARCH starts a blind turn.
    const auto& feedback = ctx.robot->sensorsuite.niclaFeedback();
    const bool keepTarget = feedback.protocol == NiclaFeedback::N03_PROTOCOL &&
        !feedback.resetPending && detected(ctx);
    visualHeight_.reset();
    visualHeightSeen_ = false;
    visualHeightLastSeenMs_ = millis();
    visualHeightPower_ = 0;
    visualHeightLastUs_ = micros();
    visited_ = 0;
    closeFrames_ = 0;
    turnSettled_ = false;
    // Consume the retained observation once on the first update, even when
    // no new UART frame has arrived since reset (also valid at uint32 wrap).
    lastVisionGeneration_ = feedback.generation - (keepTarget ? 1U : 0U);
    if (!keepTarget) ctx.robot->sensorsuite.resetNiclaTarget();
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
    // Relative barometric readings can legitimately be negative or zero.
    // The host supplies an explicit height; do not silently replace its target.
    if (isfinite(requestedHeight)) {
        searchHeight_ = requestedHeight;
    } else {
        searchHeight_ = currentHeight;
    }

    initialHeight_ = currentHeight;
    visualHeightAnchor_ = currentHeight;
    yawRef_ = ctx.sensors[SensorMap::YAW];
    courseYaw_ = yawRef_; // At RESET align the vehicle parallel to 1 -> 2.
    routeYawRef_ = courseYaw_;
    lastVisitedArea_ = 0;
    Serial.printf("[COURSE] track=%d reference_yaw=%.3f contacts=%d arrival_calibrated=%u target_preserved=%u\n",
                  track_, courseYaw_, targetCount_, (unsigned)(N03_VISIT_WIDTH_PX > 0),
                  (unsigned)keepTarget);

    altitudeCorrectionActive_ = false;
    altitudePauseStartMs_ = 0;

    resetAltitudeController(
        searchHeight_,
        ctx.sensors[SensorMap::VERTICAL_VELOCITY],
        getAltitudeConfig(ctx).maxPower);
    resetMissionYaw(ctx.sensors[SensorMap::YAW]);

    enter(ctx, keepTarget ? APPROACH : SEARCH);
}

bool BalloonMission::detected(const AppContext& ctx) const {
    const auto& feedback = ctx.robot->sensorsuite.niclaFeedback();
    if (!feedback.fresh) return false;
    if (feedback.protocol == NiclaFeedback::N03_PROTOCOL) {
        // N03 emite PARTIAL solo con tracker confirmado. Permitir tambien
        // adquisicion cercana inicial; nunca implica visita/contacto.
        const bool usableState = feedback.state == NiclaFeedback::FULL ||
            feedback.state == NiclaFeedback::PARTIAL;
        const float x = ctx.sensors[SensorMap::NICLA_X_VALUE];
        const float y = ctx.sensors[SensorMap::NICLA_Y_VALUE];
        const float w = ctx.sensors[SensorMap::NICLA_W_VALUE];
        const float h = ctx.sensors[SensorMap::NICLA_H_VALUE];
        const int flag = (int)ctx.sensors[SensorMap::NICLA_FLAG];
        return usableState && (flag == 0x41 || flag == 0x42) &&
            isfinite(x) && x >= 0 && x < 240 && isfinite(y) && y >= 0 && y < 160 &&
            isfinite(w) && w > 0 && w <= 240 && isfinite(h) && h > 0 && h <= 160;
    }
    const int flag = (int)ctx.sensors[SensorMap::NICLA_FLAG];
    const float width = ctx.sensors[SensorMap::NICLA_W_VALUE];
    return ((flag & 0x40) != 0) && ((flag & 0x03) != 0) &&
           isfinite(width) && width > 0.0f;
}

bool BalloonMission::resumeFromManual(AppContext& ctx,uint32_t pausedMs){
    if(state_==DONE || visited_>=targetCount_)return false;
    missionStartMs_+=pausedMs;
    const bool routeTurn=state_==TURN_TO_NEXT || state_==ACQUIRE_NEXT;
    closeFrames_=0;advanceGate_.reset();impulse_.cancel(millis());
    // Keep courseYaw_, visited_, track_ and routeYawRef_. Rebuild transient
    // controllers at the operator's new pose rather than using old integrals.
    enter(ctx,routeTurn?TURN_TO_NEXT:(detected(ctx)?APPROACH:SEARCH));
    return true;
}

bool BalloonMission::manualWaypointClear(AppContext& ctx){
    if(visited_>=targetCount_ || state_==DONE)return false;
    const float w=ctx.sensors[SensorMap::NICLA_W_VALUE],h=ctx.sensors[SensorMap::NICLA_H_VALUE];
    const float area=isfinite(w)&&isfinite(h)&&w>0&&h>0?w*h:0;
    // Manual acknowledgement supplies progress, not an optical contact or
    // distance measurement. Keep a conservative next-target size heuristic.
    lastVisitedArea_=fmaxf(area,N03_VISIT_WIDTH_PX*N03_VISIT_WIDTH_PX);
    ++visited_;closeFrames_=0;advanceGate_.reset();impulse_.cancel(millis());
    if(stopAfterVisit_ || visited_>=targetCount_)enter(ctx,DONE);
    else{
        routeYawRef_=wrapPi(courseYaw_+BalloonCourse::heading(track_,visited_-1));
        enter(ctx,TURN_TO_NEXT);
    }
    Serial.printf("[COURSE] waypoint_cleared=%d manual=1\n",visited_);
    return true;
}

void BalloonMission::reportPaused(AppContext& ctx,uint32_t pausedAt){
    Telemetry::sendMission(ctx,int(state_),visited_,targetCount_,searchHeight_,visitScore(ctx),
        uint32_t(pausedAt-missionStartMs_)/1000.f);
}

float BalloonMission::visitScore(const AppContext& ctx) const {
    return ctx.sensors[SensorMap::NICLA_W_VALUE];
}

void BalloonMission::enter(AppContext& ctx, State s) {
    advanceGate_.reset();
    largeTargetRejected_ = false;
    state_ = s;
    stateStartMs_ = millis();

    const float yawNow = ctx.sensors[SensorMap::YAW];

    if (s == SEARCH) {
        closeFrames_ = 0;
        lastSearchYaw_ = yawNow;
        searchYawAccum_ = 0.0f;
        searchInitialized_ = true;
        resetMissionYaw(yawNow);
        initialSearch_.reset(yawNow);
        initialHeartbeat_=initialReset_=millis();
        approachMixer_.reset(true);
        ControlCommon::resetYawState(approachYaw_,yawNow);
        approachUs_=micros();
    }
    else if (s == APPROACH) {
        resetMissionYaw(yawNow);
        approachBase_=AdaptiveLift::START_TRIM;
        approachSupport_.reset(approachBase_,millis(),AdaptiveLift::HARD_MAX);
        approachBearing_=VisualBearing();
        approachMixer_.reset(true);
        ControlCommon::resetYawState(approachYaw_,yawNow);
        approachUs_=micros();approachHadFull_=false;approachNearMemory_=false;
        approachAssociationEpoch_=uint8_t(ctx.robot->sensorsuite.niclaFeedback().sample>>8);
        impulse_.reset();
        approachMemory_.reset();
        approachReacquiring_=false;approachLastReset_=0;approachHeartbeat_=millis();
        approachGeneration_=ctx.robot->sensorsuite.niclaFeedback().generation-1U;
    }
    else if (s == VISIT_CONFIRM) {
        closeFrames_ = 0;
        resetMissionYaw(yawNow);
    }
    else if (s == TURN_TO_NEXT || s == ACQUIRE_NEXT) {
        // Barometer is only a short-term bridge while the next balloon is unseen.
        const float heightNow = ctx.sensors[SensorMap::ALTITUDE];
        if (isfinite(heightNow)) visualHeightAnchor_ = searchHeight_ = heightNow;
        yawRef_ = routeYawRef_;
        turnSettled_ = false;
        resetMissionYaw(yawNow);
    }
}

void BalloonMission::holdRouteYaw(AppContext& ctx, float altitudeDemand, float maxPower,
                                 float minPower, float altitudeMax, float& servo1,
                                 float& servo2, float& motorPower) {
    const unsigned long nowUs = micros();
    float dt = (nowUs - missionYawLastUs) * 1.0e-6f;
    missionYawLastUs = nowUs;
    if (!isfinite(dt) || dt <= 0 || dt > 0.20f) dt = 0.01f;
    const float yaw = ctx.sensors[SensorMap::YAW];
    const float rate = ControlCommon::updateYawRate(
        missionYaw, yaw, ctx.sensors[SensorMap::YAW_RATE], dt);
    auto limits = ControlCommon::defaultYawLimits();
    limits.maxPower = maxPower;
    limits.minPower = minPower;
    limits.deadbandRad = TURN_TOLERANCE_RAD;
    ControlCommon::YawGains gains;
    gains.kp = ControlCommon::DEF_YAW_KP;
    gains.kd = ControlCommon::DEF_YAW_KD;
    gains.ki = limits.ki;
    const float command = ControlCommon::computeYawCommand(
        gains, limits, missionYaw, wrapPi(yawRef_ - yaw), rate, dt);
    float etaZ, etaYaw, blend;
    ControlCommon::mixOutputs(altitudeDemand, command, limits, fmaxf(altitudeMax, maxPower),
                             servo1, servo2, motorPower, etaZ, etaYaw, blend);
}

void BalloonMission::reportDecision(AppContext& ctx, int reason, float forwardRequest) {
    // Acquisition blockers are evaluated from the same conditions as control,
    // even between camera samples. Do not infer them in the dashboard.
    if (state_ == ACQUIRE_NEXT && reason != 15) {
        const auto& feedback = ctx.robot->sensorsuite.niclaFeedback();
        const float yawNow = ctx.sensors[SensorMap::YAW];
        if (largeTargetRejected_ && uint32_t(millis() - largeTargetRejectedMs_) < 1500) reason = 13;
        else if (!feedback.fresh || feedback.resetPending) reason = 14;
        else if (isfinite(yawNow) && fabsf(wrapPi(yawRef_ - yawNow)) > TURN_TOLERANCE_RAD) reason = 11;
        else if (!detected(ctx)) reason = 4;
        else if (feedback.protocol == NiclaFeedback::N03_PROTOCOL && feedback.state != NiclaFeedback::FULL) reason = 12;
        else {
            const float area = ctx.sensors[SensorMap::NICLA_W_VALUE] * ctx.sensors[SensorMap::NICLA_H_VALUE];
            reason = BalloonCourse::smallerNextTarget(area, lastVisitedArea_) ? 4 : 13;
        }
    }
    const float yaw = ctx.sensors[SensorMap::YAW];
    const float error = isfinite(yaw) && isfinite(yawRef_)
        ? wrapPi(yawRef_ - yaw) * ControlCommon::RAD2DEG : NAN;
    Telemetry::sendMissionDecision(ctx, reason, forwardRequest, error,
        state_ == DONE ? 0 : BalloonCourse::waypoint(track_, visited_),
        float(advanceGate_.stableMs(millis())), ctx.mode == AppConfig::P12_VISION_BENCH);
}

void BalloonMission::updateN03Search(AppContext& ctx) {
    const uint32_t now=millis(),us=micros();
    float dt=uint32_t(us-approachUs_)*1.e-6f;approachUs_=us;
    if(!isfinite(dt)||dt<=0||dt>.2f)dt=.01f;
    const auto& feedback=ctx.robot->sensorsuite.niclaFeedback();
    const auto vertical=verticalObservation(ctx);
    const float yaw=ctx.sensors[SensorMap::YAW],rate=ctx.sensors[SensorMap::YAW_RATE];
    const bool armed=ctx.robot->actuatorsAreArmed();
    const float maxPower=getN03PowerCeiling(ctx);
    const float motorFloor=fminf(.10f,maxPower);
    const float supportLimit=maxPower*fminf(1.f,approachMixer_.etaZ);
    const float supportFloor=0.f; // hover may require less than a fixed6% lift command
    ctx.robot->PDterms.yawEn=false;ctx.robot->PDterms.zEn=false;
    if(feedback.fresh)initialHeartbeat_=now;
    const bool badLink=uint32_t(now-initialHeartbeat_)>=1500;
    if(armed && (badLink || !isfinite(yaw) || !isfinite(rate) || (vertical.independent && !vertical.healthy)))initialSearch_.failed=true;
    if(armed && !initialSearch_.failed && !feedback.resetPending && detected(ctx)){
        // Nicla already confirmed FULL/PARTIAL across multiple camera frames.
        // Stop scanning immediately. Transfer the slewed motor/servo commands
        // and learned support rather than restarting at15% or zero thrust.
        const auto mixer=approachMixer_;
        const auto yawState=approachYaw_;
        const float base=initialSearch_.started?initialSearch_.trim:AdaptiveLift::START_TRIM;
        enter(ctx,APPROACH);
        approachBase_=base;approachSupport_.reset(base,now,AdaptiveLift::HARD_MAX);
        approachMixer_=mixer;approachYaw_=yawState;
        updateN03Approach(ctx);return;
    }
    const bool confirming=feedback.fresh && feedback.state==NiclaFeedback::CONFIRM;
    initialSearch_.inertia.learningAllowed=!ctx.flowHeight.holding;
    initialSearch_.update(now,armed,vertical.height,
        vertical.vz,vertical.healthy,
        supportLimit,supportFloor,confirming,vertical.sampleUs,vertical.rangeVelocity);
    float yawCommand=0;
    if(initialSearch_.failed){
        initialSearch_.demand=0;
        enter(ctx,TURN_FAILED);
        applyPhysicalCommand(ctx,approachMixer_.servo1,approachMixer_.servo2,0,0);
        ctx.robot->setActuatorsArmed(false);
    }else if(!armed){
        applyPhysicalCommand(ctx,SupportYaw::VISUAL_BASE1_DEG,SupportYaw::VISUAL_BASE2_DEG,0,0);
    }else{
        yawRef_=confirming?yaw:initialSearch_.yawReference;
        const float yawRate=ControlCommon::updateYawRate(approachYaw_,yaw,rate,dt);
        ControlCommon::YawGains gains;gains.kp=.35f;gains.ki=.08f;gains.kd=3.f;
        auto limits=ControlCommon::defaultYawLimits();limits.authority=.25f;
        limits.deadbandRad=ControlCommon::DEG2RAD;
        yawCommand=SupportYaw::command(gains,limits,approachYaw_,wrapPi(yawRef_-yaw),yawRate,dt);
        approachMixer_.update(initialSearch_.demand,yawCommand,.25f,0,maxPower,
            dt,fminf(motorFloor,initialSearch_.inertia.motorFloor),0,0,true);
        applyPhysicalCommand(ctx,approachMixer_.servo1,approachMixer_.servo2,
            approachMixer_.power,approachMixer_.power);
        if(feedback.fresh && !feedback.resetPending && feedback.state==NiclaFeedback::LOST &&
           uint32_t(now-initialReset_)>=3000){
            ctx.robot->sensorsuite.resetNiclaTarget();initialReset_=now;
        }
    }
    reportDecision(ctx,initialSearch_.failed?10:4,0);
    Telemetry::sendVisualSupport(ctx,-1,0,initialSearch_.trim,initialSearch_.demand,yawCommand,
        initialSearch_.failed?VisualSupport::FAULT:(armed?5:VisualSupport::DISARMED));
    Telemetry::sendVisualBearing(ctx,-1,yawRef_,wrapPi(yawRef_-yaw),false,VisualBearing::VERSION,0);
    Telemetry::sendImpulse(ctx,5,initialSearch_.started?float(uint32_t(now-initialSearch_.since)):0,
        0,yawCommand,0,float(VisualSearch::TIMEOUT_MS));
    const auto& motion=initialSearch_.inertia;
    const int flags=ctx.robot->actuatorsAreArmed()?((motion.velocityTrusted?1:0)|(motion.brakingUp?4:0)|(motion.brakingDown?8:0)|
        (motion.evidence.trusted?64:0)|(motion.visualCorroborated?128:0)|(motion.evidence.fastTrusted?256:0)):0;
    Telemetry::sendInertia(ctx,motion.rate,motion.predictedRate,motion.desiredRate,motion.motorFloor,flags,motion.velocityConfidence);
    Telemetry::sendControl(ctx,yawRef_,searchHeight_,0);
    Telemetry::sendMission(ctx,int(state_),visited_,targetCount_,searchHeight_,0,
        uint32_t(now-missionStartMs_)/1000.f);
}

void BalloonMission::updateN03Approach(AppContext& ctx) {
    const uint32_t now=millis(),us=micros();
    float dt=uint32_t(us-approachUs_)*1.e-6f;approachUs_=us;
    if(!isfinite(dt)||dt<=0||dt>.2f)dt=.01f;
    const auto& feedback=ctx.robot->sensorsuite.niclaFeedback();
    const auto vertical=verticalObservation(ctx);
    const float yaw=ctx.sensors[SensorMap::YAW],rate=ctx.sensors[SensorMap::YAW_RATE];
    const float score=visitScore(ctx);
    const float maxPower=getN03PowerCeiling(ctx);
    const float motorFloor=fminf(.10f,maxPower);
    const float supportLimit=maxPower*fminf(1.f,approachMixer_.etaZ);
    const float supportFloor=0.f; // visual overshoot must be able to unload lift to ESC idle
    // Nicla7 can recognise again after any duration. A fresh association is
    // not automatically the old mission waypoint: retain world references,
    // stop advancing, and require a FULL within the expected bearing gate.
    const uint8_t associationEpoch=uint8_t(feedback.sample>>8);
    if(feedback.fresh && associationEpoch!=approachAssociationEpoch_){
        approachAssociationEpoch_=associationEpoch;approachReacquiring_=true;
        approachHadFull_=false;approachNearMemory_=false;
        approachMemory_.reset();
        advanceGate_.reset();impulse_.cancel(now);closeFrames_=0;
        if(state_==VISIT_CONFIRM){state_=APPROACH;stateStartMs_=now;}
    }
    VisualSupport::Observation o;
    o.fresh=feedback.fresh && !feedback.resetPending;
    o.confirmed=detected(ctx);o.full=feedback.state==NiclaFeedback::FULL;
    o.x=ctx.sensors[SensorMap::NICLA_X_VALUE];o.y=ctx.sensors[SensorMap::NICLA_Y_VALUE];
    o.w=ctx.sensors[SensorMap::NICLA_W_VALUE];o.h=ctx.sensors[SensorMap::NICLA_H_VALUE];
    o.near=approachHadFull_ && feedback.state==NiclaFeedback::PARTIAL &&
        (o.w>=120.f || o.h>=112.f); // large crop, not every distant partial
    // N03 sends measured blob bounds in ROI channels. Keep them separate
    // from the true pixel centroid, which may shift inside a clipped patch.
    if(ctx.sensors[SensorMap::NICLA_W]==o.w && ctx.sensors[SensorMap::NICLA_H]==o.h){
        o.left=ctx.sensors[SensorMap::NICLA_X];o.top=ctx.sensors[SensorMap::NICLA_Y];
    }
    o.generation=feedback.generation;
    bool rejectReacquisition=false;
    // After a tracker restart retain the old world bearing. Do not jump to a
    // candidate far from the missing waypoint just because it is confirmed.
    if(approachReacquiring_ && o.valid() && approachBearing_.ready){
        const float candidate=wrapPi(yaw-(o.x-120.f)/240.f*(70.f*ControlCommon::DEG2RAD));
        rejectReacquisition=!o.full || !isfinite(candidate) ||
            fabsf(wrapPi(candidate-approachBearing_.reference))>20.f*ControlCommon::DEG2RAD;
        if(rejectReacquisition)o.confirmed=false;
        else approachReacquiring_=false;
    }
    // New camera keeps searching autonomously. R is only a legacy LOST rescue
    // or a request to release a rejected association, not a recovery deadline.
    if(ctx.robot->actuatorsAreArmed() && !approachNearMemory_ && approachSupport_.seen &&
       uint32_t(now-approachSupport_.lastSeen)>=5200 && feedback.fresh && !feedback.resetPending &&
       uint32_t(now-approachLastReset_)>=3000 &&
       (feedback.state==NiclaFeedback::LOST || rejectReacquisition)){
        approachReacquiring_=true;approachLastReset_=now;
        approachHadFull_=false; // an explicit tracker reset loses association proof
        ctx.robot->sensorsuite.resetNiclaTarget();
        o.fresh=false;o.confirmed=false;
    }
    const bool newSample=o.valid() && o.generation!=approachGeneration_;
    float forward=0;
    float appliedYaw=0;
    int decision=6;
    bool headingLimited=false;
    bool estimatedContinuation=false;
    if(!ctx.robot->actuatorsAreArmed()){
        impulse_.reset();
        closeFrames_=0;advanceGate_.reset();
        applyPhysicalCommand(ctx,SupportYaw::VISUAL_BASE1_DEG,SupportYaw::VISUAL_BASE2_DEG,0,0);
    }else{
        ctx.robot->PDterms.yawEn=false;ctx.robot->PDterms.zEn=false;
        const auto heightOptions=MissionHeightOptions::decode(ctx.command.params[AppConfig::PARAM_AUX4]);
        if(approachSupport_.independentVelocity!=vertical.rangeVelocity){
            approachSupport_.inertia.evidence.reset();approachSupport_.inertia.rateReady=false;
        }
        approachSupport_.selectIndependentHeight(vertical.independent,now);
        approachSupport_.independentVelocity=vertical.rangeVelocity;
        approachSupport_.fixedMode=true;
        approachSupport_.runReleased=heightOptions.valid && heightOptions.released;
        approachSupport_.advanceEnabled=heightOptions.valid && heightOptions.advance;
        approachSupport_.offsetRequest=heightOptions.offset;
        approachSupport_.levelPitch=heightOptions.levelPitch;
        if(!heightOptions.valid || (vertical.independent && !vertical.healthy))approachSupport_.stop();
        approachSupport_.inertia.learningAllowed=!impulse_.neutralOnly() && !ctx.flowHeight.holding;
        approachSupport_.update(o,now,approachBase_,supportLimit,false,supportFloor,
            vertical.height,vertical.vz,
            vertical.healthy,ctx.sensors[SensorMap::PITCH],
            N03_APPROACH_TIMEOUT_MS,true,N03_RECOVERY_MS,vertical.sampleUs,rate);
        // Visual occlusion is different from loss of camera communication.
        if(feedback.fresh)approachHeartbeat_=now;
        if(uint32_t(now-approachHeartbeat_)>=1500)approachSupport_.stop();
        if(!isfinite(yaw)||!isfinite(rate))approachSupport_.stop();
        if(approachSupport_.failed){
            impulse_.reset();
            enter(ctx,TURN_FAILED);decision=10;
            applyPhysicalCommand(ctx,approachMixer_.servo1,approachMixer_.servo2,0,0);
            ctx.robot->setActuatorsArmed(false);
        }else{
            if(newSample){
                approachGeneration_=o.generation;
                if(o.near)approachBearing_.updateNear(o.boxLeft(),o.w,now);
                else approachBearing_.update(yaw,o.x,now);
                approachHadFull_=approachHadFull_||o.full;
                if(o.full && (o.w>=80 || o.h>=64))approachNearMemory_=true;
                else if(o.full && o.w<60 && o.h<48)approachNearMemory_=false;
                visualHeightSeen_=true;visualHeightLastSeenMs_=now;
                approachMemory_.observe(now,associationEpoch,vertical.height,
                    vertical.healthy);
            }
            const bool recentPulse=impulse_.phase==ImpulseApproach::PUSH && approachMemory_.ready &&
                uint32_t(now-approachMemory_.lastSeen)<=ApproachProfile::OCCLUSION_MS;
            yawRef_=approachBearing_.ready ? wrapPi(approachBearing_.reference+
                (recentPulse?0.f:approachSupport_.recoveryYawOffset(now))) : yaw;
            recoveryYaw_=approachBearing_.ready?approachBearing_.reference:yaw;
            if(approachSupport_.altitudeReady)
                visualHeightAnchor_=searchHeight_=approachSupport_.fixedTarget();
            visualHeight_=approachSupport_.height;
            missionAlt.integral=approachSupport_.height.trim;
            const float yawError=wrapPi(yawRef_-yaw);
            const float yawRate=ControlCommon::updateYawRate(approachYaw_,yaw,rate,dt);
            ControlCommon::YawGains gains;gains.kp=N03_APPROACH_YAW_KP;gains.ki=.08f;gains.kd=3.f;
            auto limits=ControlCommon::defaultYawLimits();limits.authority=.25f;
            limits.deadbandRad=ControlCommon::DEG2RAD;
            const bool tracking=approachSupport_.phase==VisualSupport::RUN && o.valid();
            const float currentHeight=vertical.height;
            const bool nearHeight=approachSupport_.altitudeReady && vertical.healthy && isfinite(currentHeight) &&
                fabsf(approachSupport_.estimatedHeight(currentHeight,now)-approachSupport_.fixedTarget())<=.20f;
            const bool nearCentred=nearHeight && o.coversCentre() && fabsf(yawError)<=8.f*ControlCommon::DEG2RAD &&
                fabsf(yawRate)<=.12f;
            const bool centered=tracking && (o.near?nearCentred:
                (fabsf(o.x-120.f)<=24 && fabsf(o.y-80.f)<=24));
            const bool heightPermission=approachSupport_.runReleased && approachSupport_.advanceEnabled &&
                approachSupport_.fixedControlReady(now) && !ctx.flowHeight.holding;
            const bool close=heightPermission && N03_VISIT_WIDTH_PX>0 && approachHadFull_ && centered && score>=N03_VISIT_WIDTH_PX;
            if(state_==APPROACH && close){enter(ctx,VISIT_CONFIRM);decision=8;}
            else if(state_==VISIT_CONFIRM){
                decision=8;
                if(close){
                    if(newSample)++closeFrames_;
                    if(closeFrames_>=CLOSE_FRAMES_REQUIRED && uint32_t(now-stateStartMs_)>=500){
                        ++visited_;lastVisitedArea_=o.w*o.h;
                        Serial.printf("[COURSE] waypoint_cleared=%d width=%.1f estimated=1\n",visited_,score);
                        if(stopAfterVisit_||visited_>=targetCount_)enter(ctx,DONE);
                        else{
                            routeYawRef_=wrapPi(courseYaw_+BalloonCourse::heading(track_,visited_-1));
                            enter(ctx,TURN_TO_NEXT);
                        }
                    }
                }else{
                    closeFrames_=0;
                    // Keep the recovery timer/controller when returning from
                    // VISIT_CONFIRM: enter(APPROACH) would reset lost history.
                    state_=APPROACH;stateStartMs_=now;advanceGate_.reset();
                }
            }
            const bool neutralReady=fabsf(approachMixer_.servo1-SupportYaw::VISUAL_BASE1_DEG)<=.5f &&
                fabsf(approachMixer_.servo2-SupportYaw::VISUAL_BASE2_DEG)<=.5f;
            if(state_==APPROACH){
                // N03 PARTIAL is an already confirmed track, not a rejected
                // candidate. Requiring a historical FULL here deadlocked
                // close starts for the entire flight003101. Keep the same
                // fresh-observation/alignment gates; FULL remains an arrival
                // calibration condition, independent of forward permission.
                // Receding short horizon: do not demand perfect alignment
                // throughout the whole pulse before allowing any movement.
                const float horizon=fminf(ApproachProfile::FORECAST_S,impulse_.headingHorizon(now));
                const bool headingAligned=ImpulseApproach::headingFits(yawError,yawRate,
                    horizon,ApproachProfile::ENTRY_YAW_DEG*ControlCommon::DEG2RAD);
                const bool headingSafe=ImpulseApproach::headingFits(yawError,yawRate,
                    horizon,ApproachProfile::CONTINUE_YAW_DEG*ControlCommon::DEG2RAD);
                headingLimited=tracking && !headingSafe;
                const bool advanceHeight=approachSupport_.altitudeReady && vertical.healthy && isfinite(currentHeight) &&
                    fabsf(approachSupport_.estimatedHeight(currentHeight,now)-approachSupport_.fixedTarget())<=ApproachProfile::ADVANCE_HEIGHT_M;
                const bool aligned=heightPermission && advanceHeight && tracking && headingAligned &&
                    (o.near?(advanceHeight && o.coversCentre()):
                     (fabsf(o.x-120.f)<=ApproachProfile::ENTRY_X_PX && fabsf(o.y-80.f)<=ApproachProfile::ENTRY_Y_PX)) &&
                    fabsf(yawRate)<=ApproachProfile::ENTRY_RATE;
                const bool visibleSafe=tracking &&
                    (o.near?(advanceHeight && o.coversCentre()):
                     (fabsf(o.x-120.f)<=ApproachProfile::CONTINUE_X_PX && fabsf(o.y-80.f)<=ApproachProfile::CONTINUE_Y_PX));
                // Only finish an ALREADY ACTIVE pulse on short visual loss.
                // World bearing stays in approachBearing_; height is checked
                // against its last real observation. Never fabricate FULL or
                // arrival from the remembered geometry or a raw candidate.
                estimatedContinuation=!tracking && impulse_.phase==ImpulseApproach::PUSH &&
                    !approachReacquiring_ && approachBearing_.ready &&
                    approachMemory_.canContinue(now,associationEpoch,o.fresh,
                        feedback.state==NiclaFeedback::AMBIG,currentHeight,
                        vertical.healthy,
                        vertical.vz);
                const bool safe=heightPermission && advanceHeight && (visibleSafe || estimatedContinuation) && headingSafe &&
                    fabsf(yawRate)<=ApproachProfile::CONTINUE_RATE;
                // Height keeps regulating during the pulse. Do not require
                // perfect vertical centring before any forward motion exists.
                const bool ready=impulse_.phase==ImpulseApproach::ALIGN &&
                    advanceGate_.update(now,aligned,newSample,
                        ApproachProfile::REQUIRED_SAMPLES,ApproachProfile::CONFIRM_MS);
                impulse_.update(now,ready,safe,neutralReady);
                if(impulse_.phase!=ImpulseApproach::ALIGN)advanceGate_.reset();
                if(impulse_.phase==ImpulseApproach::PUSH){forward=ApproachProfile::FORWARD;decision=7;}
                else decision=tracking?(fabsf(o.x-120.f)>24?5:6):4;
            }else impulse_.cancel(now);
            const float yawCommand=SupportYaw::command(gains,limits,approachYaw_,
                yawError,yawRate,dt,tracking && !impulse_.neutralOnly());
            // Separate impulse from yaw corrections, including the time to
            // return fore/aft tilt. Height feedback remains active throughout.
            appliedYaw=impulse_.neutralOnly()?0.f:yawCommand;
            if(state_==DONE){
                forward=0;decision=9;
                applyPhysicalCommand(ctx,approachMixer_.servo1,approachMixer_.servo2,0,0);
                ctx.robot->setActuatorsArmed(false);
            }else{
                const bool coastUp=approachSupport_.inertia.brakingUp && impulse_.neutralOnly();
                const float runningFloor=fminf(motorFloor,approachSupport_.inertia.motorFloor);
                approachMixer_.update(approachSupport_.demand,appliedYaw,.25f,0,maxPower,dt,
                    runningFloor,0,forward,true,ApproachProfile::TILT_DEG,!coastUp,ApproachProfile::FORWARD_SIGN);
                applyPhysicalCommand(ctx,approachMixer_.servo1,approachMixer_.servo2,
                    approachMixer_.power,approachMixer_.power);
            }
        }
    }
    reportDecision(ctx,decision,forward);
    Telemetry::sendVisualSupport(ctx,approachSupport_.lastY,
        approachSupport_.seen && !o.near?(80.f-approachSupport_.lastY)/80.f:0.f,
        approachSupport_.height.trim,approachSupport_.demand,appliedYaw,
        ctx.robot->actuatorsAreArmed()?approachSupport_.phase:VisualSupport::DISARMED);
    Telemetry::sendVisualBearing(ctx,approachBearing_.lastX,yawRef_,
        isfinite(yaw)?wrapPi(yawRef_-yaw):0,
        ctx.robot->actuatorsAreArmed() && approachBearing_.ready && !approachSupport_.failed,
        VisualBearing::VERSION,forward);
    Telemetry::sendImpulse(ctx,int(impulse_.phase),float(impulse_.elapsed(now)),
        forward,appliedYaw,float(impulse_.pulses),float(ImpulseApproach::PUSH_MS));
    const auto& motion=approachSupport_.inertia;
    const bool live=ctx.robot->actuatorsAreArmed();
    const int inertiaFlags=live?((motion.velocityTrusted?1:0) |
        ((motion.brakingUp && impulse_.neutralOnly())?2:0) |
        (motion.brakingUp?4:0) | (motion.brakingDown?8:0) | (motion.visualAbove?16:0) |
        (headingLimited?32:0) | (motion.evidence.trusted?64:0) | (motion.visualCorroborated?128:0) | (motion.evidence.fastTrusted?256:0) | (o.near?512:0) | (estimatedContinuation && forward>0?1024:0)):0;
    Telemetry::sendInertia(ctx,motion.rate,motion.predictedRate,motion.desiredRate,
        motion.motorFloor,inertiaFlags,motion.velocityConfidence);
    Telemetry::sendFixedHeight(ctx,approachSupport_.fixedTarget(),
        approachSupport_.estimatedHeight(vertical.height,now),
        approachSupport_.pressureBias(now),approachSupport_.appliedOffset,
        approachSupport_.fixedFlags(now),approachSupport_.levelPitch*ControlCommon::RAD2DEG);
    Telemetry::sendHeightReference(ctx,approachSupport_.referenceForControl(now),
        approachSupport_.referenceError(vertical.height,now),
        approachSupport_.altitudeReady?float(uint32_t(now-approachSupport_.referenceUpdatedAt)):-1.f,
        approachSupport_.independentHeight?0.f:approachSupport_.altitudeAid.rate(now),approachSupport_.altitudeAid.noiseBand,
        approachSupport_.referenceFlags(now));
    Telemetry::sendControl(ctx,yawRef_,searchHeight_,forward);
    Telemetry::sendMission(ctx,int(state_),visited_,targetCount_,searchHeight_,score,
        uint32_t(now-missionStartMs_)/1000.f);
}

void BalloonMission::update(AppContext& ctx) {
    if (!ctx.robot) return;
    const bool visionBench = ctx.mode == AppConfig::P12_VISION_BENCH;
    if (visionBench && ctx.robot->actuatorsAreArmed()) ctx.robot->setActuatorsArmed(false);
    const auto& feedback = ctx.robot->sensorsuite.niclaFeedback();
    const bool n03 = feedback.protocol == NiclaFeedback::N03_PROTOCOL;
    const bool newVisionSample = feedback.fresh && feedback.generation != lastVisionGeneration_;
    lastVisionGeneration_ = feedback.generation;
    const bool visitCalibrated = !n03 || N03_VISIT_WIDTH_PX > 0;
    const float visitThreshold = n03 ? N03_VISIT_WIDTH_PX : VISIT_SCORE_THRESHOLD;

    const float yaw = ctx.sensors[SensorMap::YAW];
    const float x = ctx.sensors[SensorMap::NICLA_X_VALUE];
    const float y = ctx.sensors[SensorMap::NICLA_Y_VALUE];
    bool see = detected(ctx);
    const float score = visitScore(ctx);

    // First validate one complete waypoint in P08/P09, through arrival.
    // Multi-waypoint turn/transition mixing is a separate pending integration.
    // P12 remains disarmed and keeps its current simulation path.
    if(n03 && !visionBench && targetCount_==1 && state_==SEARCH){
        updateN03Search(ctx);return;
    }
    if(n03 && !visionBench && targetCount_==1 && (state_==APPROACH || state_==VISIT_CONFIRM)){
        updateN03Approach(ctx);return;
    }
    // Legacy multi-waypoint paths also must not silently inherit a different
    // camera association as the current waypoint. Planned TURN/ACQUIRE_NEXT
    // remain responsible for selecting the NEXT waypoint.
    if(n03 && !visionBench && targetCount_!=1 && (state_==APPROACH || state_==VISIT_CONFIRM)){
        const uint8_t epoch=uint8_t(feedback.sample>>8);
        if(feedback.fresh && epoch!=approachAssociationEpoch_){
            approachAssociationEpoch_=epoch;approachReacquiring_=true;
            advanceGate_.reset();closeFrames_=0;
            if(state_==VISIT_CONFIRM){state_=APPROACH;stateStartMs_=millis();}
        }
        if(approachReacquiring_ && see){
            const float candidate=wrapPi(yaw-(x-120.f)/240.f*(70.f*ControlCommon::DEG2RAD));
            if(feedback.state==NiclaFeedback::FULL && visualHeightSeen_ && isfinite(candidate) &&
               fabsf(wrapPi(candidate-recoveryYaw_))<=20.f*ControlCommon::DEG2RAD)
                approachReacquiring_=false;
            else see=false;
        }
    }

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
    int decisionReason = 0;
    float forwardRequest = 0.0f;

    // Solo el avance en APPROACH usa potencias independientes por motor
    // (ver ControlCommon::computeThrustAllocation). El resto de los estados
    // siguen mandando la misma potencia a los dos motores a traves de
    // motorPower, como siempre.
    float motor1Power = 0.0f;
    float motor2Power = 0.0f;
    bool independentMotors = false;

    // Para SEARCH:
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
    const bool visualTracking = see && (state_ == APPROACH || state_ == VISIT_CONFIRM ||
        (state_ == SEARCH && visited_ == 0));
    if (visualTracking && newVisionSample) {
        visualHeight_.update(millis(), y, ctx.sensors[SensorMap::NICLA_H_VALUE],
            altitudeCfg.maxPower, !n03 || feedback.state == NiclaFeedback::FULL);
        visualHeightSeen_ = true;
        visualHeightLastSeenMs_ = millis();
        recoveryYaw_ = isfinite(missionYawRef) && missionYawRefValid ? missionYawRef : yaw;
        if (isfinite(height)) visualHeightAnchor_ = searchHeight_ = height;
        // Transfer the visual support trim to the temporary barometric bridge.
        missionAlt.integral = visualHeight_.trim;
    }
    if (!visionBench && visualTracking) {
        const uint32_t nowUs = micros();
        const float dt = fminf(.05f, uint32_t(nowUs-visualHeightLastUs_)*1.e-6f);
        visualHeightLastUs_ = nowUs;
        visualHeightPower_ = ControlCommon::rateLimit(visualHeight_.demand,
            visualHeightPower_, altitudeCfg.slewPerSec, dt);
        altitudeDemand = constrain(visualHeightPower_, 0.f, altitudeCfg.maxPower);
        missionAlt.lastPower = altitudeDemand;
    } else if (!visionBench && isfinite(searchHeight_) && isfinite(height) && isfinite(verticalVelocity)) {
        altitudeDemand = calculateAltitudePower(
            searchHeight_, height, verticalVelocity, altitudeCfg);
        visualHeightPower_ = altitudeDemand;
        visualHeightLastUs_ = micros();
    }

    const bool altitudeControlEnabled =
        !visionBench &&
        isfinite(searchHeight_) &&
        isfinite(height) &&
        isfinite(verticalVelocity);

    // Lost target: no forward thrust, keep last bearing briefly, then scan
    // +/-12deg and +/-20cm around the last observation, at most ten seconds.
    // This is NOT position hold: horizontal drift cannot be measured here.
    if (visualHeightSeen_ && !visualTracking &&
        (state_ == APPROACH || state_ == VISIT_CONFIRM || state_ == ACQUIRE_NEXT)) {
        const uint32_t missingMs = (state_ == ACQUIRE_NEXT)
            ? uint32_t(millis()-stateStartMs_) : uint32_t(millis()-visualHeightLastSeenMs_);
        advanceGate_.reset(); closeFrames_ = 0; turnSettled_ = false;
        if (state_ == VISIT_CONFIRM) stateStartMs_ = millis();
        if (missingMs >= 10000 || !isfinite(yaw) || (!visionBench && !altitudeControlEnabled)) {
            enter(ctx, TURN_FAILED); // Existing terminal safe state; reset required.
            applyPhysicalCommand(ctx, SERVO1_Z_DEG, SERVO2_Z_DEG, 0, 0);
            ctx.robot->setActuatorsArmed(false);
            reportDecision(ctx, 10, 0);
            return;
        }
        const float offset = missingMs<700 ? 0 : VisualHeight::searchOffset(missingMs-700);
        searchHeight_ = visualHeightAnchor_ + offset;
        if (!visionBench) altitudeDemand=calculateAltitudePower(searchHeight_,height,verticalVelocity,altitudeCfg);
        if (state_ != ACQUIRE_NEXT) {
            yawRef_ = wrapPi(recoveryYaw_ + (missingMs<700 ? 0 :
                (12.f*ControlCommon::DEG2RAD)*sinf((missingMs-700)*.001f*6.2831853f/6.f)));
        }
        holdRouteYaw(ctx, altitudeDemand, visualCfg.maxPower, visualCfg.minPower,
            altitudeCfg.maxPower, servo1Deg, servo2Deg, motorPower);
        applyPhysicalCommand(ctx, servo1Deg, servo2Deg, motorPower, motorPower);
        reportDecision(ctx, 4, 0);
        Telemetry::sendControl(ctx,yawRef_,searchHeight_,0);
        Telemetry::sendMission(ctx,(int)state_,visited_,targetCount_,searchHeight_,score,
            (millis()-missionStartMs_)/1000.f);
        if (n03 && feedback.fresh && newVisionSample && feedback.state==NiclaFeedback::LOST && !feedback.resetPending)
            ctx.robot->sensorsuite.resetNiclaTarget();
        // ACQUIRE still needs its original full-target / route / size filters.
        if (state_ != ACQUIRE_NEXT || !see) return;
    }

    // No planar motion without a fresh link/sample. Keep altitude control active.
    // A temporary N03 missing/ambiguous observation holds APPROACH until recovered.
    const bool observationHold = n03 && !see &&
        (state_ == APPROACH || state_ == VISIT_CONFIRM);
    const bool invalidHeight = !visionBench && !visualTracking && !altitudeControlEnabled;
    if (state_ != DONE && (!feedback.fresh || observationHold || !isfinite(yaw) || !isfinite(courseYaw_) || invalidHeight)) {
        advanceGate_.reset();
        reportDecision(ctx, invalidHeight ? 15 : 1, 0.0f);
        turnSettled_ = false;
        closeFrames_ = 0;
        if (state_ == VISIT_CONFIRM) stateStartMs_ = millis();
        resetMissionYaw(yaw);
        applyPhysicalCommand(ctx, SERVO1_Z_DEG, SERVO2_Z_DEG, altitudeDemand, altitudeDemand);
        Telemetry::sendControl(ctx, yawRef_, searchHeight_, 0.0f);
        Telemetry::sendMission(ctx, (int)state_, visited_, targetCount_, searchHeight_, score,
                               (millis() - missionStartMs_) / 1000.0f);
        if (observationHold && feedback.fresh && feedback.state == NiclaFeedback::LOST) {
            ctx.robot->sensorsuite.resetNiclaTarget();
            enter(ctx, visited_ > 0 ? ACQUIRE_NEXT : SEARCH);
        }
        return;
    }

    const float heightError = searchHeight_ - height;

    if (state_ != DONE && altitudeControlEnabled && !visualHeightSeen_ && !visualTracking) {
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
            advanceGate_.reset();
            reportDecision(ctx, 2, 0.0f);
            turnSettled_ = false;
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
        decisionReason = 4;
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
        decisionReason = 1;
        if (!see || !isfinite(x)) {
            // Si se pierde el globo, vuelve a buscar.
            enter(ctx, visited_ > 0 ? ACQUIRE_NEXT : SEARCH);
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
                advanceGate_.reset();
                decisionReason = 5;
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
                    (newVisionSample && (nowMs - missionLastVisionMs) >= VISION_UPDATE_MS)) {
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
                // Do not halve the support power while trying to centre yaw.
                const float altitudeDemandForCenter = altitudeDemand;

                float etaZ = 1.0f, etaYaw = 0.0f, blend = 0.0f;
                const float ceiling =
                    (altitudeCfg.maxPower > yl.maxPower)
                        ? altitudeCfg.maxPower : yl.maxPower;
                ControlCommon::mixOutputs(altitudeDemandForCenter, yawCmd, yl, ceiling,
                                          servo1Deg, servo2Deg, motorPower,
                                          etaZ, etaYaw, blend);
            }
            else {
                // Centrado no significa que haya terminado de girar.
                // Exigir observaciones nuevas y giro lento antes de dar avance.
                const float rate = ctx.sensors[SensorMap::YAW_RATE];
                if (!advanceGate_.active) yawRef_ = yaw;
                const bool canAdvance = advanceGate_.update(millis(),
                    VisualHeight::centered(y) && isfinite(rate) && fabsf(rate) <= TURN_RATE_TOLERANCE,
                    newVisionSample);
                if (!canAdvance) {
                    decisionReason = 6;
                    // Mantener rumbo y altura; no aplicar empuje de avance.
                    holdRouteYaw(ctx, altitudeDemand, visualCfg.maxPower,
                        visualCfg.minPower, altitudeCfg.maxPower,
                        servo1Deg, servo2Deg, motorPower);
                    break;
                }
                decisionReason = 7;
                forwardRequest = MOVE_POWER;
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
                if (visitCalibrated && newVisionSample && score >= visitThreshold) {
                    enter(ctx, VISIT_CONFIRM);
                }
            }
        }
        break;

    case VISIT_CONFIRM: {
        decisionReason = 8;
        // Retirar avance no frena la inercia. Conservar sustentacion vertical
        // durante la confirmacion; en visionBench altitudeDemand es cero.
        servo1Deg = SERVO1_Z_DEG;
        servo2Deg = SERVO2_Z_DEG;
        motorPower = altitudeDemand;

        const bool centered =
            see &&
            isfinite(x) &&
            VisualHeight::centered(y) &&
            fabsf(x - IMAGE_CENTER_X) <= visualCfg.deadbandPx;

        if (visitCalibrated && centered && score >= visitThreshold) {
            if (newVisionSample) closeFrames_++;
        }
        else {
            closeFrames_ = 0;

            if (!see) {
                enter(ctx, visited_ > 0 ? ACQUIRE_NEXT : SEARCH);
            }
            else {
                enter(ctx, APPROACH);
            }
        }

        const bool arrivalStable = !n03 ||
            (uint32_t)(millis() - stateStartMs_) >= 500;
        if (closeFrames_ >= CLOSE_FRAMES_REQUIRED && arrivalStable) {
            visited_++;
            lastVisitedArea_ = ctx.sensors[SensorMap::NICLA_W_VALUE] *
                               ctx.sensors[SensorMap::NICLA_H_VALUE];

            if (stopAfterVisit_ || visited_ >= targetCount_) {
                enter(ctx, DONE);
            }
            else {
                routeYawRef_ = wrapPi(courseYaw_ + BalloonCourse::heading(track_, visited_ - 1));
                enter(ctx, TURN_TO_NEXT);
                Serial.printf("[COURSE] next_waypoint=%d yaw_ref=%.3f\n",
                    BalloonCourse::waypoint(track_, visited_), yawRef_);
            }
        }
        break;
    }

    case TURN_TO_NEXT: {
        decisionReason = 3;
        if (millis() - stateStartMs_ >= TURN_TIMEOUT_MS) {
            enter(ctx, TURN_FAILED);
            motorPower = altitudeDemand;
            break;
        }
        holdRouteYaw(ctx, altitudeDemand, visualCfg.maxPower, visualCfg.minPower,
                     altitudeCfg.maxPower, servo1Deg, servo2Deg, motorPower);
        const float rate = ctx.sensors[SensorMap::YAW_RATE];
        const bool aligned = fabsf(wrapPi(yawRef_ - yaw)) <= TURN_TOLERANCE_RAD &&
                             isfinite(rate) && fabsf(rate) <= TURN_RATE_TOLERANCE;
        if (!aligned) turnSettled_ = false;
        else if (!turnSettled_) { turnSettled_ = true; turnSettledMs_ = millis(); }
        else if (millis() - turnSettledMs_ >= TURN_SETTLE_MS) {
            ctx.robot->sensorsuite.resetNiclaTarget();
            enter(ctx, ACQUIRE_NEXT);
        }
        break;
    }
    case ACQUIRE_NEXT:
        decisionReason = 4;
        holdRouteYaw(ctx, altitudeDemand, visualCfg.maxPower, visualCfg.minPower,
                     altitudeCfg.maxPower, servo1Deg, servo2Deg, motorPower);
        // El tracker LOST es terminal tras su ventana local de recuperacion.
        // Ya estamos buscando un nuevo waypoint: volver a SEARCH en Nicla,
        // conservando rumbo de ruta y filtros de objetivo nuevo en la ESP.
        // resetNiclaTarget invalida fresh hasta recibir un ACK distinto;
        // no se emiten reinicios repetidos mientras el anterior esta pendiente.
        if (n03 && newVisionSample && feedback.state == NiclaFeedback::LOST &&
            !feedback.resetPending) {
            ctx.robot->sensorsuite.resetNiclaTarget();
            break;
        }
        if (see && newVisionSample && fabsf(wrapPi(yawRef_ - yaw)) <= TURN_TOLERANCE_RAD &&
            (!n03 || feedback.state == NiclaFeedback::FULL)) {
            // A near/contact-sized observation can still be the balloon just visited.
            // Conservatively require a smaller next target. This may wait if occluded.
            const float area = ctx.sensors[SensorMap::NICLA_W_VALUE] *
                               ctx.sensors[SensorMap::NICLA_H_VALUE];
            if (BalloonCourse::smallerNextTarget(area, lastVisitedArea_)) enter(ctx, APPROACH);
            else if (n03) {
                largeTargetRejected_ = true;
                largeTargetRejectedMs_ = millis();
                ctx.robot->sensorsuite.resetNiclaTarget();
            }
        }
        break;
    case TURN_FAILED:
        decisionReason = 10;
        motorPower = 0;
        if (ctx.robot->actuatorsAreArmed()) ctx.robot->setActuatorsArmed(false);
        break;

    case DONE:
        decisionReason = 9;
        servo1Deg = SERVO1_Z_DEG;
        servo2Deg = SERVO2_Z_DEG;
        motorPower = 0.0f;
        break;
    }

    // Report the command actually computed this cycle; state transitions may
    // already name the next phase. Never label this as measured translation.
    reportDecision(ctx, decisionReason, forwardRequest);
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
