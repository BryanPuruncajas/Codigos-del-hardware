#include "Arduino.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>
uint32_t mockMs=1000;int reason=0;float forward=0;
// Compile the REAL mission implementation against hardware/telemetry stubs.
#define private public
#include "mission/BalloonMission.cpp"
#undef private
void setup(BalloonMission& m,AppContext& c,RobotMock& r,bool bench=false){
 c=AppContext();r=RobotMock();c.robot=&r;c.mode=bench?22:21;
 c.command.params[AppConfig::PARAM_FX]=.15f;
 c.command.params[AppConfig::PARAM_TX]=.30f;
 c.command.params[AppConfig::PARAM_TZ]=.025f;
 c.command.params[AppConfig::PARAM_AUX0]=.12f;
 c.command.params[AppConfig::PARAM_AUX1]=.18f;
 c.command.params[AppConfig::PARAM_AUX2]=.06f;
 c.command.params[AppConfig::PARAM_AUX3]=.10f;
 c.command.params[AppConfig::PARAM_AUX4]=15;
 m.configure(5,false,2);m.reset(c);
 r.sensorsuite.fb.fresh=true;r.sensorsuite.fb.resetPending=false;
 r.armed=!bench;
 c.sensors[SensorMap::NICLA_FLAG]=0x41;
 c.sensors[SensorMap::NICLA_X_VALUE]=120;
 c.sensors[SensorMap::NICLA_Y_VALUE]=80;
 c.sensors[SensorMap::NICLA_W_VALUE]=40;
 c.sensors[SensorMap::NICLA_H_VALUE]=30;
}
void tick(BalloonMission& m,AppContext& c,unsigned ms=100){
 mockMs+=ms;++c.robot->sensorsuite.fb.generation;m.update(c);
}
int main(){
 BalloonMission m;AppContext c;RobotMock r;
 setup(m,c,r);
 // Deliberately wrong operator height cannot override a visible first target.
 m.searchHeight_=8;tick(m,c);assert(m.state()==BalloonMission::APPROACH);
 for(int i=0;i<10;++i)tick(m,c);
 assert(forward>0);
 c.sensors[SensorMap::NICLA_Y_VALUE]=40;tick(m,c);
 assert(forward==0 && m.visualHeight_.demand>.10f);
 // Good vision does not require a coherent barometric height.
 c.sensors[SensorMap::ALTITUDE]=NAN;tick(m,c);
 assert(reason!=15 && m.state()==BalloonMission::APPROACH);
 c.sensors[SensorMap::ALTITUDE]=0;
 r.sensorsuite.fb.state=NiclaFeedback::MISSING;tick(m,c,100);
 assert(forward==0 && m.state()==BalloonMission::APPROACH);
 const uint32_t seen=m.visualHeightLastSeenMs_;
 for(int i=0;i<30;++i)tick(m,c);
 assert(fabsf(m.searchHeight_-m.visualHeightAnchor_)<=.20001f);
 assert(m.state()==BalloonMission::APPROACH && r.armed);
 r.sensorsuite.fb.state=NiclaFeedback::FULL;tick(m,c);
 assert(m.state()==BalloonMission::APPROACH);
 assert(m.visualHeightLastSeenMs_>seen);
 r.sensorsuite.fb.state=NiclaFeedback::MISSING;tick(m,c,10001);
 assert(m.state()==BalloonMission::TURN_FAILED && !r.armed);
 assert(r.motor_power1==0 && r.motor_power2==0 && forward==0);
 // P12 must stay physically disarmed even across recovery and timeout.
 setup(m,c,r,true);tick(m,c);tick(m,c);
 r.sensorsuite.fb.state=NiclaFeedback::MISSING;tick(m,c,1000);
 assert(m.state()==BalloonMission::APPROACH && !r.armed && r.writes==0);
 tick(m,c,10001);assert(!r.armed && r.writes==0);
 // New-target size/route filters are not bypassed by recovery scanning.
 setup(m,c,r);tick(m,c);tick(m,c);
 m.visited_=1;m.lastVisitedArea_=1000;m.routeYawRef_=0;m.enter(c,BalloonMission::ACQUIRE_NEXT);
 c.sensors[SensorMap::NICLA_W_VALUE]=40;c.sensors[SensorMap::NICLA_H_VALUE]=30;
 tick(m,c);assert(m.state()==BalloonMission::ACQUIRE_NEXT && forward==0);
 r.sensorsuite.fb.fresh=true;r.sensorsuite.fb.resetPending=false;
 c.sensors[SensorMap::NICLA_W_VALUE]=10;c.sensors[SensorMap::NICLA_H_VALUE]=10;
 tick(m,c);assert(m.state()==BalloonMission::APPROACH);
 // Regression: starting from an already confirmed target must not send R.
 // Reproduce the user's cropped-above observation, including generation wrap.
 for (const auto state : {NiclaFeedback::FULL,NiclaFeedback::PARTIAL}) {
  setup(m,c,r);r.sensorsuite.fb.state=state;r.sensorsuite.fb.generation=0;
  c.sensors[SensorMap::NICLA_Y_VALUE]=36;c.sensors[SensorMap::NICLA_H_VALUE]=84;
  const unsigned resets=r.sensorsuite.resets;
  m.reset(c);
  assert(r.sensorsuite.resets==resets && !r.sensorsuite.fb.resetPending);
  assert(m.state()==BalloonMission::APPROACH && m.visited()==0);
  m.update(c); // NO new UART frame: retained sample must initialise visual Y.
  assert(m.visualHeightSeen_ && m.visualHeight_.demand>.10f && forward==0);
  assert(m.visualHeight_.primed && m.lastVisionGeneration_==0);
  const float demand=m.visualHeight_.demand;
  m.update(c);assert(m.visualHeight_.demand==demand); // not consumed twice
  m.reset(c); // app ARM then RESET also keeps the confirmed target
  assert(m.state()==BalloonMission::APPROACH && r.sensorsuite.resets==resets);
 }
 // Stale, pending, unusable state/geometry/flag must NOT be retained.
 for (int invalid=0;invalid<6;++invalid) {
  setup(m,c,r);const unsigned resets=r.sensorsuite.resets;
  if(invalid==0)r.sensorsuite.fb.fresh=false;
  if(invalid==1)r.sensorsuite.fb.resetPending=true;
  if(invalid==2)r.sensorsuite.fb.state=NiclaFeedback::LOST;
  if(invalid==3)c.sensors[SensorMap::NICLA_Y_VALUE]=NAN;
  if(invalid==4)c.sensors[SensorMap::NICLA_W_VALUE]=0;
  if(invalid==5)c.sensors[SensorMap::NICLA_FLAG]=0;
  m.reset(c);assert(m.state()==BalloonMission::SEARCH);
  assert(r.sensorsuite.resets==resets+1);
 }
 setup(m,c,r,true);const unsigned resets=r.sensorsuite.resets;
 m.reset(c);m.update(c);
 assert(m.state()==BalloonMission::APPROACH && !r.armed && r.writes==0);
 assert(r.sensorsuite.resets==resets);
 puts("PASS real BalloonMission: visual control, recovery, P12, next-target filters, preserved FULL/PARTIAL startup, stale rejection");
}
