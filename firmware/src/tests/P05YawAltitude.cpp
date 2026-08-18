#include "tests/TestRunners.h"
#include "app/AppConfig.h"
#include "app/SensorMap.h"
#include "app/Telemetry.h"
#include <Arduino.h>
#include <math.h>
#include <stdint.h>

namespace {

// ============================================================================
// P05 v5 - CONTROL SIMULTANEO Z + YAW
//
// CAMBIO DE FONDO RESPECTO A v4
// -----------------------------
//
// En v4 el mixer hacia:
//
//     outputPower = altPower  (+ una pequena compensacion vertical)
//
// Es decir: la potencia de motor la decidia SOLO el lazo de altura.
//
// Pero fisicamente el par de yaw de este vehiculo es:
//
//     N  =  b * P * sin(phi)          <-- P = empuje total
//     Fz =      P * cos(phi)
//
// El yaw es MULTIPLICATIVO en el empuje. Si el lazo de altura esta satisfecho
// (blimp cerca de flotabilidad neutra => P ~ 0.02..0.05), inclinar los servos
// no produce practicamente ningun par. Ese es el motivo real de que "el yaw
// no actuaba manteniendo la altura": el yaw no tenia empuje que vectorizar.
//
// v5 introduce una ASIGNACION DE CONTROL explicita:
//
//     ALTITUDE PID ---> zDemand  (empuje vertical deseado)
//     YAW PID      ---> yawCmd   (mando de yaw normalizado, -1..+1)
//                          |
//                          v
//                     +---------+
//                     |  MIXER  |   1) elige la inclinacion (blend)
//                     +---------+   2) calcula eficiencias reales etaZ, etaYaw
//                          |        3) P = max(P_para_altura, P_para_yaw)
//                          v
//                  motores + servos
//
// El punto clave es el paso 3: el mixer GARANTIZA empuje para el yaw. Ninguno
// de los dos lazos puede dejar al otro sin actuador.
//
//
// GEOMETRIA (medida a partir de las constantes de P00/P02)
// --------------------------------------------------------
//
// Los servos estan montados en espejo. La inclinacion fisica de cada gondola
// respecto de su posicion "Z" (empuje vertical) es:
//
//     phi1 =  servo1 - 35
//     phi2 =  85 - servo2
//
// Con phi1 y phi2 de signo opuesto => par puro de yaw (cupla).
//
// Eficiencias del conjunto (normalizadas, 1.0 = todo el empuje va hacia arriba):
//
//     blend   servo1  servo2   etaZ    etaYaw   yaw/lift
//     0.00     35.0    85.0    1.000   0.000     0.00
//     0.10     31.5    76.5    0.994   0.104     0.11
//     0.30     24.5    59.5    0.943   0.306     0.33
//     0.50     17.5    42.5    0.845   0.488     0.58
//     0.70     10.5    25.5    0.709   0.638     0.90
//     1.00      0.0     0.0    0.453   0.785     1.73
//
// Consecuencia practica: conviene inclinar MUCHO para hacer yaw (la relacion
// par/sustentacion parasita mejora 16x entre blend 0.1 y blend 1.0), y no
// poquito. Por eso v5 satura la inclinacion rapido en lugar de usarla como
// mando proporcional fino: la magnitud del par se modula con la POTENCIA.
//
// LIMITACION FISICA QUE NO SE PUEDE ELIMINAR EN SOFTWARE:
// mientras el vehiculo hace yaw, el empuje vectorizado tiene componente
// vertical positiva (etaZ > 0 siempre). Es decir, girar SIEMPRE empuja hacia
// arriba un poco. El lazo de altura solo puede compensarlo bajando su propia
// demanda hasta 0. Si el yaw exige mas empuje del que la altura quiere, el
// blimp subira levemente durante el giro. Se minimiza usando blend alto y
// yaw max power (ymax) lo mas bajo posible que aun gire.
// ============================================================================


// ============================================================================
// GEOMETRIA
// ============================================================================

constexpr float SERVO1_Z_DEG = 35.0f;
constexpr float SERVO2_Z_DEG = 85.0f;

constexpr float SERVO1_RIGHT_DEG = 120.0f;
constexpr float SERVO2_RIGHT_DEG = 120.0f;

constexpr float SERVO1_LEFT_DEG = 0.0f;
constexpr float SERVO2_LEFT_DEG = 0.0f;

constexpr float DEG2RAD = PI / 180.0f;


// ============================================================================
// DEFAULTS - ALTURA
// ============================================================================

constexpr float DEFAULT_ALT_KP = 0.30f;
constexpr float DEFAULT_ALT_KI = 0.025f;
constexpr float DEFAULT_ALT_KD = 0.01f;

constexpr float DEFAULT_ALT_MIN = 0.00f;
constexpr float DEFAULT_ALT_MAX = 0.20f;   // subido: el mixer ahora necesita
                                           // cabecera para yaw + altura

constexpr float DEFAULT_ALT_SLEW = 0.18f;  // ahora se aplica a la salida final

constexpr float DEFAULT_ALT_SUCCESS_M = 0.10f;


// ============================================================================
// DEFAULTS - YAW
//
// ATENCION: las unidades de ykp/yki/ykd CAMBIARON respecto de v4.
//
//   v4: la salida del PID de yaw estaba en "fraccion de potencia" (0..0.085)
//   v5: la salida del PID de yaw es un mando normalizado -1..+1
//
// Equivalencia aproximada: ykp_v5 ~ ykp_v4 / ymax_v4  => 0.10 / 0.085 ~ 1.18
// Se usa 2.0 por defecto (mando saturado con ~29 grados de error).
// ============================================================================

constexpr float DEFAULT_YAW_KP = 2.00f;    // mando por radian
constexpr float DEFAULT_YAW_KI = 0.35f;    // mando por radian-segundo
constexpr float DEFAULT_YAW_KD = 0.60f;    // mando por rad/s

// ymin / ymax siguen siendo FRACCION DE POTENCIA DE MOTOR, igual que en v4,
// de modo que el pack de AUX4 y la estacion de tierra no cambian.
//
//   ymin = potencia minima que el mixer garantiza cuando hay demanda de yaw
//   ymax = potencia maxima que el mixer dedica al yaw
constexpr float DEFAULT_YAW_MIN = 0.045f;
constexpr float DEFAULT_YAW_MAX = 0.110f;

constexpr float DEFAULT_YAW_SUCCESS_DEG = 10.0f;

// Autoridad maxima de inclinacion de servo (fraccion del camino al extremo).
// Ver tabla de eficiencias: valores altos son MUCHO mas eficientes para yaw.
constexpr float DEFAULT_YAW_SERVO_AUTHORITY = 0.75f;

// Inclinacion minima cuando hay demanda de yaw. Evita el caso "servo casi en Z
// con potencia de yaw aplicada", que solo hace subir el blimp sin girarlo.
constexpr float YAW_TILT_MIN_FRACTION = 0.45f;

// Ganancia de forma: con que rapidez la inclinacion llega al maximo.
// 2.0 => inclinacion saturada con |yawCmd| >= 0.5
constexpr float YAW_TILT_SHAPE = 2.00f;


// ============================================================================
// TRANSICIONES (solo informativas: NUNCA apagan ni limitan un lazo)
// ============================================================================

constexpr uint32_t ALT_STABLE_REQUIRED_MS = 2000U;
constexpr uint32_t YAW_LOCK_REQUIRED_MS = 500U;

constexpr float YAW_REACQUIRE_MARGIN_DEG = 3.0f;


// ============================================================================
// FILTROS
// ============================================================================

constexpr float VZ_FILTER_ALPHA = 0.82f;
constexpr float YAW_RATE_FILTER_ALPHA = 0.80f;


// ============================================================================
// LIMITES PID
// ============================================================================

constexpr float ALT_DEADBAND = 0.025f;
constexpr float ALT_INTEGRAL_ZONE_M = 0.35f;

constexpr float YAW_ERROR_DEADBAND_RAD = 2.0f * DEG2RAD;
constexpr float YAW_INTEGRAL_ZONE_RAD = 30.0f * DEG2RAD;
constexpr float YAW_INTEGRAL_LIMIT = 0.60f;   // en unidades de mando (-1..1)

constexpr float YAW_CMD_DEADBAND = 0.02f;


// ============================================================================
// LIMITES DE VELOCIDAD DE ACTUADORES
// ============================================================================

constexpr float SERVO_SLEW_DEG_PER_S = 200.0f;

// Piso de eficiencia vertical usado en la division. Evita que la compensacion
// explote cuando el conjunto esta muy inclinado.
constexpr float ETA_Z_FLOOR = 0.35f;


// ============================================================================
// DEBUG
// ============================================================================

constexpr uint32_t DEBUG_PERIOD_MS = 500U;


// ============================================================================
// PACKING (formato identico a v4)
// ============================================================================

constexpr uint32_t PACK_MARKER = (1UL << 23);


// ============================================================================
// CONFIG
// ============================================================================

struct Config {

