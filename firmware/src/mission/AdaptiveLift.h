#pragma once
#include <math.h>

// Provisional vertical-speed adaptation. Power is a command fraction, not N.
// Slow, bounded learning; caller supplies fresh/coherent evidence and the
// effective limit after servo geometry. Never raises the configured ceiling.
namespace AdaptiveLift {
constexpr float HARD_MAX=.25f,START_TRIM=.18f,LEARN_RATE=.006f;
inline float learn(float trim,float error,float dt,float floor,float ceiling){
    if(!isfinite(trim)||!isfinite(error)||!isfinite(dt)||!isfinite(floor)||
       !isfinite(ceiling)||dt<=0||dt>.25f||floor>ceiling)return trim;
    const float e=fabsf(error)<=.02f?0.f:copysignf(fabsf(error)-.02f,error);
    const float delta=fmaxf(-LEARN_RATE,fminf(LEARN_RATE,.04f*e))*dt;
    return fmaxf(floor,fminf(ceiling,trim+delta));
}
}
