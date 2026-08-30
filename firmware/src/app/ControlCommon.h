#pragma once
#include <Arduino.h>
#include <math.h>
#include <stdint.h>

// ============================================================================
// ControlCommon.h  -  NUCLEO COMPARTIDO DE CONTROL  (v6)
//
// POR QUE EXISTE ESTE ARCHIVO
// ---------------------------
// Antes, P03 (yaw aislado), P04 (altura aislada) y P05 (yaw+altura) tenian
// TRES implementaciones distintas del mismo control, con unidades distintas:
//
//   - P03 sacaba "fraccion de potencia" y mandaba los servos al extremo
//     (bang-bang). No tenia integral.
//   - P04 sacaba potencia de motor con el integral topado en 6% fijo y sin
//     anti-windup.
//   - P05 v5 sacaba un mando normalizado -1..+1 con mixer y anti-windup.
//
// Consecuencia: las ganancias sintonizadas en P03/P04 NO se podian copiar a
// P05. Este header elimina el problema: los tres tests llaman a las mismas
// funciones, con las mismas unidades, los mismos deadbands, los mismos filtros
// y los mismos topes. Lo que sintonizas en P04 vale en P05. Lo que sintonizas
// en P03 vale en P05.
//
//
// UNIDADES (importante)
// ---------------------
//   ALTURA
//     error        : metros
//     kp           : fraccion de empuje por metro   (0.30 => 30% por metro)
//     ki           : fraccion de empuje por metro-segundo
//     kd           : fraccion de empuje por (m/s)
//     salida       : "zDemand" = empuje VERTICAL deseado, 0..maxPower
//
//   YAW
//     error        : radianes
//     kp           : mando por radian               (2.00 => satura a ~29 deg)
//     ki           : mando por radian-segundo
//     kd           : mando por (rad/s)
//     salida       : "yawCmd" normalizado, -1..+1
//     minPower/maxPower : FRACCION DE POTENCIA DE MOTOR (esto no cambia)
//
//
// CAMBIOS DE FONDO RESPECTO A v5
// ------------------------------
// 1) La zona muerta del yaw vive en el ERROR, no en el mando.
//    En v5 el mixer usaba `|yawCmd| > 0.02`. Con kp bajo eso se traducia en un
//    deadband REAL enorme y oculto: con kp=0.08 el yaw no actuaba hasta 14.3
//    grados de error. Ahora el deadband es explicito y configurable en grados.
//
// 2) La potencia de yaw se interpola en TODO el rango [min, max]:
//        power = min + |yawCmd| * (max - min)
//    En v5 era `clamp(|yawCmd| * max, min, max)`, que con min/max cercanos
//    dejaba la potencia pegada al piso salvo en una ventana estrechisima.
//
// 3) ki de yaw es configurable desde la estacion de tierra (antes estaba
//    fijo en 0.35 dentro del firmware, sin manera de tocarlo sin recompilar).
//
// 4) El tope del integral de altura es maxPower en TODOS los tests.
//    P04 lo tenia clavado en 6%: si la potencia de hover era mayor, el blimp
//    se quedaba colgado bajo el setpoint y parecia un problema de sintonia.
//
// 5) Los paquetes de bits llevan bandera de version (bit 22). Si la estacion
//    de tierra es vieja, el firmware lo detecta y avisa por serial en lugar
//    de interpretar basura en silencio.
// ============================================================================

