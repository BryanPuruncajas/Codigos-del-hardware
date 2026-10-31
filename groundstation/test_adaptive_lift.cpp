#include "mission/AdaptiveLift.h"
#include <assert.h>
#include <stdio.h>
int main(){
 // Deliberately simple damped vertical plant. This tests adaptation to a
 // changing required support command; it does not simulate real thrust.
 float trim=.12f,vz=0,power=0;
 for(unsigned i=0;i<2400;++i){
  const float hover=i<600?.12f:(i<1800?.17f:.12f);
  const float error=-vz;
  const float raw=trim+.10f*error;
  if(!((raw>=.20f && error>0)||(raw<=.10f && error<0)))
   trim=AdaptiveLift::learn(trim,error,.05f,.10f,.20f);
  power=fmaxf(.10f,fminf(.20f,trim+.10f*error));
  vz+=(2.f*(power-hover)-.5f*vz)*.05f;
  assert(power>=.10f && power<=.20f && isfinite(vz));
  if(i==1799)assert(trim>.15f && fabsf(vz)<.04f);
 }
 assert(trim<.14f && fabsf(vz)<.04f); // adaptation is reversible
 const float before=trim;
 assert(AdaptiveLift::learn(trim,.2f,2.f,.10f,.20f)==before); // stale gap cannot wind up
 assert(AdaptiveLift::learn(trim,NAN,.1f,.10f,.20f)==before);
 puts("PASS adaptive lift: changed support requirement, both directions, limits, stale/invalid evidence");
}
