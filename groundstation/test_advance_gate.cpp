#include "../firmware/src/mission/AdvanceGate.h"
#include <assert.h>
int main() {
    AdvanceGate g;
    assert(!g.update(0, true, true));
    assert(!g.update(100, true, false));
    assert(!g.update(200, true, false));
    assert(!g.update(301, true, false));
    assert(!g.active); // repeated frames cannot confirm permission
    for (unsigned t = 400; t <= 600; t += 50) assert(!g.update(t, true, true));
    assert(g.update(700, true, true)); // five frames AND 300 ms
    assert(!g.update(701, false, false)); // loss, off-center, excess rate
    assert(!g.update(702, true, true));
    assert(!g.update(1003, true, true)); // gap restarts timer
    assert(g.samples == 1 && g.stableMs(1003) == 0);
    g.reset();
    const uint32_t start = UINT32_MAX - 150;
    for (unsigned t = 0; t < 300; t += 50) assert(!g.update(start + t, true, true));
    assert(g.update(start + 300, true, true)); // unsigned clock rollover
    g.reset();
    assert(!g.active && g.samples == 0);
}
