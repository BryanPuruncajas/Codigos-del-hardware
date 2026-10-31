#pragma once
#include <math.h>
#include <stdint.h>

// HQVGA, upright camera: y grows DOWN. Gains are image-space, NOT P04 metres.
// No ranging assumption, no automatic contact measurement. Experimental tuning.
struct VisualHeight {
    float trim = .10f, demand = 0, derivative = 0, lastError = 0;
    uint32_t lastMs = 0;
    bool primed = false, lastReliable = false;
    static bool centered(float y) { return isfinite(y) && fabsf(y-80.f)<=18.f; }
    void reset() { *this = VisualHeight(); }
    float update(uint32_t now, float y, float h, float ceiling, bool full, float integralFloor=0.f,
                 float leadSeconds=0.f,bool speedControl=false,bool allowPartialClimb=false,
                 bool freezeIntegral=false) {
        if (!isfinite(y) || !isfinite(h) || y<0 || y>=160 || h<=0 ||
            !isfinite(ceiling) || ceiling<=0 || !isfinite(integralFloor) ||
            !isfinite(leadSeconds) || leadSeconds<0) return demand;
        integralFloor=fmaxf(0,fminf(ceiling,integralFloor));
        const float error=(80.f-y)/80.f;
        trim=fmaxf(0,fminf(ceiling,trim));
        float dt=primed ? uint32_t(now-lastMs)*.001f : .1f;
        const bool continuous=primed && dt>0 && dt<=.5f;
        if (!continuous) { dt=.1f; derivative=0; }
        // A clipped bounding-box centre can move without the balloon moving.
        const bool topClipped=y-h*.5f<=2;
        const bool bottomClipped=y+h*.5f>=158;
        const bool reliable=full && !topClipped && !bottomClipped;
        if (reliable) {
            const float rawD=continuous && lastReliable ? (error-lastError)/dt : 0;
            derivative=.7f*derivative+.3f*fmaxf(-2.f,fminf(2.f,rawD));
            // Optional image-space prediction: reduce lift BEFORE crossing the
            // centre when y is already moving down (blimp rising). Default0
            // preserves existing mission behaviour. No distance/speed in metres.
            const float projected=fmaxf(-1.f,fminf(1.f,error+fminf(1.f,leadSeconds)*derivative));
            if(speedControl){
                // Bounded desired image-space speed; PI learns support rather
                // than holding the initial15% when motion should stop.
                const float desiredUp=fabsf(projected)<.10f ? 0.f :
                    fmaxf(-.10f,fminf(.10f,.25f*projected));
                const float rateError=derivative+desiredUp;
                const float raw=trim+.18f*rateError;
                if(!freezeIntegral && !((raw>=ceiling && rateError>0) || (raw<=integralFloor && rateError<0)))
                    trim=fmaxf(integralFloor,fminf(ceiling,trim+.04f*rateError*dt));
                demand=fmaxf(0,fminf(ceiling,trim+.18f*rateError));
                lastMs=now;lastError=error;primed=true;lastReliable=true;
                return demand;
            }
            const float e=fabsf(projected)<.10f ? 0 : projected;
            const float raw=trim+.06f*e+.015f*derivative;
            // With prediction, learn hover only during slow image movement;
            // transient ascent/descent should not be mistaken for hover error.
            if (!freezeIntegral && (leadSeconds==0 || fabsf(derivative)<.12f) &&
                !((raw>=ceiling && e>0) || (raw<=integralFloor && e<0)))
                trim=fmaxf(0,fminf(ceiling,trim+.008f*e*dt));
            demand=fmaxf(0,fminf(ceiling,trim+.06f*e+.015f*derivative));
        } else {
            derivative=0;
            // Single clipped edge still tells us WHICH way to recentre.
            // No I/D from a truncated centre; both edges clipped is ambiguous.
            const float edgeError=topClipped && !bottomClipped ? fmaxf(.25f,error) :
                (bottomClipped && !topClipped ? fminf(-.25f,error) :
                 (speedControl && !topClipped && !bottomClipped ? error : 0.f));
            demand=fmaxf(0,fminf(ceiling,trim+.06f*edgeError));
            // Diagnostic default keeps the historical no-boost rule. The
            // one-waypoint mission opts into bounded directional P (no I/D).
            if(speedControl && !allowPartialClimb)demand=fminf(trim,demand);
        }
        lastMs=now; lastError=error; primed=true; lastReliable=reliable;
        return demand;
    }
    // Finite search about last observation; offset never accumulates.
    static float searchOffset(uint32_t elapsed) {
        return .20f*sinf(float(elapsed)*.001f*6.2831853f/6.f);
    }
};
