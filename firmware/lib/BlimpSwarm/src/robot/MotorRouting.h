#pragma once

// User confirmed the physical motors are on opposite sides (2026-10-29).
// Pins remain D9/D10. Values here map logical motor commands to those outputs;
// servo wiring/angles and ESC rotation are not changed by this map.
namespace MotorRouting {
constexpr bool SWAP_MOTORS=true;
struct Outputs {float pin1,pin2;};
inline Outputs outputs(float logical1,float logical2){
    return SWAP_MOTORS?Outputs{logical2,logical1}:Outputs{logical1,logical2};
}
}
