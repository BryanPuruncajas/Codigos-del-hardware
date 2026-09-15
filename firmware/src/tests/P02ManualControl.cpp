#include "tests/TestRunners.h"
#include "app/AppConfig.h"
#include "app/ControlCommon.h"
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


// Calibracion fisica validada en P00/P00M (16-08-2026)

// P02 es una prueba manual limitada: usamos la potencia que ya fue validada
// fisicamente en P00M para no depender del mixer/PID general.
constexpr float P02_MOTOR_POWER = 0.20f;
constexpr float COMMAND_DEADBAND = 0.001f;

// Magnitud de fx/tz que corresponde a POTENCIA MAXIMA (P02_MOTOR_POWER).
// Son los mismos valores que ya mandaban los botones fijos de la app/CLI
// (fx=+/-0.12, tz=+/-0.05): con eso, un mando/joystick a fondo llega
// exactamente a la potencia ya validada, y a mitad de camino llega a la
// mitad -- antes cualquier magnitud por encima del deadband disparaba
// siempre el 100%, así que un joystick no aportaba nada sobre un boton.
constexpr float P02_FX_FULL_SCALE = 0.12f;
constexpr float P02_TZ_FULL_SCALE = 0.05f;
constexpr float P02_FZ_FULL_SCALE = 0.12f;

int degreesToPulseUs(float deg) {
    const float clipped = constrain(deg,
                                    AppConfig::SERVO_ANGLE_MIN_DEG,
                                    AppConfig::SERVO_ANGLE_MAX_DEG);
    const float spanDeg = AppConfig::SERVO_ANGLE_MAX_DEG - AppConfig::SERVO_ANGLE_MIN_DEG;
    const float spanUs = (float)(AppConfig::SERVO_PULSE_MAX_US - AppConfig::SERVO_PULSE_MIN_US);

    return (int)lroundf(AppConfig::SERVO_PULSE_MIN_US +
                        (clipped - AppConfig::SERVO_ANGLE_MIN_DEG) * spanUs / spanDeg);
}

} // namespace

namespace TestRunners {

void p02Manual(AppContext& ctx) {
    const float fx = ctx.command.params[AppConfig::PARAM_FX];
    const float fz = ctx.command.params[AppConfig::PARAM_FZ];
    const float tz = ctx.command.params[AppConfig::PARAM_TZ];

    // Posicion segura/base: ambos vectores de empuje alineados con Z.
    float servo1Deg = SERVO1_Z_DEG;
    float servo2Deg = SERVO2_Z_DEG;
    float motor1Power = 0.0f;
    float motor2Power = 0.0f;

    // P02 prueba un eje por vez. Si por error llegan varios comandos a la vez,
    // se ejecuta el de mayor magnitud para evitar combinaciones no validadas.
    const float absFx = fabsf(fx);
    const float absFz = fabsf(fz);
    const float absTz = fabsf(tz);

    if (absFx > COMMAND_DEADBAND && absFx >= absFz && absFx >= absTz) {
        const float scale = constrain(absFx / P02_FX_FULL_SCALE, 0.0f, 1.0f);
        motor1Power = P02_MOTOR_POWER * scale;
        motor2Power = P02_MOTOR_POWER * scale;

        if (fx > 0.0f) {
            // AVANCE validado: S1=120, S2=0
            servo1Deg = SERVO1_FORWARD_DEG;
            servo2Deg = SERVO2_FORWARD_DEG;
        } else {
            // RETROCESO validado: S1=0, S2=120
            servo1Deg = SERVO1_BACK_DEG;
            servo2Deg = SERVO2_BACK_DEG;
        }
    }
    else if (absTz > COMMAND_DEADBAND && absTz >= absFz) {
        const float scale = constrain(absTz / P02_TZ_FULL_SCALE, 0.0f, 1.0f);
        motor1Power = P02_MOTOR_POWER * scale;
        motor2Power = P02_MOTOR_POWER * scale;

        if (tz > 0.0f) {
            // GIRO DERECHA validado: S1=120, S2=120
            servo1Deg = SERVO1_RIGHT_DEG;
            servo2Deg = SERVO2_RIGHT_DEG;
        } else {
            // GIRO IZQUIERDA validado: S1=0, S2=0
            servo1Deg = SERVO1_LEFT_DEG;
            servo2Deg = SERVO2_LEFT_DEG;
        }
    }
    else if (absFz > COMMAND_DEADBAND) {
        // Empuje sobre Z validado mecanicamente: S1=35, S2=85.
        // Los brushless son unidireccionales y no se ha validado una orientacion
        // activa para -Z dentro del limite mecanico 0..180 grados.
        // Por seguridad: +Fz aplica empuje; -Fz corta motores (descenso/pasivo).
        servo1Deg = SERVO1_Z_DEG;
        servo2Deg = SERVO2_Z_DEG;

        if (fz > 0.0f) {
            const float scale = constrain(absFz / P02_FZ_FULL_SCALE, 0.0f, 1.0f);
            motor1Power = P02_MOTOR_POWER * scale;
            motor2Power = P02_MOTOR_POWER * scale;
        }
    }

    const int servo1Us = degreesToPulseUs(servo1Deg);
    const int servo2Us = degreesToPulseUs(servo2Deg);

    if (ctx.robot->actuatorsAreArmed()) {
        ctx.robot->commandMotorPowerTest(motor1Power, motor2Power, servo1Us, servo2Us);
    }

    // Telemetria F3 debe representar lo que P02 realmente esta ordenando.
    ctx.robot->servo_old1 = servo1Deg;
    ctx.robot->servo_old2 = servo2Deg;
    ctx.robot->motor_power1 = motor1Power;
    ctx.robot->motor_power2 = motor2Power;

    Telemetry::sendControl(ctx, tz, fz, fx);
}

} // namespace TestRunners