#include "app/VisualBearing.h"
#include <assert.h>
#include <stdio.h>
int main(){
 VisualBearing centre,left,right;
 assert(centre.update(1,120,100) && fabsf(centre.reference-1)<1.e-6f);
 left.update(1,60,100);right.update(1,180,100);
 assert(left.reference>1 && right.reference<1);
 const float target=left.reference;
 const float yaw=1.1f;
 const float x=120-(target-yaw)/(70*VisualBearing::PI_F/180)*240;
 left.update(yaw,x,200);assert(fabsf(left.reference-target)<1.e-5f);
 const float old=centre.reference;centre.update(1,0,200);
 assert(fabsf(VisualBearing::wrap(centre.reference-old))<=.03501f);
 VisualBearing cross;cross.update(179*VisualBearing::PI_F/180,120,100);
 const float before=cross.reference;
 cross.update(-179*VisualBearing::PI_F/180,120,200);
 assert(fabsf(VisualBearing::wrap(cross.reference-before))<.04f);
 const float safe=left.reference;
 assert(!left.update(NAN,60,300) && left.reference==safe);
 assert(!left.update(0,240,300) && left.reference==safe);
 VisualBearing wrap;wrap.update(0,120,0xffffffc0U);wrap.update(0,0,36);
 assert(isfinite(wrap.reference) && fabsf(wrap.reference)<=.03501f);
 puts("PASS visual bearing: sign, target offset, world-bearing consistency, reference slew, pi crossing, invalid data, clock wrap");
}