    // Altitude
    float akp;
    float aki;
    float akd;

    float amin;
    float amax;
    float aslew;
    float altSuccessM;

    // Yaw
    float ykp;
    float yki;
    float ykd;

    float ymin;
    float ymax;

    float yawSuccessRad;

    float yawServoAuthority;
};


// ============================================================================
// PHASE
// ============================================================================

enum class Phase : uint8_t {

    ALTITUDE_ACQUIRE = 0,
    YAW_ACQUIRE = 1,
    HOLD = 2,
};


// ============================================================================
// STATE
// ============================================================================

struct State {

    bool initialized = false;

    unsigned long modeEnteredMs = 0U;
    unsigned long lastUs = 0U;

    // ------------------------- ALTITUDE -------------------------

    float filteredVz = 0.0f;
    float altIntegral = 0.0f;

    // --------------------------- YAW ----------------------------

    float lastYaw = 0.0f;
    float filteredYawVel = 0.0f;
    float yawIntegral = 0.0f;

    // ------------------------ ACTUADORES ------------------------

    float lastPower = 0.0f;
    float lastServo1 = SERVO1_Z_DEG;
    float lastServo2 = SERVO2_Z_DEG;

    // -------------------------- PHASE ---------------------------

    Phase phase = Phase::ALTITUDE_ACQUIRE;

