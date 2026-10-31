#include "mission/InertialHeight.h"
#include <assert.h>
#include <stdio.h>

int main(){
    // One isolated pressure velocity excursion cannot cause motor corrections.
    InertialHeight spike;spike.learningAllowed=false;float trim=.15f,p=trim;
    for(unsigned us=40000;us<=3000000;us+=40000){
        p=spike.update(us/1000,trim,0,0,false,false,true,us==1600000?.24f:0.f,.20f,0,0,true,us);
        assert(p==trim && !spike.velocityTrusted);
    }
    // Correlated alternating pressure noise of the measured magnitude must
    // not repeatedly drive the pressure-only support or teach a new trim.
    InertialHeight noise;trim=.15f;float initial=trim;
    for(unsigned us=40000;us<=10000000;us+=40000){
        const float v=.20f*sinf(us*1.e-6f*6.2831853f*1.5f);
        p=noise.update(us/1000,trim,0,0,false,false,true,v,.20f,0,0,true,us);
        assert(fabsf(p-initial)<.0001f && trim==initial);
    }
    // Preserve the USER slow-fall rate; do not implement a +/-0.24m/s deadband.
    InertialHeight fall;fall.learningAllowed=false;trim=.15f;unsigned accepted=0;
    for(unsigned us=40000;us<=4000000;us+=40000){
        p=fall.update(us/1000,trim,0,0,false,false,true,-.32f/11.f,.20f,0,0,true,us);
        if(fall.velocityTrusted && !accepted)accepted=us;
    }
    assert(accepted>=1000000 && accepted<=1200000);
    assert(p>trim+.008f && fall.velocityTrusted);
    // Reliable visual evidence can request immediate braking/recovery; it is
    // not forced to wait for the pressure-only confirmation window.
    InertialHeight visual;trim=.13f;
    p=visual.update(100,trim,-.3f,.30f,true,true,true,-.15f,.20f,0,0,true,100000);
    assert(visual.visualCorroborated && visual.velocityTrusted && visual.brakingDown && p>trim);
    p=visual.update(140,trim,.2f,-.30f,true,true,true,-.15f,.20f,0,0,true,140000);
    assert(!visual.velocityTrusted && p<trim);
    // Repeated sample cannot confirm by repeating the250Hz controller tick.
    InertialHeight cached;trim=.15f;
    for(unsigned ms=100;ms<2000;ms+=4)
        cached.update(ms,trim,0,0,false,false,true,-.15f,.20f,0,0,true,100000);
    assert(!cached.velocityTrusted);
    // Loss/invalidity and direction changes revoke accumulated evidence.
    fall.update(4100,trim,0,0,false,false,false,NAN,.20f,0,0,true,4100000);
    assert(!fall.velocityTrusted && !fall.evidence.initialized);
    VerticalMotionEvidence direction;
    for(unsigned us=40000;us<2000000;us+=40000)direction.update(.08f,us);
    assert(direction.trusted);
    direction.update(-.08f,2000000);assert(!direction.trusted);
    for(unsigned us=2000000;us<2400000;us+=40000)direction.update(-.08f,us);
    assert(!direction.trusted);
    // Unsigned elapsed arithmetic remains valid across micros() wrap.
    VerticalMotionEvidence rollover;uint32_t clock=UINT32_MAX-600000;
    for(unsigned i=0;i<40;++i){clock+=40000;rollover.update(-.03f,clock);}
    assert(rollover.trusted);
    printf("PASS confidence: isolated/oscillating noise, slow fall accepted at%ums, immediate visual response, stale/cached samples, reversal and wrap\n",accepted/1000);
}
