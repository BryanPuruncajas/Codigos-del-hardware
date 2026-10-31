#define CONTROL_COMMON_HOST_TEST
#define PI 3.14159265358979323846f
template<class T> T constrain(T x, T lo, T hi) { return x<lo ? lo : (x>hi ? hi : x); }
#include "../firmware/src/app/ControlCommon.h"
#include <assert.h>
#include <stdio.h>
int main() {
    using namespace ControlCommon;
    AltState s;
    AltGains g{.30f,.025f,.10f};
    AltLimits lim;
    lim.minPower=0; lim.maxPower=.10f; lim.slew=.18f;
    resetAltState(s,0,lim.maxPower);
    assert(fabsf(s.integral-.10f)<1.e-6f && s.lastPower==0);
    assert(fabsf(computeAltDemand(g,lim,s,0,.004f)-.10f)<1.e-6f);
    float p=0; unsigned reached=0;
    for (unsigned i=1;i<=250;++i) {
        const float demand=computeAltDemand(g,lim,s,0,.004f);
        const float next=rateLimit(demand,p,lim.slew,.004f);
        assert(next>=0 && next<=.100001f && next-p<=.000721f);
        p=next;
        if (!reached && p>=.0999f) reached=i*4;
    }
    printf("Current-config startup reaches10%% via existing slew in%u ms\n",reached);
    assert(reached>0 && reached<1000);
    resetAltState(s,0,.06f);
    assert(s.integral<=.06f);
    resetAltState(s,0,lim.maxPower);
    const float oldI=s.integral;
    const float lower=computeAltDemand(g,lim,s,-.20f,.004f);
    assert(lower<.05f && s.integral<oldI); // preload is not a throttle floor
    assert(computeAltDemand(g,lim,s,1,.004f)<=.1f);
    assert(computeAltDemand(g,lim,s,NAN,.004f)==0);
    resetAltState(s,NAN,NAN);
    assert(s.integral==0 && s.filteredVz==0);
}