    bool altitudeStableTiming = false;
    uint32_t altitudeStableSinceMs = 0U;

    bool yawLockTiming = false;
    uint32_t yawLockSinceMs = 0U;

    uint32_t lastDebugMs = 0U;
};

State ctrl;


// ============================================================================
// UTILIDADES
// ============================================================================

float wrapPi(float a) {

    while (a > PI) {
        a -= 2.0f * PI;
    }

    while (a < -PI) {
        a += 2.0f * PI;
    }

    return a;
}


float lerpFloat(float a, float b, float t) {

    t = constrain(t, 0.0f, 1.0f);

    return a + (b - a) * t;
}


float signOf(float v) {

    return (v >= 0.0f) ? 1.0f : -1.0f;
}


bool elapsedMs(uint32_t now,
               uint32_t since,
               uint32_t duration) {

    return (uint32_t)(now - since) >= duration;
}


float rateLimit(float target,
                float previous,
                float maxRate,
                float dt) {

    const float maxDelta = maxRate * dt;

    return constrain(
        target,
        previous - maxDelta,
        previous + maxDelta
    );
}


const char* phaseName(Phase p) {

    switch (p) {

        case Phase::ALTITUDE_ACQUIRE:
            return "ALT_ACQ";

        case Phase::YAW_ACQUIRE:
            return "YAW_ACQ";

        case Phase::HOLD:
            return "HOLD";

        default:
            return "?";
    }
}


// ============================================================================
// GEOMETRIA: EFICIENCIAS REALES DEL CONJUNTO
// ============================================================================
//
// phi1 = servo1 - SERVO1_Z_DEG
// phi2 = SERVO2_Z_DEG - servo2      (montaje en espejo)
//
// etaZ   = componente vertical media   -> sustentacion por unidad de potencia
// etaYaw = componente lateral media    -> par de yaw por unidad de potencia
//
// Esto REEMPLAZA a calculateVerticalCompensation() de v4, que usaba
// cos(servoDeg) directamente. Ese calculo era incorrecto: tomaba cos(85) = 0.087
// como si el servo2 en su posicion vertical estuviera casi horizontal, con lo
// que la "compensacion" nunca correspondia a la fisica real.
// ============================================================================

void computeEfficiencies(float servo1Deg,
                         float servo2Deg,
                         float& etaZ,
                         float& etaYaw) {

    const float phi1 =
        (servo1Deg - SERVO1_Z_DEG) * DEG2RAD;

    const float phi2 =
        (SERVO2_Z_DEG - servo2Deg) * DEG2RAD;

    etaZ =
        0.5f * (cosf(phi1) + cosf(phi2));

    etaYaw =
        0.5f * (fabsf(sinf(phi1)) + fabsf(sinf(phi2)));

    etaZ = constrain(etaZ, 0.0f, 1.0f);
    etaYaw = constrain(etaYaw, 0.0f, 1.0f);
}


// ============================================================================
// GEOMETRIA: BLEND -> ANGULOS DE SERVO
// ============================================================================
//
// blend > 0 : izquierda / antihorario  (servos hacia 0)
// blend < 0 : derecha / horario        (servos hacia 120)
//
// Se conserva exactamente la convencion validada en P00/P02.
// ============================================================================

void servosFromBlend(float blend,
                     float& servo1Deg,
                     float& servo2Deg) {

    const float magnitude =
        constrain(fabsf(blend), 0.0f, 1.0f);

    if (blend >= 0.0f) {

        servo1Deg =
            lerpFloat(SERVO1_Z_DEG, SERVO1_LEFT_DEG, magnitude);

        servo2Deg =
            lerpFloat(SERVO2_Z_DEG, SERVO2_LEFT_DEG, magnitude);
    }
    else {

        servo1Deg =
            lerpFloat(SERVO1_Z_DEG, SERVO1_RIGHT_DEG, magnitude);

        servo2Deg =
            lerpFloat(SERVO2_Z_DEG, SERVO2_RIGHT_DEG, magnitude);
    }
}


// ============================================================================
// PHASE
// ============================================================================

void enterPhase(Phase next,
                uint32_t nowMs) {

    if (ctrl.phase == next) {
        return;
    }

    ctrl.phase = next;

    ctrl.altitudeStableTiming = false;
    ctrl.yawLockTiming = false;

    Serial.printf("[P05] phase -> %s\n", phaseName(next));

    (void)nowMs;
}


// ============================================================================
// ALTITUDE PACK  (formato identico a v4)
//
// AUX1:
//   bits  0.. 6 : alt min power, %
//   bits  7..13 : alt max power, %
//   bits 14..17 : slew / 0.02
//   bits 18..22 : success height, cm
//   bit      23 : marker
// ============================================================================

void decodeAltitudePack(float raw,
                        float& amin,
                        float& amax,
                        float& aslew,
                        float& successM) {

    amin = DEFAULT_ALT_MIN;
    amax = DEFAULT_ALT_MAX;
    aslew = DEFAULT_ALT_SLEW;
    successM = DEFAULT_ALT_SUCCESS_M;

    if (!isfinite(raw) || raw <= 1.0f) {
        return;
    }

    const uint32_t packed = (uint32_t)lroundf(raw);

    if ((packed & PACK_MARKER) == 0U) {
        return;
    }

    const uint32_t payload = packed & (~PACK_MARKER);

    const uint32_t minPct = payload & 0x7FU;
    const uint32_t maxPct = (payload >> 7) & 0x7FU;
    const uint32_t slewUnits = (payload >> 14) & 0x0FU;
    const uint32_t successCm = (payload >> 18) & 0x1FU;

    amin = constrain((float)minPct / 100.0f, 0.0f, 1.0f);

    amax = constrain((float)maxPct / 100.0f, 0.001f, 1.0f);

    aslew = constrain(
        (float)max((uint32_t)1U, slewUnits) * 0.02f,
        0.02f,
        0.30f
    );

    successM = constrain(
        (float)max((uint32_t)1U, successCm) / 100.0f,
        0.01f,
        0.31f
    );
}


// ============================================================================
// YAW PACK  (formato identico a v4)
//
// AUX4:
//   bits  0.. 6 : yaw min power en decimas de %
//   bits  7..13 : yaw max power en decimas de %
//   bits 14..18 : success yaw en pasos de 0.5 grados
//   bits 19..22 : autoridad servo en pasos de 10%
//   bit      23 : marker
// ============================================================================

void decodeYawPack(float raw,
                   float& ymin,
                   float& ymax,
                   float& successRad,
                   float& servoAuthority) {

    ymin = DEFAULT_YAW_MIN;
    ymax = DEFAULT_YAW_MAX;
    successRad = DEFAULT_YAW_SUCCESS_DEG * DEG2RAD;
    servoAuthority = DEFAULT_YAW_SERVO_AUTHORITY;

    if (!isfinite(raw) || raw <= 1.0f) {
        return;
    }

    const uint32_t packed = (uint32_t)lroundf(raw);

    if ((packed & PACK_MARKER) == 0U) {
        return;
    }

    const uint32_t payload = packed & (~PACK_MARKER);

    const uint32_t minTenthPct = payload & 0x7FU;
    const uint32_t maxTenthPct = (payload >> 7) & 0x7FU;
    const uint32_t successHalfDeg = (payload >> 14) & 0x1FU;
    const uint32_t authority10Pct = (payload >> 19) & 0x0FU;

    ymin = constrain((float)minTenthPct / 1000.0f, 0.0f, 0.127f);

    ymax = constrain((float)maxTenthPct / 1000.0f, 0.001f, 0.127f);

    successRad = constrain(
        (float)max((uint32_t)1U, successHalfDeg) * 0.5f,
        0.5f,
        15.5f
    ) * DEG2RAD;

    // authority10Pct == 0 significa "no configurado" -> usar el default.
    // En v4, un 0 aqui dejaba el yaw sin autoridad de servo y por tanto sin
    // ningun efecto fisico, de forma totalmente silenciosa.
    if (authority10Pct == 0U) {
        servoAuthority = DEFAULT_YAW_SERVO_AUTHORITY;
    }
    else {
        servoAuthority = constrain(
            (float)authority10Pct / 10.0f,
            0.10f,
            1.0f
        );
    }
}


// ============================================================================
// CONFIG
//
// Diferencia importante con v4: la validacion es POR CAMPO.
//
// En v4, si un solo parametro venia mal, se descartaban TODOS los valores del
// operador y se caia a defaults completos, sin aviso. Ademas un ykp == 0 se
// consideraba "valido", con lo que el PID de yaw quedaba reducido a su termino
// integral (0.008) y tardaba ~20 s en saturar: en la practica, yaw muerto.
// ============================================================================

float pickPositive(float value,
                   float fallback,
                   float lo,
                   float hi) {

    if (!isfinite(value) || value <= 0.0f) {
        return fallback;
    }

    return constrain(value, lo, hi);
}


float pickNonNegative(float value,
                      float fallback,
                      float lo,
                      float hi) {

    if (!isfinite(value) || value < 0.0f) {
        return fallback;
    }

    return constrain(value, lo, hi);
}


Config getConfig(const AppContext& ctx) {

    Config cfg;

    // ------------------------------ ALTURA ------------------------------

    cfg.akp = pickPositive(
        ctx.command.params[AppConfig::PARAM_FX],
        DEFAULT_ALT_KP, 0.0f, 5.0f);

    cfg.aki = pickNonNegative(
        ctx.command.params[AppConfig::PARAM_AUX0],
        DEFAULT_ALT_KI, 0.0f, 1.0f);

    cfg.akd = pickNonNegative(
        ctx.command.params[AppConfig::PARAM_TX],
        DEFAULT_ALT_KD, 0.0f, 1.0f);

    decodeAltitudePack(
        ctx.command.params[AppConfig::PARAM_AUX1],
        cfg.amin,
        cfg.amax,
        cfg.aslew,
        cfg.altSuccessM
    );

    if (cfg.amin > cfg.amax) {
        cfg.amin = 0.0f;
    }

    // ------------------------------- YAW --------------------------------

    cfg.ykp = pickPositive(
        ctx.command.params[AppConfig::PARAM_AUX2],
        DEFAULT_YAW_KP, 0.0f, 20.0f);

    cfg.ykd = pickPositive(
        ctx.command.params[AppConfig::PARAM_AUX3],
        DEFAULT_YAW_KD, 0.0f, 10.0f);

    cfg.yki = DEFAULT_YAW_KI;

    decodeYawPack(
        ctx.command.params[AppConfig::PARAM_AUX4],
        cfg.ymin,
        cfg.ymax,
        cfg.yawSuccessRad,
        cfg.yawServoAuthority
    );

    if (cfg.ymin > cfg.ymax) {
        cfg.ymin = 0.0f;
    }

    return cfg;
}


void dumpConfig(const Config& cfg) {

    Serial.printf(
        "[P05] cfg ALT kp=%.3f ki=%.4f kd=%.3f min=%.3f max=%.3f "
        "slew=%.3f ok=%.3f\n",
        cfg.akp, cfg.aki, cfg.akd,
        cfg.amin, cfg.amax, cfg.aslew, cfg.altSuccessM
    );

    Serial.printf(
        "[P05] cfg YAW kp=%.3f ki=%.4f kd=%.3f min=%.4f max=%.4f "
        "ok=%.1fdeg auth=%.2f\n",
        cfg.ykp, cfg.yki, cfg.ykd,
        cfg.ymin, cfg.ymax,
        cfg.yawSuccessRad / DEG2RAD,
        cfg.yawServoAuthority
    );
}


// ============================================================================
// SERVO -> PWM
// ============================================================================

int pulse(float deg) {

    const float c = constrain(
        deg,
        AppConfig::P0025_MIN_DEG,
        AppConfig::P0025_MAX_DEG
    );

    return (int)lroundf(

        AppConfig::P0025_MIN_US +

        (c - AppConfig::P0025_MIN_DEG) *

        (AppConfig::P0025_MAX_US - AppConfig::P0025_MIN_US) /

        (AppConfig::P0025_MAX_DEG - AppConfig::P0025_MIN_DEG)
    );
}


// ============================================================================
// RESET
// ============================================================================

void resetController(const Config& cfg,
                     float yaw,
                     float vz) {

    ctrl = State{};

    ctrl.initialized = true;
    ctrl.lastUs = micros();

    ctrl.filteredVz = isfinite(vz) ? vz : 0.0f;
    ctrl.lastYaw = isfinite(yaw) ? yaw : 0.0f;

    ctrl.lastServo1 = SERVO1_Z_DEG;
    ctrl.lastServo2 = SERVO2_Z_DEG;

    ctrl.phase = Phase::ALTITUDE_ACQUIRE;
    ctrl.lastDebugMs = millis();

    Serial.println("[P05] reset -> ALT_ACQ");

    dumpConfig(cfg);
}


// ============================================================================
// DT
// ============================================================================

float getDt() {

    const unsigned long now = micros();

    float dt = (now - ctrl.lastUs) * 1e-6f;

    ctrl.lastUs = now;

    if (!isfinite(dt) || dt <= 0.0f || dt > 0.20f) {
        dt = 0.01f;
    }

    return dt;
}


// ============================================================================
// ALTITUDE PID  ->  demanda de EMPUJE VERTICAL (no de potencia de motor)
//
// La salida ya no es "lo que se manda al motor". Es "cuanta componente
// vertical de empuje quiero". El mixer decide despues cuanta potencia hace
// falta para conseguirla con la inclinacion actual.
//
// Anti-windup: integracion condicional. Si la salida esta saturada y el error
// empuja en la misma direccion, no se integra. En v4 el integrador seguia
// cargando durante la saturacion y durante el limite de slew.
// ============================================================================

float computeAltitudeDemand(const Config& cfg,
                            float heightError,
                            float filteredVz,
                            float dt) {

    const float pError =
        (fabsf(heightError) < ALT_DEADBAND) ? 0.0f : heightError;

    float raw =
        cfg.akp * pError + ctrl.altIntegral - cfg.akd * filteredVz;

    if (fabsf(heightError) < ALT_INTEGRAL_ZONE_M) {

        const bool saturatedHigh = (raw >= cfg.amax) && (heightError > 0.0f);
        const bool saturatedLow  = (raw <= 0.0f)     && (heightError < 0.0f);

        if (!saturatedHigh && !saturatedLow) {

            ctrl.altIntegral += cfg.aki * heightError * dt;

            ctrl.altIntegral =
                constrain(ctrl.altIntegral, 0.0f, cfg.amax);

            raw =
                cfg.akp * pError + ctrl.altIntegral - cfg.akd * filteredVz;
        }
    }

    float zDemand = constrain(raw, 0.0f, cfg.amax);

    if (zDemand > 0.0f && zDemand < cfg.amin) {
        zDemand = cfg.amin;
    }

    return zDemand;
}


// ============================================================================
// YAW PID  ->  mando normalizado -1 .. +1
//
// Diferencias clave con v4:
//
//  1) La salida ya NO lleva un piso (ymin). En v4 el piso hacia que
//     normalizedYaw solo pudiera valer entre 0.82 y 1.00, es decir, el mando
//     de yaw era practicamente ON/OFF: los servos saltaban a la autoridad
//     completa ante cualquier error y las ganancias solo decidian el signo.
//     El piso pertenece a la POTENCIA, no al angulo, y ahora vive en el mixer.
//
//  2) Zona muerta en el error para que los servos no vibren en el punto de
//     consigna.
//
//  3) Anti-windup por integracion condicional.
// ============================================================================

float computeYawCommand(const Config& cfg,
                        float yawError,
                        float yawVelocity,
                        float dt) {

    if (!isfinite(yawError) ||
        !isfinite(yawVelocity) ||
        !isfinite(dt)) {

        return 0.0f;
    }

    const float pError =
        (fabsf(yawError) < YAW_ERROR_DEADBAND_RAD) ? 0.0f : yawError;

    float raw =
        cfg.ykp * pError + ctrl.yawIntegral - cfg.ykd * yawVelocity;

    if (fabsf(yawError) < YAW_INTEGRAL_ZONE_RAD) {

        const bool saturatedHigh = (raw >= 1.0f)  && (pError > 0.0f);
        const bool saturatedLow  = (raw <= -1.0f) && (pError < 0.0f);

        if (!saturatedHigh && !saturatedLow) {

            ctrl.yawIntegral += cfg.yki * pError * dt;

            ctrl.yawIntegral = constrain(
                ctrl.yawIntegral,
                -YAW_INTEGRAL_LIMIT,
                YAW_INTEGRAL_LIMIT
            );

            raw =
                cfg.ykp * pError + ctrl.yawIntegral - cfg.ykd * yawVelocity;
        }
    }

    return constrain(raw, -1.0f, 1.0f);
}


// ============================================================================
// MIXER  -  ASIGNACION DE CONTROL
//
// Entradas:
//   zDemand : empuje vertical deseado   [0 .. amax]
//   yawCmd  : mando de yaw normalizado  [-1 .. +1]
//
// Salidas:
//   servo1Deg, servo2Deg, power
//
// Procedimiento:
//
//   1) La inclinacion se elige a partir del mando de yaw, con un minimo
//      (YAW_TILT_MIN_FRACTION) para no quedarse en la zona ineficiente.
//
//   2) Se calculan las eficiencias REALES de esa inclinacion.
//
//   3) Potencia necesaria para la altura:   pAlt = zDemand / etaZ
//      Potencia necesaria para el yaw:      pYaw = |yawCmd| * ymax, con piso ymin
//
//   4) power = max(pAlt, pYaw)
//
// El paso 4 es la correccion central: la altura ya no puede dejar al yaw sin
// empuje, y el yaw ya no puede reducir el empuje que la altura necesita.
// Ninguno de los dos lazos secuestra al otro.
// ============================================================================

void mixOutputs(const Config& cfg,
                float zDemand,
                float yawCmd,
                float& servo1Deg,
                float& servo2Deg,
                float& power,
                float& etaZOut,
                float& etaYawOut,
                float& blendOut) {

    float blend = 0.0f;

    const bool yawActive = fabsf(yawCmd) > YAW_CMD_DEADBAND;

    if (yawActive) {

        const float shaped = constrain(

            YAW_TILT_MIN_FRACTION +

            (1.0f - YAW_TILT_MIN_FRACTION) *
            constrain(YAW_TILT_SHAPE * fabsf(yawCmd), 0.0f, 1.0f),

            0.0f,
            1.0f
        );

        blend = signOf(yawCmd) * shaped * cfg.yawServoAuthority;
    }

    servosFromBlend(blend, servo1Deg, servo2Deg);

    float etaZ;
    float etaYaw;

    computeEfficiencies(servo1Deg, servo2Deg, etaZ, etaYaw);

    // ------------------------------------------------------------
    // Potencia requerida por el lazo de altura
    //
    // Esto sustituye a la compensacion vertical de v4 y es exacto:
    // el empuje vertical entregado es power * etaZ.
    // ------------------------------------------------------------

    const float pAlt =
        zDemand / fmaxf(etaZ, ETA_Z_FLOOR);

    // ------------------------------------------------------------
    // Potencia requerida por el lazo de yaw
    // ------------------------------------------------------------

    float pYaw = 0.0f;

    if (yawActive) {

        pYaw = constrain(
            fabsf(yawCmd) * cfg.ymax,
            cfg.ymin,
            cfg.ymax
        );
    }

    // ------------------------------------------------------------
    // Asignacion final
    // ------------------------------------------------------------

    power = constrain(fmaxf(pAlt, pYaw), 0.0f, cfg.amax);

    etaZOut = etaZ;
    etaYawOut = etaYaw;
    blendOut = blend;
}


// ============================================================================
// MAQUINA DE ESTADOS
//
// Solo reporta el progreso de adquisicion. NUNCA limita, apaga ni escala
// ningun lazo. En v4, la fase ALTITUDE_ACQUIRE reducia la autoridad de yaw al
// 35% (0.30 * 0.35 = 0.105 de blend), lo que da etaYaw = 0.11: el yaw estaba
// efectivamente desactivado mientras el blimp buscaba altura, y no salia de esa
// fase hasta 2 s continuos dentro de la ventana de altura.
// ============================================================================

void updateAltitudeStableState(const Config& cfg,
                               float heightError,
                               bool valid,
                               uint32_t nowMs) {

    const bool stable =
        valid && (fabsf(heightError) <= cfg.altSuccessM);

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


// ============================================================================
// TEST RUNNER
// ============================================================================

namespace TestRunners {


void p05YawAltitude(AppContext& ctx) {

    // ========================================================================
    // SETPOINTS
    // ========================================================================

    const float yawRef =
        ctx.command.params[AppConfig::PARAM_TZ];

    const float heightRef =
        ctx.command.params[AppConfig::PARAM_FZ];


    // ========================================================================
    // SENSORES
    // ========================================================================

    const float yaw =
        ctx.sensors[SensorMap::YAW];

    const float height =
        ctx.sensors[SensorMap::ALTITUDE];

    const float vz =
        ctx.sensors[SensorMap::VERTICAL_VELOCITY];


    // ========================================================================
    // CONFIG
    // ========================================================================

    const Config cfg = getConfig(ctx);


    // ========================================================================
    // RESET
    // ========================================================================

    if (!ctrl.initialized ||
        ctrl.modeEnteredMs != ctx.modeEnteredMs) {

        resetController(cfg, yaw, vz);

        ctrl.modeEnteredMs = ctx.modeEnteredMs;
    }


    // ========================================================================
    // DT
    // ========================================================================

    const float dt = getDt();

    const uint32_t nowMs = millis();


    // ========================================================================
    // YAW RATE
    // ========================================================================

    float yawVel = 0.0f;

    if (isfinite(yaw)) {

        const float rawYawVel =
            wrapPi(yaw - ctrl.lastYaw) / dt;

        ctrl.lastYaw = yaw;

        ctrl.filteredYawVel =

            YAW_RATE_FILTER_ALPHA * ctrl.filteredYawVel +

            (1.0f - YAW_RATE_FILTER_ALPHA) * rawYawVel;

        yawVel = ctrl.filteredYawVel;
    }


    // ========================================================================
    // VERTICAL VELOCITY
    // ========================================================================

    const float safeVz = isfinite(vz) ? vz : 0.0f;

    ctrl.filteredVz =

        VZ_FILTER_ALPHA * ctrl.filteredVz +

        (1.0f - VZ_FILTER_ALPHA) * safeVz;


    // ========================================================================
    // VALIDACION
    // ========================================================================

    const bool yawValid =
        isfinite(yawRef) && isfinite(yaw);

    const bool heightValid =
        isfinite(heightRef) && isfinite(height) && isfinite(vz);


    // ========================================================================
    // ERRORES
    // ========================================================================

    const float yawError =
        yawValid ? wrapPi(yawRef - yaw) : 0.0f;

    const float absYawError = fabsf(yawError);

    const float heightError =
        heightValid ? (heightRef - height) : 0.0f;


    // ========================================================================
    // LAZO DE ALTURA  -  SIEMPRE ACTIVO
    // ========================================================================

    float zDemand = 0.0f;

    if (heightValid) {

        zDemand = computeAltitudeDemand(
            cfg,
            heightError,
            ctrl.filteredVz,
            dt
        );
    }


    // ========================================================================
    // LAZO DE YAW  -  SIEMPRE ACTIVO, SIEMPRE CON AUTORIDAD COMPLETA
    // ========================================================================

    float yawCmd = 0.0f;

    if (yawValid) {

        yawCmd = computeYawCommand(
            cfg,
            yawError,
            yawVel,
            dt
        );
    }


    // ========================================================================
    // MAQUINA DE ESTADOS (solo reporte)
    // ========================================================================

    if (ctrl.phase == Phase::ALTITUDE_ACQUIRE) {

        updateAltitudeStableState(cfg, heightError, heightValid, nowMs);
    }

    else if (ctrl.phase == Phase::YAW_ACQUIRE) {

        updateYawLockState(cfg, absYawError, yawValid, nowMs);
    }

    else if (ctrl.phase == Phase::HOLD) {

        const float reacquireRad =
            cfg.yawSuccessRad + YAW_REACQUIRE_MARGIN_DEG * DEG2RAD;

        if (yawValid && absYawError > reacquireRad) {

            enterPhase(Phase::YAW_ACQUIRE, nowMs);
        }
    }


    // ========================================================================
    // MIXER
    // ========================================================================

    float servo1Deg = SERVO1_Z_DEG;
    float servo2Deg = SERVO2_Z_DEG;
    float power = 0.0f;

    float etaZ = 1.0f;
    float etaYaw = 0.0f;
    float blend = 0.0f;

    mixOutputs(
        cfg,
        zDemand,
        yawCmd,
        servo1Deg,
        servo2Deg,
        power,
        etaZ,
        etaYaw,
        blend
    );


    // ========================================================================
    // LIMITES DE VELOCIDAD DE ACTUADORES
    //
    // El slew se aplica ahora a la salida FINAL, no dentro del PID de altura.
    // Asi tambien se protege al ESC del escalon que introduce el piso de
    // potencia de yaw, y el integrador de altura no se carga contra el slew.
    // ========================================================================

    power = rateLimit(power, ctrl.lastPower, cfg.aslew, dt);
    power = constrain(power, 0.0f, cfg.amax);
    ctrl.lastPower = power;

    servo1Deg = rateLimit(
        servo1Deg, ctrl.lastServo1, SERVO_SLEW_DEG_PER_S, dt);

    servo2Deg = rateLimit(
        servo2Deg, ctrl.lastServo2, SERVO_SLEW_DEG_PER_S, dt);

    servo1Deg = constrain(
        servo1Deg,
        AppConfig::P0025_MIN_DEG,
        AppConfig::P0025_MAX_DEG
    );

    servo2Deg = constrain(
        servo2Deg,
        AppConfig::P0025_MIN_DEG,
        AppConfig::P0025_MAX_DEG
    );

    ctrl.lastServo1 = servo1Deg;
    ctrl.lastServo2 = servo2Deg;


    // ========================================================================
    // ACTUADORES  -  UNICO BLOQUE QUE ESCRIBE
    // ========================================================================

    if (ctx.robot->actuatorsAreArmed()) {

        ctx.robot->commandMotorPowerTest(
            power,
            power,
            pulse(servo1Deg),
            pulse(servo2Deg)
        );
    }
    else {

        // Sin armar: mantener el estado interno coherente para que al armar
        // no haya un salto de potencia ni de servo.
        ctrl.lastPower = 0.0f;
    }


    // ========================================================================
    // ESTADO DE ACTUADORES
    // ========================================================================

    ctx.robot->servo_old1 = servo1Deg;
    ctx.robot->servo_old2 = servo2Deg;

    ctx.robot->motor_power1 = power;
    ctx.robot->motor_power2 = power;


    // ========================================================================
    // DEBUG
    // ========================================================================

    if (elapsedMs(nowMs, ctrl.lastDebugMs, DEBUG_PERIOD_MS)) {

        ctrl.lastDebugMs = nowMs;

        Serial.printf(

            "[P05] %s "
            "zErr=%.3f vz=%.3f zDem=%.3f | "
            "yawErr=%.1f yawVel=%.2f yawCmd=%.3f yawI=%.3f | "
            "blend=%.2f etaZ=%.2f etaY=%.2f | "
            "P=%.3f Fz=%.3f S1=%.1f S2=%.1f\n",

            phaseName(ctrl.phase),

            heightError,
            ctrl.filteredVz,
            zDemand,

            yawError / DEG2RAD,
            yawVel,
            yawCmd,
            ctrl.yawIntegral,

            blend,
            etaZ,
            etaYaw,

            power,
            power * etaZ,      // empuje vertical realmente entregado
            servo1Deg,
            servo2Deg
        );
    }


    // ========================================================================
    // TELEMETRIA
    // ========================================================================

    Telemetry::sendControl(
        ctx,
        yawRef,
        heightRef,
        heightError
    );
}


} // namespace TestRunners
