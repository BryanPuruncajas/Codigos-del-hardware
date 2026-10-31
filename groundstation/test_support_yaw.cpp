#define CONTROL_COMMON_HOST_TEST
#define PI 3.14159265358979323846f
template<class T> T constrain(T x,T lo,T hi){return x<lo?lo:(x>hi?hi:x);}
#include "app/SupportYaw.h"
#include <assert.h>
#include <stdio.h>
int main(){
 using namespace ControlCommon;
 YawGains g{.4f,.25f,2.f};
 auto lim=defaultYawLimits();lim.authority=.25f;lim.deadbandRad=DEG2RAD;
 lim.minPower=0;lim.maxPower=.16f;
 YawState s;resetYawState(s,0);
 // Right drift: yaw derivative negative. Brake even while heading error is 0.
 const float rate=updateYawRate(s,0,.1f,.004f);
 assert(rate<0);
 const float cmd=SupportYaw::command(g,lim,s,0,rate,.004f);
 assert(cmd>0 && s.integral>0);
 const float learned=s.integral;
 assert(SupportYaw::command(g,lim,s,0,0,.004f)>0 && s.integral==learned);
 assert(SupportYaw::command(g,lim,s,NAN,0,.004f)==0);
 // Measured trim applies at zero error/rate without integral preload.
 YawState trimmed;resetYawState(trimmed,0);
 const float bias=SupportYaw::VISUAL_TRIM_BLEND/lim.authority;
 assert(fabsf(SupportYaw::command(g,lim,trimmed,0,0,.004f,false,bias)-.4f)<1.e-6f);
 assert(trimmed.integral==0);
 SupportYaw::Mixer trimMixer;
 for(int i=0;i<500;++i)trimMixer.update(.15f,.4f,.25f,0,.16f,.004f,.14f);
 assert(fabsf(trimMixer.servo1-31.5f)<1.e-5f && fabsf(trimMixer.servo2-85.5f)<1.e-5f);
 SupportYaw::Mixer pose;
 for(int i=0;i<500;++i)pose.update(.15f,.4f,.25f,0,.16f,.004f,.14f,SupportYaw::VISUAL_TRANSLATION_TRIM_DEG);
 assert(fabsf(pose.servo1-28.5f)<1.e-5f && fabsf(pose.servo2-88.5f)<1.e-5f);
 assert(fabsf(pose.power*pose.etaZ-.15f)<1.e-5f);
 for(int i=0;i<3000;++i){
  const float a=pose.servo1,b=pose.servo2;
  pose.update(.16f,(i/250)%2?1.f:-1.f,.25f,0,.16f,.004f,.14f,3.f);
  assert(fabsf(pose.servo1-a)<=.12001f && fabsf(pose.servo2-b)<=.12001f);
  assert(pose.servo1>=26.249f && pose.servo1<=56.251f);
  assert(pose.servo2>=71.249f && pose.servo2<=101.251f);
  assert(pose.power<=.160001f && pose.power>=.13999f);
 }
 pose.update(.15f,.4f,.25f,0,.16f,.004f,.14f,NAN);assert(pose.power==0);
 SupportYaw::Mixer advance;
 for(int i=0;i<500;++i)advance.update(.15f,.4f,.25f,0,.16f,.004f,.14f,3.f,.01f);
 assert(advance.servo1>28.5f && advance.servo2<88.5f);
 assert(advance.forwardAngle>0 && advance.forwardAngle<=4 && advance.power<=.16f);
 advance.update(.15f,.4f,.25f,0,.16f,.004f,.14f,3.f,0);
 assert(advance.forwardAngle==0); // remove request immediately; servos retain slew
 SupportYaw::Mixer calibrated;calibrated.reset(true);
 for(int i=0;i<500;++i)calibrated.update(.15f,0,.25f,0,.16f,.004f,.14f,0,0,true);
 assert(fabsf(calibrated.servo1-52.5f)<1.e-5f && fabsf(calibrated.servo2-64.5f)<1.e-5f);
 assert(calibrated.power>.15f && fabsf(calibrated.power*calibrated.etaZ-.15f)<1.e-6f);
 for(int i=0;i<3000;++i){
  const float a=calibrated.servo1,b=calibrated.servo2;
  calibrated.update(.15f,(i/250)%2?1.f:-1.f,.25f,0,.16f,.004f,.14f,0,0,true);
  assert(fabsf(calibrated.servo1-a)<=.12001f && fabsf(calibrated.servo2-b)<=.12001f);
  // Float slew accumulation tolerance remains below one PWM angle step0.1deg.
  assert(fabsf((calibrated.servo2-calibrated.servo1)-12.f)<.01f);
  assert(calibrated.servo1>=40.374f && calibrated.servo1<=65.376f);
  assert(calibrated.servo2>=52.374f && calibrated.servo2<=77.376f);
  assert(calibrated.power>=.13999f && calibrated.power<=.160001f);
 }
 for(int i=0;i<500;++i)calibrated.update(.15f,0,.25f,0,.16f,.004f,.14f,0,.01f,true);
 assert(calibrated.servo1>52.5f && calibrated.servo2<64.5f);
 assert(calibrated.forwardAngle<=4.f && calibrated.power<=.16f);
 for(int i=0;i<500;++i)calibrated.update(.13f,0,.25f,0,.16f,.004f,.10f,0,.03f,true);
 assert(calibrated.forwardAngle>8 && calibrated.forwardAngle<=12);
 assert(calibrated.servo1>60.5f && calibrated.servo2<56.5f);
 assert(calibrated.servo1<=65.376f && calibrated.servo2>=52.374f && calibrated.power<=.16f);
 for(int i=0;i<500;++i)calibrated.update(.13f,1,.25f,0,.16f,.004f,.10f,0,.03f,true);
 assert(calibrated.forwardAngle<1.e-5f && calibrated.servo2>=52.374f);
 // Ascent coast redirects thrust forward without compensating away the loss
 // of vertical thrust. Other modes retain the original12deg envelope.
 SupportYaw::Mixer redirect,compensated;redirect.reset(true);compensated.reset(true);
 for(int i=0;i<300;++i){
  redirect.update(.08f,0,.25f,0,.20f,.004f,.06f,0,.03f,true,18.f,false);
  compensated.update(.08f,0,.25f,0,.20f,.004f,.10f,0,.03f,true);
  assert(redirect.servo1>=0 && redirect.servo1<=120 && redirect.servo2>=0 && redirect.servo2<=120);
 }
 assert(redirect.forwardAngle>17.9f && redirect.forwardAngle<=18.f);
 assert(redirect.power<compensated.power && redirect.power*redirect.etaZ<compensated.power*compensated.etaZ);
 assert(redirect.power>=.06f && redirect.power<=.20f);
 redirect.update(.08f,1,.25f,0,.20f,.004f,.06f,0,.03f,true,18.f,false);
 assert(redirect.forwardAngle<1.e-5f); // yaw still has priority, no extra-angle misuse
 // PID may cancel/reverse bias; antiwindup sees total biased saturation.
 assert(SupportYaw::command(g,lim,trimmed,-1.f,.5f,.004f,false,bias)<0);
 trimmed.integral=.3f;
 assert(SupportYaw::command(g,lim,trimmed,1.f,-1.f,.004f,true,bias)==1);
 assert(trimmed.integral==.3f);
 SupportYaw::Mixer small,big;
 for(int i=0;i<500;++i){
  small.update(.14f,.01f,.25f,.16f,.16f,.004f);
  big.update(.14f,.5f,.25f,.16f,.16f,.004f);
 }
 assert(fabsf(small.servo1-SERVO1_Z_DEG)<fabsf(big.servo1-SERVO1_Z_DEG)*.03f);
 assert(fabsf(small.servo2-SERVO2_Z_DEG)<fabsf(big.servo2-SERVO2_Z_DEG)*.03f);
 assert(fabsf(big.power*big.etaZ-.14f)<1.e-5f); // room under ceiling preserves Z
 SupportYaw::Mixer m;
 for(int i=0;i<3000;++i){
  const float a=m.servo1,b=m.servo2,p=m.power;
  m.update(.16f,(i/250)%2?1.f:-1.f,1.f,.16f,.16f,.004f);
  assert(fabsf(m.servo1-a)<=.12001f && fabsf(m.servo2-b)<=.12001f);
  assert(fabsf(m.power-p)<=.000721f && m.power<=.160001f && m.power>=0);
  assert(m.servo1>=26.249f && m.servo1<=56.251f);
  assert(m.servo2>=71.249f && m.servo2<=101.251f);
  assert(m.etaZ>=.95f);
 }
 assert(m.power*m.etaZ>=.1519f); // saturation limitation is explicit, NOT 16% Z
 m.update(.16f,NAN,.25f,.16f,.16f,.004f);assert(m.power==0);
 printf("PASS support yaw: measured bias, reversal, biased antiwindup, rate braking, proportional angles, slew and power limits\n");
}
