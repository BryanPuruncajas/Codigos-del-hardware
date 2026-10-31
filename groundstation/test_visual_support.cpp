#define CONTROL_COMMON_HOST_TEST
#define PI 3.14159265358979323846f
template<class T>T constrain(T x,T lo,T hi){return x<lo?lo:(x>hi?hi:x);}
#include "app/VisualSupport.h"
#include "app/SupportYaw.h"
#include <assert.h>
#include <stdio.h>
int main(){
 using namespace VisualSupport;
 Observation o;o.fresh=o.confirmed=o.full=true;o.x=120;o.y=80;o.w=o.h=30;o.generation=1;
 Loop c;c.reset(.15f,100);c.update(o,100,.15f,.16f,true);
 assert(c.phase==PREPARE && c.demand==.15f && !c.height.primed);
 o.y=40;++o.generation;c.update(o,200,.15f,.16f,true);
 assert(c.demand==.15f && c.height.trim==.15f); // no learning while held
 SupportYaw::Mixer m;
 for(int i=0;i<300;++i)m.update(c.demand,0,.25f,0,.16f,.004f);
 const float prepared=m.power;
 c.update(o,300,.15f,.16f,false);
 assert(c.phase==RUN && c.demand>.15f && c.demand<=.16f);
 m.update(c.demand,0,.25f,0,.16f,.004f);
 assert(m.power>=prepared && m.power>0); // transition does not reset spool
 o.y=120;++o.generation;c.update(o,400,.15f,.16f,false);
 assert(c.demand<.15f);
 for(int i=0;i<1000;++i)m.update(c.demand,1,.25f,0,.16f,.004f);
 assert(m.power<.15f); // yaw cannot impose 16% during visual descent
    for(int i=0;i<1000;++i)m.update(0,1,.25f,0,.16f,.004f,.14f);
    assert(fabsf(m.power-.14f)<1.e-6f);
    SupportYaw::Mixer startup;startup.update(0,0,.25f,0,.16f,.004f,.14f);
    assert(startup.power>0 && startup.power<.001f); // no startup jump to floor
    startup.update(0,NAN,.25f,0,.16f,.004f,.14f);assert(startup.power==0);
    VisualHeight antiwindup;antiwindup.trim=.15f;
    for(unsigned t=100;t<10100;t+=100)antiwindup.update(t,120,30,.16f,true,.14f);
    assert(fabsf(antiwindup.trim-.15f)<1.e-6f); // floor saturation freezes descent I
    assert(antiwindup.update(10200,40,30,.16f,true,.14f)>.15f);
 o.fresh=false;c.update(o,600,.15f,.16f,false);
 assert(c.phase==HOLD && c.demand==c.height.trim);
    o.fresh=true;++o.generation;c.update(o,901,.15f,.16f,false);
 assert(c.phase==RUN && c.height.derivative==0);
    c.update(o,1601,.15f,.16f,false);assert(c.phase==HOLD && !c.failed);
    c.update(o,4901,.15f,.16f,false);
 assert(c.phase==FAULT && c.demand==0); // unchanged generation is not new data
    ++o.generation;c.update(o,4902,.15f,.16f,false);assert(c.failed && c.demand==0);
 c.reset(.15f,100);++o.generation;c.update(o,100,.15f,.16f,true);
 ++o.generation;c.update(o,20100,.15f,.16f,true);assert(c.failed);
 c.reset(.15f,100);++o.generation;c.update(o,100,.15f,.16f,false);
 ++o.generation;c.update(o,15100,.15f,.16f,false);assert(c.failed);
 c.reset(.15f,100);++o.generation;c.update(o,100,.15f,.16f,false);
 ++o.generation;c.update(o,101,.15f,.16f,true);assert(c.failed); // no timer rewind
 c.reset(.15f,100);o.confirmed=false;c.update(o,100,.15f,.16f,false);assert(c.failed);
 o.confirmed=true;c.reset(.15f,100);o.y=NAN;c.update(o,100,.15f,.16f,true);assert(c.failed);
 o.y=40;c.reset(.20f,100);++o.generation;c.update(o,100,.20f,.30f,false);
 assert(c.demand<=.16f && c.height.trim<=.16f); // hard experimental ceiling
 c.reset(.15f,0xffffff00U);++o.generation;c.update(o,0xffffff00U,.15f,.16f,false);
 o.fresh=false;c.update(o,44,.15f,.16f,false);assert(!c.failed);
 c.update(o,744,.15f,.16f,false);assert(!c.failed); // one-second occlusion survives wrap
 c.update(o,3744,.15f,.16f,false);assert(c.failed); // wrap-safe loss4s
 o.fresh=o.confirmed=o.full=true;o.x=120;o.y=80;o.w=o.h=30;++o.generation;
 c.reset(.15f,100);c.update(o,100,.15f,.16f,true);
 assert(c.forwardRequest(o,100,.01f,0)==0); // held preparation
 ++o.generation;c.update(o,200,.15f,.16f,false);
 assert(c.forwardRequest(o,200,.5f,0)==.03f);
 assert(c.forwardRequest(o,401,.01f,0)==0); // cached sample expired
 assert(c.forwardRequest(o,200,.01f,.2f)==0); // turn before advancing
 o.x=80;assert(c.forwardRequest(o,200,.01f,0)==0);o.x=120;
 o.y=30;assert(c.forwardRequest(o,200,.01f,0)==0);o.y=80;
 o.w=130;assert(c.forwardRequest(o,200,.01f,0)==0);o.w=30;
 o.full=false;assert(c.forwardRequest(o,200,.01f,0)==0);o.full=true;
 o.fresh=false;c.update(o,300,.15f,.16f,false);
 assert(c.forwardRequest(o,300,.01f,0)==0);
 c.stop();assert(c.forwardRequest(o,300,.01f,0)==0);
 Loop braking;braking.reset(.15f,100);
 o.fresh=o.confirmed=o.full=true;o.x=120;o.w=o.h=20;
 for(unsigned i=0;i<=5;++i){o.y=40+i*4;++o.generation;braking.update(o,100+i*100,.15f,.16f,false,.14f);}
 assert(o.y<80 && braking.demand<.15f && braking.height.trim<=.15f);
 Loop inertial;inertial.reset(.15f,100);
 SupportYaw::Mixer oldFloor,newFloor;oldFloor.reset(true);newFloor.reset(true);
 for(unsigned i=0;i<=7;++i){o.y=40+i*4;++o.generation;inertial.update(o,100+i*100,.15f,.16f,false,.10f);}
 assert(o.y<80 && inertial.height.derivative<0 && inertial.demand<.14f);
 for(unsigned i=0;i<400;++i){
  oldFloor.update(inertial.demand,0,.25f,0,.16f,.004f,.14f,0,0,true);
  newFloor.update(inertial.demand,0,.25f,0,.16f,.004f,MIN_RUNNING_POWER,0,0,true);
 }
 assert(fabsf(oldFloor.power-.14f)<1.e-6f);
 assert(newFloor.power>=.09999f && newFloor.power<.14f && newFloor.power>0);
 // Descent approaching the centre must regain lift BEFORE crossing it.
 inertial.reset(.15f,100);
 for(unsigned i=0;i<=5;++i){o.y=120-i*4;++o.generation;inertial.update(o,100+i*100,.15f,.16f,false,.10f);}
 assert(o.y>80 && inertial.height.derivative>0 && inertial.demand>.15f);
 for(unsigned i=0;i<400;++i)newFloor.update(inertial.demand,0,.25f,0,.16f,.004f,.10f,0,0,true);
 assert(newFloor.power>.15f && newFloor.power<=.160001f);
 Loop partial;partial.reset(.15f,100);o.y=15;o.h=40;o.full=false;++o.generation;
 partial.update(o,100,.15f,.16f,false,.10f);
 assert(partial.demand<=.15f && partial.height.derivative==0 && partial.height.trim==.15f);
 Loop above;above.reset(.15f,100);o.full=true;o.h=20;
 for(unsigned i=0;i<6;++i){o.y=82+i*2;++o.generation;above.update(o,100+i*100,.15f,.16f,false,.10f);
  assert(above.demand<=above.height.trim && above.demand>0);}
 assert(above.height.trim<.15f);
 Loop advance;advance.reset(.15f,100);o.fresh=o.confirmed=o.full=true;
 o.x=120;o.y=80;o.w=o.h=30;++o.generation;advance.update(o,100,.15f,.16f,false,.10f);
 assert(advance.forwardRequest(o,100,.03f,0)==.03f);
 o.x=150;assert(advance.forwardRequest(o,100,.03f,0)==.03f);
 o.x=160;assert(advance.forwardRequest(o,100,.03f,0)==0);
 o.x=150;assert(advance.forwardRequest(o,100,.03f,0)==0);
 o.x=120;assert(advance.forwardRequest(o,100,.03f,0)==.03f);
 o.fresh=false;assert(advance.forwardRequest(o,100,.03f,0)==0 && !advance.advanceReady);
 // A stable full target captures one relative height anchor onboard.
 Loop recovery;recovery.reset(.15f,100);o.fresh=o.confirmed=o.full=true;
 o.x=120;o.y=80;o.w=o.h=30;
 for(unsigned t=100;t<=600;t+=100){++o.generation;
  recovery.update(o,t,.15f,.16f,false,.10f,.8f,0,true,0);}
 assert(recovery.altitudeReady && fabsf(recovery.altitudeReference-.8f)<1.e-6f);
 o.fresh=false;
 recovery.update(o,1600,.15f,.16f,false,.10f,.9f,.1f,true,0);
 assert(!recovery.failed && recovery.phase==HOLD && recovery.demand<recovery.height.trim);
 assert(recovery.forwardRequest(o,1600,.03f,0)==0);
 assert(fabsf(recovery.recoveryYawOffset(1600))>0);
 recovery.update(o,1700,.15f,.16f,false,.10f,.6f,-.1f,true,0);
 assert(recovery.demand>recovery.height.trim && recovery.demand<=.16f);
 recovery.update(o,1800,.15f,.16f,false,.10f,NAN,NAN,false,0);
 assert(recovery.demand==recovery.height.trim); // failed barometer cannot drive recovery
 o.fresh=true;++o.generation;
 recovery.update(o,1900,.15f,.16f,false,.10f,1.2f,0,true,0);
 assert(recovery.phase==RUN && recovery.recoveryYawOffset(1900)==0);
 assert(recovery.altitudeReference==.8f); // reference does not chase sensor/target
 Loop partialAnchor;partialAnchor.reset(.15f,100);o.full=false;
 for(unsigned t=100;t<=700;t+=100){++o.generation;
  partialAnchor.update(o,t,.15f,.16f,false,.10f,.8f,0,true,0);}
 assert(!partialAnchor.altitudeReady);
 // Reproduce the flight trace: target below the image centre, PARTIAL at the
 // bottom edge, interleaved with gaps. Do not restore15% at every occlusion.
 Loop high;high.reset(.15f,100);o.full=false;o.y=125;o.h=70;o.w=60;
 for(unsigned t=100;t<=1100;t+=100){++o.generation;
  high.update(o,t,.15f,.20f,false,.06f,0,.2f,true,0,180000,true,15000);}
 assert(high.height.trim<.15f && high.demand<.10f && high.inertia.brakingUp);
 const float descent=high.demand,learnedDown=high.height.trim;
 o.fresh=false;
 high.update(o,1200,.15f,.20f,false,.06f,NAN,NAN,false,0,180000,true,15000);
 assert(high.phase==HOLD && high.demand==learnedDown && high.height.trim==learnedDown);
 // Invalid pressure freezes support at learned trim. A fresh sustained fall
 // must re-establish evidence, rather than act on a single sign-changing tick.
 for(unsigned t=1300;t<=2500;t+=100)
  high.update(o,t,.15f,.20f,false,.06f,1.f-.1f*(t-1300)*.001f,-.1f,true,0,180000,true,15000);
 assert(high.demand>learnedDown && high.inertia.brakingDown); // brake the fall early
 SupportYaw::Mixer descentMixer;descentMixer.reset(true);
 for(unsigned i=0;i<400;++i)
  descentMixer.update(descent,0,.25f,0,.20f,.004f,.06f,0,0,true);
 assert(descentMixer.power>=.06f && descentMixer.power<.10f); // positive low-power braking
 // Returning to a higher visible target regains lift; no permanent low latch.
 o.fresh=true;o.full=true;o.y=40;o.h=30;++o.generation;
 high.update(o,2600,.15f,.20f,false,.06f,.87f,-.1f,true,0,180000,true,15000);
 assert(high.phase==RUN && high.demand>learnedDown);
 // Sustained PARTIAL above can learn more support after buoyancy loss.
 Loop depleted;depleted.reset(.12f,100,.20f);
 o.fresh=o.confirmed=true;o.full=false;o.y=30;o.h=70;o.w=40;
 for(unsigned t=100;t<=10100;t+=100){++o.generation;
  depleted.update(o,t,.12f,.20f,false,.10f,0,-.1f,true,0,180000,true,15000);}
 // Anti-windup stops further trim learning once the combined position/rate
 // demand saturates. Requiring trim>.13 here contradicted that existing rule.
 assert(depleted.height.trim>.12f && depleted.demand>.18f && depleted.demand<=.200001f);
 // A short camera gap freezes when barometer evidence is invalid. Sustained
 // descent with fresh healthy feedback can adapt support within a2pp window.
 o.fresh=false;
 depleted.update(o,10200,.12f,.20f,false,.10f,0,NAN,false,0,180000,true,15000);
 const float holdTrim=depleted.height.trim;
 for(unsigned t=10300;t<=14300;t+=100)
  depleted.update(o,t,.12f,.20f,false,.10f,0,-.1f,true,0,180000,true,15000);
 assert(depleted.height.trim>holdTrim && depleted.height.trim<=holdTrim+.020001f);
 assert(depleted.demand>holdTrim && depleted.demand<=.20f);
 const float frozen=depleted.height.trim;
 for(unsigned t=14400;t<=15000;t+=100)
  depleted.update(o,t,.12f,.20f,false,.10f,NAN,NAN,false,0,180000,true,15000);
 assert(depleted.height.trim==frozen);
 puts("PASS visual speed: partial no climb boost, learned support, rising above target brakes, positive floor/loss");
}
