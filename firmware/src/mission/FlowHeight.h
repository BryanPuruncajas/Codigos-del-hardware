#pragma once
#include <math.h>
#include <stdint.h>
#include "comm/FlowProtocol.h"

// Flowdeck-only altitude above a fixed flat floor. No barometer input, offset
// or fallback exists in this estimator. Height is the deck-to-floor distance.
// Optical X/Y are diagnostic counts/s, NOT calibrated world-frame velocities.
struct FlowHeight {
    enum Source {NONE=0,FLOW_RANGE=2};
    static constexpr uint32_t FRESH_MS=250;
    static constexpr uint32_t RECOVERY_MS=500;
    static constexpr unsigned WINDOW=24;
    static constexpr float MIN_RANGE_M=.04f,MAX_RANGE_M=3.f,MAX_TILT=.34906585f;
    static constexpr float HEIGHT_TAU_S=.08f,VELOCITY_TAU_S=.10f;
    static constexpr float MAX_REJOIN_ERROR_M=.30f;
    // Gate the raw measurement BEFORE it enters the median history. A rejected
    // spike must not bias the next otherwise good sample or its velocity fit.
    static constexpr float RANGE_JUMP_BASE_M=.08f,RANGE_JUMP_RATE_MPS=1.f;
    static constexpr uint32_t VELOCITY_WINDOW_MS=160;
    static constexpr unsigned MIN_QUALITY=20; // provisional optical-only threshold
    Source source=NONE;
    bool engaged=false,valid=false,rangeReady=false,rangeFresh=false,tiltGood=false;
    bool sequenceSeen=false,rejected=false,opticalValid=false,packetFresh=false,holding=false,recoveryHold=false;
    float height=0,vz=0,rangeHeight=0,rangeVz=0;
    float rateX=0,rateY=0,motionScore=0;
    uint32_t sampleTimeUs=0,lastSequence=0,lastSequenceAt=0,lastRangeAt=0,lastTrustedAt=0;
    uint32_t packetAge=UINT32_MAX,samplePeriodUs=0,windowSpanMs=0;
    uint8_t quality=0;
    unsigned count=0,index=0,rawCount=0,rawIndex=0,rejectedSamples=0;
    float heights[WINDOW]={},raw[3]={};
    uint32_t times[WINDOW]={};

    void resetRange(){count=index=rawCount=rawIndex=0;rangeReady=false;rangeVz=0;}
    static float median3(float a,float b,float c){return fmaxf(fminf(a,b),fminf(fmaxf(a,b),c));}
    bool observeRange(float value,uint32_t at){
        const uint32_t elapsed=uint32_t(at-lastRangeAt);
        if(count && elapsed>RECOVERY_MS)resetRange();
        const float jumpLimit=RANGE_JUMP_BASE_M+RANGE_JUMP_RATE_MPS*float(elapsed)*.001f;
        if(count && fabsf(value-rangeHeight)>jumpLimit){
            rejected=true;++rejectedSamples;return false;
        }
        raw[rawIndex]=value;rawIndex=(rawIndex+1)%3;if(rawCount<3)++rawCount;
        const float middle=rawCount==3?median3(raw[0],raw[1],raw[2]):value;
        if(count && fabsf(middle-rangeHeight)>jumpLimit){
            rejected=true;++rejectedSamples;return false;
        }
        rejected=false;
        const float dt=float(elapsed)*.001f;
        samplePeriodUs=count?elapsed*1000u:0;
        if(!count)rangeHeight=middle;
        else rangeHeight+=dt/(HEIGHT_TAU_S+dt)*(middle-rangeHeight);
        heights[index]=rangeHeight;times[index]=at;
        index=(index+1)%WINDOW;if(count<WINDOW)++count;
        lastRangeAt=at;
        const unsigned oldest=count<WINDOW?0:index;
        const uint32_t span=uint32_t(at-times[oldest]);windowSpanMs=span;
        if(count<5 || span<VELOCITY_WINDOW_MS){rangeReady=false;return true;}
        float st=0,sz=0,stt=0,stz=0;
        for(unsigned i=0;i<count;++i){
            const float t=-float(uint32_t(at-times[i]))*.001f;
            const float z=heights[i]-rangeHeight;
            st+=t;sz+=z;stt+=t*t;stz+=t*z;
        }
        const float denominator=float(count)*stt-st*st;
        const float slope=denominator>1.e-7f?(float(count)*stz-st*sz)/denominator:0.f;
        if(!isfinite(slope) || fabsf(slope)>.6f){rangeReady=false;return true;}
        if(!rangeReady)rangeVz=slope;
        else rangeVz+=dt/(VELOCITY_TAU_S+dt)*(slope-rangeVz);
        rangeReady=true;
        return true;
    }

