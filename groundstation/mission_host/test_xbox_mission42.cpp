#include "Arduino.h"
#include <assert.h>
#include <stdio.h>
uint32_t mockMs=1000;int reason=0;float forward=0;
#define private public
#include "mission/BalloonMission.cpp"
#undef private
namespace TestRunners {
void p00MotorPower(AppContext&){} void p00ServoCalibration(AppContext&){}
void p01Sensors(AppContext&){} void p02Manual(AppContext&){}
void p03Yaw(AppContext&){} void p04Altitude(AppContext&){}
void p05YawAltitude(AppContext&){} void p06NiclaRaw(AppContext&){}
void p07VisualCenter(AppContext&){}
}
#include "tests/TestModes.cpp"
#include "app/PilotWatchdog.h"

int main(){
    assert(VisualBearing::VERSION==42);
    AppContext c;RobotMock robot;c.robot=&robot;c.mode=0;c.flowHeight.valid=true;
    c.sensors[SensorMap::YAW]=.3f;c.sensors[SensorMap::ALTITUDE]=.8f;
    c.sensors[SensorMap::NICLA_FLAG]=65;c.sensors[SensorMap::NICLA_X_VALUE]=120;
    c.sensors[SensorMap::NICLA_Y_VALUE]=80;c.sensors[SensorMap::NICLA_W_VALUE]=120;
    c.sensors[SensorMap::NICLA_H_VALUE]=110;
    TestModes::onModeEnter(c,AppConfig::P11_INFINITY);robot.armed=true;
    const float heading=mission.courseYaw_;
    TestModes::onModeEnter(c,AppConfig::P02_MANUAL_CONTROL);
    assert(c.mode==11 && mission.visited()==0);
    c.command.params[AppConfig::PARAM_AUX1]=1;
    assert(TestModes::completePausedWaypoint(c));assert(mission.visited()==1);
    assert(TestModes::completePausedWaypoint(c));assert(mission.visited()==1); // same RB retry
    assert(mission.state()==BalloonMission::TURN_TO_NEXT);
    assert(fabsf(mission.routeYawRef_-(heading+atan2f(2.f,4.f)))<.001f);
    mockMs+=5000;
    assert(TestModes::resumePausedMission(c,AppConfig::P11_INFINITY));
    assert(c.mode==21 && mission.visited()==1 && mission.courseYaw_==heading);
    assert(mission.state()==BalloonMission::TURN_TO_NEXT);
    TestModes::onModeEnter(c,AppConfig::P02_MANUAL_CONTROL);
    c.command.params[AppConfig::PARAM_AUX1]=2;
    assert(TestModes::completePausedWaypoint(c));assert(mission.visited()==2);
    c.command.params[AppConfig::PARAM_AUX1]=1;
    assert(!TestModes::completePausedWaypoint(c));assert(mission.visited()==2); // stale acknowledgement
    c.flowHeight.valid=false;
    assert(!TestModes::resumePausedMission(c,21));assert(c.mode==11);
    c.flowHeight.valid=true;robot.armed=false;
    assert(!TestModes::resumePausedMission(c,21));assert(c.mode==11);
    robot.armed=true;
    c.command.params[AppConfig::PARAM_AUX1]=3;assert(TestModes::completePausedWaypoint(c));
    c.command.params[AppConfig::PARAM_AUX1]=4;assert(TestModes::completePausedWaypoint(c));
    assert(mission.state()==BalloonMission::TURN_TO_NEXT && mission.visited()==4);
    c.command.params[AppConfig::PARAM_AUX1]=5;assert(TestModes::completePausedWaypoint(c));
    assert(mission.state()==BalloonMission::DONE && mission.visited()==5);
    assert(!TestModes::resumePausedMission(c,21));
    TestModes::onModeEnter(c,AppConfig::SAFE_STOP);
    TestModes::onModeEnter(c,AppConfig::P02_MANUAL_CONTROL);
    assert(!TestModes::completePausedWaypoint(c));
    TestModes::onModeEnter(c,AppConfig::P08_ONE_BALLOON);
    TestModes::onModeEnter(c,AppConfig::P02_MANUAL_CONTROL);
    c.command.params[AppConfig::PARAM_AUX1]=1;
    assert(TestModes::completePausedWaypoint(c));
    assert(mission.state()==BalloonMission::DONE && mission.visited()==1);
    assert(!PilotWatchdog::expired(1000,250,true,true));
    assert(PilotWatchdog::expired(1000,249,true,true));
    assert(!PilotWatchdog::expired(1000,0,false,true));
    assert(!PilotWatchdog::expired(1000,0,true,false));
    assert(PilotWatchdog::expired(500,UINT32_MAX-500,true,true));
    puts("PASS Xbox42 actual mission: pause/resume progress, track2 heading, idempotent RB, final waypoint, no hidden arm, timeout and timestamp wrap");
}
