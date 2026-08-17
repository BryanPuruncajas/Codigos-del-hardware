#pragma once
#include <Arduino.h>
#include "app/AppConfig.h"

namespace HardwareSafety {
inline void preInit() {
    // Libera el RST del BNO antes de cualquier inicialización I2C.
    pinMode(AppConfig::BNO_RST_PIN, OUTPUT);
    digitalWrite(AppConfig::BNO_RST_PIN, HIGH);

    // Pre-init en alta impedancia. En RawBicopter::startup los ESC pasan a PWM minimo
    // continuo (~1100 us), pero siguen logicamente desarmados hasta ARM explicito.
    pinMode(AppConfig::MOTOR1_PIN, INPUT);
    pinMode(AppConfig::MOTOR2_PIN, INPUT);
    pinMode(AppConfig::SERVO1_PIN, INPUT);
    pinMode(AppConfig::SERVO2_PIN, INPUT);
    delay(50);
}
}