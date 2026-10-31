#include "Arduino.h"
#include <assert.h>
#include <stdio.h>
uint32_t mockMs=1000;int reason=0;float forward=0;
#define private public
#include "mission/BalloonMission.cpp"
#undef private
struct Trial{
    AppContext c;RobotMock r;BalloonMission m;
    Trial(){
        c.robot=&r;c.mode=17;r.sensorsuite.barometer.healthy=true;
        c.sensors[SensorMap::ALTITUDE]=.8f;c.sensors[SensorMap::VERTICAL_VELOCITY]=0;
        c.sensors[SensorMap::YAW]=c.sensors[SensorMap::YAW_RATE]=c.sensors[SensorMap::PITCH]=0;
        image(true,70,40,100,80,120,80);
        m.configure(1,true);m.reset(c);r.armed=true;
    }
    void image(bool full,float left,float top,float w,float h,float cx,float cy){
        r.sensorsuite.fb.state=full?NiclaFeedback::FULL:NiclaFeedback::PARTIAL;
        c.sensors[SensorMap::NICLA_FLAG]=full?65:66;
        c.sensors[SensorMap::NICLA_X]=left;c.sensors[SensorMap::NICLA_Y]=top;
        c.sensors[SensorMap::NICLA_W]=c.sensors[SensorMap::NICLA_W_VALUE]=w;
        c.sensors[SensorMap::NICLA_H]=c.sensors[SensorMap::NICLA_H_VALUE]=h;
        c.sensors[SensorMap::NICLA_X_VALUE]=cx;c.sensors[SensorMap::NICLA_Y_VALUE]=cy;
    }
    void step(){
        mockMs+=20;++r.sensorsuite.fb.generation;
        r.sensorsuite.barometer.sampleTimeUs=mockMs*1000;m.update(c);
        if(forward>0)assert(Telemetry::impulseYaw==0);
        assert(r.motor_power1<=.200001f && r.motor_power2<=.200001f);
    }
    void run(unsigned ms){for(unsigned i=0;i<ms;i+=20)step();}
};
int main(){
    Trial t;t.run(4500);
    assert(t.m.approachHadFull_ && t.m.approachSupport_.altitudeReady && t.m.approachNearMemory_);
    const float anchor=t.m.approachSupport_.altitudeReference,bearing=t.m.approachBearing_.reference;
    const unsigned pulses=t.m.impulse_.pulses;
    t.image(false,40,20,119,140,50,120);t.run(4000);
    assert(t.m.impulse_.pulses>pulses && t.r.armed);
    assert(t.m.approachSupport_.altitudeReference==anchor && t.m.approachBearing_.reference==bearing);
    assert(t.m.visited()==0); // large crop is not automatically arrival
    const unsigned resets=t.r.sensorsuite.resets;
    t.r.sensorsuite.fb.state=NiclaFeedback::MISSING;t.run(6200);
    assert(t.r.armed && forward==0 && t.r.sensorsuite.resets==resets);
    assert(t.m.approachSupport_.altitudeReference==anchor && t.m.approachBearing_.reference==bearing);
    t.image(false,40,20,119,140,50,120);t.run(1000);
    assert(t.r.armed && t.m.approachSupport_.phase==VisualSupport::RUN);
    // Final cropped view can meet the existing ESTIMATED width criterion
    // without pretending that its raw centroid is the balloon centre.
    t.image(false,40,0,120,160,55,120);t.run(800);
    assert(t.m.visited()==1 && t.m.state()==BalloonMission::DONE && !t.r.armed);
    // The same 120 px width cannot confirm with cached camera observations.
    Trial cached;cached.run(4500);
    cached.image(false,40,0,120,160,55,120);
    for(unsigned i=0;i<40;++i){mockMs+=20;cached.m.update(cached.c);}
    assert(cached.m.visited()==0 && cached.r.armed);
    Trial wrongHeight;wrongHeight.run(4500);wrongHeight.c.sensors[SensorMap::ALTITUDE]=1.2f;
    wrongHeight.image(false,0,0,240,160,55,120);wrongHeight.run(1200);
    assert(wrongHeight.m.visited()==0 && forward==0);
    Trial noAnchor;noAnchor.c.sensors[SensorMap::ALTITUDE]=NAN;
    noAnchor.r.sensorsuite.barometer.healthy=false;noAnchor.run(800);
    noAnchor.image(false,0,0,240,160,55,120);noAnchor.run(1000);
    assert(noAnchor.m.visited()==0 && forward==0);
    Trial offAxis;offAxis.run(4500);offAxis.image(false,180,0,60,160,210,80);offAxis.run(1000);
    assert(offAxis.m.visited()==0 && forward==0);
    puts("PASS actual P08 near: advance/hold/recover cropped target, no centroid bias,6s loss preserves identity, provisional arrival gated by anchor/heading/coverage");
}
