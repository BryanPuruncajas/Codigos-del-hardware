#pragma once
#include <Arduino.h>

namespace AppConfig {
// PCB real del proyecto
constexpr int SERVO1_PIN = D0;
constexpr int SERVO2_PIN = D1;
constexpr int BNO_RST_PIN = D2;
constexpr int SDA_PIN = D4;
constexpr int SCL_PIN = D5;
constexpr int NICLA_TX_PIN = D6;
constexpr int NICLA_RX_PIN = D7;
constexpr int BATTERY_ADC_PIN = D8;
constexpr int MOTOR1_PIN = D9;
constexpr int MOTOR2_PIN = D10;

constexpr int NICLA_OFFSET = 11;
constexpr int SENSOR_COUNT = 21;

// Potencia original del firmware heredado: 0.0 .. 1.0.
// 1.0 conserva el mismo max_thrust=35 g y la misma conversion a PWM del proyecto viejo.
constexpr float ABSOLUTE_MOTOR_POWER_LIMIT = 1.00f;

// ControlInput.params[]
constexpr int PARAM_MODE = 0;
constexpr int PARAM_FX = 1;
constexpr int PARAM_FZ = 2;
constexpr int PARAM_TX = 3;
constexpr int PARAM_TZ = 4;
constexpr int PARAM_AUX0 = 5;
constexpr int PARAM_AUX1 = 6;
constexpr int PARAM_AUX2 = 7;
constexpr int PARAM_AUX3 = 8;
constexpr int PARAM_AUX4 = 9;
constexpr int PARAM_ARM = 10;
constexpr int PARAM_RELOAD = 11;
constexpr int PARAM_RESET = 12;

// K-Power P0025: 0..120 deg nominales = 900..2100 us.
constexpr int P0025_MIN_US = 900;
constexpr int P0025_CENTER_US = 1500;
constexpr int P0025_MAX_US = 2100;
constexpr float P0025_MIN_DEG = 0.0f;
constexpr float P0025_CENTER_DEG = 60.0f;
constexpr float P0025_MAX_DEG = 120.0f;

enum Mode : int {
    SAFE_STOP = 0,
    P00_MOTOR_POWER_TEST = 8,
    P00_SERVO_CALIBRATION = 9,
    P01_SENSOR_INTEGRATION = 10,
    P02_MANUAL_CONTROL = 11,
    P03_YAW_CONTROL = 12,
    P04_ALTITUDE_CONTROL = 13,
    P05_YAW_ALTITUDE = 14,
    P06_NICLA_RAW = 15,
    P07_VISUAL_CENTER = 16,
    P08_ONE_BALLOON = 17,
    P09_VISIT_ESCAPE = 18,
    P10_TWO_BALLOONS = 19,
    P11_FOUR_BALLOONS = 20,
};

constexpr uint8_t NICLA_BALLOON_MODE = 0x40;
constexpr uint8_t NICLA_GOAL_YELLOW = 0x80;
constexpr uint8_t NICLA_GOAL_ORANGE = 0x81;

constexpr uint32_t CONTROL_LOOP_US = 4000;
constexpr uint32_t TELEMETRY_PERIOD_MS = 100;
}