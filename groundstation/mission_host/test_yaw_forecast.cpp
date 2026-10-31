#include "Arduino.h"
#include <assert.h>
#include <stdio.h>
uint32_t mockMs=1000;int reason=0;float forward=0;
#define private public
#include "mission/BalloonMission.cpp"
#undef private
void tick(BalloonMission& m,AppContext& c){
    mockMs+=20;++c.robot->sensorsuite.fb.generation;c.robot->sensorsuite.barometer.sampleTimeUs=mockMs*1000;m.update(c);
    if(forward>0)assert(Telemetry::impulseYaw==0);
}
int main(){
    AppContext c;RobotMock r;c.robot=&r;c.mode=17;
    c.sensors[SensorMap::ALTITUDE]=.8f;c.sensors[SensorMap::VERTICAL_VELOCITY]=0;r.sensorsuite.barometer.healthy=true;
    c.sensors[SensorMap::NICLA_FLAG]=65;
    c.sensors[SensorMap::NICLA_X_VALUE]=120;c.sensors[SensorMap::NICLA_Y_VALUE]=80;
    c.sensors[SensorMap::NICLA_W_VALUE]=100;c.sensors[SensorMap::NICLA_H_VALUE]=100;
    r.sensorsuite.fb.state=NiclaFeedback::FULL;
    // Modest rotation is allowed with a short receding prediction horizon;
    // perfect alignment throughout the whole pulse is no longer required.
    c.sensors[SensorMap::YAW_RATE]=0;
    c.command.params[AppConfig::PARAM_AUX4]=float(8388608u|2u|(50u<<2)|(24u<<9)|(60u<<16));
    BalloonMission m;m.configure(1,true);m.reset(c);r.armed=true;
    for(unsigned i=0;i<35;++i)tick(m,c);
    assert(m.approachSupport_.altitudeReady && m.impulse_.pulses==0);
    c.command.params[AppConfig::PARAM_AUX4]=24;
    c.sensors[SensorMap::NICLA_FLAG]=66;r.sensorsuite.fb.state=NiclaFeedback::PARTIAL;
    c.sensors[SensorMap::YAW_RATE]=-.10f;
    for(unsigned i=0;i<110;++i)tick(m,c);
    assert(m.impulse_.pulses>=1);
    // It must resume progress once rotation has been damped, without reset.
    c.sensors[SensorMap::YAW_RATE]=0;
    for(unsigned i=0;i<300 && m.impulse_.phase!=ImpulseApproach::PUSH;++i)tick(m,c);
    assert(m.impulse_.phase==ImpulseApproach::PUSH && forward>0);
    // Developing angular drift ends the pulse BEFORE the present angle is
    // outside the instantaneous bound, leaving existing coast correction active.
    c.sensors[SensorMap::YAW_RATE]=-.4f;
    for(unsigned i=0;i<15 && m.impulse_.phase==ImpulseApproach::PUSH;++i)tick(m,c);
    assert(m.impulse_.phase==ImpulseApproach::RETURN && forward==0);
    assert(r.armed && m.visited()==0);
    puts("PASS P08 yaw prediction: brake before pulse, resume without reset, early drift cancellation");
}
