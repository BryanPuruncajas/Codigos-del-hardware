#pragma once
#include <stdint.h>

// Project calibration in SI units. A sensor offset is not a scale calibration.
// Stationary blocked run: sigma~0.0198m, range0.1036m (old estimate).
// User fall:0.50m->0.18m in11s ~= -0.029m/s; do NOT erase this with a vz deadband.
namespace VerticalProfile {
constexpr float HEIGHT_SCALE=1.f; // no defensible fitted scale in existing evidence
constexpr float HEIGHT_TAU_S=.20f,VELOCITY_TAU_S=.15f;
constexpr uint32_t WINDOW_US=500000,GAP_US=500000,STALE_MS=300,CONTROL_AGE_MS=250;
constexpr unsigned WINDOW_CAPACITY=25;
constexpr uint32_t POLL_US=20000;
// BMP390 high resolution: pressure8x, temperature1x,25Hz normal mode.
// Datasheet maximum conversion21.53ms <40ms period. IIR coefficient1.
constexpr uint32_t SAMPLE_PERIOD_US=40000,MAX_CONVERSION_US=21530;
static_assert(MAX_CONVERSION_US<SAMPLE_PERIOD_US,"BMP390 conversion exceeds sample period");
}
