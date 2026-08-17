#pragma once
namespace SensorMap {
enum Index : int {
    TEMPERATURE = 0,
    ALTITUDE = 1,
    VERTICAL_VELOCITY = 2,
    ROLL = 3,
    PITCH = 4,
    YAW = 5,
    ROLL_RATE = 6,
    PITCH_RATE = 7,
    YAW_RATE = 8,
    RESERVED_9 = 9,
    BATTERY = 10,

    // Sin ultrasonico: la Nicla comienza inmediatamente despues del monitor de bateria.
    NICLA_FLAG = 11,
    NICLA_X = 12,
    NICLA_Y = 13,
    NICLA_W = 14,
    NICLA_H = 15,
    NICLA_X_VALUE = 16,
    NICLA_Y_VALUE = 17,
    NICLA_W_VALUE = 18,
    NICLA_H_VALUE = 19,
    NICLA_DISTANCE = 20,
};
}
