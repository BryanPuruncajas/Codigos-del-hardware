#include "Arduino.h"
#include <assert.h>
#include <stdio.h>
uint32_t mockMs=1000;int reason=0;float forward=0;
#include "tests/P03YawControl.cpp"
int main(){
 AppContext c;RobotMock r;c.robot=&r;c.mode=12;c.modeEnteredMs=1;r.armed=true;
 c.command.params[AppConfig::PARAM_TZ]=0;
 c.command.params[AppConfig::PARAM_AUX0]=.4f;
 c.command.params[AppConfig::PARAM_AUX1]=2.f;
 c.command.params[AppConfig::PARAM_AUX3]=.16f;
 c.command.params[AppConfig::PARAM_AUX4]=1;
 // Actual v2 pack: yaw 0..16%, db1deg, authority25%, Ki.25.
 c.command.params[AppConfig::PARAM_AUX2]=float((1U<<24)|((uint32_t(32<<6)|uint32_t(1<<12)|uint32_t(1<<16)|uint32_t(1<<19))<<1));
 c.sensors[SensorMap::YAW]=0;c.sensors[SensorMap::YAW_RATE]=.1f;
 c.sensors[SensorMap::ALTITUDE]=NAN; // no altitude feedback permitted in this test
 for(int i=0;i<500;++i){
  mockMs+=4;TestRunners::p03Yaw(c);
  assert(r.armed && r.motor_power1<=.160001f && r.motor_power2<=.160001f);
 }
 assert(r.motor_power1>.1599f && r.writes==500);
 assert(r.servo_old1<35 && r.servo_old2<95); // left correction for right drift
 assert(ctrl.yaw.integral>0);
 c.sensors[SensorMap::ALTITUDE]=10000; // irrelevant reference must not alter heading mode
 mockMs+=4;TestRunners::p03Yaw(c);assert(r.armed);
 c.sensors[SensorMap::YAW_RATE]=NAN;mockMs+=4;TestRunners::p03Yaw(c);
 assert(!r.armed && r.motor_power1==0 && r.motor_power2==0);
 const unsigned writes=r.writes;
 c.sensors[SensorMap::YAW_RATE]=0;mockMs+=4;TestRunners::p03Yaw(c);
 assert(!r.armed && r.writes==writes && r.motor_power1==0);
 puts("PASS actual P03 support path: no barometer, right-drift compensation, 16% bound, invalid gyro disarm, no auto rearm");
}
