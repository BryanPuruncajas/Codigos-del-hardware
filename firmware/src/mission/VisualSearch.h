#pragma once
#include <math.h>
#include <stdint.h>
#include "mission/AdaptiveLift.h"
#include "mission/InertialHeight.h"

// Initial acquisition only. Bounded slow-climb request + heading sweep;
// pressure velocity is provisional feedback, not ground/ceiling clearance.
struct VisualSearch {
    static constexpr uint32_t TIMEOUT_MS=20000,BLIND_MS=8000;
    static constexpr float START_POWER=AdaptiveLift::START_TRIM,MAX_POWER=AdaptiveLift::HARD_MAX,MIN_POWER=.06f;
    InertialHeight inertia;
    bool started=false,failed=false,verticalReady=false,climbLimited=false;
    uint32_t since=0,lastMs=0,lastVerticalMs=0;
    float trim=START_POWER,demand=0,initialHeight=0,heading=0,yawReference=0;
    static float wrap(float a){return atan2f(sinf(a),cosf(a));}
    void reset(float yaw){*this=VisualSearch();heading=yaw;yawReference=yaw;}
    void update(uint32_t now,bool armed,float altitude,float vz,bool verticalValid,
                float ceiling=MAX_POWER,float floor=MIN_POWER,bool confirming=false,uint32_t sampleTimeUs=0,bool independentVelocity=false){
        if(failed)return;
        if(!armed){demand=0;return;}
        if(!isfinite(ceiling)||!isfinite(floor)||ceiling<=0||floor<0||floor>ceiling){
            failed=true;demand=0;return;
        }
        ceiling=fminf(MAX_POWER,ceiling);floor=fminf(floor,ceiling);
        trim=fmaxf(floor,fminf(ceiling,trim));
        if(!started){started=true;since=lastMs=lastVerticalMs=now;}
        lastMs=now;
        const uint32_t elapsed=uint32_t(now-since);
        if(!isfinite(heading) || elapsed>=TIMEOUT_MS){failed=true;demand=0;return;}
        const bool usable=verticalValid && isfinite(altitude) && isfinite(vz);
        if(usable){
            lastVerticalMs=now;
            if(!verticalReady){verticalReady=true;initialHeight=altitude;}
            // Latch a provisional relative rise limit; pressure drift cannot
            // make the blind climb restart repeatedly at this threshold.
            if(altitude-initialHeight>=.20f)climbLimited=true;
        }else if(uint32_t(now-lastVerticalMs)>=BLIND_MS){
            failed=true;demand=0;return;
        }
        // +/-20deg around the initial heading, max reference speed6.3deg/s.
        yawReference=wrap(heading+.34906585f*sinf(elapsed*.001f*6.2831853f/20.f));
        // The same anticipatory governor must run BEFORE a balloon is seen.
        // Pause climb during camera confirmation; don't sweep past the target
        // while its temporal confirmation is still pending.
        const float desired=(climbLimited||confirming)?0.f:.03f;
        demand=inertia.update(now,trim,0,0,false,false,usable,vz,ceiling,floor,desired,false,sampleTimeUs,altitude,NAN,independentVelocity);
    }
};
