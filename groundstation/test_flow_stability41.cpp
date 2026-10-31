#include "mission/FlowHeight.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

struct Rig41 {
    FlowHeight f;FlowProtocol::Packet p{};uint32_t now=1000;
    Rig41(){p.magic=FlowProtocol::MAGIC;p.version=1;p.flags=3;p.quality=80;p.range_mm=1500;}
    void step(uint16_t mm,bool next=true){
        now+=20;if(next)++p.sequence;p.range_mm=mm;
        f.update(now,true,p,0,3.14159265f,0,0,false);
    }
    void warm(){for(unsigned i=0;i<60;++i)step(1500);assert(f.valid);}
};

int main(){
    Rig41 burst;burst.warm();const auto trustedAt=burst.f.lastTrustedAt;
    for(unsigned i=0;i<16;++i){burst.step(i%2?977:781);assert(burst.f.valid);}
    assert(burst.f.recoveryHold && burst.f.holding && burst.f.lastTrustedAt==trustedAt);
    assert(burst.f.sampleTimeUs==trustedAt*1000u);
    for(unsigned i=0;i<30;++i){burst.step(1450);assert(burst.f.valid);}
    assert(!burst.f.holding && fabsf(burst.f.height-1.45f)<.004f);
    Rig41 loss;loss.warm();
    for(unsigned i=0;i<13;++i)loss.step(1500,false);
    assert(!loss.f.valid); // duplicate packets never extend the 250 ms link deadline
    Rig41 invalid;invalid.warm();
    for(unsigned i=0;i<26;++i)invalid.step(0);
    assert(!invalid.f.valid); // fresh optical flow is not indefinite valid height
    Rig41 contradiction;contradiction.warm();
    for(unsigned i=0;i<100;++i)contradiction.step(780);
    assert(!contradiction.f.valid);
    Rig41 descent;descent.warm();
    for(unsigned i=0;i<120;++i){descent.step(1500-i*5);assert(descent.f.valid);}
    assert(descent.f.vz<-.22f && descent.f.vz>-.28f);
    Rig41 swing;swing.warm();
    for(unsigned i=0;i<200;++i){
        const float pitch=.30f*sinf(float(i)*.1f);
        swing.now+=20;++swing.p.sequence;swing.p.range_mm=uint16_t(1500.f/cosf(pitch));
        swing.f.update(swing.now,true,swing.p,0,3.14159265f,pitch,0,false);
        assert(swing.f.valid && fabsf(swing.f.height-1.5f)<.003f);
    }
    swing.f.update(swing.now+20,true,swing.p,0,3.14159265f,.5f,0,false);
    assert(!swing.f.valid);
    puts("PASS Flow41: transient recovery, no false timestamps, 250 ms link loss, 500 ms invalid range, false floor, real descent and IMU swing");
}
