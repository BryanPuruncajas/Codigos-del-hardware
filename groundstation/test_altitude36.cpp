#include "app/VisualSupport.h"
#include <assert.h>
#include <stdio.h>

int main(){
    using namespace VisualSupport;
    Observation o;o.fresh=o.confirmed=o.full=true;o.x=120;o.y=80;o.w=80;o.h=60;
    Loop c;c.reset(.18f,100,.25f);
    const float drift=.004f;
    for(unsigned ms=100;ms<=12100;ms+=40){++o.generation;
        const float pressure=.8f+drift*(ms-100)*.001f;
        c.update(o,ms,.18f,.25f,false,0,pressure,drift,true,0,60000,true,15000,ms*1000,0);
    }
    assert(c.altitudeReady && c.referenceUsable(12100));
    assert(c.altitudeAid.driftValid(12100) && c.altitudeAid.driftRate>.003f && c.altitudeAid.driftRate<=.005f);
    assert(fabsf(c.referenceError(.848f,12100))<1.e-6f);
    assert(c.altitudeAid.noiseBand>=.03f);
    const float anchor=c.altitudeReference,base=c.height.trim;
    c.inertia.learningAllowed=false;o.fresh=false;
    for(unsigned ms=12140;ms<=14100;ms+=40){
        const float pressure=.8f+drift*(ms-100)*.001f;
        c.update(o,ms,.18f,.25f,false,0,pressure,drift,true,0,60000,true,15000,ms*1000,0);
    }
    assert(c.altitudeReference==anchor && c.height.trim==base);
    assert(fabsf(c.demand-base)<.0005f); // stationary pressure drift must not drive lift
    assert(c.referenceUsable(14100));
    assert(!c.referenceUsable(16200));
    assert(c.referenceError(.6f,16200)==0); // expired anchor cannot pull toward old pressure
    assert((c.referenceFlags(16200)&1) && !(c.referenceFlags(16200)&2));
    // The real measured slow fall is preserved, even with an active drift model.
    Loop falling;falling.reset(.18f,100,.25f);o.fresh=o.full=true;
    for(unsigned ms=100;ms<=4100;ms+=40){++o.generation;
        falling.update(o,ms,.18f,.25f,false,0,.8f+drift*ms*.001f,drift,true,0,60000,true,15000,ms*1000,0);}
    falling.inertia.learningAllowed=false;o.fresh=false;
    const float physicalRate=-.32f/11.f;
    for(unsigned ms=4140;ms<=6100;ms+=40){
        const float pressure=.8f+drift*ms*.001f+physicalRate*(ms-4100)*.001f;
        falling.update(o,ms,.18f,.25f,false,0,pressure,physicalRate+drift,true,0,60000,true,15000,ms*1000,0);}
    assert(falling.inertia.velocityTrusted && falling.demand>falling.height.trim+.005f);
    // Never misclassify a coherent real rise/fall of29mm/s as5mm/s drift.
    VisualAltitudeAid real;
    for(unsigned ms=100;ms<=8100;ms+=40)real.observe(ms,.8f+physicalRate*ms*.001f,0,80,80,0);
    assert(!real.driftReady);
    VisualAltitudeAid pitching,scaling,noisy;
    for(unsigned ms=100;ms<=8100;ms+=40){
        const float t=ms*.001f,level=.8f+drift*t;
        pitching.observe(ms,level,0,80,80,.025f*sinf(t*3));
        scaling.observe(ms,level,0,80,80+12*sinf(t*3),0);
        noisy.observe(ms,level,.04f,80,80,0);
    }
    assert(!pitching.driftReady && !scaling.driftReady && !noisy.driftReady);
    assert(noisy.noiseBand>=.10f && !noisy.referenceUsable(true,8000,8100));
    VisualAltitudeAid cached;cached.begin(100,.8f,80,80,0);
    cached.pause();assert(!cached.candidate && cached.samples==0);
    VisualAltitudeAid interrupted;
    for(unsigned ms=100;ms<=500;ms+=40)interrupted.observe(ms,.8f+drift*ms*.001f,0,80,80,0);
    interrupted.pause(true);assert(interrupted.suspended && !interrupted.driftReady);
    for(unsigned ms=4000;ms<=4500;ms+=40)interrupted.observe(ms,.8f+drift*ms*.001f,0,80,80,0);
    assert(interrupted.driftReady && fabsf(interrupted.driftRate-drift)<.0001f);
    const uint32_t beforeWrap=UINT32_MAX-1000u;
    assert(cached.referenceUsable(true,beforeWrap,999)); //2s across millis wrap
    assert(!cached.referenceUsable(true,beforeWrap,4000));
    assert(VisualAltitudeAid::deadband(.02f,.03f)==0);
    assert(fabsf(VisualAltitudeAid::deadband(.08f,.03f)-.05f)<1.e-6f);
    puts("PASS altitude36: stable visual drift learned, uncertainty band, bounded-age reference, true29mm/s fall retained, pitch/scale/noise rejection, wrap");
}