    void update(uint32_t now,bool received,const FlowProtocol::Packet& p,uint32_t age,
                float roll,float pitch,float angularRate,bool disarmed=false){
        packetAge=received?age:UINT32_MAX;
        quality=received?p.quality:0;
        // BNO on this rig reports roll near pi when level. abs(cos roll)
        // accommodates that mounting convention; the deck must face DOWN.
        const float projection=fabsf(cosf(roll)*cosf(pitch));
        tiltGood=isfinite(roll) && isfinite(pitch) && projection>=cosf(MAX_TILT);
        packetFresh=received && p.magic==FlowProtocol::MAGIC && p.version==FlowProtocol::VERSION && age<=FRESH_MS;
        const bool restart=sequenceSeen && uint32_t(p.sequence-lastSequence)>=0x80000000u &&
            uint32_t(now-lastSequenceAt)>FlowProtocol::RX_TIMEOUT_MS;
        const bool newSample=packetFresh && (!sequenceSeen ||
            (p.sequence!=lastSequence && uint32_t(p.sequence-lastSequence)<0x80000000u) || restart);
        const bool inputRange=packetFresh && (p.flags&FlowProtocol::RANGE_VALID) && tiltGood &&
            float(p.range_mm)*.001f>=MIN_RANGE_M && float(p.range_mm)*.001f<=MAX_RANGE_M;
        if(newSample){
            const uint32_t at=now-age;
            const uint32_t gap=uint32_t(at-lastSequenceAt);
            const uint32_t step=uint32_t(p.sequence-lastSequence);
            if(restart)resetRange();
            opticalValid=sequenceSeen && step==1 && gap>=10 && gap<=200 &&
                (p.flags&FlowProtocol::FLOW_VALID) && quality>=MIN_QUALITY &&
                tiltGood && isfinite(angularRate) && fabsf(angularRate)<=.15f;
            if(opticalValid){
                const float dt=gap*.001f,a=dt/(.15f+dt);
                rateX+=a*(float(p.dx)/dt-rateX);rateY+=a*(float(p.dy)/dt-rateY);
            }else rateX=rateY=0;
            lastSequence=p.sequence;lastSequenceAt=at;sequenceSeen=true;
            if(inputRange)observeRange(float(p.range_mm)*.001f*projection,at);
        }
        opticalValid=opticalValid && packetFresh && uint32_t(now-lastSequenceAt)<=FRESH_MS;
        rangeFresh=inputRange && rangeReady && !rejected && uint32_t(now-lastRangeAt)<=FRESH_MS;
        // In flight, reject a contradictory return (e.g. a table under the
        // sensor). While disarmed, a newly qualified window may reacquire
        // absolute floor height after manually repositioning the blimp.
        // This changes neither the waypoint setpoint nor actuator arming.
        if(rangeFresh && engaged && source!=FLOW_RANGE && !disarmed &&
           fabsf(rangeHeight-height)>MAX_REJOIN_ERROR_M){
            rangeFresh=false;rejected=true;
        }
        motionScore=opticalValid && rangeFresh?rangeHeight*hypotf(rateX,rateY):0.f;
        if(rangeFresh){
            lastTrustedAt=lastRangeAt;
            height=rangeHeight;vz=rangeVz;source=FLOW_RANGE;valid=engaged=true;
            sampleTimeUs=lastRangeAt*1000u;holding=false;recoveryHold=false;
        }else if(engaged && tiltGood && (uint32_t(now-lastTrustedAt)<=FRESH_MS ||
            (packetFresh && uint32_t(now-lastSequenceAt)<=FRESH_MS &&
             uint32_t(now-lastTrustedAt)<=RECOVERY_MS))){
            // Briefly retain the last QUALIFIED Flowdeck sample across a lost
            // packet. Never refresh its time or substitute another sensor.
            source=FLOW_RANGE;valid=true;holding=true;
            // This is explicitly held, not a new measurement. Keep the old
            // sample timestamp and stop advance/arrival during extended hold.
            recoveryHold=uint32_t(now-lastTrustedAt)>FRESH_MS;
        }else{
            source=NONE;valid=false;vz=0;holding=false;recoveryHold=false;
        }
    }

    int flags() const {
        // Old barometer/fallback bits 16/32/2048 are permanently zero.
        return (packetFresh?1:0)|(rangeFresh?2:0)|(rangeReady?4:0)|(valid?8:0)|
            (rejected?64:0)|(opticalValid?128:0)|(quality==0?256:0)|
            (engaged?512:0)|(tiltGood?1024:0)|4096|(holding?8192:0)|(recoveryHold?16384:0);
    }
};
