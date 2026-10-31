#pragma once
#include <stdint.h>

// Additional metadata; the ten legacy sensor channels and F2 remain unchanged.
struct NiclaFeedback {
    enum State { SEARCH=0, CONFIRM=1, FULL=2, PARTIAL=3, MISSING=4, AMBIG=5, LOST=6 };
    static constexpr uint16_t N03_PROTOCOL = 0x4E03;
    uint32_t generation = 0;
    uint16_t sample = 0;
    uint16_t protocol = 0;
    uint16_t state = SEARCH;
    uint16_t resetAck = 0;
    bool resetPending = false;
    bool fresh = false;
};
