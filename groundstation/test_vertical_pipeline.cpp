#include "sense/VerticalEstimate.h"
#include "mission/InertialHeight.h"
#include <assert.h>
#include <stdio.h>
int main(){
 // Preserve the USER-OBSERVED32cm/11s fall; this is a synthetic ramp at that
 // measured rate, not a new fit of scale from pressure recordings.
 const float fall=-.32f/11.f;VerticalEstimate e;e.reset(0,0);
 for(unsigned i=1;i<=275;++i)e.update(fall*i*.04f,i*40000);
 assert(fabsf(e.velocity-fall)<.002f && e.windowSpanUs<=500000);
 // Irregular slower acquisition must not silently stretch the velocity window.
 uint32_t us=0;e.reset(0,0);
 for(unsigned i=0;i<150;++i){us+=i%2?40000:80000;e.update(.04f*us*1.e-6f,us);}
 assert(e.windowSpanUs<=500000 && fabsf(e.velocity-.04f)<.003f);
 InertialHeight c;float trim=.12f;
 c.update(1000,trim,0,0,false,false,true,0,.20f,0,0,true,1000000);
 c.update(1040,trim,0,0,false,false,true,-.08f,.20f,0,0,true,1040000);
 assert(c.rate==-.08f); // already-filtered source reaches controller without another250ms low-pass
 const float acceleration=c.acceleration;
 for(unsigned ms=1044;ms<1080;ms+=4)
  c.update(ms,trim,0,0,false,false,true,-.08f,.20f,0,0,true,1040000);
 assert(c.rate==-.08f && c.acceleration==acceleration); // repeated samples aren't re-differentiated
 c.update(1080,trim,0,0,false,false,true,-.04f,.20f,0,0,true,1080000);
 assert(c.rate==-.04f && c.lastMeasurementUs==1080000);
 c.update(1400,trim,0,0,false,false,false,NAN,.20f,0,0,true,1080000);
 assert(!c.velocityTrusted);
 puts("PASS user-rate slow fall, time-bounded velocity window, timestamped single velocity source, stale rejection");
}
