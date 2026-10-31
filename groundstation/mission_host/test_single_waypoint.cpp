#include "Arduino.h"
#include <assert.h>
#include <stdio.h>
uint32_t mockMs=1000;int reason=0;float forward=0;
#define private public
#include "mission/BalloonMission.cpp"
#undef private
void tick(BalloonMission& m,AppContext& c,unsigned ms=20){
 mockMs+=ms;++c.robot->sensorsuite.fb.generation;c.robot->sensorsuite.barometer.sampleTimeUs=mockMs*1000;m.update(c);
 if(forward>0)assert(Telemetry::impulseYaw==0);
 assert(c.robot->motor_power1<=.200001f);
}
void waitPhase(BalloonMission& m,AppContext& c,ImpulseApproach::Phase phase,unsigned limit=5000){
 const auto start=mockMs;
 while(m.impulse_.phase!=phase && uint32_t(mockMs-start)<limit)tick(m,c);
 assert(m.impulse_.phase==phase);
}
int main(){
 ImpulseApproach p;p.update(100,true,true,false);assert(p.phase==ImpulseApproach::SETTLE);
 p.update(100+ImpulseApproach::SETTLE_MS,true,true,false);assert(p.phase==ImpulseApproach::SETTLE);
 const unsigned pushStart=120+ImpulseApproach::SETTLE_MS;
 p.update(pushStart,true,true,true);assert(p.phase==ImpulseApproach::PUSH && p.pulses==1);
 p.update(pushStart+ImpulseApproach::PUSH_MS-1,true,true,true);assert(p.phase==ImpulseApproach::PUSH);
 const unsigned returnStart=pushStart+ImpulseApproach::PUSH_MS;
 p.update(returnStart,true,true,false);assert(p.phase==ImpulseApproach::RETURN);
 p.update(returnStart+ImpulseApproach::SETTLE_MS,true,true,false);assert(p.phase==ImpulseApproach::RETURN);
 const unsigned coastStartUnit=returnStart+ImpulseApproach::SETTLE_MS+20;
 p.update(coastStartUnit,true,true,true);assert(p.phase==ImpulseApproach::COAST);
 p.update(coastStartUnit+ImpulseApproach::COAST_MS-1,true,true,true);assert(p.phase==ImpulseApproach::COAST && p.pulses==1);
 p.update(coastStartUnit+ImpulseApproach::COAST_MS,true,true,true);assert(p.phase==ImpulseApproach::ALIGN);
 p.enter(ImpulseApproach::PUSH,0xffffff00U);p.update(uint32_t(0xffffff00U+ImpulseApproach::PUSH_MS),true,true,true);
 assert(p.phase==ImpulseApproach::RETURN);
 p.reset();p.enter(ImpulseApproach::PUSH,100);p.update(120,true,false,false);
 assert(p.phase==ImpulseApproach::RETURN);
 AppContext c;RobotMock r;c.robot=&r;c.mode=17;
 c.command.params[AppConfig::PARAM_FZ]=8;
 c.sensors[SensorMap::YAW]=0;c.sensors[SensorMap::YAW_RATE]=0;
 c.sensors[SensorMap::ALTITUDE]=.8f;c.sensors[SensorMap::VERTICAL_VELOCITY]=0;r.sensorsuite.barometer.healthy=true;
 c.sensors[SensorMap::NICLA_FLAG]=65;c.sensors[SensorMap::NICLA_X_VALUE]=120;
 c.sensors[SensorMap::NICLA_Y_VALUE]=80;c.sensors[SensorMap::NICLA_W_VALUE]=100;
 c.sensors[SensorMap::NICLA_H_VALUE]=145;
 BalloonMission m;m.configure(1,true);m.reset(c);r.armed=true;
 waitPhase(m,c,ImpulseApproach::SETTLE);
 assert(forward==0 && Telemetry::impulseYaw==0);
 waitPhase(m,c,ImpulseApproach::PUSH);
 assert(forward==ApproachProfile::FORWARD && m.impulse_.pulses==1);
 const float integral=m.approachYaw_.integral;
 c.sensors[SensorMap::YAW_RATE]=.03f;c.sensors[SensorMap::NICLA_X_VALUE]=130;
 for(int i=0;i<50;++i)tick(m,c);
 assert(m.impulse_.phase==ImpulseApproach::PUSH && forward==ApproachProfile::FORWARD);
 assert(m.approachMixer_.forwardAngle>8);
 assert((r.servo_old1-52.5f)*ApproachProfile::FORWARD_SIGN>0 &&
        (64.5f-r.servo_old2)*ApproachProfile::FORWARD_SIGN>0);
 assert(Telemetry::impulseYaw==0 && m.approachYaw_.integral==integral);
 c.sensors[SensorMap::NICLA_X_VALUE]=175;tick(m,c);
 assert(m.impulse_.phase==ImpulseApproach::RETURN && forward==0 && Telemetry::impulseYaw==0);
 c.sensors[SensorMap::YAW_RATE]=0;
 waitPhase(m,c,ImpulseApproach::COAST);
 assert(forward==0 && fabsf(Telemetry::impulseYaw)>0);
 const auto coastStart=mockMs;c.sensors[SensorMap::NICLA_X_VALUE]=120;
 while(uint32_t(mockMs-coastStart)<ImpulseApproach::COAST_MS-20){tick(m,c);assert(forward==0);}
 waitPhase(m,c,ImpulseApproach::PUSH);
 assert(m.impulse_.pulses==2 && forward==ApproachProfile::FORWARD);
 // Cropped arrival needs the last FULL height anchor, not a cropped centroid.
 // Reference capture itself is covered in test_near_approach.
 r.sensorsuite.barometer.healthy=true;c.sensors[SensorMap::ALTITUDE]=.8f;
 c.sensors[SensorMap::VERTICAL_VELOCITY]=0;
 m.approachSupport_.altitudeReady=true;m.approachSupport_.altitudeReference=.8f;m.approachSupport_.fixedBase=.8f;
 r.sensorsuite.fb.state=NiclaFeedback::PARTIAL;c.sensors[SensorMap::NICLA_FLAG]=66;
 c.sensors[SensorMap::NICLA_W_VALUE]=225;tick(m,c);
 assert(m.state()==BalloonMission::VISIT_CONFIRM && forward==0 && m.visited()==0);
 c.sensors[SensorMap::NICLA_W_VALUE]=119;tick(m,c);
 assert(m.state()==BalloonMission::APPROACH && m.visited()==0);
 c.sensors[SensorMap::NICLA_W_VALUE]=225;
 for(int i=0;i<35;++i)tick(m,c);
 assert(m.state()==BalloonMission::DONE && m.visited()==1 && !r.armed);
 assert(r.motor_power1==0 && r.motor_power2==0 && forward==0);
 tick(m,c);assert(m.visited()==1 && !r.armed);
 c.sensors[SensorMap::NICLA_W_VALUE]=40;c.sensors[SensorMap::NICLA_H_VALUE]=30;
 c.sensors[SensorMap::NICLA_Y_VALUE]=60;c.sensors[SensorMap::ALTITUDE]=.7f;
 c.sensors[SensorMap::VERTICAL_VELOCITY]=-.12f;r.sensorsuite.barometer.healthy=true;
 m.reset(c);r.armed=true;
 for(int i=0;i<60;++i)tick(m,c);
 // Confirmed PARTIAL can now pulse while height recovery stays active.
 // Compare the vertical request, not an old PWM value: the reversed vector
 // changes the geometric lift compensation during the active pulse.
 assert(r.armed && m.approachSupport_.demand>0 &&
        r.motor_power1>0 && r.motor_power1<=.160001f);
 assert(!m.approachSupport_.altitudeReady && forward==0); // partial cannot invent plane
 c.sensors[SensorMap::VERTICAL_VELOCITY]=.15f;
 for(int i=0;i<60;++i)tick(m,c);
 assert(m.approachSupport_.demand<m.approachSupport_.height.trim && r.armed);
 r.sensorsuite.fb.state=NiclaFeedback::MISSING;tick(m,c,1000);
 assert(r.armed && forward==0 && m.visited()==0);
 r.sensorsuite.fb.state=NiclaFeedback::FULL;c.sensors[SensorMap::NICLA_FLAG]=65;
 c.sensors[SensorMap::NICLA_Y_VALUE]=80;c.sensors[SensorMap::VERTICAL_VELOCITY]=0;
 tick(m,c);assert(r.armed && m.approachSupport_.phase==VisualSupport::RUN);
 for(int i=0;i<30;++i)tick(m,c);
 const float anchor=m.approachSupport_.altitudeReference;
 const float bearing=m.approachBearing_.reference;
 r.sensorsuite.fb.state=NiclaFeedback::MISSING;tick(m,c,4500);
 assert(r.armed && forward==0 && m.state()==BalloonMission::APPROACH);
 const unsigned resets=r.sensorsuite.resets;
 r.sensorsuite.fb.state=NiclaFeedback::LOST;tick(m,c,800);
 assert(r.armed && r.sensorsuite.resets==resets+1 && r.sensorsuite.fb.resetPending);
 assert(m.approachSupport_.altitudeReference==anchor && m.approachBearing_.reference==bearing);
 tick(m,c,200);assert(r.sensorsuite.resets==resets+1 && r.armed);
 r.sensorsuite.fb.resetPending=false;r.sensorsuite.fb.fresh=true;
 r.sensorsuite.fb.state=NiclaFeedback::SEARCH;tick(m,c,100);
 assert(r.armed && forward==0);
 r.sensorsuite.fb.state=NiclaFeedback::FULL;c.sensors[SensorMap::NICLA_X_VALUE]=230;
 tick(m,c,100);assert(m.approachReacquiring_ && forward==0 && m.approachBearing_.reference==bearing);
 c.sensors[SensorMap::NICLA_X_VALUE]=120;tick(m,c,100);
 assert(!m.approachReacquiring_ && r.armed && m.approachSupport_.phase==VisualSupport::RUN);
 assert(m.approachSupport_.altitudeReference==anchor && m.visited()==0);
 r.sensorsuite.fb.state=NiclaFeedback::MISSING;tick(m,c,10000);
 assert(r.armed && forward==0);
 tick(m,c,4900);assert(r.armed);
 tick(m,c,101);assert(!r.armed && m.state()==BalloonMission::TURN_FAILED && r.motor_power1==0);
 r.sensorsuite.fb.state=NiclaFeedback::FULL;m.reset(c);r.armed=true;tick(m,c);
 r.sensorsuite.fb.fresh=false;tick(m,c,1600);assert(!r.armed && r.motor_power1==0);
 r.sensorsuite.fb.fresh=true;r.sensorsuite.fb.resetPending=false;
 r.sensorsuite.fb.state=NiclaFeedback::FULL;m.reset(c);r.armed=true;
 for(int i=0;i<35;++i)tick(m,c);
 assert(m.approachSupport_.altitudeReady);
 for(int i=0;i<62;++i)tick(m,c,1000);
 assert(r.armed && m.visited()==0);
 tick(m,c,120000);assert(!r.armed && m.state()==BalloonMission::TURN_FAILED);
 // After capturing the plane, cropped y125 must not redefine metric height.
 // A partial track can pulse while the saved height stays controlled.
 c.sensors[SensorMap::YAW]=0;c.sensors[SensorMap::YAW_RATE]=0;
 c.sensors[SensorMap::NICLA_X_VALUE]=120;c.sensors[SensorMap::NICLA_Y_VALUE]=80;
 c.sensors[SensorMap::NICLA_W_VALUE]=60;c.sensors[SensorMap::NICLA_H_VALUE]=40;
 c.sensors[SensorMap::VERTICAL_VELOCITY]=0;
 r.sensorsuite.barometer.healthy=true;
 m.reset(c);r.armed=true;for(int i=0;i<35;++i)tick(m,c);assert(m.approachHadFull_ && m.approachSupport_.altitudeReady);
 r.sensorsuite.fb.state=NiclaFeedback::PARTIAL;c.sensors[SensorMap::NICLA_FLAG]=66;
 c.sensors[SensorMap::NICLA_Y_VALUE]=125;c.sensors[SensorMap::NICLA_H_VALUE]=70;
 waitPhase(m,c,ImpulseApproach::PUSH);
 assert(forward==ApproachProfile::FORWARD && m.approachSupport_.inertia.desiredRate==0);
 assert(Telemetry::impulseYaw==0 && r.armed && m.visited()==0);
 // Visibility or horizontal alignment is still mandatory.
 c.sensors[SensorMap::NICLA_X_VALUE]=180;tick(m,c);
 assert(forward==0 && m.impulse_.phase==ImpulseApproach::RETURN);
 // Start WITHOUT a balloon. SEARCH must actuate gently instead of requiring
 // operator alignment first, and must hand over without a power restart.
 r.sensorsuite.fb.state=NiclaFeedback::SEARCH;c.sensors[SensorMap::NICLA_FLAG]=0;
 c.sensors[SensorMap::ALTITUDE]=0;c.sensors[SensorMap::VERTICAL_VELOCITY]=0;
 c.sensors[SensorMap::YAW]=0;c.sensors[SensorMap::YAW_RATE]=0;
 r.sensorsuite.barometer.healthy=true;r.armed=false;m.reset(c);
 r.sensorsuite.fb.fresh=true;r.sensorsuite.fb.resetPending=false;
 tick(m,c,2000);assert(!m.initialSearch_.started && r.motor_power1==0);
 r.armed=true;
 for(unsigned i=0;i<100;++i)tick(m,c);
 assert(m.state()==BalloonMission::SEARCH && r.armed && forward==0);
 assert(r.motor_power1>=.10f && r.motor_power1<=.160001f);
 assert(fabsf(m.yawRef_)<=20.f*ControlCommon::DEG2RAD);
 assert(Telemetry::impulsePhase==5);
 r.sensorsuite.fb.state=NiclaFeedback::CONFIRM;tick(m,c);
 assert(m.state()==BalloonMission::SEARCH && forward==0);
 const float searchPower=r.motor_power1,searchTrim=m.initialSearch_.trim;
 r.sensorsuite.fb.state=NiclaFeedback::FULL;c.sensors[SensorMap::NICLA_FLAG]=65;
 c.sensors[SensorMap::NICLA_X_VALUE]=120;c.sensors[SensorMap::NICLA_Y_VALUE]=80;
 c.sensors[SensorMap::NICLA_W_VALUE]=60;c.sensors[SensorMap::NICLA_H_VALUE]=40;
 tick(m,c);
 assert(m.state()==BalloonMission::APPROACH && m.approachHadFull_ && r.armed);
 assert(m.approachBase_==searchTrim && fabsf(r.motor_power1-searchPower)<.01f);
 waitPhase(m,c,ImpulseApproach::PUSH);assert(forward==ApproachProfile::FORWARD);
 // Onboard deadlines apply even if the host disappears.
 r.sensorsuite.fb.state=NiclaFeedback::SEARCH;c.sensors[SensorMap::NICLA_FLAG]=0;
 m.reset(c);r.armed=true;r.sensorsuite.fb.fresh=true;r.sensorsuite.fb.resetPending=false;
 tick(m,c);tick(m,c,20000);
 assert(m.state()==BalloonMission::TURN_FAILED && !r.armed && r.motor_power1==0);
 m.reset(c);r.armed=true;r.sensorsuite.fb.fresh=true;r.sensorsuite.fb.resetPending=false;
 tick(m,c);r.sensorsuite.fb.fresh=false;tick(m,c,1600);
 assert(m.state()==BalloonMission::TURN_FAILED && !r.armed && r.motor_power1==0);
 // A depleted envelope requires support above the old16% cap. The explicit
 //20% ceiling must survive search -> approach; no reset to12/15/16%.
 c.command.params[AppConfig::PARAM_AUX0]=.20f;
 c.command.params[AppConfig::PARAM_AUX1]=.18f;
 r.sensorsuite.fb.state=NiclaFeedback::SEARCH;c.sensors[SensorMap::NICLA_FLAG]=0;
 c.sensors[SensorMap::ALTITUDE]=0;c.sensors[SensorMap::VERTICAL_VELOCITY]=-.1f;
 m.reset(c);r.armed=true;r.sensorsuite.fb.fresh=true;r.sensorsuite.fb.resetPending=false;
 for(unsigned i=0;i<180;++i){c.sensors[SensorMap::ALTITUDE]-=.01f;tick(m,c,100);}
 assert(m.state()==BalloonMission::SEARCH && r.armed && r.motor_power1>.18f);
 const float adapted=m.initialSearch_.trim;
 r.sensorsuite.fb.state=NiclaFeedback::FULL;c.sensors[SensorMap::NICLA_FLAG]=65;
 c.sensors[SensorMap::NICLA_Y_VALUE]=40;tick(m,c);
 assert(m.state()==BalloonMission::APPROACH && m.approachBase_==adapted);
 assert(m.approachSupport_.height.trim>=adapted-1.e-5f && r.motor_power1>.16f);
 // Reproduce the last real failure: SEARCH, no accepted target, ascending.
 // The new governor must be active here, before APPROACH ever starts.
 r.sensorsuite.fb.state=NiclaFeedback::SEARCH;c.sensors[SensorMap::NICLA_FLAG]=0;
 c.sensors[SensorMap::NICLA_X_VALUE]=200;c.sensors[SensorMap::NICLA_W_VALUE]=40;
 c.sensors[SensorMap::NICLA_H_VALUE]=40;c.sensors[SensorMap::ALTITUDE]=0;
 c.sensors[SensorMap::VERTICAL_VELOCITY]=.2f;
 m.reset(c);r.armed=true;r.sensorsuite.fb.fresh=true;r.sensorsuite.fb.resetPending=false;
 for(unsigned i=0;i<60;++i){c.sensors[SensorMap::ALTITUDE]=.2f*(i+1)*.02f;tick(m,c);}
 assert(m.state()==BalloonMission::SEARCH && forward==0 && r.armed);
 assert(m.initialSearch_.inertia.brakingUp && r.motor_power1<m.initialSearch_.trim && r.motor_power1>=.05999f);
 r.sensorsuite.fb.state=NiclaFeedback::CONFIRM;c.sensors[SensorMap::YAW]=.1f;tick(m,c);
 assert(m.state()==BalloonMission::SEARCH && m.yawRef_==.1f);
 assert(m.initialSearch_.inertia.desiredRate==0);
 puts("PASS impulse mission:15s visual recovery, camera reset/ACK, bearing gate, reference retained, heartbeat fault, impulse/arrival");
}
