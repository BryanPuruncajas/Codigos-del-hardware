#pragma once
#include <math.h>
#include <stdint.h>

// Optional extension in the existing AUX4 float; all integers are exactly
// representable in float32. Legacy values remain a pixel deadband.
struct MissionHeightOptions {
    bool valid=true,released=true,advance=true;
    float offset=0,levelPitch=0,deadband=24;
    static MissionHeightOptions decode(float raw){
        MissionHeightOptions o;
        if(isfinite(raw) && raw>=1 && raw<=100){o.deadband=raw;return o;}
        if(raw==0)return o; // legacy default
        if(!isfinite(raw)||raw<8388608.f||raw>16777215.f||floorf(raw)!=raw){
            o.valid=o.released=o.advance=false;return o;
        }
        const uint32_t bits=uint32_t(raw);
        const unsigned cm=(bits>>2)&127u,db=(bits>>9)&127u,pitch=(bits>>16)&127u;
        if(cm>100 || db<1 || db>100 || pitch>120){
            o.valid=o.released=o.advance=false;return o;
        }
        o.released=bits&1;o.advance=bits&2;o.offset=(int(cm)-50)*.01f;
        o.deadband=float(db);o.levelPitch=(float(pitch)*.5f-30.f)*.01745329252f;
        return o;
    }
};
