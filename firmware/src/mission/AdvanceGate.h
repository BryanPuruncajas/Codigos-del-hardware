#pragma once
#include <stdint.h>

// Permission to request forward thrust, NOT a position hold or brake.
// All timings are provisional and must be validated in tethered flight.
struct AdvanceGate {
    bool active = false;
    uint32_t since = 0, lastSample = 0;
    unsigned samples = 0;
    void reset() { active = false; samples = 0; }
    uint32_t stableMs(uint32_t now) const { return active ? now - since : 0; }
    bool update(uint32_t now, bool eligible, bool newSample,
                unsigned requiredSamples=5,uint32_t requiredMs=300) {
        if(requiredSamples==0){reset();return false;}
        if (!eligible || (active && uint32_t(now - lastSample) > 200)) reset();
        if (!eligible) return false;
        if (newSample) {
            if (!active) { active = true; since = now; }
            lastSample = now;
            if (samples < requiredSamples) ++samples;
        }
        return active && samples >= requiredSamples && stableMs(now) >= requiredMs;
    }
};
