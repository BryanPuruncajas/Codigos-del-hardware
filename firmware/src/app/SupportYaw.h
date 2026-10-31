#pragma once
#include "app/ControlCommon.h"

// Experimental support/heading controller. Optional bounded forward tilt.
// Positive yaw command retains the existing physical LEFT convention.
namespace SupportYaw {
constexpr float SERVO_RATE = 30.f; // deg/s, provisional (mechanical range unchanged)
constexpr float MAX_AUTHORITY = .25f;
constexpr float MAX_RATE = .20f; // rad/s target, not a measured speed guarantee
// Provisional bias for current rig, measured at equal 15% motor power.
// Blend +.10 maps existing geometry 35/95 -> 31.5/85.5. Not a new vertical.
constexpr float VISUAL_TRIM_BLEND = .10f;
// Base-pose adjustment validated in a separate 15% open-loop trial:
// 31.5/85.5 -> 28.5/88.5. Does not redefine vertical or global geometry.
constexpr float VISUAL_TRANSLATION_TRIM_DEG = 3.f;
// Keep the previous power/yaw calibration as a reference. That pose now has
// user-confirmed backward drift. Apply fore/aft correction in EVERY support
// phase, rather than briefly compensating it with a forward impulse.
constexpr float VISUAL_REFERENCE1_DEG=48.5f,VISUAL_REFERENCE2_DEG=68.5f;
constexpr float FORE_AFT_CORRECTION_DEG=4.f;
constexpr float VISUAL_BASE1_DEG=VISUAL_REFERENCE1_DEG+FORE_AFT_CORRECTION_DEG;
constexpr float VISUAL_BASE2_DEG=VISUAL_REFERENCE2_DEG-FORE_AFT_CORRECTION_DEG;
inline void calibratedYaw(float blend,float& s1,float& s2){
    // Equal physical angle increments retain the calibrated fore/aft offset.
    const float travel=blend>=0 ? fminf(VISUAL_REFERENCE1_DEG,VISUAL_REFERENCE2_DEG) :
        fminf(120.f-VISUAL_REFERENCE1_DEG,120.f-VISUAL_REFERENCE2_DEG);
    const float delta=constrain(blend,-MAX_AUTHORITY,MAX_AUTHORITY)*travel;
    s1=VISUAL_BASE1_DEG-delta;s2=VISUAL_BASE2_DEG-delta;
}

inline float command(const ControlCommon::YawGains& g,
                     const ControlCommon::YawLimits& lim,
                     ControlCommon::YawState& state,
                     float error, float rate, float dt, bool learnIntegral=true,
                     float feedForward=0.f) {
    if (!isfinite(error) || !isfinite(rate) || !isfinite(dt) || !isfinite(feedForward) || dt<=0) return 0;
    // Soft angle deadband; rate damping and learned trim remain active INSIDE
    // it. Otherwise the persistent right-hand disturbance is never cancelled.
    const float a=fabsf(error);
    const float effective=a<=lim.deadbandRad ? 0.f : copysignf(a-lim.deadbandRad,error);
    const float desired=constrain(g.kp*effective,-MAX_RATE,MAX_RATE);
    const float e=desired-rate;
    const float bias=constrain(feedForward,-1.f,1.f);
    float raw=g.kd*e+state.integral+bias;
    const bool saturated=(raw>=1 && e>0)||(raw<=-1 && e<0);
    if (learnIntegral && !saturated && fabsf(e)>.005f) {
        state.integral=constrain(state.integral+g.ki*e*dt,-.35f,.35f);
        raw=g.kd*e+state.integral+bias;
    }
    return constrain(raw,-1.f,1.f);
}

struct Mixer {
    float servo1=ControlCommon::SERVO1_Z_DEG;
    float servo2=ControlCommon::SERVO2_Z_DEG;
    float power=0,etaZ=1,forwardAngle=0;
    void reset(bool calibrated=false){
        *this=Mixer();
        if(calibrated){servo1=VISUAL_BASE1_DEG;servo2=VISUAL_BASE2_DEG;}
    }
    void update(float support,float yawCommand,float authority,float yawMax,
                float ceiling,float dt,float motorFloor=0.f,float translationTrimDeg=0.f,
                float forwardDemand=0.f,bool calibrated=false,float forwardAngleLimit=12.f,
                bool compensateLift=true,float forwardDirection=1.f) {
        if (!isfinite(support)||!isfinite(yawCommand)||!isfinite(authority)||
            !isfinite(yawMax)||!isfinite(ceiling)||!isfinite(dt)||!isfinite(motorFloor)||
            !isfinite(translationTrimDeg)||!isfinite(forwardDemand)||!isfinite(forwardAngleLimit)||
            !isfinite(forwardDirection)||fabsf(forwardDirection)!=1.f||
            dt<=0||ceiling<=0) {
            power=0; forwardAngle=0; return;
        }
        const float auth=constrain(authority,0.f,MAX_AUTHORITY);
        const float blend=constrain(yawCommand,-1.f,1.f)*auth;
        float target1,target2;
        if(calibrated)calibratedYaw(blend,target1,target2);
        else ControlCommon::servosFromBlend(blend,target1,target2);
        if(!calibrated && translationTrimDeg!=0.f){
            float left1,left2,right1,right2;
            ControlCommon::servosFromBlend(auth,left1,left2);
            ControlCommon::servosFromBlend(-auth,right1,right2);
            // Keep the original authority envelope, including at PID saturation.
            // This calibrates the base pose; it is not position/velocity control.
            target1=constrain(target1-translationTrimDeg,left1,right1);
            target2=constrain(target2+translationTrimDeg,left2,right2);
        }
        // Default convention: S1 toward120, S2 toward0. A caller can reverse
        // that sign for the actual installation, preserving the neutral pose.
        // Extended attack is opt-in and requires exclusive forward vectoring.
        // Legacy callers keep their12deg/.03 envelope; yaw retains priority.
        const bool extendedForward=calibrated && fabsf(yawCommand)<1.e-5f && forwardAngleLimit>12.f;
        const float fx=constrain(forwardDemand,0.f,extendedForward?.12f:.03f);
        const bool inertialCoast=calibrated && !compensateLift && fabsf(yawCommand)<1.e-5f;
        const float angleLimit=extendedForward?constrain(forwardAngleLimit,0.f,28.f):12.f;
        const float requestedAngle=fx>0 ? fminf(angleLimit,atan2f(fx,fmaxf(.01f,support))*ControlCommon::RAD2DEG) : 0.f;
        forwardAngle=fx>0 ? ControlCommon::rateLimit(requestedAngle,forwardAngle,
            angleLimit>12.f?30.f:12.f,dt) : 0.f;
        if(forwardAngle>0){
            float left1,left2,right1,right2;
            if(calibrated){calibratedYaw(auth,left1,left2);calibratedYaw(-auth,right1,right2);}
            else {ControlCommon::servosFromBlend(auth,left1,left2);ControlCommon::servosFromBlend(-auth,right1,right2);}
            // Yaw has priority. Forward uses common angular headroom, avoiding
            // asymmetric clipping that would change the requested yaw torque.
            const float headroom=forwardDirection>0 ?
                (extendedForward?fminf(120.f-target1,target2):fminf(right1-target1,target2-left2)):
                (extendedForward?fminf(target1,120.f-target2):fminf(target1-left1,right2-target2));
            forwardAngle=fminf(forwardAngle,fmaxf(0,headroom));
            target1+=forwardDirection*forwardAngle;target2-=forwardDirection*forwardAngle;
        }
        servo1=constrain(ControlCommon::rateLimit(target1,servo1,SERVO_RATE,dt),0.f,120.f);
        servo2=constrain(ControlCommon::rateLimit(target2,servo2,SERVO_RATE,dt),0.f,120.f);
        float etaYaw;
        // Compensate using the SLEW-LIMITED angles actually commanded, not the
        // future requested angles. The existing model is not a thrust sensor.
        ControlCommon::computeEfficiencies(servo1,servo2,etaZ,etaYaw);
        if(calibrated){
            float baseEta,unused;
            ControlCommon::computeEfficiencies(VISUAL_REFERENCE1_DEG,VISUAL_REFERENCE2_DEG,baseEta,unused);
            // Retain the old lift reference so the fore/aft compensation does
            // not silently discard its vertical component. Same16% ceiling.
            etaZ/=fmaxf(ControlCommon::ETA_Z_FLOOR,baseEta);
        }
        // While arresting ascent, forward tilt redirects part of the existing
        // thrust. Do not increase RPM to restore the lift being removed.
        const float pAlt=fmaxf(0,support)/(inertialCoast?1.f:fmaxf(etaZ,ControlCommon::ETA_Z_FLOOR));
        const float pYaw=fmaxf(0,yawMax)*fabsf(constrain(yawCommand,-1.f,1.f));
        // Running descent floor only; startup still ramps from zero.
        // STOP/faults bypass this mixer and command zero immediately.
        const float floor=constrain(motorFloor,0.f,ceiling);
        const float requested=constrain(fmaxf(fmaxf(pAlt,pYaw),floor),0.f,ceiling);
        power=constrain(ControlCommon::rateLimit(requested,power,ControlCommon::DEF_ALT_SLEW,dt),0.f,ceiling);
    }
};
}
