#pragma once
#include "mission/VisualHeight.h"
#include "mission/AdaptiveLift.h"
#include "mission/InertialHeight.h"
#include "mission/VisualAltitudeAid.h"

// Isolated trial, NOT navigation or obstacle avoidance. Barometer is used
// relatively and briefly during visual recovery, never as absolute altitude.
namespace VisualSupport {
constexpr float MAX_POWER=.16f;
constexpr float MIN_RUNNING_POWER=.10f; // positive floor, provisional current rig
constexpr float HEIGHT_LEAD_SECONDS=.8f; // provisional, isolated visual trial only
constexpr uint32_t LOSS_MS=4000, PREPARE_MS=20000, RUN_MS=15000;
enum Phase {PREPARE=0,RUN=1,HOLD=2,FAULT=3,DISARMED=4};
struct Observation {
    bool fresh=false,confirmed=false,full=false;
    bool near=false; // confirmed crop AFTER a prior FULL, never initial acquisition
    float x=0,y=0,w=0,h=0;
    float left=NAN,top=NAN;
    uint32_t generation=0;
    bool valid() const {
        return fresh && confirmed && isfinite(x) && x>=0 && x<240 &&
            isfinite(y) && y>=0 && y<160 && isfinite(w) && w>0 && w<=240 &&
            isfinite(h) && h>0 && h<=160;
    }
    float boxLeft() const {return isfinite(left)?left:x-w*.5f;}
    float boxTop() const {return isfinite(top)?top:y-h*.5f;}
    bool coversCentre() const {return boxLeft()<=120 && boxLeft()+w>=120 &&
        boxTop()<=80 && boxTop()+h>=80;}
    float edgeError() const {
        // Only a direction is known; a clipped centroid is not a position.
        return boxTop()+h<80?.25f:(boxTop()>80?-.25f:0.f);
    }
};
struct Loop {
    VisualHeight height;
    InertialHeight inertia;
    bool seen=false,released=false,failed=false,advanceReady=false,nearRemembered=false;
    uint32_t preparedAt=0,releasedAt=0,lastSeen=0,lastGeneration=0;
    float demand=0,lastY=-1;
    bool altitudeReady=false,centering=false;
    uint32_t centeredAt=0;
    float altitudeReference=0;
    static constexpr unsigned REFERENCE_SAMPLES=16;
    float referenceHeights[REFERENCE_SAMPLES]={};
    unsigned referenceCount=0,referenceIndex=0;
    uint32_t referenceSampleUs=0,referenceUpdatedAt=0,referenceUpdates=0;
    float referenceMinX=0,referenceMaxX=0,referenceMinY=0,referenceMaxY=0;
    VisualAltitudeAid altitudeAid;
    // The physical target is immutable after capture. Pressure bias is the
    // separate raw visual anchor minus this original level.
    bool fixedMode=false,runReleased=true,advanceEnabled=true,barometerValidNow=false;
    float fixedBase=0,offsetRequest=0,appliedOffset=0,levelPitch=0;
    uint32_t offsetAt=0;
    bool independentHeight=true,independentVelocity=true;
    float independentBias=0;
    void selectIndependentHeight(bool enabled,uint32_t now){
        if(enabled && !independentHeight){independentBias=pressureBias(now);independentHeight=true;pauseReference();}
    }
    bool acquiringHeight=false;
    uint32_t heightAcquireAt=0;
    float fixedTarget() const {return fixedBase+appliedOffset;}
    float pressureBias(uint32_t now) const {
        if(!altitudeReady)return 0.f;
        if(independentHeight)return independentBias;
        // Project only a recently learned drift, for at most four seconds.
        // Freeze that bias afterwards; expiring the model must not undo it.
        const bool learned=altitudeAid.driftReady &&
            uint32_t(referenceUpdatedAt-altitudeAid.learnedAt)<=VisualAltitudeAid::FRESH_MS;
        const uint32_t age=uint32_t(now-referenceUpdatedAt);
        const float projected=learned?altitudeAid.driftRate*
            fminf(float(age),float(VisualAltitudeAid::FRESH_MS))*.001f:0.f;
        return altitudeReference-fixedBase+projected;
    }
    float estimatedHeight(float raw,uint32_t now) const {return raw-pressureBias(now);}
    int fixedFlags(uint32_t now) const {
        return (altitudeReady?1:0)|(altitudeReady && barometerValidNow && altitudeAid.noiseBand<.10f?2:0)|
            (runReleased?4:0)|(advanceEnabled?8:0)|(fabsf(offsetRequest)>.0001f?16:0)|
            (altitudeReady && uint32_t(now-referenceUpdatedAt)<=4000?32:0)|(!altitudeReady?64:0);
    }
    bool fixedControlReady(uint32_t now) const {return (fixedFlags(now)&3)==3;}
    Phase phase=PREPARE;
    float recoveryYawOffset(uint32_t now) const {
        if(!seen || phase!=HOLD || nearRemembered)return 0;
        const uint32_t elapsed=uint32_t(now-lastSeen);
        // Scan about the last world bearing, never accumulate a turn.
        return elapsed<500 ? 0.f : .13962634f*sinf((elapsed-500)*.001f*6.2831853f/4.f);
    }
    float forwardRequest(const Observation& o,uint32_t now,float requested,float yawError) {
        if(failed || !released || phase!=RUN || !o.valid() || !o.full ||
           uint32_t(now-lastSeen)>200 || !isfinite(requested) || !isfinite(yawError) ||
           requested<=0 || o.w>120.f || o.h>112.f){advanceReady=false;return 0.f;}
        const float xLimit=advanceReady?36.f:24.f;
        const float yLimit=advanceReady?40.f:30.f;
        const float yawLimit=advanceReady?.20943951f:.13962634f;
        if(fabsf(yawError)>yawLimit || fabsf(o.x-120.f)>xLimit || fabsf(o.y-80.f)>yLimit){
            advanceReady=false;return 0.f;
        }
        advanceReady=true;
        return fminf(.03f,requested);
    }
    void reset(float base,uint32_t now,float maxAllowed=MAX_POWER) {
        *this=Loop();preparedAt=now;
        const float limit=isfinite(maxAllowed)?fmaxf(0,fminf(AdaptiveLift::HARD_MAX,maxAllowed)):0;
        height.trim=isfinite(base)?fmaxf(0,fminf(limit,base)):0;
        height.demand=height.trim;
    }
    void stop(){failed=true;demand=0;phase=FAULT;}
    void pauseReference(bool preserveDrift=false){
        centering=false;referenceCount=referenceIndex=0;referenceSampleUs=0;altitudeAid.pause(preserveDrift);
    }
    bool referenceUsable(uint32_t now) const {
        if(fixedMode || independentHeight)return altitudeReady && barometerValidNow && altitudeAid.noiseBand<.10f;
        return altitudeAid.referenceUsable(altitudeReady,referenceUpdatedAt,now);
    }
    float referenceForControl(uint32_t now) const {
        if(fixedMode)return fixedTarget()+pressureBias(now);
        return altitudeAid.predictedReference(altitudeReference,referenceUpdatedAt,now);
    }
    float referenceError(float altitude,uint32_t now) const {
        return referenceUsable(now) && isfinite(altitude)?
            VisualAltitudeAid::deadband(referenceForControl(now)-altitude,altitudeAid.noiseBand):0.f;
    }
    int referenceFlags(uint32_t now) const {
        return (altitudeReady?1:0)|(referenceUsable(now)?2:0)|(altitudeAid.driftValid(now)?4:0)|
            (altitudeAid.candidate && !altitudeAid.suspended?8:0)|(altitudeAid.noiseBand>=.10f?16:0);
    }
    void updateReference(const Observation& o,uint32_t now,float altitude,float pitch,
                         bool valid,bool preparing,uint32_t sampleUs,float yawRate){
        // Independent range height does not need visual pressure-bias learning.
        if(independentHeight && altitudeReady){pauseReference();return;}
        // A deliberate height offset is real motion, never pressure drift.
        if(fixedMode && altitudeReady && (fabsf(offsetRequest)>.0001f || fabsf(appliedOffset)>.0001f)){
            pauseReference();return;
        }
        const bool stableGeometry=!preparing && !o.near && o.full &&
            fabsf(o.x-120.f)<=24 && fabsf(o.y-80.f)<=10 &&
            o.y-o.h*.5f>4 && o.y+o.h*.5f<156 && valid &&
            isfinite(pitch) && fabsf(pitch-(fixedMode?levelPitch:0.f))<=
                (fixedMode?.05235988f:.17453293f) &&
            isfinite(yawRate) && fabsf(yawRate)<=.08f &&
            height.lastReliable && fabsf(height.derivative)<=.08f;
        if(!stableGeometry || !inertia.learningAllowed){
            pauseReference(stableGeometry && !inertia.learningAllowed);return;
        }
        if(!centering || uint32_t(now-lastSeen)>250){
            pauseReference(true);centering=true;centeredAt=now;
            referenceMinX=referenceMaxX=o.x;referenceMinY=referenceMaxY=o.y;
        }
        referenceMinX=fminf(referenceMinX,o.x);referenceMaxX=fmaxf(referenceMaxX,o.x);
        referenceMinY=fminf(referenceMinY,o.y);referenceMaxY=fmaxf(referenceMaxY,o.y);
        if(referenceMaxX-referenceMinX>8 || referenceMaxY-referenceMinY>6){
            pauseReference();return;
        }
        const uint32_t sample=sampleUs?sampleUs:now*1000u;
        if(referenceCount && sample==referenceSampleUs)return;
        referenceSampleUs=sample;
        referenceHeights[referenceIndex]=altitude;
        referenceIndex=(referenceIndex+1)%REFERENCE_SAMPLES;
        if(referenceCount<REFERENCE_SAMPLES)++referenceCount;
        if(uint32_t(now-centeredAt)<500 || referenceCount<5)return;
        float values[REFERENCE_SAMPLES];
        for(unsigned i=0;i<referenceCount;++i)values[i]=referenceHeights[i];
        const float level=VerticalMotionEvidence::middle(values,referenceCount);
        float deviations[REFERENCE_SAMPLES];
        for(unsigned i=0;i<referenceCount;++i)deviations[i]=fabsf(referenceHeights[i]-level);
        altitudeAid.observe(now,level,VerticalMotionEvidence::middle(deviations,referenceCount),o.y,o.w,pitch);
        if(fixedMode && altitudeAid.noiseBand>=.10f){pauseReference();return;}
        if(!altitudeReady){altitudeReference=fixedBase=level;altitudeReady=true;}
        else{
            // Refresh a CONTROL reference; never reset/scale the sensor. Cap
            // catch-up after loss so reacquisition cannot produce a jump.
            const float dt=fminf(.25f,uint32_t(now-referenceUpdatedAt)*.001f);
            const float correction=dt/(2.f+dt)*(level-altitudeReference);
            const float cap=fixedMode?.005f:.02f;
            altitudeReference+=fmaxf(-cap*dt,fminf(cap*dt,correction));
        }
        referenceUpdatedAt=now;++referenceUpdates;
    }
    void update(const Observation& o,uint32_t now,float base,float ceiling,bool preparing,float integralFloor=0.f,
                float altitude=NAN,float vz=NAN,bool barometerHealthy=false,float pitch=NAN,
                uint32_t runLimitMs=RUN_MS,bool allowPartialClimb=false,uint32_t lossLimitMs=LOSS_MS,
                uint32_t barometerSampleUs=0,float yawRate=0.f) {
        if(failed)return;
        if(!isfinite(base)||base<0||!isfinite(ceiling)||ceiling<=0||!isfinite(integralFloor)){stop();return;}
        ceiling=fminf(allowPartialClimb?AdaptiveLift::HARD_MAX:MAX_POWER,ceiling);
        if(preparing && released){stop();return;} // cannot rewind flight timer
        const bool starting=!preparing && !released;
        if(starting){
            released=true;releasedAt=now;
            height.reset();height.trim=fminf(base,ceiling);height.demand=height.trim;
        }
        if((preparing && uint32_t(now-preparedAt)>=PREPARE_MS)||
           (released && uint32_t(now-releasedAt)>=runLimitMs)){stop();return;}
        const bool observation=o.valid();
        const bool newObservation=observation && (!seen || starting || o.generation!=lastGeneration);
        const bool barometerValid=barometerHealthy && isfinite(altitude) && isfinite(vz);
        barometerValidNow=barometerValid;
        const float offsetDt=offsetAt?fminf(.25f,uint32_t(now-offsetAt)*.001f):0.f;
        offsetAt=now;
        if(fixedMode && altitudeReady && runReleased)
            appliedOffset+=fmaxf(-.02f*offsetDt,fminf(.02f*offsetDt,offsetRequest-appliedOffset));
        if(fixedMode && runReleased && !altitudeReady){
            if(!acquiringHeight){acquiringHeight=true;heightAcquireAt=now;}
            if(uint32_t(now-heightAcquireAt)>=20000){stop();return;}
        }
        // Repeated cached frames do not refresh the observation deadline.
        if(newObservation){
            if(!preparing && !o.near){
                height.update(now,o.y,o.h,ceiling,o.full,integralFloor,HEIGHT_LEAD_SECONDS,
                    true,allowPartialClimb,allowPartialClimb);
            }
            updateReference(o,now,altitude,pitch,barometerValid,preparing,barometerSampleUs,yawRate);
            nearRemembered=o.near;
            seen=true;lastSeen=now;lastGeneration=o.generation;lastY=o.y;
        }
        if(!seen || uint32_t(now-lastSeen)>=lossLimitMs){stop();return;}
        const bool tracking=observation && uint32_t(now-lastSeen)<=200;
        if(allowPartialClimb && !preparing){
            const float error=tracking?(o.near?o.edgeError():(80.f-o.y)/80.f):0.f;
            const float recoveryRate=barometerValid?
                fmaxf(-.04f,fminf(.04f,.16f*referenceError(altitude,now))):0.f;
            const bool imageControl=tracking && (!o.near || error!=0);
            height.demand=inertia.update(now,height.trim,error,o.near?0.f:height.derivative,
                tracking && !o.near && height.lastReliable &&
                    (!fixedMode || (isfinite(pitch) && fabsf(pitch-levelPitch)<=.05235988f)),
                imageControl,barometerValid,
                vz-(barometerValid && !independentHeight?altitudeAid.rate(now):0.f),
                ceiling,integralFloor,recoveryRate,true,barometerSampleUs,altitude,
                fixedMode && altitudeReady?(barometerValid?referenceError(altitude,now):0.f):NAN,independentVelocity);
            demand=height.demand;phase=tracking?RUN:HOLD;
            if(!tracking){pauseReference();advanceReady=false;}
            return;
        }
        if(!tracking){
            pauseReference();advanceReady=false;phase=HOLD;
            // Never retain a climb/advance request from the last visible frame.
            // Freeze learned support; bounded relative barometric P/D brakes
            // descent without being trapped below the required support power.
            float correction=0;
            if(!preparing && altitudeReady && barometerValid)
                correction=fmaxf(-.04f,fminf(.04f,
                    .08f*(altitudeReference-altitude)-.05f*vz));
            demand=fmaxf(0,fminf(ceiling,height.trim+correction));
        }else if(preparing){demand=fminf(base,ceiling);phase=PREPARE;}
        else {demand=fmaxf(0,fminf(height.demand,ceiling));phase=RUN;}
    }
};
}
