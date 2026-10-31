#include "sense/VerticalEstimate.h"
#include "app/VisualSupport.h"
#include <assert.h>
#include <stdio.h>

int main(){
    // Sensor estimator + controller at25Hz. A brief real rise/fall must be
    // accepted in both directions, without demanding constant velocity first.
    VerticalEstimate source;InertialHeight control;control.learningAllowed=false;
    float trim=.15f;unsigned upAt=0,downAt=0;
    for(unsigned us=40000;us<=10000000;us+=40000){
        const float t=us*1.e-6f;
        const float h=t<2?0:(t<4?.4f*(t-2):(t<6?.8f:(t<8?.8f-.4f*(t-6):0)));
        source.update(h,us);
        control.update(us/1000,trim,0,0,false,false,true,source.velocity,.20f,0,0,true,us,source.height);
        if(t>=2 && t<4 && control.evidence.fastTrusted && control.controlRate>0 && !upAt)upAt=us;
        if(t>=6 && t<8 && control.evidence.fastTrusted && control.controlRate<0 && !downAt)downAt=us;
    }
    assert(upAt && downAt && upAt-2000000<1400000 && downAt-6000000<1400000);
    // Isolated pressure-height spikes and oscillation must not create fast
    // displacement evidence. Source velocity is consistent with that noise.
    VerticalMotionEvidence noise;
    for(unsigned us=40000;us<=10000000;us+=40000){
        const float t=us*1.e-6f,h=.02f*sinf(t*6.2831853f*1.5f);
        noise.update(.18849556f*cosf(t*6.2831853f*1.5f),us,us==4000000?h+.8f:h);
        assert(!noise.trusted);
    }
    // Real slow descent remains accepted without a displacement threshold.
    VerticalMotionEvidence slow;
    for(unsigned us=40000;us<2000000;us+=40000)
        slow.update(-.32f/11.f,us,-.32f/11.f*us*1.e-6f);
    assert(slow.slowTrusted && slow.trusted && !slow.fastTrusted);

    using namespace VisualSupport;
    Observation o;o.fresh=o.confirmed=o.full=true;o.x=120;o.y=80;o.w=o.h=30;
    Loop c;c.reset(.15f,100);
    for(unsigned ms=100;ms<=1100;ms+=40){++o.generation;
        c.update(o,ms,.15f,.20f,false,0,.8f,0,true,0,60000,true,4000,ms*1000,0);}
    assert(c.altitudeReady && fabsf(c.altitudeReference-.8f)<1.e-6f);
    // Correct a slow pressure-reference drift only while visual centring is
    // sustained. Sensor readings themselves are never zeroed or rescaled.
    for(unsigned ms=1140;ms<=11100;ms+=40){++o.generation;
        const float h=.8f+.004f*(ms-1100)*.001f;
        c.update(o,ms,.15f,.20f,false,0,h,.004f,true,0,60000,true,4000,ms*1000,0);}
    assert(c.altitudeReference>.825f && c.altitudeReference<.84f && c.referenceUpdates>10);
    const float reference=c.altitudeReference;
    o.fresh=false;
    for(unsigned ms=11140;ms<=12100;ms+=40)
        c.update(o,ms,.15f,.20f,false,0,1.5f,0,true,0,60000,true,4000,ms*1000,0);
    assert(c.altitudeReference==reference);
    o.fresh=true;o.full=false;
    for(unsigned ms=12140;ms<=13100;ms+=40){++o.generation;
        c.update(o,ms,.15f,.20f,false,0,1.5f,0,true,0,60000,true,4000,ms*1000,0);}
    assert(c.altitudeReference==reference);
    o.full=true;
    for(unsigned ms=13140;ms<=14100;ms+=40){++o.generation;
        c.update(o,ms,.15f,.20f,false,0,1.5f,0,true,.3f,60000,true,4000,ms*1000,0);}
    assert(c.altitudeReference==reference); // large pitch
    for(unsigned ms=14140;ms<=15100;ms+=40){++o.generation;
        c.update(o,ms,.15f,.20f,false,0,1.5f,0,true,0,60000,true,4000,ms*1000,.2f);}
    assert(c.altitudeReference==reference); // turning
    c.inertia.learningAllowed=false;
    for(unsigned ms=15140;ms<=16100;ms+=40){++o.generation;
        c.update(o,ms,.15f,.20f,false,0,1.5f,0,true,0,60000,true,4000,ms*1000,0);}
    assert(c.altitudeReference==reference); // impulse phases freeze reference
    c.inertia.learningAllowed=true;
    for(unsigned ms=16140;ms<=16500;ms+=40){++o.generation;
        c.update(o,ms,.15f,.20f,false,0,1.5f,0,true,0,60000,true,4000,ms*1000,0);}
    assert(c.altitudeReference==reference); // must re-confirm before refresh
    ++o.generation;c.update(o,16660,.15f,.20f,false,0,1.5f,0,true,0,60000,true,4000,16660000,0);
    assert(c.altitudeReference>reference && c.altitudeReference-reference<=.00501f);
    const float beforeCached=c.altitudeReference;
    for(unsigned ms=16700;ms<=17000;ms+=40)
        c.update(o,ms,.15f,.20f,false,0,1.5f,0,true,0,60000,true,4000,ms*1000,0);
    assert(c.altitudeReference==beforeCached);
    printf("PASS v30: fast rise at%ums/fall at%ums, noise/slow fall, stable visual refresh and freeze on loss/partial/pitch/turn/impulse/cached frames\n",(upAt-2000000)/1000,(downAt-6000000)/1000);
}
