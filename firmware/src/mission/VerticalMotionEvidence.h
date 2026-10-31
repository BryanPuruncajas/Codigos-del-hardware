#pragma once
#include <stdint.h>
#include <math.h>

// Evidence, not a probability or a new sensor. Pressure drift can still look
// like motion. Fixed memory; acquisition timestamps, never control-loop ticks.
struct VerticalMotionEvidence {
    static constexpr unsigned CAPACITY=40;
    static constexpr uint32_t WINDOW_US=1200000,MIN_SPAN_US=600000,
        CONFIRM_US=400000,GAP_US=300000;
    static constexpr uint32_t FAST_CONFIRM_US=200000;
    static constexpr float FAST_MIN_DISPLACEMENT_M=.12f;
    float values[CAPACITY]={},heights[CAPACITY]={};uint32_t times[CAPACITY]={};
    unsigned count=0,index=0;
    uint32_t lastUs=0,candidateSince=0,spanUs=0;
    uint32_t fastSince=0;
    int candidateSign=0,fastSign=0;
    bool slowTrusted=false,fastTrusted=false;
    bool initialized=false,trusted=false,settled=false;
    float median=0,mad=0,confidence=0,consistency=0;
    float fastRate=0,fastConfidence=0,displacement=0,acceptedRate=0;
    void reset(){*this=VerticalMotionEvidence();}
    static float middle(float* a,unsigned n){
        for(unsigned i=1;i<n;++i){const float v=a[i];unsigned j=i;
            while(j && a[j-1]>v){a[j]=a[j-1];--j;}a[j]=v;}
        return n%2?a[n/2]:.5f*(a[n/2-1]+a[n/2]);
    }
    void update(float velocity,uint32_t now,float height=NAN){
        if(!isfinite(velocity)||fabsf(velocity)>.6f){reset();return;}
        if(initialized && now==lastUs)return;
        if(initialized && uint32_t(now-lastUs)>GAP_US)reset();
        initialized=true;lastUs=now;
        values[index]=velocity;heights[index]=height;times[index]=now;index=(index+1)%CAPACITY;
        if(count<CAPACITY)++count;
        float active[CAPACITY],deviation[CAPACITY];unsigned n=0;spanUs=0;
        for(unsigned i=0;i<count;++i){const uint32_t age=uint32_t(now-times[i]);
            if(age>WINDOW_US)continue;
            active[n++]=values[i];if(age>spanUs)spanUs=age;
        }
        median=middle(active,n);
        unsigned same=0;
        for(unsigned i=0;i<n;++i){deviation[i]=fabsf(active[i]-median);
            if(active[i]*median>0)++same;}
        mad=middle(deviation,n);consistency=float(same)/n;
        // A heuristic signal-to-dispersion score, NOT statistical confidence:
        // these velocities are correlated by the source's500ms regression.
        confidence=fabsf(median)/(fabsf(median)+3.f*mad+.002f);
        const bool ready=n>=5 && spanUs>=MIN_SPAN_US;
        settled=ready && fabsf(median)<.01f && mad<.01f;
        const int sign=median>0?1:-1;
        const bool candidate=ready && velocity*median>0 && fabsf(median)>=.002f &&
            confidence>=.65f && consistency>=.8f;
        if(!candidate){candidateSign=0;slowTrusted=false;}
        else{
            if(candidateSign!=sign){candidateSign=sign;candidateSince=now;}
            slowTrusted=uint32_t(now-candidateSince)>=CONFIRM_US;
        }
        // A short, clear displacement can have a changing velocity during
        // acceleration/deceleration. Requiring low velocity MAD rejected it
        // in29. Fit HEIGHT over the same bounded window, and compare robust
        // early/late levels. Height and velocity are NOT independent sensors.
        float mt=0,mh=0;unsigned hn=0;uint32_t hspan=0;
        for(unsigned i=0;i<count;++i){const uint32_t age=uint32_t(now-times[i]);
            if(age>WINDOW_US || !isfinite(heights[i]))continue;
            ++hn;mt-=age*1.e-6f;mh+=heights[i];if(age>hspan)hspan=age;
        }
        bool fastCandidate=false;fastRate=fastConfidence=displacement=0;
        if(hn>=5 && hspan>=MIN_SPAN_US){
            mt/=hn;mh/=hn;float cov=0,var=0;
            for(unsigned i=0;i<count;++i){const uint32_t age=uint32_t(now-times[i]);
                if(age>WINDOW_US || !isfinite(heights[i]))continue;
                const float t=-float(age)*1.e-6f-mt;cov+=t*(heights[i]-mh);var+=t*t;
            }
            const float slope=var>1.e-6f?cov/var:0;
            float early[CAPACITY],late[CAPACITY],residual[CAPACITY];
            unsigned en=0,ln=0,rn=0;float et=0,lt=0;
            for(unsigned i=0;i<count;++i){const uint32_t age=uint32_t(now-times[i]);
                if(age>WINDOW_US || !isfinite(heights[i]))continue;
                residual[rn++]=fabsf(heights[i]-mh-slope*(-float(age)*1.e-6f-mt));
                if(age>=hspan*3/4){early[en++]=heights[i];et-=age*1.e-6f;}
                if(age<=hspan/4){late[ln++]=heights[i];lt-=age*1.e-6f;}
            }
            if(en && ln){
                displacement=middle(late,ln)-middle(early,en);
                const float duration=lt/ln-et/en;
                fastRate=duration>0?displacement/duration:0;
                const float residualMad=middle(residual,rn);
                fastConfidence=fabsf(displacement)/(fabsf(displacement)+4.f*residualMad+.02f);
                fastCandidate=fabsf(displacement)>=FAST_MIN_DISPLACEMENT_M &&
                    fastConfidence>=.65f && displacement*velocity>0 &&
                    displacement*slope>0 && fabsf(fastRate)<=.6f;
            }
        }
        const int newFastSign=displacement>0?1:-1;
        if(!fastCandidate){fastSign=0;fastTrusted=false;}
        else{
            if(fastSign!=newFastSign){fastSign=newFastSign;fastSince=now;}
            fastTrusted=uint32_t(now-fastSince)>=FAST_CONFIRM_US;
        }
        trusted=slowTrusted || fastTrusted;
        acceptedRate=fastTrusted?fastRate:median;
        if(fastTrusted)confidence=fastConfidence;
    }
};
