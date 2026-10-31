#include "mission/InertialHeight.h"
#include <assert.h>
#include <stdio.h>
float trajectory(bool predictive){
 InertialHeight c;c.learningAllowed=false;
 float trim=.12f,z=-.2f,v=.16f,power=.12f,peak=z;
 for(unsigned t=0;t<=15000;t+=20){
  const float e=fmaxf(-1.f,fminf(1.f,-z/.5f));
  float demand;
  if(predictive)demand=c.update(t,trim,e,0,false,true,true,v,.20f,.06f);
  else demand=fmaxf(.10f,fminf(.20f,trim+.06f*e));
  const float floor=predictive?c.motorFloor:.10f;
  demand=fmaxf(floor,demand);
  power+=.02f/(.15f+.02f)*(demand-power);
  v+=(3.f*(power-.12f)-.2f*v)*.02f;
  z+=v*.02f;peak=fmaxf(peak,z);
  assert(power>0 && power<=.20001f && isfinite(z));
 }
 return peak;
}
int main(){
 InertialHeight up;up.learningAllowed=false;float trim=.13f;
 // Pressure-only corrections now require acquisition-time evidence.
 for(unsigned t=40;t<=1080;t+=40)
  up.update(t,trim,0,0,false,false,true,.15f,.20f,.06f,0,true,t*1000);
 float p=up.update(1120,trim,.3f,0,false,true,true,.15f,.20f,.06f,0,true,1120000);
 assert(p<trim && up.brakingUp && up.motorFloor==0.f); // target still ABOVE
 InertialHeight down;float dtrim=.13f;
 p=down.update(100,dtrim,-.3f,.30f,true,true,true,-.15f,.20f,.06f);
 assert(p>dtrim && down.brakingDown); // visual descent anticipates crossing
 const float original=trim;
 for(unsigned t=1140;t<=2200;t+=20)
  up.update(t,trim,t%200?.3f:-.3f,0,false,true,true,t%200?.2f:-.2f,.20f,.06f);
 assert(trim==original); // rapid bounce-like transitions never teach hover trim
 InertialHeight badBaro;float btrim=.13f;
 p=badBaro.update(100,btrim,.2f,-.3f,true,true,true,-.15f,.20f,.06f);
 assert(!badBaro.velocityTrusted && p<btrim); // visual rise contradicts pressure fall
 p=badBaro.update(200,btrim,0,0,false,false,false,NAN,.20f,.06f);
 assert(p<=btrim && btrim==.13f); // no blind climb / integral learning
 InertialHeight visual;float vtrim=.13f;
 p=visual.update(100,vtrim,.2f,-.3f,true,true,false,NAN,.20f,.06f);
 assert(p<vtrim && visual.brakingUp);
 const float oldPeak=trajectory(false),newPeak=trajectory(true);
 printf("simplified plant overshoot: position-only=%.3f predictive=%.3f\n",oldPeak,newPeak);
 assert(newPeak<oldPeak*.7f);
 puts("PASS inertia: early reduction/restoration, positive thrust, frozen learning during bounce, visual fallback, overshoot");
}
