#include "mission/FlowHeight.h"
#include "mission/InertialHeight.h"
#include <assert.h>
#include <stdio.h>

struct SensorTrial {
    FlowHeight f;FlowProtocol::Packet p{};uint32_t now=1000;
    float baro=5,baroVz=0,roll=3.14159265f,pitch=0,angular=0;
    bool healthy=true;
    SensorTrial(){p.magic=FlowProtocol::MAGIC;p.version=1;p.flags=3;p.range_mm=800;p.quality=80;}
    void tick(unsigned ms=40,bool increment=true,unsigned age=0){
        now+=ms;if(increment)++p.sequence;
        f.update(now,true,p,age,baro,baroVz,healthy,now*1000,roll,pitch,angular);
    }
    void run(unsigned ms){for(unsigned i=0;i<ms;i+=40)tick();}
};
int main(){
    InertialHeight fallingAbove;float trim=.18f;
    const float braking=fallingAbove.update(1000,trim,0,0,false,false,true,-.2f,.25f,0,
        0,true,1000000,1.f,-.2f,true);
    assert(fallingAbove.velocityTrusted && fallingAbove.brakingDown && braking>trim);
    SensorTrial t;t.run(1600);
    assert(t.f.source==FlowHeight::FLOW_RANGE && t.f.valid && t.f.engaged);
    assert(fabsf(t.f.rangeHeight-.8f)<.001f && fabsf(t.f.height-5.f)<.001f && fabsf(t.f.vz)<.001f);
    const float anchor=t.f.height;
    for(unsigned i=0;i<100;++i){t.baro+=.01f;t.tick();}
    assert(fabsf(t.f.height-anchor)<.001f); // pressure drift does not move ToF height
    for(unsigned i=0;i<60;++i){t.p.range_mm+=4;t.tick();}
    assert(t.f.rangeVz>.08f && t.f.rangeVz<.11f && t.f.height>anchor+.20f);
    const float before=t.f.height;
    t.p.range_mm=2800;t.tick(); // one isolated spike must not move the estimator
    assert(fabsf(t.f.height-before)<.02f);
    t.p.range_mm=1040;t.run(400);
    assert(t.f.source==FlowHeight::FLOW_RANGE);
    // Repeated sequence numbers cannot refresh control even if radio age=0.
    const float held=t.f.height;
    for(unsigned i=0;i<8;++i)t.tick(40,false);
    assert(t.f.source==FlowHeight::BARO_BRIDGE && t.f.valid && fabsf(t.f.height-held)<.01f);
    t.baro+=.03f;t.tick(40,false);
    assert(fabsf(t.f.height-(held+.03f))<.005f);
    for(unsigned i=0;i<50;++i)t.tick(40,false);
    assert(t.f.source==FlowHeight::NONE && !t.f.valid);
    const float originOffset=t.f.rangeOffset;t.baro+=20;
    t.run(1000);
    assert(t.f.source==FlowHeight::FLOW_RANGE && t.f.rangeOffset==originOffset);
    assert(fabsf(t.f.height-(t.f.rangeHeight+originOffset))<.01f); // undo the bridge drift
    // Range validity is independent of optical quality; old sender quality=0
    // remains height-compatible, while optical counts/s are deliberately gated.
    SensorTrial quality;quality.p.quality=0;quality.p.dx=4;quality.run(1200);
    assert(quality.f.source==FlowHeight::FLOW_RANGE && !quality.f.opticalValid && quality.f.motionScore==0);
    quality.p.quality=80;quality.run(1200);
    assert(quality.f.opticalValid && quality.f.rateX>90 && quality.f.rateX<101);
    quality.angular=.4f;quality.tick();assert(!quality.f.opticalValid && quality.f.rangeFresh);
    quality.angular=0;quality.pitch=.5f;quality.tick();
    assert(!quality.f.rangeFresh && quality.f.source==FlowHeight::BARO_BRIDGE);
    SensorTrial tilted;tilted.pitch=.25f;tilted.p.range_mm=826;tilted.run(1200);
    assert(tilted.f.source==FlowHeight::FLOW_RANGE && fabsf(tilted.f.rangeHeight-.8f)<.004f);
    SensorTrial invalid;invalid.p.range_mm=0;invalid.run(1000);
    assert(!invalid.f.engaged && invalid.f.source==FlowHeight::BAROMETER);
    invalid.p.range_mm=4000;invalid.run(1000);assert(!invalid.f.engaged);
    invalid.p.range_mm=800;invalid.p.flags=1;invalid.run(1000);assert(!invalid.f.engaged);
    invalid.p.flags=3;invalid.p.version=2;invalid.run(1000);assert(!invalid.f.engaged);
    SensorTrial stale;stale.run(1000);stale.tick(40,true,300);
    assert(stale.f.source==FlowHeight::BARO_BRIDGE && !stale.f.rangeFresh);
    stale.healthy=false;stale.tick(40,true,300);assert(!stale.f.valid);
    SensorTrial wrapping;wrapping.now=UINT32_MAX-400u;wrapping.p.sequence=UINT32_MAX-3u;
    wrapping.run(1600);assert(wrapping.f.source==FlowHeight::FLOW_RANGE && wrapping.f.rangeFresh);
    wrapping.p.sequence=0;wrapping.tick(); // old sequence before receive timeout is rejected
    assert(wrapping.f.lastSequence!=0);
    wrapping.tick(600,false);wrapping.run(800);assert(wrapping.f.rangeFresh);
    SensorTrial falling;falling.run(1000);const float trimPlane=falling.f.height;
    for(unsigned i=0;i<150;++i){falling.p.range_mm=uint16_t(800.f-29.09f*.04f*(i+1));falling.tick();}
    assert(falling.f.rangeVz<-.025f && falling.f.rangeVz>-.035f && falling.f.height<trimPlane-.15f);
    SensorTrial noisy;
    for(unsigned i=0;i<150;++i){noisy.p.range_mm=uint16_t(800+int(i%3)*3-3);noisy.tick();}
    assert(noisy.f.rangeFresh && fabsf(noisy.f.rangeVz)<.01f);
    SensorTrial movingFloor;movingFloor.run(1200);
    const float previous=movingFloor.f.height;
    movingFloor.p.range_mm=2500;movingFloor.tick();movingFloor.tick();
    assert(movingFloor.f.rejected && movingFloor.f.source==FlowHeight::BARO_BRIDGE);
    movingFloor.run(2200);
    assert(movingFloor.f.source==FlowHeight::NONE && fabsf(movingFloor.f.height-previous)<.02f);
    puts("PASS FlowHeight38: stable height despite baro drift, slope in m/s, spike/tilt/range/quality/age/sequence guards, bounded aligned fallback, recovery, wrap, slow fall and noise");
}
