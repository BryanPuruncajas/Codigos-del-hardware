#include "mission/VisualSearch.h"
#include <assert.h>
#include <stdio.h>
int main(){
 VisualSearch s;s.reset(0);
 s.update(10000,false,0,0,true);assert(!s.started && s.demand==0);
 s.update(11000,true,0,0,true);assert(s.started && s.demand>=VisualSearch::START_POWER && s.demand<=.25f);
 for(unsigned t=11100;t<14000;t+=100){s.update(t,true,0,0,true);
  assert(!s.failed && fabsf(s.yawReference)<=.349066f && s.demand<=.25f);}
 const float before=s.demand;
 // v30 needs coherent pressure evidence; one abrupt velocity sample is not
 // a measured climb. Supply consistent displacement before testing braking.
 for(unsigned t=14000;t<=15800;t+=100)
  s.update(t,true,.1f+.3f*(t-14000)*.001f,.3f,true);
 assert(s.demand<before);
 s.update(15900,true,.7f,0,true);assert(s.climbLimited && s.demand<=s.trim && s.inertia.desiredRate==0);
 s.update(16000,true,.1f,0,true);assert(s.climbLimited && s.demand<=s.trim && s.inertia.desiredRate==0);
 s.update(31000,true,.1f,0,true);assert(s.failed && s.demand==0);
 s.reset(0);s.update(100,true,NAN,NAN,false);
 s.update(8099,true,NAN,NAN,false);assert(!s.failed && s.demand<=VisualSearch::START_POWER);
 s.update(8100,true,NAN,NAN,false);assert(s.failed && s.demand==0);
 s.reset(0);s.update(0xfffffff0U,true,0,0,true);
 s.update(uint32_t(0xfffffff0U+19999U),true,0,0,true);assert(!s.failed);
 s.update(uint32_t(0xfffffff0U+20000U),true,0,0,true);assert(s.failed);
 // More support is learned while buoyancy is inadequate, not fixed at14%.
 s.reset(0);
 for(unsigned t=100;t<=18100;t+=100)s.update(t,true,-.1f*t*.001f,-.1f,true);
 assert(!s.failed && s.trim>VisualSearch::START_POWER && s.demand>.20f && s.demand<=.25f);
 const float learned=s.trim;
 s.update(18200,true,-1.8f,NAN,false);assert(s.trim==learned && s.demand==learned);
 // Operator's lower ceiling is honoured, including integrator saturation.
 s.reset(0);
 for(unsigned t=100;t<=18100;t+=100)s.update(t,true,-.1f*t*.001f,-.2f,true,.16f);
 assert(s.demand<=.16f && s.trim<=.16f);
 // No target yet: excessive ascent must reduce lift below the learned trim.
 s.reset(0);
 for(unsigned t=100;t<=1900;t+=100)s.update(t,true,.2f*(t-100)*.001f,.2f,true);
 assert(s.inertia.brakingUp && s.demand<s.trim && s.inertia.motorFloor==0);
 s.reset(0);s.update(100,true,0,0,true,.20f,.06f,true);
 assert(s.inertia.desiredRate==0); // no climb while the camera confirms a candidate
 puts("PASS initial search: armed-only, bounded rise/sweep, speed braking, relative ceiling, timeout, sensor loss, wrap");
}
