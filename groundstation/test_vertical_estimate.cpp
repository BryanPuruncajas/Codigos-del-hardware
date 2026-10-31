#include "../firmware/lib/BlimpSwarm/src/sense/VerticalEstimate.h"
#include <assert.h>
#include <stdio.h>
int main() {
    VerticalEstimate e;
    e.update(2.f, 100);
    assert(e.height == 2.f && e.velocity == 0);
    assert(!e.update(NAN, 20100));
    assert(e.lastUs == 100 && e.height == 2.f);
    assert(!e.update(3.f, 100)); // repeated timestamp
    // Stationary synthetic noise: 2 and 7 Hz, no drift. Compare old .9/.1
    // derivative smoothing with actual new estimator, not a Python copy.
    e.reset(0, 0);
    float last = 0, oldVz = 0, oldEnergy = 0, newEnergy = 0;
    float rawEnergy = 0, filteredEnergy = 0;
    for (unsigned i=1; i<=1500; ++i) {
        const float t = i * .02f;
        const float h = .02f*sinf(2*3.14159265f*2*t) + .01f*sinf(2*3.14159265f*7*t);
        oldVz = .9f*oldVz + .1f*(h-last)/.02f;
        last = h;
        e.update(h, i*20000);
        if (i>100) {
            oldEnergy += oldVz*oldVz; newEnergy += e.velocity*e.velocity;
            rawEnergy += h*h; filteredEnergy += e.height*e.height;
        }
    }
    printf("synthetic RMS ratios: height=%.3f velocity_vs_old=%.3f\n",
           sqrtf(filteredEnergy/rawEnergy), sqrtf(newEnergy/oldEnergy));
    assert(filteredEnergy < rawEnergy*.5f);
    assert(newEnergy < oldEnergy*.25f);
    // Real ramp is preserved, not erased by auto-zero or deadband.
    e.reset(0, 0);
    for (unsigned i=1; i<=250; ++i) e.update(-.2f*i*.02f, i*20000);
    assert(fabsf(e.velocity + .2f) < .01f);
    assert(fabsf(e.height + 1.f) < .06f);
    // 50 cm step settles, record lag rather than claiming instantaneous response.
    e.reset(0, 0);
    unsigned t90 = 0;
    for (unsigned i=1; i<=150; ++i) {
        e.update(.5f, i*20000);
        if (!t90 && e.height>=.45f) t90=i*20;
    }
    printf("synthetic height step t90=%u ms\n", t90);
    assert(t90 > 0 && t90 < 1000);
    assert(fabsf(e.height-.5f)<.001f && fabsf(e.velocity)<.01f);
    // One-sample pressure spike removed by median, without clamping real ramps.
    e.reset(0, 0);
    e.update(5.f, 20000); e.update(0, 40000); e.update(0, 60000);
    assert(e.height==0 && e.velocity==0);
    // Timestamp rollover and long gaps cannot create derivative explosions.
    e.reset(1, UINT32_MAX-10000);
    e.update(1, 9999);
    assert(e.velocity==0);
    e.update(2, 1000000);
    assert(e.height==2 && e.velocity==0);
    // Slow drift cannot be distinguished from real motion by a barometer.
    e.reset(0, 0);
    for (unsigned i=1; i<=1500; ++i) e.update(.002f*i*.02f, i*20000);
    assert(e.height > .058f && fabsf(e.velocity-.002f)<.0002f);
    // Irregular scheduling still preserves a physical ramp.
    uint32_t now = 0;
    e.reset(0, now);
    for (unsigned i=1; i<=300; ++i) {
        now += i%2 ? 20000 : 40000;
        e.update(.1f*now*1.e-6f, now);
    }
    assert(fabsf(e.velocity-.1f)<.01f);
}
