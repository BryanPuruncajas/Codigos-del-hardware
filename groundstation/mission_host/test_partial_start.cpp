#include "Arduino.h"
#include <assert.h>
#include <stdio.h>
uint32_t mockMs=1000;int reason=0;float forward=0;
#define private public
#include "mission/BalloonMission.cpp"
#undef private

struct Trial {
    AppContext c;RobotMock r;BalloonMission m;
    Trial(){
        c.robot=&r;c.mode=17;
        c.sensors[SensorMap::ALTITUDE]=.8f;r.sensorsuite.barometer.healthy=true;
        c.sensors[SensorMap::VERTICAL_VELOCITY]=0;
        c.sensors[SensorMap::NICLA_FLAG]=66;
        c.sensors[SensorMap::NICLA_X_VALUE]=120;
        c.sensors[SensorMap::NICLA_Y_VALUE]=80;
        c.sensors[SensorMap::NICLA_W_VALUE]=100;
        c.sensors[SensorMap::NICLA_H_VALUE]=100;
        r.sensorsuite.fb.state=NiclaFeedback::PARTIAL;
        m.configure(1,true);m.reset(c);r.armed=true;
    }
    void step(){
        mockMs+=20;++r.sensorsuite.fb.generation;r.sensorsuite.barometer.sampleTimeUs=mockMs*1000;m.update(c);
        if(forward>0)assert(Telemetry::impulseYaw==0);
    }
    void run(unsigned milliseconds){for(unsigned i=0;i<milliseconds;i+=20)step();}
};

int main(){
    // Exact failure class in flight003101: confirmed PARTIAL throughout,
    // without a captured plane. v37 may regulate vertically but must not
    // invent a metric reference from a clipped centre or begin a blind pulse.
    Trial partial;partial.run(1800);
    assert(!partial.m.approachHadFull_);
    assert(!partial.m.approachSupport_.altitudeReady && partial.m.impulse_.pulses==0 && forward==0);
    partial.c.sensors[SensorMap::NICLA_FLAG]=65;partial.r.sensorsuite.fb.state=NiclaFeedback::FULL;
    partial.run(700);assert(partial.m.approachSupport_.altitudeReady);
    partial.c.sensors[SensorMap::NICLA_FLAG]=66;partial.r.sensorsuite.fb.state=NiclaFeedback::PARTIAL;
    partial.run(1500);assert(partial.m.impulse_.pulses>=1 && forward>0);
    assert(partial.r.armed && partial.m.visited()==0);
    // Healthy anchored height permits only 800ms of existing-pulse memory.
    // A cached frame still cannot unlock a new pulse.
    partial.r.sensorsuite.fb.state=NiclaFeedback::MISSING;partial.run(820);
    assert(forward==0);
    Trial cached;
    for(unsigned i=0;i<100;++i){mockMs+=20;cached.m.update(cached.c);}
    assert(cached.m.impulse_.pulses==0 && forward==0);
    // Partial does not waive horizontal/vertical/rate alignment gates.
    Trial offCentre;offCentre.c.sensors[SensorMap::NICLA_X_VALUE]=180;
    offCentre.run(2000);assert(offCentre.m.impulse_.pulses==0 && forward==0);
    Trial above;above.c.sensors[SensorMap::NICLA_Y_VALUE]=159;
    above.run(2000);assert(above.m.impulse_.pulses==0 && forward==0);
    Trial spinning;spinning.c.sensors[SensorMap::YAW_RATE]=.4f;
    spinning.run(2000);assert(spinning.m.impulse_.pulses==0 && forward==0);
    Trial candidate;candidate.c.sensors[SensorMap::NICLA_FLAG]=0;
    candidate.r.sensorsuite.fb.state=NiclaFeedback::SEARCH;
    candidate.run(2000);assert(candidate.m.impulse_.pulses==0 && forward==0);
    Trial ambiguous;ambiguous.r.sensorsuite.fb.state=NiclaFeedback::AMBIG;
    ambiguous.run(2000);assert(ambiguous.m.impulse_.pulses==0 && forward==0);
    // This patch does not relabel partial geometry as a calibrated arrival.
    Trial large;large.c.sensors[SensorMap::NICLA_W_VALUE]=225;
    large.run(2000);assert(large.m.visited()==0 && large.r.armed);
    puts("PASS actual P08 partial-only acquisition, alignment, loss, ambiguity, arrival separation");
}
