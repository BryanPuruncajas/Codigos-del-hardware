#pragma once
#include <stdint.h>
#include <math.h>

// Local control aid, never a new sensor or an absolute-height estimate.
// Drift is observable here only under the assumption of a stationary waypoint,
// stable image geometry and stable pitch. Slow real motion is NOT discarded.
struct VisualAltitudeAid {
    static constexpr uint32_t FRESH_MS=4000,LEARN_MS=2000;
    static constexpr float MAX_DRIFT=.005f; // at most5mm/s, provisional
    float noiseBand=.03f,driftRate=0;
    bool candidate=false,driftReady=false,suspended=false;
    uint32_t startMs=0,lastMs=0,learnedAt=0;
    unsigned samples=0;
    float firstHeight=0,minY=0,maxY=0,minWidth=0,maxWidth=0,minPitch=0,maxPitch=0;
    void pause(bool preserve=false){
        if(preserve && candidate){suspended=true;return;}
        candidate=false;suspended=false;samples=0;
    }
    bool driftValid(uint32_t now) const {return driftReady && uint32_t(now-learnedAt)<=FRESH_MS;}
    void begin(uint32_t now,float level,float y,float width,float pitch){
        candidate=true;suspended=false;startMs=lastMs=now;samples=1;firstHeight=level;
        minY=maxY=y;minWidth=maxWidth=width;minPitch=maxPitch=pitch;
    }
    void observe(uint32_t now,float level,float mad,float y,float width,float pitch){
        if(!isfinite(level)||!isfinite(mad)||!isfinite(y)||!isfinite(width)||width<=0||!isfinite(pitch)){
            pause();return;
        }
        // Median absolute deviation is a noise band, not a probability or a
        // claim of centimetre accuracy. Do not react to errors below that band.
        noiseBand=fmaxf(.03f,fminf(.12f,3.f*1.4826f*mad));
        if(noiseBand>.06f){pause();return;}
        // An impulse can suspend learning between two stable visual anchors.
        // Resume only if the SAME image geometry returns within8s; do not
        // learn from the intervening pressure transient or extrapolated frames.
        if(!candidate || uint32_t(now-lastMs)>(suspended?8000u:250u)){
            begin(now,level,y,width,pitch);return;
        }
        minY=fminf(minY,y);maxY=fmaxf(maxY,y);
        minWidth=fminf(minWidth,width);maxWidth=fmaxf(maxWidth,width);
        minPitch=fminf(minPitch,pitch);maxPitch=fmaxf(maxPitch,pitch);
        if(maxY-minY>2.f || maxWidth-minWidth>.05f*minWidth || maxPitch-minPitch>.015f){
            begin(now,level,y,width,pitch);return;
        }
        suspended=false;
        const float dt=fminf(.25f,uint32_t(now-lastMs)*.001f);lastMs=now;++samples;
        const uint32_t elapsed=uint32_t(now-startMs);
        if(elapsed<LEARN_MS || samples<8)return;
        const float measured=(level-firstHeight)/(elapsed*.001f);
        // A real slow fall of~0.029m/s from the user's tests is outside this
        // drift hypothesis. Never clamp such motion INTO a drift estimate.
        if(fabsf(measured)>.01f){begin(now,level,y,width,pitch);return;}
        const float bounded=fmaxf(-MAX_DRIFT,fminf(MAX_DRIFT,measured));
        if(!driftValid(now))driftRate=bounded;
        else driftRate+=dt/(4.f+dt)*(bounded-driftRate);
        driftReady=true;learnedAt=now;
        if(elapsed>=20000)begin(now,level,y,width,pitch);
    }
    static float deadband(float error,float band){
        return fabsf(error)<=band?0.f:copysignf(fabsf(error)-band,error);
    }
    float rate(uint32_t now) const {return driftValid(now)?driftRate:0.f;}
    float predictedReference(float raw,uint32_t updatedAt,uint32_t now) const {
        const uint32_t age=uint32_t(now-updatedAt);
        return raw+(age<=FRESH_MS?rate(now)*age*.001f:0.f);
    }
    bool referenceUsable(bool captured,uint32_t updatedAt,uint32_t now) const {
        return captured && uint32_t(now-updatedAt)<=FRESH_MS && noiseBand<.10f;
    }
};
