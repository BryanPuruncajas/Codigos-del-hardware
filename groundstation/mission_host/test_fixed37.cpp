#include "Arduino.h"
#include <assert.h>
#include <stdio.h>
#include <initializer_list>
uint32_t mockMs=1000;int reason=0;float forward=0;
#define private public
#include "mission/BalloonMission.cpp"
#undef private
float options(bool released,bool advance=true,int cm=0,int pitchCode=60){
    return float(8388608u|unsigned(released)|(unsigned(advance)<<1)|
        (unsigned(cm+50)<<2)|(24u<<9)|(unsigned(pitchCode)<<16));
}
struct Flight {
    AppContext c;RobotMock r;BalloonMission m;
    Flight(){
        c.robot=&r;c.mode=17;c.command.params[AppConfig::PARAM_AUX0]=.25f;
        c.command.params[AppConfig::PARAM_AUX1]=.18f;
        c.command.params[AppConfig::PARAM_AUX4]=options(false);
        c.sensors[SensorMap::ALTITUDE]=.8f;r.sensorsuite.barometer.healthy=true;
        image();m.configure(1,true);m.reset(c);r.armed=true;
    }
    void image(bool full=true,float y=80,float width=80){
        r.sensorsuite.fb.state=full?NiclaFeedback::FULL:NiclaFeedback::PARTIAL;
        c.sensors[SensorMap::NICLA_FLAG]=full?65:66;
        c.sensors[SensorMap::NICLA_X_VALUE]=120;c.sensors[SensorMap::NICLA_Y_VALUE]=y;
        c.sensors[SensorMap::NICLA_W_VALUE]=width;c.sensors[SensorMap::NICLA_H_VALUE]=60;
    }
    void step(){
        mockMs+=20;++r.sensorsuite.fb.generation;
        r.sensorsuite.barometer.sampleTimeUs=mockMs*1000;m.update(c);
        assert(r.motor_power1<=.250001f && r.motor_power2<=.250001f);
        assert(r.servo_old1>=0 && r.servo_old1<=120 && r.servo_old2>=0 && r.servo_old2<=120);
        if(forward>0){assert(Telemetry::impulseYaw==0);assert(m.approachSupport_.fixedControlReady(mockMs));}
    }
    void run(unsigned ms){for(unsigned i=0;i<ms;i+=20)step();}
    VisualSupport::Loop& height(){return m.approachSupport_;}
};
int main(){
    assert(VisualBearing::VERSION>=37);
    for(int cm=-50;cm<=50;++cm){
        const float raw=options(true,false,cm,120);
        const auto d=MissionHeightOptions::decode(raw);
        assert(d.valid && d.released && !d.advance && fabsf(d.offset-cm*.01f)<1.e-6f);
        assert(fabsf(d.levelPitch-.52359878f)<1.e-6f && d.deadband==24);
    }
    for(float bad : {NAN,INFINITY,-1.f,8388608.f,16777215.f,200.f}){
        const auto d=MissionHeightOptions::decode(bad);assert(!d.valid && !d.advance && !d.released);
    }
    assert(MissionHeightOptions::decode(24).deadband==24);
    Flight held;held.run(2500);
    assert(held.height().altitudeReady && held.height().fixedTarget()==.8f);
    assert(held.m.impulse_.pulses==0 && forward==0 && held.m.visited()==0);
    const float target=held.height().fixedTarget();
    held.c.command.params[AppConfig::PARAM_AUX4]=options(true);
    held.run(1500);assert(held.m.impulse_.pulses>=1 && forward>0);
    // Approaching changes image scale/y/pitch, never the captured metric target.
    held.image(false,120,110);held.c.sensors[SensorMap::PITCH]=.20f;held.run(500);
    assert(held.height().fixedTarget()==target && held.height().inertia.desiredRate==0);
    const float bias=held.height().pressureBias(mockMs);
    held.r.sensorsuite.fb.state=NiclaFeedback::MISSING;held.run(5000);
    assert(held.height().fixedTarget()==target && held.height().pressureBias(mockMs)==bias && held.r.armed);
    // A new epoch must retain height, but cannot invent FULL/arrival proof.
    held.r.sensorsuite.fb.sample=256;held.image(false,80,119);held.run(200);
    assert(held.height().fixedTarget()==target && held.m.visited()==0 && forward==0);
    held.r.sensorsuite.fb.fresh=true;held.r.sensorsuite.fb.resetPending=false;
    held.c.sensors[SensorMap::PITCH]=0;held.image();held.run(2000);
    assert(held.height().fixedTarget()==target && held.r.armed);
    // Reset, rather than reacquisition, permits a new base height.
    held.c.sensors[SensorMap::ALTITUDE]=1.1f;held.m.reset(held.c);held.run(1200);
    assert(fabsf(held.height().fixedTarget()-1.1f)<1.e-6f);
    Flight noReference;noReference.image(true,45);noReference.c.command.params[AppConfig::PARAM_AUX4]=options(true);
    noReference.run(2500);assert(!noReference.height().altitudeReady && noReference.m.impulse_.pulses==0);
    noReference.image();noReference.run(3000);assert(noReference.height().altitudeReady && noReference.m.impulse_.pulses>0);
    Flight partial;partial.image(false);partial.c.command.params[AppConfig::PARAM_AUX4]=options(true);
    partial.run(2000);assert(!partial.height().altitudeReady && partial.m.impulse_.pulses==0);
    Flight tilted;tilted.c.sensors[SensorMap::PITCH]=.1f;tilted.run(1500);
    assert(!tilted.height().altitudeReady);
    tilted.c.command.params[AppConfig::PARAM_AUX4]=options(false,true,0,72); // camera level at +6deg
    tilted.run(1000);assert(tilted.height().altitudeReady);
    Flight vertical;vertical.c.command.params[AppConfig::PARAM_AUX4]=options(true,false,10);vertical.run(7000);
    assert(vertical.height().altitudeReady && fabsf(vertical.height().fixedTarget()-.9f)<1.e-5f);
    assert(vertical.height().fixedBase==.8f && vertical.height().altitudeReference==.8f);
    assert(vertical.m.impulse_.pulses==0 && vertical.m.visited()==0);
    vertical.image(true,80,130);vertical.run(1000);assert(vertical.m.visited()==0 && vertical.r.armed);
    Flight missingBaro;missingBaro.r.sensorsuite.barometer.healthy=false;missingBaro.run(1500);
    assert(!missingBaro.height().altitudeReady && missingBaro.m.impulse_.pulses==0);
    missingBaro.c.command.params[AppConfig::PARAM_AUX4]=options(true);missingBaro.run(21000);
    assert(!missingBaro.r.armed && missingBaro.m.state()==BalloonMission::TURN_FAILED);
    Flight arrival;arrival.run(1000);arrival.image(true,80,125);arrival.run(800);
    assert(arrival.m.visited()==0); // held during countdown
    arrival.c.command.params[AppConfig::PARAM_AUX4]=options(true);arrival.run(800);
    assert(arrival.m.visited()==1 && !arrival.r.armed);
    // Read the actual loop with constant image geometry and an injected slow
    // pressure drift. Base target stays fixed; only the separate bias changes.
    VisualSupport::Loop loop;loop.reset(.18f,100,.25f);loop.fixedMode=true;
    VisualSupport::Observation o;o.fresh=o.confirmed=o.full=true;o.x=120;o.y=80;o.w=80;o.h=60;
    for(unsigned ms=100;ms<=10100;ms+=40){++o.generation;
        loop.update(o,ms,.18f,.25f,false,0,.8f+.004f*(ms-100)*.001f,.004f,true,0,60000,true,15000,ms*1000,0);}
    const float base=loop.fixedTarget();assert(loop.pressureBias(10100)>.015f);
    assert(base==loop.fixedBase && fabsf(loop.referenceError(.84f,10100))<.005f);
    loop.inertia.learningAllowed=false;o.fresh=false;
    // Preserve the measured 0.32m/11s fall: the controller must restore lift.
    for(unsigned ms=10140;ms<=14100;ms+=40){
        const float t=(ms-10100)*.001f;
        loop.update(o,ms,.18f,.25f,false,0,.84f-.02909f*t,-.02909f,true,0,60000,true,15000,ms*1000,0);
    }
    assert(loop.fixedTarget()==base && loop.inertia.velocityTrusted && loop.demand>loop.height.trim+.01f);
    const float savedBase=loop.fixedBase;
    loop.barometerValidNow=false;assert(!loop.fixedControlReady(14100) && loop.fixedBase==savedBase);
    puts("PASS fixed37: real mission capture/release gates, partial/loss/epoch retention, reset, level calibration, ramped offset, vertical-only, arrival interlock, separate bias and slow fall");
}
