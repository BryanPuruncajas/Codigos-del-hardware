#pragma once
#include "app/AppContext.h"
extern int reason;extern float forward;
namespace Telemetry {
inline void sendCore(AppContext&){}
inline void sendNicla(AppContext&){}
inline void sendNiclaStatus(AppContext&){}
inline void sendActuators(AppContext&){}
inline void sendFlow(AppContext&){}
inline void sendFlowControl(AppContext&){}
inline void sendFixedHeight(AppContext&,float,float,float,float,int,float){}
inline void sendHeightReference(AppContext&,float,float,float,float,float,int){}
inline void sendInertia(AppContext&,float,float,float,float,int,float=0){}
static int impulsePhase=0;
static float impulseYaw=0,impulseForward=0;
inline void sendImpulse(AppContext&,int phase,float,float f,float yaw,float,float){
 impulsePhase=phase;impulseYaw=yaw;impulseForward=f;
}
inline void sendVisualBearing(AppContext&,float,float,float,bool,int,float f=0){forward=f;}
inline void sendVisualSupport(AppContext&,float,float,float,float,float,int){}
inline void sendMissionDecision(AppContext&,int r,float f,float,int,float,bool){reason=r;forward=f;}
inline void sendControl(AppContext&,float,float,float){}
inline void sendMission(AppContext&,int,int,int,float,float,float){}
}