namespace ControlCommon {

// ============================================================================
// GEOMETRIA DE LOS SERVOS  (posiciones validadas fisicamente en P00/P02)
// ============================================================================
//
//   Vector vertical +Z : S1=35   S2=95
//   Giro izquierda     : S1=0    S2=0
//   Giro derecha       : S1=120  S2=120
//
// OJO CON LA ASIMETRIA: desde Z, el servo 1 recorre 35 grados hasta su extremo
// y el servo 2 recorre 95. Con autoridad alta los angulos quedan muy distintos
// (phi1=-35, phi2=+95), asi que el giro NO es una cupla pura: hay una fuerza
// lateral neta y el blimp deriva mientras gira. Es geometria del montaje, no
// se arregla en software. Caracterizala con P03 antes de pasar a P07.
// ============================================================================

constexpr float SERVO1_Z_DEG     = 35.0f;

// VERTICAL REAL DE LA GONDOLA IZQUIERDA
// -------------------------------------
// Era 85.0. Medido en vuelo el 23/08: con los dos motores al 13% y el servo 2
// en 85 el blimp giraba de forma continua (~28 deg/s antihorario, una vuelta
// completa cada 13 s); a 95 el ascenso sale limpio, sin yaw.
//
// Esos 10 grados de desalineacion producian componente lateral de empuje
// permanente, y eran el par perturbador que aparecia en todos los ensayos:
//   - el offset de 0.043 que el PD de yaw no podia anular en P03
//   - la rotacion continua durante los ensayos de altura P04
//   - el trabajo extra que tenia que hacer el integral de yaw
//
// OJO: esta constante es la base de phi2 = SERVO2_Z_DEG - servo2, o sea de
// toda la tabla de eficiencias etaZ/etaYaw y del mapeo de blend a angulos.
// Al subir de 85 a 95 el recorrido hacia la izquierda pasa de 85 a 95 grados
// y hacia la derecha de 35 a 25, asi que la asimetria entre girar a un lado y
// al otro se acentua. Es geometria del montaje, no se corrige en software.
//
// Pendiente: confirmar si el optimo esta exactamente en 95 (probar 91 y 99) y
// si conviene enderezar la gondola mecanicamente en vez de compensarla aqui.
constexpr float SERVO2_Z_DEG     = 95.0f;
constexpr float SERVO1_LEFT_DEG  = 0.0f;
constexpr float SERVO2_LEFT_DEG  = 0.0f;
constexpr float SERVO1_RIGHT_DEG = 120.0f;
constexpr float SERVO2_RIGHT_DEG = 120.0f;

// Traslacion horizontal: las dos gondolas se inclinan en el MISMO sentido
// fisico, asi que sus comandos van a extremos opuestos (montaje en espejo).
// Las usan P02 (manual) y BalloonMission.
constexpr float SERVO1_FORWARD_DEG = 120.0f;
constexpr float SERVO2_FORWARD_DEG = 0.0f;
constexpr float SERVO1_BACK_DEG    = 0.0f;
constexpr float SERVO2_BACK_DEG    = 120.0f;

constexpr float DEG2RAD = PI / 180.0f;
constexpr float RAD2DEG = 180.0f / PI;


// ============================================================================
// DEFAULTS - ALTURA
// ============================================================================

constexpr float DEF_ALT_KP        = 0.30f;
constexpr float DEF_ALT_KI        = 0.025f;
constexpr float DEF_ALT_KD        = 0.10f;
constexpr float DEF_ALT_MIN_POWER = 0.00f;
constexpr float DEF_ALT_MAX_POWER = 0.15f;
constexpr float DEF_ALT_SLEW      = 0.18f;
constexpr float DEF_ALT_SUCCESS_M = 0.10f;

// Constantes estructurales de altura. IGUALES en P04 y P05: si cambias una,
// cambiala aqui y afecta a los dos, que es justamente lo que queremos.
constexpr float ALT_DEADBAND_M       = 0.025f;

// ZONA DE INTEGRACION DE ALTURA
// -----------------------------
// Era 0.35 m, heredado de configuraciones tipo multirrotor. En un dirigible
// eso es un error de diseno: aqui el integral tiene que aportar el 100% de la
// potencia de sostenimiento (~9.5% en este vehiculo), no un ajuste fino.
//
// Con 0.35 m, cualquier despegue desde el suelo hacia un objetivo a 1 m deja
// el error en ~1.0 m, FUERA de la zona, con el integral congelado en cero. El
// lazo se queda solo con kp*error, que no llega al hover, y el vehiculo nunca
// levanta. Peor: una vez fuera de la zona no hay forma de recuperarse, porque
// el mecanismo que deberia recuperar es justamente el que esta apagado.
//
// 1.5 m cubre todo el espacio util tipico (techos de 2-2.5 m). El anti-windup
// por integracion condicional ya protege contra la saturacion, que era la
// razon original de existir de esta zona.
constexpr float ALT_INTEGRAL_ZONE_M  = 1.50f;

constexpr float VZ_FILTER_ALPHA      = 0.82f;


// ============================================================================
// DEFAULTS - YAW
// ============================================================================

// GANANCIAS DE LA CASCADA DE YAW  (v7 - unidades NUEVAS otra vez)
//
//   ykp : lazo EXTERNO, velocidad deseada por radian de error   [1/s]
//   ykd : lazo INTERNO, mando por (rad/s) de error de velocidad
//   yki : lazo INTERNO, integral del error de velocidad
//
// Los valores de v6 (ykp 0.5, ykd 6.0) NO sirven aqui: ahi ykp iba de
// angulo a mando directo; ahora va de angulo a velocidad deseada.
// VALORES VALIDADOS EN VUELO EL 23/08 (P03, cinco corridas comparadas).
// Estos defaults NO son adivinados: BalloonMission los usa directamente, asi
// que la mision hereda la misma sintonia que P03/P05/P07.
//
//   kd    amplitud   desviacion   sobrepico
//   1.2     36.8°       9.2°        11.8%
//   3.0     27.6°       6.3°        54.7%
//   5.0     22.8°       5.6°         6.6%
//   7.0     18.0°       3.9°         5.1%
//   9.0     16.2°       3.6°        <- elegido
//
// Con tau ~125 s en el eje de yaw (practicamente sin amortiguamiento
// aerodinamico) TODO el amortiguamiento tiene que venir del control, de ahi
// que kd/kp llegue a 22. No es un error de escala.
constexpr float DEF_YAW_KP           = 0.40f;
constexpr float DEF_YAW_KI           = 1.00f;
constexpr float DEF_YAW_KD           = 9.00f;
constexpr float DEF_YAW_MIN_POWER    = 0.045f;
constexpr float DEF_YAW_MAX_POWER    = 0.110f;
// 6 grados: validado en P03. Por debajo el vehiculo persigue ruido; el error
// permanente medido con esta zona muerta fue de 0.4 grados.
constexpr float DEF_YAW_DEADBAND_DEG = 6.0f;
constexpr float DEF_YAW_AUTHORITY    = 0.75f;

// Constantes estructurales de yaw.
// SATURACION DEL ERROR DE ANGULO
// ------------------------------
// El lazo externo trunca el error antes de convertirlo en velocidad deseada.
// Sin esto, un escalon de 120 grados pedia el triple de velocidad que uno de
// 40, y el exceso se pagaba en sobrepico: medimos 76% con giros grandes.
// Con el truncado, cualquier error mayor a 36 grados pide la MISMA velocidad
// maxima, y el giro grande se resuelve como una sucesion de giros pequenos.
constexpr float YAW_ERROR_CLAMP_RAD    = 36.0f * DEG2RAD;

constexpr float YAW_INTEGRAL_LIMIT     = 0.60f;

// SIGNO DEL GIROSCOPIO
// --------------------
// El BNO085 entrega angVelZ con signo OPUESTO a la derivada del yaw que
// produce quaternionToEuler. Verificado sobre 24 logs de vuelo del 23/08:
// correlacion negativa en TODOS, sin excepcion, con pendiente media -0.76
// (los logs de rotacion limpia y sostenida dan -0.99).
//
// Si algun dia se cambia el IMU o la conversion a Euler, hay que volver a
// medirlo: correlacionar d(yaw)/dt contra sensors[YAW_RATE] sobre un vuelo
// con rotacion sostenida. Con el signo equivocado el lazo interno se vuelve
// realimentacion POSITIVA y el vehiculo gira sin control.
constexpr float YAW_RATE_SIGN          = -1.0f;
// FILTRO DE VELOCIDAD DE YAW
// --------------------------
// El lazo corre a 250 Hz (CONTROL_LOOP_US = 4000) pero el BNO085 reporta mas
// lento. Entre reportes, (yaw - lastYaw) vale exactamente cero; cuando llega
// uno nuevo, todo el salto cae en un solo tick y se divide por 0.004 s. La
// senal derivada es un tren de ceros y picos.
//
// Con alpha = 0.80 la constante de tiempo era de solo 20 ms: practicamente no
// filtraba, y ese ruido entraba multiplicado por ykd directo a la inclinacion
// de los servos (medido: +/-5 grados de temblor con ykd = 2.5).
//
// Con 0.95 la constante sube a ~80 ms. Frente al retardo del eje de yaw, que
// es de varios segundos, ese desfase es despreciable.
//
// OJO - NO USAR SensorMap::YAW_RATE COMO ATAJO:
// El BNO085 entrega angVelZ del giroscopio y parece la solucion obvia, pero
// esta INVERTIDA respecto de la derivada del angulo yaw que produce
// quaternionToEuler (verificado sobre datos de vuelo: correlacion -0.98,
// pendiente -0.96). Usarla sin negar el signo convierte el termino derivativo
// en realimentacion POSITIVA. Si algun dia se migra a esa senal, hay que
// negarla y volver a verificar la correlacion con datos reales.
constexpr float YAW_RATE_FILTER_ALPHA  = 0.95f;

// INCLINACION FIJA PARA GIRAR
// ---------------------------
// Antes valia 0.45 y la inclinacion se modulaba con el error:
//
//     shaped = 0.45 + 0.55 * min(2*|yawCmd|, 1)
//
// La intencion era dar control fino cerca del objetivo, pero para este
// vehiculo salia caro:
//
//   1) A media inclinacion se pierde ~40% del par por unidad de potencia
//      (etaYaw 0.49 contra 0.785 a inclinacion completa). Todo ese empuje se
//      va hacia arriba, y en P03 no sirve para nada.
//
//   2) ykp terminaba controlando DOS cosas a la vez, la inclinacion y la
//      potencia, con puntos de saturacion distintos (14 y 29 grados de error).
//      Eso hacia la sintonia casi imposible: mover ykp cambiaba dos lazos.
//
// Con 1.00 la inclinacion es FIJA mientras haya demanda de yaw, y el angulo lo
// elige --yaw-servo-authority. Los servos solo se mueven al cambiar de
// sentido, no continuamente. La POTENCIA queda como unico mando modulado, que
// es como funciona fisicamente el vehiculo:  par = P * etaYaw.
//
// Eleccion de autoridad (etaZ = sustentacion, etaYaw = par):
//
//     autoridad  servo1  servo2   etaZ   etaYaw
//        50.0%    17.5    42.5    0.845   0.488
//        75.0%     8.8    21.2    0.670   0.670   <- equilibrio, usar en P05
//       100.0%     0.0     0.0    0.453   0.785   <- todo el par, usar en P03
//
// Se pierde resolucion de par cerca de cero, pero la zona muerta configurable
// (--yaw-deadband-deg) ya cubre esa region.
constexpr float YAW_TILT_MIN_FRACTION  = 1.00f;

// Con que rapidez la inclinacion llega al maximo.
// 2.0 => inclinacion saturada con |yawCmd| >= 0.5
constexpr float YAW_TILT_SHAPE         = 2.00f;

// Piso de eficiencia vertical usado en la division. Evita que la compensacion
// explote cuando el conjunto esta muy inclinado.
constexpr float ETA_Z_FLOOR            = 0.35f;

constexpr float SERVO_SLEW_DEG_PER_S   = 200.0f;


// ============================================================================
// PAQUETES DE BITS
//
// ControlInput solo tiene 9 huecos utiles (FX, FZ, TX, TZ, AUX0..AUX4) y P05
// necesita 2 referencias + 6 ganancias + 8 limites. Los limites viajan
// empaquetados en dos floats. Un float32 representa enteros exactos hasta
// 2^24, asi que los bits 0..23 son seguros.
//
// DETECCION DE VERSION
// --------------------
// En el formato v1 los bits 0..22 eran TODOS payload, asi que no queda ningun
// bit libre ahi para marcar la version: un paquete v1 con success_cm >= 20 o
// con authority = 100% activa el bit 22 por casualidad y se leeria como v2,
// produciendo limites basura EN SILENCIO. Que es justo lo que queremos evitar.
//
// La solucion es salir del rango que v1 podia generar. v1 vivia en
// [2^23, 2^24). v2 pone su marcador en el bit 24 y desplaza el payload un bit
// a la izquierda, de modo que:
//
//   - todo valor v2 es >= 2^24  ->  imposible de confundir con v1
//   - todo valor v2 es PAR      ->  exacto en float32
//
// Lo segundo importa: float32 tiene 24 bits de mantisa, asi que por encima de
// 2^24 el espaciado es 2 y solo los enteros pares son representables sin
// error. Por eso el bit 0 queda siempre en cero.
//
//   bit 24      : marcador de version 2
//   bits 1..22  : payload (22 bits)
//   bit 0       : siempre 0 (mantiene el valor par)
//
//   valor maximo = 2^24 + 0x7FFFFE = 25165822  ->  par, exacto
// ============================================================================

constexpr uint32_t PACK_MARKER_V1   = (1UL << 23);
constexpr uint32_t PACK_MARKER_V2   = (1UL << 24);
constexpr uint32_t PACK_PAYLOAD_MASK = 0x3FFFFFUL;   // 22 bits

enum class PackStatus : uint8_t {
    ABSENT     = 0,   // no llego nada -> defaults
    LEGACY_V1  = 1,   // estacion de tierra desactualizada -> defaults + aviso
    OK_V2      = 2,   // configuracion valida
};

// Tabla de ki de yaw. 3 bits => 8 valores. Fina cerca de cero, que es donde
// importa, y suficientemente amplia arriba.
// Tabla de ki del lazo INTERNO de velocidad. El rango subio respecto de v6
// porque ahora el integral actua sobre el error de velocidad (valores tipicos
// del orden de 0.05 rad/s), no sobre el de angulo.
constexpr float YAW_KI_TABLE[8] = {
    0.00f, 0.25f, 0.50f, 1.00f, 2.00f, 3.00f, 5.00f, 8.00f
};


struct AltLimits {
    float minPower;
    float maxPower;
    float slew;
    float successM;
};

struct YawLimits {
    float minPower;
    float maxPower;
    float deadbandRad;
    float authority;
    float ki;
};


inline AltLimits defaultAltLimits() {
    return { DEF_ALT_MIN_POWER, DEF_ALT_MAX_POWER, DEF_ALT_SLEW, DEF_ALT_SUCCESS_M };
}

inline YawLimits defaultYawLimits() {
    return { DEF_YAW_MIN_POWER, DEF_YAW_MAX_POWER,
             DEF_YAW_DEADBAND_DEG * DEG2RAD, DEF_YAW_AUTHORITY, DEF_YAW_KI };
}


// ----------------------------------------------------------------------------
// ALT_PACK v2
//   bits  0.. 6 (7) : potencia minima, % entero        0..127
//   bits  7..13 (7) : potencia maxima, % entero        0..127
//   bits 14..17 (4) : slew / 0.02                      1..15  => 0.02..0.30 /s
//   bits 18..21 (4) : banda de exito / 2 cm            1..15  => 2..30 cm
//   bit  22         : version 2
//   bit  23         : marcador
// ----------------------------------------------------------------------------

inline PackStatus decodeAltPack(float raw, AltLimits& out) {

    out = defaultAltLimits();

    if (!isfinite(raw) || raw <= 1.0f) {
        return PackStatus::ABSENT;
    }

    const uint32_t packed = (uint32_t)lroundf(raw);

    if ((packed & PACK_MARKER_V2) == 0U) {
        return (packed & PACK_MARKER_V1)
            ? PackStatus::LEGACY_V1
            : PackStatus::ABSENT;
    }

    const uint32_t p = (packed >> 1) & PACK_PAYLOAD_MASK;

    const uint32_t minPct  =  p        & 0x7FU;
    const uint32_t maxPct  = (p >>  7) & 0x7FU;
    const uint32_t slewU   = (p >> 14) & 0x0FU;
    const uint32_t okUnits = (p >> 18) & 0x0FU;

    out.minPower = constrain((float)minPct / 100.0f, 0.0f, 1.0f);
    out.maxPower = constrain((float)maxPct / 100.0f, 0.01f, 1.0f);

    out.slew = (slewU == 0U)
        ? DEF_ALT_SLEW
        : constrain((float)slewU * 0.02f, 0.02f, 0.30f);

    out.successM = (okUnits == 0U)
        ? DEF_ALT_SUCCESS_M
        : constrain((float)okUnits * 0.02f, 0.02f, 0.30f);

    if (out.minPower > out.maxPower) {
        out.minPower = 0.0f;
    }

    return PackStatus::OK_V2;
}


// ----------------------------------------------------------------------------
// YAW_PACK v2
//   bits  0.. 5 (6) : potencia minima / 0.5 %          0..63 => 0..31.5 %
//   bits  6..11 (6) : potencia maxima / 0.5 %          0..63 => 0..31.5 %
//   bits 12..15 (4) : zona muerta en grados enteros    1..15  (0 => default)
//   bits 16..18 (3) : autoridad de servo, indice       (i+1) * 12.5 %
//   bits 19..21 (3) : ki de yaw, indice en YAW_KI_TABLE
//   bit  22         : version 2
//   bit  23         : marcador
// ----------------------------------------------------------------------------

inline PackStatus decodeYawPack(float raw, YawLimits& out) {

    out = defaultYawLimits();

    if (!isfinite(raw) || raw <= 1.0f) {
        return PackStatus::ABSENT;
    }

    const uint32_t packed = (uint32_t)lroundf(raw);

    if ((packed & PACK_MARKER_V2) == 0U) {
        return (packed & PACK_MARKER_V1)
            ? PackStatus::LEGACY_V1
            : PackStatus::ABSENT;
    }

    const uint32_t p = (packed >> 1) & PACK_PAYLOAD_MASK;

    const uint32_t minHalf  =  p        & 0x3FU;
    const uint32_t maxHalf  = (p >>  6) & 0x3FU;
    const uint32_t dbDeg    = (p >> 12) & 0x0FU;
    const uint32_t authIdx  = (p >> 16) & 0x07U;
    const uint32_t kiIdx    = (p >> 19) & 0x07U;

    out.minPower = constrain((float)minHalf * 0.005f, 0.0f, 0.315f);
    out.maxPower = constrain((float)maxHalf * 0.005f, 0.005f, 0.315f);

    out.deadbandRad = (dbDeg == 0U)
        ? (DEF_YAW_DEADBAND_DEG * DEG2RAD)
        : ((float)dbDeg * DEG2RAD);

    out.authority = constrain((float)(authIdx + 1U) * 0.125f, 0.125f, 1.0f);

    out.ki = YAW_KI_TABLE[kiIdx];

    if (out.minPower > out.maxPower) {
        out.minPower = 0.0f;
    }

    return PackStatus::OK_V2;
}


inline const char* packStatusName(PackStatus s) {
    switch (s) {
        case PackStatus::OK_V2:     return "OK";
        case PackStatus::LEGACY_V1: return "ESTACION DE TIERRA VIEJA -> DEFAULTS";
        default:                    return "SIN CONFIG -> DEFAULTS";
    }
}


// ============================================================================
// VALIDACION POR CAMPO
//
// Nunca descartamos toda la configuracion porque un campo venga mal: eso hacia
// que un solo valor invalido tumbara silenciosamente TODAS las ganancias del
// operador y el vehiculo volara con defaults sin avisar.
// ============================================================================

inline float pickPositive(float v, float fallback, float lo, float hi) {
    if (!isfinite(v) || v <= 0.0f) return fallback;
    return constrain(v, lo, hi);
}

inline float pickNonNegative(float v, float fallback, float lo, float hi) {
    if (!isfinite(v) || v < 0.0f) return fallback;
    return constrain(v, lo, hi);
}


// ============================================================================
// UTILIDADES
// ============================================================================

inline float wrapPi(float a) {
    while (a >  PI) a -= 2.0f * PI;
    while (a < -PI) a += 2.0f * PI;
    return a;
}

inline float lerpFloat(float a, float b, float t) {
    t = constrain(t, 0.0f, 1.0f);
    return a + (b - a) * t;
}

inline float signOf(float v) {
    return (v >= 0.0f) ? 1.0f : -1.0f;
}

inline float rateLimit(float target, float previous, float maxRate, float dt) {
    const float maxDelta = maxRate * dt;
    return constrain(target, previous - maxDelta, previous + maxDelta);
}

inline bool elapsedMs(uint32_t now, uint32_t since, uint32_t duration) {
    return (uint32_t)(now - since) >= duration;
}

inline int degreesToPulseUs(float deg,
                            float minDeg, float maxDeg,
                            int minUs, int maxUs) {
    const float c = constrain(deg, minDeg, maxDeg);
    return (int)lroundf(minUs + (c - minDeg) * (float)(maxUs - minUs) / (maxDeg - minDeg));
}


// ============================================================================
// EFICIENCIAS REALES DEL CONJUNTO
//
//   phi1 = servo1 - 35        (inclinacion de la gondola 1 respecto de Z)
//   phi2 = 85 - servo2        (montaje en espejo)
//
//   etaZ   = componente vertical media -> sustentacion por unidad de potencia
//   etaYaw = componente lateral media  -> par de yaw por unidad de potencia
//
//   blend   S1     S2     etaZ    etaYaw
//   0.00   35.0   85.0    1.000   0.000
//   0.30   24.5   59.5    0.943   0.306
//   0.50   17.5   42.5    0.845   0.488
//   0.75    8.8   23.8    0.749   0.667
//   1.00    0.0    0.0    0.453   0.785
//
// Girar SIEMPRE empuja un poco hacia arriba (etaZ > 0 siempre). El lazo de
// altura solo puede compensarlo bajando su propia demanda hasta 0. Se minimiza
// con blend alto y maxPower de yaw lo mas bajo posible que aun gire.
// ============================================================================

inline void computeEfficiencies(float servo1Deg, float servo2Deg,
                                float& etaZ, float& etaYaw) {

    const float phi1 = (servo1Deg - SERVO1_Z_DEG) * DEG2RAD;
    const float phi2 = (SERVO2_Z_DEG - servo2Deg) * DEG2RAD;

    etaZ   = 0.5f * (cosf(phi1) + cosf(phi2));
    etaYaw = 0.5f * (fabsf(sinf(phi1)) + fabsf(sinf(phi2)));

    etaZ   = constrain(etaZ,   0.0f, 1.0f);
    etaYaw = constrain(etaYaw, 0.0f, 1.0f);
}


// blend > 0 : izquierda / antihorario (servos hacia 0)
// blend < 0 : derecha / horario       (servos hacia 120)
inline void servosFromBlend(float blend, float& servo1Deg, float& servo2Deg) {

    const float m = constrain(fabsf(blend), 0.0f, 1.0f);

    if (blend >= 0.0f) {
        servo1Deg = lerpFloat(SERVO1_Z_DEG, SERVO1_LEFT_DEG, m);
        servo2Deg = lerpFloat(SERVO2_Z_DEG, SERVO2_LEFT_DEG, m);
    } else {
        servo1Deg = lerpFloat(SERVO1_Z_DEG, SERVO1_RIGHT_DEG, m);
        servo2Deg = lerpFloat(SERVO2_Z_DEG, SERVO2_RIGHT_DEG, m);
    }
}


// ============================================================================
// LAZO DE ALTURA
// ============================================================================

struct AltGains {
    float kp;
    float ki;
    float kd;
};

struct AltState {
    float filteredVz = 0.0f;
    float integral   = 0.0f;
    float lastPower  = 0.0f;
};


inline void resetAltState(AltState& s, float vz) {
    s.filteredVz = isfinite(vz) ? vz : 0.0f;
    s.integral   = 0.0f;
    s.lastPower  = 0.0f;
}

inline float updateVz(AltState& s, float rawVz) {
    const float safe = isfinite(rawVz) ? rawVz : 0.0f;
    s.filteredVz = VZ_FILTER_ALPHA * s.filteredVz + (1.0f - VZ_FILTER_ALPHA) * safe;
    return s.filteredVz;
}


// Devuelve la DEMANDA DE EMPUJE VERTICAL, no la potencia de motor.
// En P04 (servos siempre en Z, etaZ = 1) las dos cosas coinciden. En P05 el
// mixer convierte la demanda en potencia segun la inclinacion real.
//
// Anti-windup por integracion condicional: si la salida esta saturada y el
// error empuja en la misma direccion, no se integra.
//
// El tope del integral es maxPower, IGUAL en P04 y P05. Para un blimp esto es
// lo correcto: el integral tiene que poder llegar a la potencia de hover.
inline float computeAltDemand(const AltGains& g,
                              const AltLimits& lim,
                              AltState& s,
                              float heightError,
                              float dt) {

    if (!isfinite(heightError) || !isfinite(dt)) {
        return 0.0f;
    }

    const float pError = (fabsf(heightError) < ALT_DEADBAND_M) ? 0.0f : heightError;

    float raw = g.kp * pError + s.integral - g.kd * s.filteredVz;

    if (fabsf(heightError) < ALT_INTEGRAL_ZONE_M) {

        const bool satHigh = (raw >= lim.maxPower) && (heightError > 0.0f);
        const bool satLow  = (raw <= 0.0f)         && (heightError < 0.0f);

        if (!satHigh && !satLow) {
            s.integral += g.ki * heightError * dt;
            s.integral  = constrain(s.integral, 0.0f, lim.maxPower);
            raw = g.kp * pError + s.integral - g.kd * s.filteredVz;
        }
    }

    float demand = constrain(raw, 0.0f, lim.maxPower);

    if (demand > 0.0f && demand < lim.minPower) {
        demand = lim.minPower;
    }

    return demand;
}


// ============================================================================
// LAZO DE YAW
// ============================================================================

struct YawState {
    float lastYaw      = 0.0f;
    float filteredRate = 0.0f;
    float integral     = 0.0f;   // integral del lazo INTERNO (de velocidad)
    bool  primed       = false;
};


inline void resetYawState(YawState& s, float yaw) {
    s.lastYaw      = isfinite(yaw) ? yaw : 0.0f;
    s.filteredRate = 0.0f;
    s.integral     = 0.0f;
    s.primed       = isfinite(yaw);
}


// ----------------------------------------------------------------------------
// VELOCIDAD DE YAW
//
// Prefiere el GIROSCOPIO (SensorMap::YAW_RATE), negado por YAW_RATE_SIGN.
// Es una medida directa, no una derivada numerica: el lazo corre a 250 Hz
// pero el IMU reporta mas lento, asi que derivar el angulo daba un tren de
// ceros y picos que entraba multiplicado por ykd a los actuadores.
//
// Si el giroscopio no esta disponible, cae al metodo anterior de derivar el
// angulo. El filtro es mas suave para el giroscopio porque la senal ya viene
// limpia del sensor.
// ----------------------------------------------------------------------------
inline float updateYawRate(YawState& s, float yaw, float measuredRate, float dt) {

    if (isfinite(yaw)) {
        s.lastYaw = yaw;
        s.primed  = true;
    }

    if (isfinite(measuredRate)) {
        const float r = YAW_RATE_SIGN * measuredRate;
        s.filteredRate = 0.70f * s.filteredRate + 0.30f * r;
        return s.filteredRate;
    }

    // ---- respaldo: derivar el angulo ----
    if (!isfinite(yaw) || dt <= 0.0f || !s.primed) {
        return s.filteredRate;
    }

    const float rawRate = wrapPi(yaw - s.lastYaw) / dt;

    s.filteredRate = YAW_RATE_FILTER_ALPHA * s.filteredRate +
                     (1.0f - YAW_RATE_FILTER_ALPHA) * rawRate;

    return s.filteredRate;
}


// ============================================================================
// CONTROL DE YAW EN CASCADA  (v7)
//
// POR QUE CASCADA
// ---------------
// El PID directo de v6 tenia tres problemas que la cascada resuelve de raiz:
//
//  1) SOBREPICO EN ESCALONES GRANDES. ykp*error crecia sin limite util, asi
//     que un giro de 120 grados pedia el triple de mando que uno de 40.
//     Medido: 76% de sobrepico. Ahora el error se trunca a 36 grados antes
//     de convertirse en velocidad deseada, y cualquier giro grande se
//     resuelve como una sucesion de giros pequenos identicos.
//
//  2) RUIDO EN EL DERIVATIVO. El termino -kd*rate usaba la derivada numerica
//     del angulo. Ahora el lazo interno cierra sobre el giroscopio, que es
//     una medida directa.
//
//  3) PAR PERTURBADOR CONSTANTE. El integral de v6 actuaba sobre el error de
//     ANGULO, lejos de donde esta la perturbacion. Aqui el integral vive en
//     el lazo de VELOCIDAD, que es exactamente donde el par entra al sistema:
//     si un par constante frena el giro, el error de velocidad persiste y el
//     integral lo anula directamente.
//
// ESTRUCTURA
// ----------
//   externo:  e_ang  = clamp(wrapPi(ref - yaw), +/-36 grados)
//             w_des  = ykp * e_ang                        [rad/s]
//
//   interno:  e_rate = w_des - rate_medida
//             I     += yki * e_rate * dt
//             cmd    = ykd * e_rate + I
//
// La zona muerta actua sobre el error de ANGULO y es una compuerta dura:
// dentro de la banda el yaw no hace nada y el integral se congela.
// ============================================================================

struct YawGains {
    float kp;   // externo: rad/s por radian de error   [1/s]
    float ki;   // interno: integral del error de velocidad
    float kd;   // interno: mando por (rad/s) de error
};


inline float computeYawCommand(const YawGains& g,
                               const YawLimits& lim,
                               YawState& s,
                               float yawError,
                               float yawRate,
                               float dt) {

    if (!isfinite(yawError) || !isfinite(yawRate) || !isfinite(dt)) {
        return 0.0f;
    }

    // Compuerta de zona muerta: dentro de la banda no se actua y el integral
    // se congela (no se borra, para no perder el trim aprendido).
    if (fabsf(yawError) <= lim.deadbandRad) {
        return 0.0f;
    }

    // ---------------- lazo externo: angulo -> velocidad deseada -------------
    const float eAng = constrain(yawError,
                                 -YAW_ERROR_CLAMP_RAD,
                                  YAW_ERROR_CLAMP_RAD);

    const float wDes = g.kp * eAng;

    // ---------------- lazo interno: velocidad -> mando ----------------------
    const float eRate = wDes - yawRate;

    float raw = g.kd * eRate + s.integral;

    // Anti-windup por integracion condicional: si el mando ya esta saturado y
    // el error empuja en la misma direccion, no se integra.
    const bool satHigh = (raw >=  1.0f) && (eRate > 0.0f);
    const bool satLow  = (raw <= -1.0f) && (eRate < 0.0f);

    if (!satHigh && !satLow) {
        s.integral += g.ki * eRate * dt;
        s.integral  = constrain(s.integral, -YAW_INTEGRAL_LIMIT, YAW_INTEGRAL_LIMIT);
        raw = g.kd * eRate + s.integral;
    }

    return constrain(raw, -1.0f, 1.0f);
}


inline bool yawIsActive(float yawCmd) {
    return fabsf(yawCmd) > 1.0e-3f;
}


// Inclinacion pedida por el mando de yaw, con piso para no quedarse en la
// zona ineficiente.
inline float yawBlendFromCommand(float yawCmd, float authority) {

    if (!yawIsActive(yawCmd)) {
        return 0.0f;
    }

    const float shaped = constrain(
        YAW_TILT_MIN_FRACTION +
        (1.0f - YAW_TILT_MIN_FRACTION) *
        constrain(YAW_TILT_SHAPE * fabsf(yawCmd), 0.0f, 1.0f),
        0.0f, 1.0f);

    return signOf(yawCmd) * shaped * authority;
}


// Potencia que el yaw reclama.
//
// Interpolacion lineal en TODO el rango [min, max]. En v5 era
// clamp(|yawCmd| * max, min, max), que con min y max cercanos dejaba la
// potencia pegada al piso salvo en una ventana estrechisima, y el yaw se
// comportaba como un rele on/off.
inline float yawPowerFromCommand(float yawCmd, const YawLimits& lim) {

    if (!yawIsActive(yawCmd)) {
        return 0.0f;
    }

    const float span = lim.maxPower - lim.minPower;

    return constrain(lim.minPower + fabsf(yawCmd) * span,
                     0.0f, lim.maxPower);
}


// ============================================================================
// MIXER COMPARTIDO
//
//   zDemand : empuje vertical deseado    (P04/P05: PID de altura;
//                                         P03: potencia base constante)
//   yawCmd  : mando de yaw normalizado
//
//   1) la inclinacion sale del mando de yaw
//   2) se calculan las eficiencias reales de esa inclinacion
//   3) pAlt = zDemand / etaZ     (exacto: el empuje vertical es power * etaZ)
//      pYaw = interpolacion en [min, max]
//   4) power = max(pAlt, pYaw)
//
// El paso 4 es lo esencial: ninguno de los dos lazos puede dejar al otro sin
// actuador. La altura no le quita empuje al yaw y el yaw no reduce el empuje
// que la altura necesita.
// ============================================================================

inline void mixOutputs(float zDemand,
                       float yawCmd,
                       const YawLimits& yawLim,
                       float maxPower,
                       float& servo1Deg,
                       float& servo2Deg,
                       float& power,
                       float& etaZ,
                       float& etaYaw,
                       float& blend) {

    blend = yawBlendFromCommand(yawCmd, yawLim.authority);

    servosFromBlend(blend, servo1Deg, servo2Deg);

    computeEfficiencies(servo1Deg, servo2Deg, etaZ, etaYaw);

    const float pAlt = zDemand / fmaxf(etaZ, ETA_Z_FLOOR);
    const float pYaw = yawPowerFromCommand(yawCmd, yawLim);

    power = constrain(fmaxf(pAlt, pYaw), 0.0f, maxPower);
}


// ============================================================================
// ASIGNACION ANALITICA DE EMPUJE (avance + altura)
//
// Fuente: Xu et al., "MochiSwarm: A testbed for robotic blimps in realistic
// environments" (arXiv:2503.03077), ecuaciones (4),(5),(9)-(11). El paper
// resuelve el angulo de cada rotor por trigonometria directa a partir de la
// fuerza horizontal y vertical deseadas, en vez de fijar una inclinacion de
// antemano y despues subir potencia para compensar la sustentacion perdida
// (que es lo que hacia BalloonMission::update() antes de esto: inclinar los
// servos a una FRACCION FIJA del recorrido hacia adelante y dividir la
// demanda de altura entre la eficiencia resultante -- funcionaba, pero
// dejaba de sobra mas componente vertical de la necesaria, o mas horizontal
// de la que el motor podia sostener, segun el punto de operacion).
//
// Aca no hay reparto de yaw todavia (tauZ=0 en el paper): los dos motores
// reciben la MISMA fx y fz, asi que power1 siempre sale igual a power2. El
// angulo de cada servo SI difiere entre motor 1 y 2 porque cada gondola
// tiene su propia geometria (SERVO*_Z_DEG/FORWARD_DEG/BACK_DEG, calibrados
// por separado en P00/P02 por la asimetria del montaje).
//
//   fx : empuje horizontal deseado, fraccion de potencia. Positivo = avanza,
//        negativo = retrocede.
//   fz : empuje vertical deseado, fraccion de potencia. Se recorta a >= 0:
//        este vehiculo no tiene vector activo hacia -Z (ver README, "no
//        existe empuje activo hacia -Z").
//   maxPower : tope de potencia por motor.
//
// La altura tiene PRIORIDAD sobre el avance: si fx y fz juntos pedirian mas
// de maxPower, se recorta fx (nunca fz) para no perder sustentacion --mismo
// criterio de seguridad que mixOutputs() usa con max(pAlt,pYaw), adaptado a
// que aca hay una sola potencia por motor que se reparte en ANGULO en vez
// de en dos mandos separados.
// ============================================================================
inline void computeThrustAllocation(float fx, float fz, float maxPower,
                                    float& servo1Deg, float& servo2Deg,
                                    float& power1, float& power2) {
    const float fzClamped = constrain(fz, 0.0f, maxPower);

    const float maxFxAvailable = sqrtf(
        fmaxf(maxPower * maxPower - fzClamped * fzClamped, 0.0f));
    const float fxClamped = constrain(fx, -maxFxAvailable, maxFxAvailable);

    const float power = sqrtf(fxClamped * fxClamped + fzClamped * fzClamped);

    // Angulo del vector de empuje medido desde la vertical: 0 = vector Z
    // puro, PI/2 = horizontal puro. m es la misma fraccion de recorrido
    // Z->FORWARD/BACK que usaban servosFromBlend()/APPROACH_FORWARD_FRACTION,
    // pero calculada exactamente en vez de fijada de antemano.
    const float angleFromVertical = atan2f(fabsf(fxClamped), fzClamped);
    const float m = constrain(angleFromVertical / (PI * 0.5f), 0.0f, 1.0f);

    const float s1Target = (fxClamped >= 0.0f) ? SERVO1_FORWARD_DEG : SERVO1_BACK_DEG;
    const float s2Target = (fxClamped >= 0.0f) ? SERVO2_FORWARD_DEG : SERVO2_BACK_DEG;

    servo1Deg = SERVO1_Z_DEG + (s1Target - SERVO1_Z_DEG) * m;
    servo2Deg = SERVO2_Z_DEG + (s2Target - SERVO2_Z_DEG) * m;
    power1 = power;
    power2 = power;
}

} // namespace ControlCommon