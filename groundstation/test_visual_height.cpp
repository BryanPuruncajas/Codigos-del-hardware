#include "VisualHeight.h"
#include <assert.h>
#include <stdio.h>
int main() {
    VisualHeight c;
    const float center=c.update(100,80,30,.12f,true);
    assert(fabsf(center-.10f)<1.e-6);
    const float up=c.update(200,40,30,.12f,true);
    assert(up>center && up<=.12f);
    c.reset(); c.update(100,80,30,.12f,true);
    assert(c.update(200,120,30,.12f,true)<center);
    for (unsigned i=0;i<1000;++i) {
        const float p=c.update(300+i*100,20,20,.12f,true);
        assert(p>=0 && p<=.120001f);
    }
    const float trim=c.trim;
    c.update(100400,20,40,.12f,false);
    assert(c.trim==trim); // partial/clipped does not learn bogus hover
    c.update(100500,4,20,.12f,true);
    assert(c.trim==trim);
    c.reset();
    assert(c.update(100,36,84,.12f,false)>.10f); // top edge from actual P12
    assert(c.trim==.10f && c.derivative==0);
    assert(c.update(200,101,127,.12f,false)<.10f); // lower edge from P12
    assert(c.trim==.10f && c.derivative==0);
    assert(c.update(300,80,160,.12f,false)==c.trim); // both edges: no direction
    c.update(400,80,30,.12f,true);
    assert(c.derivative==0); // no D kick after clipped centre
    c.update(500,80,30,.06f,true);
    assert(c.trim<=.06f && c.demand<=.06f);
    const float last=c.demand;
    assert(c.update(100600,NAN,30,.12f,true)==last);
    c.reset();c.update(100,40,20,.12f,true);
    c.update(5000,120,20,.12f,true);
    assert(c.derivative==0); // no derivative kick on reacquisition
    assert(VisualHeight::centered(80) && !VisualHeight::centered(120));
    assert(!VisualHeight::centered(NAN));
    for(unsigned t=0;t<20000;++t) assert(fabsf(VisualHeight::searchOffset(t))<=.200001f);
    c.reset();c.update(0xffffffc0U,80,20,.12f,true);
    c.update(36,60,20,.12f,true);
    assert(isfinite(c.demand) && c.demand<=.12f);
    VisualHeight original,predictive;original.trim=predictive.trim=.15f;
    // Same approaching target: both still above image centre, but predictive
    // control must already reduce lift at y60; no hover-I learning in motion.
    for(unsigned i=0;i<=5;++i){
        original.update(100+i*100,40+i*4,20,.16f,true,.14f);
        predictive.update(100+i*100,40+i*4,20,.16f,true,.14f,.8f);
    }
    assert(predictive.lastError>0 && predictive.derivative<0);
    assert(predictive.demand<.15f && predictive.demand<original.demand);
    assert(fabsf(predictive.trim-.15f)<1.e-6f);
    // Reverse movement must increase lift to brake descent before crossing.
    predictive.reset();predictive.trim=.15f;
    for(unsigned i=0;i<=5;++i)predictive.update(100+i*100,120-i*4,20,.16f,true,.14f,.8f);
    assert(predictive.lastError<0 && predictive.derivative>0 && predictive.demand>.15f);
    predictive.update(5000,60,20,.16f,true,.14f,.8f);
    assert(predictive.derivative==0); // reacquisition has no predicted kick
    const float learned=predictive.trim;
    predictive.update(5100,4,40,.16f,false,.14f,.8f);
    assert(predictive.derivative==0 && predictive.trim==learned);
    puts("PASS visual sign, default regression, predictive braking both ways, no transient I, clipping, gap, wrap");
}
