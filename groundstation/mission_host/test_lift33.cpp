#include "Arduino.h"
#include <assert.h>
#include <stdio.h>
uint32_t mockMs=1000;int reason=0;float forward=0;
#define private public
#include "mission/BalloonMission.cpp"
#undef private
struct LiftTrial {
    AppContext c;RobotMock r;BalloonMission m;
    LiftTrial(float ceiling=.25f) {
        c.robot=&r;c.mode=17;c.command.params[AppConfig::PARAM_AUX0]=ceiling;
        c.command.params[AppConfig::PARAM_AUX1]=.18f;
        c.sensors[SensorMap::ALTITUDE]=0;c.sensors[SensorMap::VERTICAL_VELOCITY]=0;
        c.sensors[SensorMap::YAW]=c.sensors[SensorMap::YAW_RATE]=c.sensors[SensorMap::PITCH]=0;
        image(40);m.configure(1,true);m.reset(c);r.armed=true;
    }
    void image(float y) {
        r.sensorsuite.fb.state=NiclaFeedback::FULL;c.sensors[SensorMap::NICLA_FLAG]=65;
        c.sensors[SensorMap::NICLA_X_VALUE]=120;c.sensors[SensorMap::NICLA_Y_VALUE]=y;
        c.sensors[SensorMap::NICLA_W_VALUE]=80;c.sensors[SensorMap::NICLA_H_VALUE]=60;
    }
    void step() {mockMs+=20;++r.sensorsuite.fb.generation;m.update(c);
        assert(r.motor_power1<=.250001f && r.motor_power2<=.250001f);
        if(forward>0)assert(Telemetry::impulseYaw==0);}
    void run(unsigned ms){for(unsigned i=0;i<ms;i+=20)step();}
};
int main() {
    assert(VisualBearing::VERSION>=33);
    LiftTrial high;assert(high.m.approachBase_==.18f);
    // Isolate lift authority from forward vectoring, whose physical angle
    // changes the geometry compensation when its installation sign changes.
    high.c.sensors[SensorMap::NICLA_X_VALUE]=170;
    high.run(1800);assert(high.r.armed && high.r.motor_power1>.20f && high.m.visited()==0);
    const float climbing=high.m.approachSupport_.demand;
    high.image(120);high.run(1800);
    assert(high.r.armed && high.m.approachSupport_.demand<climbing); // unloading still works
    LiftTrial low(.16f);low.run(2000);assert(low.r.motor_power1<=.160001f);
    LiftTrial search;
    search.r.sensorsuite.fb.state=NiclaFeedback::SEARCH;search.c.sensors[SensorMap::NICLA_FLAG]=0;
    search.m.reset(search.c);search.run(200);
    assert(search.m.initialSearch_.trim>=.18f);
    const float inherited=search.m.initialSearch_.trim;
    search.image(40);search.step();
    assert(search.m.approachBase_==inherited && search.m.approachBase_>=.18f);
    search.r.sensorsuite.fb.fresh=false;
    for(unsigned i=0;i<85 && search.r.armed;++i)search.step();
    assert(!search.r.armed && search.r.motor_power1==0);
    puts("PASS lift33:18% seed, above20% support,25% cap, reduced lift above target, lower requested cap, search handover, link-loss stop");
}
