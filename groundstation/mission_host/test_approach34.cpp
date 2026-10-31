#include "Arduino.h"
#include <assert.h>
#include <stdio.h>
uint32_t mockMs=1000;int reason=0;float forward=0;
#define private public
#include "mission/BalloonMission.cpp"
#undef private
struct Trial34 {
    AppContext c;RobotMock r;BalloonMission m;
    Trial34(){
        c.robot=&r;c.mode=17;c.command.params[AppConfig::PARAM_AUX0]=.25f;
        c.command.params[AppConfig::PARAM_AUX1]=.18f;
        c.sensors[SensorMap::ALTITUDE]=.8f;c.sensors[SensorMap::VERTICAL_VELOCITY]=0;
        r.sensorsuite.barometer.healthy=true;
        c.sensors[SensorMap::YAW]=c.sensors[SensorMap::YAW_RATE]=c.sensors[SensorMap::PITCH]=0;
        image();c.sensors[SensorMap::NICLA_X_VALUE]=120;c.sensors[SensorMap::NICLA_Y_VALUE]=80;
        m.configure(1,true);m.reset(c);r.armed=true;
        run(600);assert(m.approachSupport_.altitudeReady);image();
    }
    void image(){
        r.sensorsuite.fb.state=NiclaFeedback::FULL;c.sensors[SensorMap::NICLA_FLAG]=65;
        c.sensors[SensorMap::NICLA_X_VALUE]=145;c.sensors[SensorMap::NICLA_Y_VALUE]=45;
        c.sensors[SensorMap::NICLA_W_VALUE]=80;c.sensors[SensorMap::NICLA_H_VALUE]=60;
    }
    void step(){mockMs+=20;++r.sensorsuite.fb.generation;
        r.sensorsuite.barometer.sampleTimeUs=mockMs*1000;m.update(c);
        if(forward>0)assert(Telemetry::impulseYaw==0);
        assert(r.motor_power1<=.250001f && r.motor_power2<=.250001f);
        assert(r.servo_old1>=0 && r.servo_old1<=120 && r.servo_old2>=0 && r.servo_old2<=120);}
    void run(unsigned ms){for(unsigned i=0;i<ms;i+=20)step();}
    void push(){for(unsigned i=0;i<200 && m.impulse_.phase!=ImpulseApproach::PUSH;++i)step();
        assert(m.impulse_.phase==ImpulseApproach::PUSH && forward==ApproachProfile::FORWARD);}
    void hide(){r.sensorsuite.fb.state=NiclaFeedback::MISSING;c.sensors[SensorMap::NICLA_FLAG]=0;}
};
int main(){
    assert(VisualBearing::VERSION>=34);
    Trial34 t;t.push();t.run(600);
    assert(t.m.approachMixer_.forwardAngle>15 && t.m.approachMixer_.forwardAngle<=22);
    const float bearing=t.m.approachBearing_.reference;
    t.hide();t.run(400);
    assert(forward==ApproachProfile::FORWARD && t.r.armed && t.m.visited()==0 && t.m.approachBearing_.reference==bearing);
    t.run(420);assert(forward==0 && t.m.visited()==0); //800ms memory expires
    const unsigned pulses=t.m.impulse_.pulses;
    t.run(2500);assert(t.m.impulse_.pulses==pulses && forward==0); //no blind new pulse
    Trial34 height;height.push();height.hide();height.c.sensors[SensorMap::ALTITUDE]=1.1f;
    height.step();assert(forward==0);
    Trial34 noBaro;noBaro.push();noBaro.hide();noBaro.r.sensorsuite.barometer.healthy=false;
    noBaro.step();assert(forward==0);
    Trial34 ambiguity;ambiguity.push();ambiguity.r.sensorsuite.fb.state=NiclaFeedback::AMBIG;
    ambiguity.step();assert(forward==0);
    Trial34 epoch;epoch.push();epoch.r.sensorsuite.fb.sample=256;epoch.hide();epoch.step();
    assert(forward==0 && !epoch.m.approachMemory_.ready && epoch.m.visited()==0);
    Trial34 link;link.push();link.hide();link.r.sensorsuite.fb.fresh=false;link.step();assert(forward==0);
    // Wider angle is explicit; existing profiles retain12deg and yaw priority.
    SupportYaw::Mixer tilt,legacy;tilt.reset(true);legacy.reset(true);
    for(int i=0;i<200;++i){
        tilt.update(.18f,0,.25f,0,.25f,.02f,0,0,.08f,true,22.f);
        legacy.update(.18f,0,.25f,0,.25f,.02f,0,0,.08f,true);
    }
    assert(tilt.forwardAngle>21.9f && tilt.forwardAngle<=22);
    assert(legacy.forwardAngle<=12);
    tilt.update(.18f,1,.25f,0,.25f,.02f,0,0,.08f,true,22.f);
    assert(tilt.forwardAngle<1.e-5f);
    puts("PASS approach34: imperfect alignment,22deg attack,.08 impulse,800ms bounded occlusion, no blind restart/arrival, baro/epoch/link/ambiguity gates, mechanical envelope");
}
