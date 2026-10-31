#pragma once
#include <stdint.h>

// Generic vertical sample metadata. v39 populates it ONLY from Flowdeck.
struct VerticalDiagnostics {
    float rawHeight, filteredHeight, rawVelocity, filteredVelocity;
    uint32_t ageMs;
    bool healthy;
    uint32_t sampleTimeUs, samplePeriodUs, windowMs;
};
