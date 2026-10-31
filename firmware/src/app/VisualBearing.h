#pragma once
#include <math.h>
#include <stdint.h>

// Same x/yaw sign and approximate 70deg FOV used by BalloonMission.
// Coordinates are centroids, not range or object identities.
struct VisualBearing {
    static constexpr int VERSION=42; // Flow41 recovery + Xbox pause/resume/manual waypoint acknowledgement
    static constexpr float PI_F=3.14159265358979323846f;
    bool ready=false;
    uint32_t lastMs=0;
    float reference=0,lastX=-1;
    uint32_t lastNearMs=0;
    bool nearSeen=false;
    static float wrap(float x){return atan2f(sinf(x),cosf(x));}
    bool updateNear(float left,float width,uint32_t now){
        if(!ready || !isfinite(left) || !isfinite(width) || width<=0)return false;
        const float dt=nearSeen?fminf(.1f,uint32_t(now-lastNearMs)*.001f):0.f;
        nearSeen=true;lastNearMs=now;
        // Freeze when the coloured region covers the image centre. Otherwise
        // use only the known SIDE, bounded3deg/s, never its cropped centroid.
        const int direction=left+width<120?1:(left>120?-1:0);
        reference=wrap(reference+direction*(3.f*PI_F/180.f)*dt);
        return true;
    }
    bool update(float yaw,float x,uint32_t now){
        if(!isfinite(yaw)||!isfinite(x)||x<0||x>=240)return false;
        nearSeen=false;
        const float raw=wrap(yaw-(x-120.f)/240.f*(70.f*PI_F/180.f));
        if(!ready)reference=raw; // first target bearing, not arbitrary initial yaw
        else{
            const float dt=fminf(.5f,uint32_t(now-lastMs)*.001f);
            const float alpha=dt/(.35f+dt);
            const float step=wrap(raw-reference)*alpha;
            const float limit=.35f*dt; // provisional reference slew 20deg/s
            reference=wrap(reference+fmaxf(-limit,fminf(limit,step)));
        }
        lastMs=now;lastX=x;ready=true;return true;
    }
};
