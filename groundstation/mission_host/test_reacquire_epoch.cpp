#include "Arduino.h"
#include <assert.h>
#include <stdio.h>
uint32_t mockMs=1000;int reason=0;float forward=0;
#define private public
#include "mission/BalloonMission.cpp"
#undef private
struct Trial{
    AppContext c;RobotMock r;BalloonMission m;
    Trial(){c.robot=&r;c.mode=17;r.sensorsuite.barometer.healthy=true;
        c.sensors[SensorMap::ALTITUDE]=.8f;c.sensors[SensorMap::VERTICAL_VELOCITY]=0;
        image(true,70,40,100,80,120,80);m.configure(1,true);m.reset(c);r.armed=true;}
    void image(bool full,float left,float top,float w,float h,float cx,float cy){
        r.sensorsuite.fb.state=full?NiclaFeedback::FULL:NiclaFeedback::PARTIAL;
        c.sensors[SensorMap::NICLA_FLAG]=full?65:66;
        c.sensors[SensorMap::NICLA_X]=left;c.sensors[SensorMap::NICLA_Y]=top;
        c.sensors[SensorMap::NICLA_W]=c.sensors[SensorMap::NICLA_W_VALUE]=w;
        c.sensors[SensorMap::NICLA_H]=c.sensors[SensorMap::NICLA_H_VALUE]=h;
        c.sensors[SensorMap::NICLA_X_VALUE]=cx;c.sensors[SensorMap::NICLA_Y_VALUE]=cy;
    }
    void step(){mockMs+=20;++r.sensorsuite.fb.generation;
        r.sensorsuite.fb.sample=(r.sensorsuite.fb.sample&0xff00)|((r.sensorsuite.fb.sample+1)&255);
        r.sensorsuite.barometer.sampleTimeUs=mockMs*1000;m.update(c);
        if(forward>0)assert(Telemetry::impulseYaw==0);
        assert(r.motor_power1<=.200001f && r.motor_power2<=.200001f);}
    void run(unsigned ms){for(unsigned i=0;i<ms;i+=20)step();}
    void epoch(unsigned value){r.sensorsuite.fb.sample=(value<<8)|(r.sensorsuite.fb.sample&255);}
};
int main(){
    Trial t;t.run(4500);
    assert(t.m.approachSupport_.altitudeReady);
    const float anchor=t.m.approachSupport_.altitudeReference,bearing=t.m.approachBearing_.reference;
    t.epoch(1);t.image(true,195,65,30,30,210,80);t.run(1000);
    assert(t.m.approachReacquiring_ && !t.m.approachHadFull_ && forward==0);
    assert(t.m.approachSupport_.altitudeReference==anchor && t.m.approachBearing_.reference==bearing);
    // A new association cannot inherit the old FULL to bypass near gates.
    t.image(false,0,0,240,160,120,80);t.run(500);
    assert(t.m.approachReacquiring_ && !t.m.approachHadFull_ && forward==0 && t.m.visited()==0);
    t.image(true,70,40,100,80,120,80);t.run(1000);
    assert(!t.m.approachReacquiring_ && t.m.approachHadFull_ && t.r.armed);
    assert(t.m.approachSupport_.altitudeReference==anchor);
    t.r.sensorsuite.fb.sample=511;t.step(); // frame255->0, SAME epoch1
    assert(t.m.approachAssociationEpoch_==1 && !t.m.approachReacquiring_);
    Trial confirming;confirming.run(4500);
    confirming.image(false,0,0,240,160,55,120);confirming.step();
    assert(confirming.m.state()==BalloonMission::VISIT_CONFIRM);
    confirming.epoch(1);confirming.image(true,195,65,30,30,210,80);confirming.run(700);
    assert(confirming.m.state()==BalloonMission::APPROACH && confirming.m.visited()==0 && forward==0);
    Trial course;course.m.configure(2,false);course.m.reset(course.c);course.r.armed=true;
    course.run(100);assert(course.m.visualHeightSeen_);
    const float courseAnchor=course.m.visualHeightAnchor_;
    course.epoch(1);course.image(true,195,65,30,30,210,80);course.run(100);
    assert(course.m.approachReacquiring_ && forward==0 && course.m.visited()==0);
    assert(course.m.visualHeightAnchor_==courseAnchor);
    course.image(true,70,40,100,80,120,80);course.run(100);
    assert(!course.m.approachReacquiring_ && course.r.armed);
    puts("PASS epoch: wrong bearing rejected, new partial cannot inherit FULL, expected FULL accepted, frame wrap ignored, arrival confirmation cancelled on reassociation");
}
