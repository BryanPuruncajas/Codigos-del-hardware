#include "Arduino.h"
#include <assert.h>
#include <stdio.h>
uint32_t mockMs=1000;int reason=0;float forward=0;
#define private public
#include "mission/BalloonMission.cpp"
#undef private
struct Trial38 {
    AppContext c;RobotMock r;BalloonMission m;FlowProtocol::Packet p{};
    Trial38(){
        c.robot=&r;c.mode=17;c.command.params[AppConfig::PARAM_AUX0]=.25f;
        c.command.params[AppConfig::PARAM_AUX1]=.18f;
        c.command.params[AppConfig::PARAM_AUX4]=float(8388608u|(50u<<2)|(24u<<9)|(60u<<16)); // held, vertical only
        c.sensors[SensorMap::ROLL]=3.14159265f;
        c.sensors[SensorMap::ALTITUDE]=.3f;r.sensorsuite.barometer.healthy=true;
        c.sensors[SensorMap::NICLA_FLAG]=65;c.sensors[SensorMap::NICLA_X_VALUE]=120;
        c.sensors[SensorMap::NICLA_Y_VALUE]=80;c.sensors[SensorMap::NICLA_W_VALUE]=80;c.sensors[SensorMap::NICLA_H_VALUE]=60;
        p.magic=FlowProtocol::MAGIC;p.version=1;p.flags=3;p.range_mm=800;p.quality=80;
        for(unsigned i=0;i<30;++i)sensors();
        m.configure(1,true);m.reset(c);r.armed=true;
    }
    void sensors(bool flow=true){
        mockMs+=20;r.sensorsuite.barometer.sampleTimeUs=mockMs*1000;
        if(flow)++p.sequence;
        c.flowHeight.update(mockMs,true,p,0,c.sensors[SensorMap::ALTITUDE],c.sensors[SensorMap::VERTICAL_VELOCITY],true,
            mockMs*1000,c.sensors[SensorMap::ROLL],c.sensors[SensorMap::PITCH],0);
    }
    void step(bool flow=true){sensors(flow);++r.sensorsuite.fb.generation;m.update(c);
        assert(r.motor_power1<=.250001f && r.motor_power2<=.250001f);
        if(forward>0)assert(Telemetry::impulseYaw==0);
    }
    void run(unsigned ms,bool flow=true){for(unsigned i=0;i<ms;i+=20)step(flow);}
};
int main(){
    assert(VisualBearing::VERSION==38);
    Trial38 t;t.run(1000);
    assert(t.m.approachSupport_.independentHeight && t.m.approachSupport_.altitudeReady && forward==0);
    const float target=t.m.approachSupport_.fixedTarget();
    t.c.command.params[AppConfig::PARAM_AUX4]+=1; // released, still vertical only
    for(unsigned i=0;i<150;++i){t.c.sensors[SensorMap::ALTITUDE]+=.01f;t.step();}
    assert(t.m.approachSupport_.fixedTarget()==target);
    assert(fabsf(t.m.approachSupport_.referenceError(t.c.flowHeight.height,mockMs))<.001f);
    assert(t.m.approachSupport_.inertia.desiredRate==0 && t.m.impulse_.pulses==0 && t.r.armed);
    for(unsigned i=0;i<40;++i){t.p.range_mm-=5;t.step();}t.run(1200);
    assert(t.m.approachSupport_.demand>t.m.approachSupport_.height.trim && t.r.armed);
    for(unsigned i=0;i<80;++i){t.p.range_mm+=5;t.step();}t.run(1200);
    assert(t.m.approachSupport_.demand<t.m.approachSupport_.height.trim && t.r.armed);
    for(unsigned i=0;i<40;++i){t.p.range_mm-=5;t.step();}t.run(1200);
    t.run(300,false);assert(t.c.flowHeight.source==FlowHeight::BARO_BRIDGE && t.r.armed && forward==0);
    t.run(1900,false);assert(!t.r.armed && t.m.state()==BalloonMission::TURN_FAILED && t.r.motor_power1==0);
    Trial38 forwardTrial;forwardTrial.run(1000);
    forwardTrial.c.command.params[AppConfig::PARAM_AUX4]+=3; // enable forward and release
    forwardTrial.run(1400);assert(forwardTrial.m.impulse_.pulses>0);
    // Source loss cuts an already active pulse before the bounded bridge ends.
    forwardTrial.run(2200,false);assert(!forwardTrial.r.armed && forward==0);
    puts("PASS real mission38: independent Flowdeck plane, pressure drift ignored, fixed metric up/down response, unchanged limits and release/yaw gates, bounded source-loss stop");
}
