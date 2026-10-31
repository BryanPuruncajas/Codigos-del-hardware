#pragma once
#include <stdint.h>
#include <math.h>
#include "VerticalProfile.h"
#include "VerticalDiagnostics.h"

// Barometer-only estimate. Does not measure distance to ground or remove drift.
// Fixed memory, time-based filters; no dependence on control-loop frequency.
struct VerticalEstimate {
    static constexpr unsigned WINDOW = VerticalProfile::WINDOW_CAPACITY;
    bool initialized = false;
    float height = 0, velocity = 0, rawHeight = 0, rawVelocity = 0;
    uint32_t lastUs = 0;
    float recent[3] = {}, heights[WINDOW] = {};
    uint32_t times[WINDOW] = {};
    unsigned recentIndex = 0, count = 0, index = 0;
    uint32_t windowSpanUs=0;

    void reset(float h, uint32_t now) {
        initialized = true;
        height = rawHeight = h;
        velocity = rawVelocity = 0;
        lastUs = now;
        for (unsigned i = 0; i < 3; ++i) recent[i] = h;
        recentIndex = count = index = 0;
        windowSpanUs=0;
    }
    bool update(float raw, uint32_t now) {
        if (!isfinite(raw)) return false;
        if (!initialized) { reset(raw, now); return true; }
        const float dt = uint32_t(now - lastUs) * 1.e-6f;
        if (dt <= 0) return false;
        if (dt > VerticalProfile::GAP_US*1.e-6f) { reset(raw, now); return true; }
        rawVelocity = (raw - rawHeight) / dt;
        rawHeight = raw;
        lastUs = now;
        recent[recentIndex++ % 3] = raw;
        const float a = recent[0], b = recent[1], c = recent[2];
        const float median = fmaxf(fminf(a, b), fminf(fmaxf(a, b), c));
        height += (1.f - expf(-dt / VerticalProfile::HEIGHT_TAU_S)) * (median - height);
        heights[index] = height;
        times[index] = now;
        index = (index + 1) % WINDOW;
        if (count < WINDOW) ++count;
        float mt = 0, mh = 0;unsigned active=0;windowSpanUs=0;
        for (unsigned i = 0; i < count; ++i) {
            const uint32_t age=uint32_t(now-times[i]);
            if(age>VerticalProfile::WINDOW_US)continue;
            if(age>windowSpanUs)windowSpanUs=age;
            ++active;mt -= age * 1.e-6f;
            mh += heights[i];
        }
        if(!active)return true;
        mt /= active; mh /= active;
        float covariance = 0, variance = 0;
        for (unsigned i = 0; i < count; ++i) {
            if(uint32_t(now-times[i])>VerticalProfile::WINDOW_US)continue;
            const float t = -float(uint32_t(now - times[i])) * 1.e-6f - mt;
            covariance += t * (heights[i] - mh);
            variance += t * t;
        }
        if (active >= 5 && variance > 1.e-6f) {
            const float slope = covariance / variance;
            velocity += (1.f - expf(-dt / VerticalProfile::VELOCITY_TAU_S)) * (slope - velocity);
        }
        return true;
    }
};

