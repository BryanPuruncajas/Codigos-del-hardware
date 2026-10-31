#include "Arduino.h"
#include <assert.h>
#include <stdio.h>
uint32_t mockMs=1000;int reason=0;float forward=0;
#define private public
#include "mission/BalloonMission.cpp"
#undef private
int main(){
    assert(VisualBearing::VERSION>=35 && ApproachProfile::FORWARD_SIGN==-1);
    SupportYaw::Mixer neutral,attack;neutral.reset(true);attack.reset(true);
    for(int i=0;i<200;++i){
        neutral.update(.18f,0,.25f,0,.25f,.02f,0,0,0,true,28.f,true,-1.f);
        attack.update(.18f,0,.25f,0,.25f,.02f,0,0,.12f,true,28.f,true,-1.f);
        assert(attack.servo1>=0 && attack.servo1<=120 && attack.servo2>=0 && attack.servo2<=120);
        assert(attack.power<=.250001f);
    }
    assert(fabsf(neutral.servo1-52.5f)<.01f && fabsf(neutral.servo2-64.5f)<.01f);
    assert(attack.forwardAngle>27.9f && attack.forwardAngle<=28);
    assert(fabsf(attack.servo1-24.5f)<.01f && fabsf(attack.servo2-92.5f)<.01f);
    attack.update(.18f,1,.25f,0,.25f,.02f,0,0,.12f,true,28.f,true,-1.f);
    assert(attack.forwardAngle<1.e-5f); //yaw still has priority
    AppContext c;RobotMock r;BalloonMission m;c.robot=&r;c.mode=17;
    c.command.params[AppConfig::PARAM_AUX0]=.25f;c.command.params[AppConfig::PARAM_AUX1]=.18f;
    c.sensors[SensorMap::ALTITUDE]=.8f;c.sensors[SensorMap::VERTICAL_VELOCITY]=0;
    c.sensors[SensorMap::YAW]=c.sensors[SensorMap::YAW_RATE]=c.sensors[SensorMap::PITCH]=0;
    c.sensors[SensorMap::NICLA_FLAG]=65;c.sensors[SensorMap::NICLA_X_VALUE]=120;
    c.sensors[SensorMap::NICLA_Y_VALUE]=80;c.sensors[SensorMap::NICLA_W_VALUE]=80;
    c.sensors[SensorMap::NICLA_H_VALUE]=60;r.sensorsuite.barometer.healthy=true;
    m.configure(1,true);m.reset(c);r.armed=true;
    for(int i=0;i<90;++i){mockMs+=20;++r.sensorsuite.fb.generation;
        r.sensorsuite.barometer.sampleTimeUs=mockMs*1000;m.update(c);
        if(forward>0)assert(Telemetry::impulseYaw==0);
        assert(r.motor_power1<=.250001f && r.motor_power2<=.250001f);}
    assert(m.impulse_.pulses>=1 && m.approachMixer_.forwardAngle>24);
    assert(r.servo_old1<28.5f && r.servo_old2>88.5f && m.visited()==0);
    puts("PASS routing35: reversed28deg attack, unchanged neutral,25% cap, actual mission requests corrected sign, yaw interlock");
}
