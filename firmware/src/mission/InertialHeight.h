#pragma once
#include <math.h>
#include <stdint.h>
#include "mission/AdaptiveLift.h"
#include "mission/VerticalMotionEvidence.h"

// Positive thrust only. Arrest ascent by reducing lift early; arrest descent
// by restoring lift early. No downward thrust or physical contact detection.
struct InertialHeight {
    float rate=0,acceleration=0,predictedRate=0,desiredRate=0,motorFloor=.10f;
    bool rateReady=false,velocityTrusted=false,brakingUp=false,brakingDown=false,visualAbove=false;
    bool initialized=false,wasTracking=false,learningAllowed=true;
    uint32_t lastMs=0,quietSince=0;
    uint32_t visualSince=0;
    uint32_t lastMeasurementUs=0;
    int visualSign=0;
    VerticalMotionEvidence evidence;
    bool visualCorroborated=false;
    float velocityConfidence=0,controlRate=0;
    float recoveryTrim=0,lastNegativeCorrection=0;
    static float bound(float v,float lo,float hi){return fmaxf(lo,fminf(hi,v));}
    void reset(){*this=InertialHeight();}
    float update(uint32_t now,float& trim,float error,float imageRate,bool reliable,
                 bool tracking,bool barometerValid,float vz,float ceiling,float floor,
                 float recoveryRate=0,bool boundRecoveryLearning=true,uint32_t sampleTimeUs=0,float altitude=NAN,float fixedHeightError=NAN,bool independentVelocity=false){
        const bool fixed=isfinite(fixedHeightError);
        const bool first=!initialized;
        float dt=initialized?uint32_t(now-lastMs)*.001f:0.f;
        const bool continuous=initialized && dt>0 && dt<=.25f;
        if(!continuous){dt=0;rateReady=false;quietSince=now;}
        initialized=true;lastMs=now;
        trim=bound(trim,floor,ceiling);
        if(first)recoveryTrim=trim;
        const bool usable=barometerValid && isfinite(vz) && fabsf(vz)<=.6f;
        const uint32_t measurementUs=sampleTimeUs?sampleTimeUs:now*1000u;
        if(usable && (!rateReady || sampleTimeUs==0 || sampleTimeUs!=lastMeasurementUs)){
            evidence.update(vz,measurementUs,altitude);
            const float measurementDt=sampleTimeUs && lastMeasurementUs?
                uint32_t(sampleTimeUs-lastMeasurementUs)*1.e-6f:dt;
            if(!rateReady){rate=vz;acceleration=0;rateReady=true;}
            else{
                const float old=rate;
                // Sensor layer already estimates/filters velocity. Process
                // once per acquisition, using its own dt, not the control tick.
                rate=vz;
                if(measurementDt>0 && measurementDt<=.5f){
                    const float a=bound((rate-old)/measurementDt,-.2f,.2f);
                    acceleration+=measurementDt/(.5f+measurementDt)*(a-acceleration);
                }else acceleration=0;
            }
            lastMeasurementUs=sampleTimeUs;
        }else if(!usable){rateReady=false;acceleration=0;lastMeasurementUs=0;evidence.reset();}
        // Reliable visual motion wins if the pressure velocity contradicts
        // its direction. Clipped centres are never used as derivatives.
        const bool visualMotion=tracking && reliable && isfinite(imageRate) && fabsf(imageRate)>.08f;
        visualCorroborated=usable && rateReady && visualMotion && fabsf(rate)>.01f && imageRate*rate<0;
        const bool contradicts=visualMotion && fabsf(rate)>.01f && imageRate*rate>0;
        velocityTrusted=usable && rateReady && (independentVelocity ||
            (!contradicts && (evidence.trusted || visualCorroborated)));
        velocityConfidence=velocityTrusted?((independentVelocity || visualCorroborated)?1.f:evidence.confidence):0.f;
        controlRate=velocityTrusted?((independentVelocity || visualCorroborated)?rate:evidence.acceptedRate):0.f;
        const float projected=bound(error+(reliable?1.2f*imageRate:0.f),-1.f,1.f);
        desiredRate=fixed?bound(.16f*fixedHeightError,-.04f,.04f):
            (tracking?bound(.08f*projected,-.04f,.04f):bound(recoveryRate,-.04f,.04f));
        // Pressure-only trends use a robust median without differentiating
        // noise a second time. Fast anticipation needs visual corroboration.
        predictedRate=velocityTrusted?bound(controlRate+((independentVelocity || visualCorroborated)?.5f*acceleration:0.f),-.35f,.35f):0;
        brakingUp=velocityTrusted ? controlRate>.035f && predictedRate>desiredRate+.025f :
            tracking && reliable && imageRate<-.08f;
        brakingDown=velocityTrusted ? controlRate<-.035f && predictedRate<desiredRate-.025f :
            tracking && reliable && imageRate>.08f;
        const bool above=fixed?fixedHeightError<-.01f:(tracking && projected<-.10f);
        visualAbove=above;
        float correction=0;
        if(velocityTrusted)correction=velocityConfidence*bound(.35f*(desiredRate-predictedRate),-.07f,.07f);
        else if(tracking)correction=reliable?.08f*imageRate:0.f;
        else correction=0; // no indefinite cached braking from an old observation
        if(fixed){
            // Metric position is fixed; image y is no longer its setpoint.
            correction=bound(correction+bound(.10f*fixedHeightError,-.06f,.06f),-.10f,.10f);
            // A trustworthy falling range can demand braking lift even above
            // the plane. A clipped visual centre must not veto that evidence.
            if(above && !reliable && !independentVelocity)correction=fminf(correction,0.f);
        }else if(tracking){
            const float visual=.06f*projected;
            correction=bound(correction+visual,-.10f,.10f);
            // A clipped image has no trustworthy visual velocity. Pressure
            // descent alone cannot command MORE lift while the visible target
            // is already below us. Reliable visual lead may anticipate a real
            // downward crossing; it changes projected before this condition.
            if(above && !reliable)correction=fminf(correction,.5f*visual);
        }
        // Pressure-only learning waits for quiet motion. Visual learning below
        // requires a persistent signed error and pauses during impulses or
        // fast reliable image motion; it is not a measured buoyancy estimate.
        const bool quiet=usable && (evidence.slowTrusted || evidence.settled) && !evidence.fastTrusted &&
            !contradicts && fabsf(acceleration)<.04f && fabsf(evidence.median)<.12f &&
            (!reliable || fabsf(imageRate)<.15f);
        if(!quiet || !continuous || !learningAllowed)quietSince=now;
        if(!tracking && wasTracking)recoveryTrim=trim;
        const int sign=tracking && fabsf(projected)>.10f?(projected>0?1:-1):0;
        const bool slowVisual=!reliable || fabsf(imageRate)<=.35f;
        if(!continuous || sign!=visualSign || !learningAllowed || !slowVisual)visualSince=now;
        visualSign=sign;
        if(!fixed && tracking && continuous && learningAllowed && slowVisual && sign!=0 &&
           uint32_t(now-visualSince)>=600){
            // Learn the support required at the VISUAL target height. The old
            // governor learned only pressure velocity and could keep15% while
            // the target stayed below centre for tens of seconds. No fixed
            // neutral-buoyancy percentage is imposed here.
            const float raw=trim+correction;
            if(!((raw>=ceiling && projected>0)||(raw<=floor && projected<0)))
                trim=bound(trim+bound(.012f*projected,-.008f,.004f)*dt,floor,ceiling);
        }
        if((!tracking || fixed) && quiet && continuous && learningAllowed && uint32_t(now-quietSince)>=1000){
            const float raw=trim+.35f*(desiredRate-predictedRate);
            if(!((raw>=ceiling && desiredRate>controlRate)||(raw<=floor && desiredRate<controlRate))){
                float next=AdaptiveLift::learn(trim,desiredRate-(evidence.settled?0.f:controlRate),dt,floor,ceiling);
                next=bound(next,trim-.002f*dt,trim+.002f*dt);
                if(!tracking && boundRecoveryLearning){
                    const float low=fminf(ceiling,fmaxf(floor,recoveryTrim-.02f));
                    next=bound(next,low,fmaxf(low,fminf(ceiling,recoveryTrim+.02f)));
                }
                trim=next;
            }
        }
        if(tracking)lastNegativeCorrection=fminf(0.f,correction);
        wasTracking=tracking;
        // Reducing the lift command must be possible all the way to ESC idle.
        // A requested spinning-motor floor cannot stand in for hover thrust.
        motorFloor=(brakingUp || above || (!tracking && !velocityTrusted && lastNegativeCorrection<-.01f))?
            0.f:fminf(.10f,trim);
        const float demand=bound(trim+correction,floor,ceiling);
        brakingUp=brakingUp && demand<trim;
        brakingDown=brakingDown && demand>trim;
        return demand;
    }
};
