#include "app/VisualSupport.h"
#include "app/VisualBearing.h"
#include <assert.h>
#include <stdio.h>
int main(){
    VisualSupport::Observation o;o.fresh=o.confirmed=o.full=true;
    o.x=120;o.y=80;o.w=100;o.h=80;o.left=70;o.top=40;
    VisualSupport::Loop c;c.reset(.15f,100);
    for(unsigned ms=100;ms<=1200;ms+=40){++o.generation;
        c.update(o,ms,.15f,.20f,false,0,.8f,0,true,0,60000,true,15000,ms*1000,0);}
    assert(c.altitudeReady);
    const float anchor=c.altitudeReference,trim=c.height.trim;
    o.near=true;o.full=false;o.left=0;o.top=0;o.w=240;o.h=160;o.x=55;o.y=120;
    for(unsigned ms=1240;ms<=2200;ms+=40){++o.generation;
        c.update(o,ms,.15f,.20f,false,0,.8f,0,true,0,60000,true,15000,ms*1000,0);}
    assert(o.coversCentre() && o.edgeError()==0);
    assert(c.phase==VisualSupport::RUN && c.altitudeReference==anchor);
    assert(fabsf(c.demand-trim)<1.e-6f && c.height.derivative==0);
    // Clipped centroid would falsely ask for descent; it must not be used.
    assert(c.inertia.desiredRate==0 && c.height.trim==trim);
    o.fresh=false;
    c.update(o,2400,.15f,.20f,false,0,.8f,0,true,0,60000,true,15000,2400000,0);
    assert(c.phase==VisualSupport::HOLD && c.recoveryYawOffset(3200)==0 && c.altitudeReference==anchor);
    o.fresh=true;o.top=0;o.h=50;++o.generation;
    c.update(o,2500,.15f,.20f,false,0,.8f,0,true,0,60000,true,15000,2500000,0);
    assert(o.edgeError()>0 && c.demand>trim && c.altitudeReference==anchor);
    o.top=110;o.h=50;++o.generation;
    c.update(o,2600,.15f,.20f,false,0,.8f,0,true,0,60000,true,15000,2600000,0);
    assert(o.edgeError()<0 && c.demand<trim && c.altitudeReference==anchor);
    VisualBearing bearing;bearing.update(0,120,0);
    for(unsigned ms=100;ms<=1100;ms+=100)bearing.updateNear(0,240,ms);
    assert(bearing.reference==0); // no cropped-centroid steering
    for(unsigned ms=1200;ms<=2200;ms+=100)bearing.updateNear(0,80,ms);
    assert(bearing.reference>0 && bearing.reference<.058f); // side only,<=3deg/s
    const float before=bearing.reference;bearing.updateNear(0,240,2300);
    assert(bearing.reference==before);
    puts("PASS near support: cropped centre ignored, reference retained, edge-only hints, bounded yaw, missing-frame hold");
}
