#include "tests/TestRunners.h"
#include "app/AppConfig.h"
#include "app/Telemetry.h"
#include <Arduino.h>
#include <math.h>

namespace {

// Calibracion fisica validada en P00/P00M (16-08-2026)
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

// P02 es una prueba manual limitada: usamos la potencia que ya fue validada
// fisicamente en P00M para no depender del mixer/PID general.
constexpr float P02_MOTOR_POWER = 0.20f;
constexpr float COMMAND_DEADBAND = 0.001f;

int degreesToPulseUs(float deg) {
    const float clipped = constrain(deg,
                                    AppConfig::P0025_MIN_DEG,
                                    AppConfig::P0025_MAX_DEG);
    const float spanDeg = AppConfig::P0025_MAX_DEG - AppConfig::P0025_MIN_DEG;
    const float spanUs = (float)(AppConfig::P0025_MAX_US - AppConfig::P0025_MIN_US);

    return (int)lroundf(AppConfig::P0025_MIN_US +
                        (clipped - AppConfig::P0025_MIN_DEG) * spanUs / spanDeg);
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
        motor1Power = P02_MOTOR_POWER;
        motor2Power = P02_MOTOR_POWER;

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
        motor1Power = P02_MOTOR_POWER;
        motor2Power = P02_MOTOR_POWER;

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
        // activa para -Z dentro del limite mecanico 0..120 grados.
        // Por seguridad: +Fz aplica empuje; -Fz corta motores (descenso/pasivo).
        servo1Deg = SERVO1_Z_DEG;
        servo2Deg = SERVO2_Z_DEG;

        if (fz > 0.0f) {
            motor1Power = P02_MOTOR_POWER;
            motor2Power = P02_MOTOR_POWER;
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