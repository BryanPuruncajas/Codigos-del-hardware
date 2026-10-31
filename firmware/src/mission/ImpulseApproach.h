#pragma once
#include <stdint.h>
#include <math.h>
#include "mission/ApproachProfile.h"

// Timed impulse/coast experiment, NOT measured horizontal velocity control.
struct ImpulseApproach {
    enum Phase {ALIGN=0,SETTLE=1,PUSH=2,RETURN=3,COAST=4};
    static constexpr uint32_t SETTLE_MS=ApproachProfile::SETTLE_MS,
        PUSH_MS=ApproachProfile::PUSH_MS,COAST_MS=ApproachProfile::COAST_MS;
    // 22deg at30deg/s plus margin. Actual neutral command is also required.
    static constexpr uint32_t RETURN_VECTOR_MS=ApproachProfile::RETURN_VECTOR_MS;
    Phase phase=ALIGN;
    uint32_t since=0,pulses=0;
    uint32_t elapsed(uint32_t now) const{return uint32_t(now-since);}
    bool neutralOnly() const{return phase==SETTLE || phase==PUSH || phase==RETURN;}
    // Seconds until yaw correction can resume. RETURN waits for the physical
    // servo command to reach neutral. This is a command-travel budget,
    // not a measured angular stopping time of the airship.
    float headingHorizon(uint32_t now) const {
        if(phase==ALIGN)return float(SETTLE_MS+PUSH_MS+RETURN_VECTOR_MS)*.001f;
        if(phase==SETTLE)return float((elapsed(now)<SETTLE_MS?SETTLE_MS-elapsed(now):0)+PUSH_MS+RETURN_VECTOR_MS)*.001f;
        if(phase==PUSH)return float((elapsed(now)<PUSH_MS?PUSH_MS-elapsed(now):0)+RETURN_VECTOR_MS)*.001f;
        return 0.f;
    }
    static bool headingFits(float error,float rate,float seconds,float limit){
        if(!isfinite(error)||!isfinite(rate)||!isfinite(seconds)||!isfinite(limit)||
           seconds<0||limit<=0)return false;
        // Signed prediction: yaw error is reference-yaw, hence e_dot=-rate.
        // Motion towards the target is allowed when it stays inside the band.
        return fabsf(error)<=limit && fabsf(error-rate*seconds)<=limit;
    }
    void reset(){*this=ImpulseApproach();}
    void enter(Phase next,uint32_t now){phase=next;since=now;if(next==PUSH)++pulses;}
    void cancel(uint32_t now){
        if(phase==PUSH || phase==SETTLE)enter(RETURN,now);
    }
    void update(uint32_t now,bool aligned,bool safe,bool neutralReady){
        switch(phase){
        case ALIGN:
            if(aligned && safe)enter(SETTLE,now);
            break;
        case SETTLE:
            if(!safe || elapsed(now)>=1500)enter(RETURN,now);
            else if(elapsed(now)>=SETTLE_MS && neutralReady)enter(PUSH,now);
            break;
        case PUSH:
            if(!safe || elapsed(now)>=PUSH_MS)enter(RETURN,now);
            break;
        case RETURN:
            // No new yaw request until commanded fore/aft tilt has returned.
            if(elapsed(now)>=SETTLE_MS && neutralReady)enter(COAST,now);
            break;
        case COAST:
            if(elapsed(now)>=COAST_MS)enter(ALIGN,now);
            break;
        }
    }
};
