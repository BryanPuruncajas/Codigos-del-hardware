#include "mission/InertialHeight.h"
#include <assert.h>
#include <stdio.h>

int main(){
    // Recorded failure class: PARTIAL at y136, but pressure reports falling.
    // A pressure-only recovery must not override persistent visual overshoot.
    InertialHeight partial;float trim=.15f;
    float p=partial.update(100,trim,-.7f,0,false,true,true,-.10f,.20f,0);
    assert(p<trim-.01f);
    assert(partial.motorFloor==0);
    // At rest above the waypoint, learn DOWN through the former6/10% floors.
    InertialHeight high;float highTrim=.15f;
    for(unsigned t=100;t<=20000;t+=20)
        p=high.update(t,highTrim,-.7f,0,false,true,true,0,.20f,0);
    assert(highTrim<.06f && p<.06f);
    // Crossing the visual centre after learning does not restore a10% floor.
    p=high.update(20020,highTrim,0,0,false,true,true,0,.20f,0);
    assert(p<=highTrim+.00001f && high.motorFloor<=highTrim);
    // Reliable visual descent can anticipate the crossing while still above.
    InertialHeight descending;float dtrim=.13f;
    p=descending.update(100,dtrim,-.3f,.30f,true,true,true,-.15f,.20f,0);
    assert(p>dtrim && descending.brakingDown);
    // No increased blind support from a retained upward/overshoot correction.
    p=high.update(20040,highTrim,0,0,false,false,false,NAN,.20f,0);
    assert(p<=highTrim+.00001f);
    // Learning stays frozen during impulses or rapid sign-changing motion.
    InertialHeight frozen;float ftrim=.15f;frozen.learningAllowed=false;
    for(unsigned t=100;t<4000;t+=20)
        frozen.update(t,ftrim,-.7f,0,false,true,true,-.1f,.20f,0);
    assert(ftrim==.15f);
    InertialHeight bouncing;float btrim=.15f;
    for(unsigned t=100;t<4000;t+=20)
        bouncing.update(t,btrim,(t/200)%2?.7f:-.7f,0,false,true,true,0,.20f,0);
    assert(btrim==.15f);
    puts("PASS visual overshoot priority, learned support below fixed floors, visual lead, frozen learning");
}
