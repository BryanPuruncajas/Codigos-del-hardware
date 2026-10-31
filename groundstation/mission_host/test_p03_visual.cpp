#include "Arduino.h"
#include <assert.h>
#include <stdio.h>
uint32_t mockMs=1000;int reason=0;float forward=0;
#include "tests/P03YawControl.cpp"
int main(){
 AppContext c;RobotMock r;c.robot=&r;c.mode=12;c.modeEnteredMs=1;r.armed=true;
 c.command.params[AppConfig::PARAM_TZ]=0;c.command.params[AppConfig::PARAM_FX]=1;
 c.command.params[AppConfig::PARAM_AUX0]=.4f;c.command.params[AppConfig::PARAM_AUX1]=2;
 c.command.params[AppConfig::PARAM_AUX3]=.15f;c.command.params[AppConfig::PARAM_AUX4]=2;
 c.command.params[AppConfig::PARAM_AUX2]=float((1U<<24)|((uint32_t(20)|uint32_t(32<<6)|uint32_t(1<<12)|uint32_t(1<<16)|uint32_t(1<<19))<<1));
 c.sensors[SensorMap::ALTITUDE]=NAN;c.sensors[SensorMap::VERTICAL_VELOCITY]=NAN;
 c.sensors[SensorMap::YAW]=0;c.sensors[SensorMap::YAW_RATE]=0;
 c.sensors[SensorMap::NICLA_FLAG]=65;c.sensors[SensorMap::NICLA_X_VALUE]=120;
 c.sensors[SensorMap::NICLA_Y_VALUE]=40;c.sensors[SensorMap::NICLA_W_VALUE]=30;c.sensors[SensorMap::NICLA_H_VALUE]=30;
 for(int i=0;i<500;++i){
  mockMs+=4;if(i%25==0)++r.sensorsuite.fb.generation;TestRunners::p03Yaw(c);
  assert(r.armed && r.motor_power1<=.16f);
 }
 assert(ctrl.visual.phase==VisualSupport::PREPARE && r.motor_power1>=.15f && r.motor_power1<=.16f);
 assert(fabsf(r.servo_old1-52.5f)<1.e-5f && fabsf(r.servo_old2-64.5f)<1.e-5f);
 assert(r.motor_power1>.15f && fabsf(r.motor_power1*ctrl.supportMixer.etaZ-.15f)<1.e-6f);
 assert(ctrl.yaw.integral==0);
 c.command.params[AppConfig::PARAM_TX]=.01f;
 assert(forward==0); // no forward while held, even with opt-in
 c.sensors[SensorMap::YAW_RATE]=.1f;
 for(int i=0;i<100;++i){mockMs+=4;if(i%25==0)++r.sensorsuite.fb.generation;TestRunners::p03Yaw(c);}
 // Right drift is opposed BEFORE release, even at centered target. No
 // learning while held, so restraint cannot wind up the yaw integrator.
 assert(r.servo_old1<52.5f && r.servo_old2<64.5f && ctrl.yaw.integral==0);
 assert(fabsf((r.servo_old2-r.servo_old1)-12.f)<1.e-5f);
 assert(ctrl.bearing.ready && fabsf(ctrl.bearing.reference)<1.e-6f);
 const float prepServo1=r.servo_old1,prepServo2=r.servo_old2,prepPower=r.motor_power1;
 c.command.params[AppConfig::PARAM_FX]=0;mockMs+=4;TestRunners::p03Yaw(c);
 assert(ctrl.visual.released && r.armed && r.motor_power1>.149f); // spool not reset
 assert(fabsf(r.servo_old1-prepServo1)<=.12001f && fabsf(r.servo_old2-prepServo2)<=.12001f);
 assert(fabsf(r.motor_power1-prepPower)<=.000721f && ctrl.yaw.integral>0);
 assert(ctrl.bearing.ready && fabsf(ctrl.bearing.reference)<1.e-6f);
 for(int i=0;i<200;++i){mockMs+=4;if(i%25==0)++r.sensorsuite.fb.generation;TestRunners::p03Yaw(c);}
 assert(r.motor_power1>.159f && ctrl.visual.height.trim<=.16f);
 c.sensors[SensorMap::NICLA_Y_VALUE]=120;
 for(int i=0;i<500;++i){mockMs+=4;if(i%25==0)++r.sensorsuite.fb.generation;TestRunners::p03Yaw(c);}
 assert(r.armed && r.motor_power1<.14f && r.motor_power1>=.09999f && ctrl.yaw.integral>0);
 c.sensors[SensorMap::NICLA_Y_VALUE]=145;c.sensors[SensorMap::NICLA_H_VALUE]=20;
 for(int i=0;i<300;++i){mockMs+=4;if(i%25==0)++r.sensorsuite.fb.generation;TestRunners::p03Yaw(c);}
 assert(r.armed && r.motor_power1>=.09999f && r.motor_power1<.14f);
 c.sensors[SensorMap::NICLA_Y_VALUE]=80;++r.sensorsuite.fb.generation;
 mockMs+=100;TestRunners::p03Yaw(c);assert(forward==.01f);
 c.command.params[AppConfig::PARAM_TX]=0;mockMs+=4;TestRunners::p03Yaw(c);assert(forward==0);
 c.command.params[AppConfig::PARAM_TX]=.01f;
 // Offset target must create a new heading reference without resetting I.
 const float learned=ctrl.yaw.integral;
 c.sensors[SensorMap::NICLA_X_VALUE]=60;++r.sensorsuite.fb.generation;
 mockMs+=100;TestRunners::p03Yaw(c);
 assert(ctrl.bearing.reference>0 && ctrl.yaw.integral>=learned);
 assert(forward==0); // off-centre target
 const float reference=ctrl.bearing.reference;
 mockMs+=4;TestRunners::p03Yaw(c);
 assert(ctrl.bearing.reference==reference); // no re-filtering cached camera data
 r.sensorsuite.fb.state=NiclaFeedback::MISSING;mockMs+=701;TestRunners::p03Yaw(c);
 assert(r.armed && r.motor_power1>0 && ctrl.visual.phase==VisualSupport::HOLD && forward==0);
 const float missingRef=ctrl.bearing.reference;
 mockMs+=300;TestRunners::p03Yaw(c);
 assert(r.armed && ctrl.bearing.reference==missingRef && forward==0);
 r.sensorsuite.fb.state=NiclaFeedback::FULL;++r.sensorsuite.fb.generation;
 mockMs+=100;TestRunners::p03Yaw(c);assert(r.armed && ctrl.visual.phase==VisualSupport::RUN);
 r.sensorsuite.fb.state=NiclaFeedback::MISSING;mockMs+=4001;TestRunners::p03Yaw(c);
 assert(!r.armed && r.motor_power1==0 && ctrl.visual.failed);
 assert(forward==0);
 r.sensorsuite.fb.state=NiclaFeedback::FULL;++r.sensorsuite.fb.generation;
 mockMs+=4;TestRunners::p03Yaw(c);assert(!r.armed && r.motor_power1==0);
 // Real P03 integration: reference captured from sensors, then used while missing.
 ++c.modeEnteredMs;r.armed=true;c.sensors[SensorMap::YAW_RATE]=0;
 c.sensors[SensorMap::NICLA_X_VALUE]=120;c.sensors[SensorMap::NICLA_Y_VALUE]=80;
 c.sensors[SensorMap::NICLA_H_VALUE]=30;c.sensors[SensorMap::PITCH]=0;
 c.sensors[SensorMap::ALTITUDE]=.8f;c.sensors[SensorMap::VERTICAL_VELOCITY]=0;
 r.sensorsuite.barometer.healthy=true;
 for(int i=0;i<8;++i){mockMs+=100;++r.sensorsuite.fb.generation;TestRunners::p03Yaw(c);}
 assert(ctrl.visual.altitudeReady && ctrl.visual.altitudeReference==.8f);
 r.sensorsuite.fb.state=NiclaFeedback::MISSING;
 c.sensors[SensorMap::ALTITUDE]=1.f;c.sensors[SensorMap::VERTICAL_VELOCITY]=.1f;
 mockMs+=1000;TestRunners::p03Yaw(c);
 assert(r.armed && ctrl.visual.phase==VisualSupport::HOLD);
 assert(ctrl.visual.demand<ctrl.visual.height.trim && forward==0);
 r.sensorsuite.barometer.ageMs=1000;mockMs+=100;TestRunners::p03Yaw(c);
 assert(ctrl.visual.demand==ctrl.visual.height.trim);
 puts("PASS actual P03: bounded visual recovery, no forward while missing, reacquisition, timeout disarm");
}
