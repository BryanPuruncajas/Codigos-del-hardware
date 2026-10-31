#include "mission/FlowHeight.h"
#include "mission/InertialHeight.h"
#include <assert.h>
#include <stdio.h>
struct Trial {
    FlowHeight f;FlowProtocol::Packet p{};uint32_t now=1000;
    float roll=3.14159265f,pitch=0,angular=0;
    bool disarmed=false;
    Trial(){p.magic=FlowProtocol::MAGIC;p.version=1;p.flags=3;p.range_mm=800;p.quality=80;}
    void tick(unsigned ms=40,bool next=true,unsigned age=0){
        now+=ms;if(next)++p.sequence;
        f.update(now,true,p,age,roll,pitch,angular,disarmed);
        assert(f.source==FlowHeight::NONE || f.source==FlowHeight::FLOW_RANGE);
        assert((f.flags()&(16|32|2048))==0 && (f.flags()&4096));
    }
    void run(unsigned ms){for(unsigned i=0;i<ms;i+=40)tick();}
};
int main(){
    Trial t;FlowProtocol::Packet empty{};
    t.f.update(t.now,false,empty,UINT32_MAX,0,0,0);
    assert(!t.f.valid && t.f.source==FlowHeight::NONE); // no barometer input exists
    t.run(1200);assert(t.f.valid && fabsf(t.f.height-.8f)<.001f && fabsf(t.f.vz)<.001f);
    for(unsigned i=0;i<80;++i){t.p.range_mm+=4;t.tick();}
    assert(t.f.vz>.08f && t.f.vz<.11f && t.f.height>1.09f);
    const float before=t.f.height;t.p.range_mm=2800;t.tick();
    assert(fabsf(t.f.height-before)<.02f);
    t.p.range_mm=1120;t.run(1000);
    t.tick(240,false);assert(t.f.valid);
    t.tick(11,false);assert(!t.f.valid && t.f.source==FlowHeight::NONE); // no 2s pressure bridge
    t.run(1000);assert(t.f.valid && fabsf(t.f.height-1.12f)<.005f);
    // Regression from live trial: last accepted 0.57 m, manually moved to
    // 1.61 m. A disarmed rig must recover without a power cycle; in-flight
    // contradictory returns must still be rejected.
    Trial reposition;reposition.p.range_mm=570;reposition.run(1000);
    reposition.tick(300,false);assert(!reposition.f.valid);
    reposition.p.range_mm=1610;reposition.run(1600);
    assert(!reposition.f.valid && reposition.f.rejected);
    reposition.disarmed=true;reposition.run(800);
    assert(reposition.f.valid && fabsf(reposition.f.height-1.61f)<.005f);
    reposition.p.range_mm=0;reposition.run(600);assert(!reposition.f.valid);
    reposition.p.range_mm=1610;reposition.pitch=.5f;reposition.run(400);
    assert(!reposition.f.valid); // disarmed recovery never bypasses range/tilt guards
    Trial quality;quality.p.quality=0;quality.run(1000);
    assert(quality.f.valid && !quality.f.opticalValid);
    quality.p.quality=80;quality.p.dx=4;quality.run(1200);
    assert(quality.f.opticalValid && quality.f.rateX>90 && quality.f.rateX<101);
    quality.angular=.4f;quality.tick();assert(!quality.f.opticalValid && quality.f.valid);
    quality.angular=0;quality.pitch=.5f;quality.tick();assert(!quality.f.valid);
    Trial tilted;tilted.pitch=.25f;tilted.p.range_mm=826;tilted.run(1000);
    assert(tilted.f.valid && fabsf(tilted.f.height-.8f)<.004f);
    Trial invalid;invalid.p.range_mm=0;invalid.run(1000);assert(!invalid.f.valid);
    invalid.p.range_mm=4000;invalid.run(1000);assert(!invalid.f.valid);
    invalid.p.range_mm=800;invalid.p.flags=1;invalid.run(1000);assert(!invalid.f.valid);
    invalid.p.flags=3;invalid.p.version=2;invalid.run(1000);assert(!invalid.f.valid);
    Trial stale;stale.run(1000);stale.tick(40,true,300);assert(stale.f.valid && stale.f.holding);
    stale.tick(211,false,300);assert(!stale.f.valid);
    Trial wrap;wrap.now=UINT32_MAX-400u;wrap.p.sequence=UINT32_MAX-3u;
    wrap.run(1600);assert(wrap.f.valid);
    wrap.p.sequence=0;wrap.tick();assert(wrap.f.lastSequence!=0);
    wrap.tick(600,false);wrap.run(800);assert(wrap.f.valid);
    Trial falling;falling.run(1000);
    for(unsigned i=0;i<150;++i){falling.p.range_mm=uint16_t(800.f-29.09f*.04f*(i+1));falling.tick();}
    assert(falling.f.vz<-.025f && falling.f.vz>-.035f && falling.f.height<.65f);
    Trial noisy;for(unsigned i=0;i<150;++i){noisy.p.range_mm=uint16_t(800+int(i%3)*3-3);noisy.tick();}
    assert(noisy.f.valid && fabsf(noisy.f.vz)<.01f);
    InertialHeight controller;float trim=.18f;
    assert(controller.update(1000,trim,0,0,false,false,true,-.2f,.25f,0,0,true,1000000,1,-.2f,true)>trim);
    puts("PASS Flow-only41: no pressure inputs, bounded recovery, disarmed reacquisition after manual height change, armed contradictory return rejected, absolute floor height, vz, freshness/duplicate/tilt/range/sequence guards, no fallback, noise and braking");
}
