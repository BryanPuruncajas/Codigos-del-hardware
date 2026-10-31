#pragma once
#include "app/AppContext.h"

namespace Telemetry {
void sendFlowControl(AppContext&);
void sendFixedHeight(AppContext&,float,float,float,float,int,float);
void sendHeightReference(AppContext& ctx,float reference,float error,float ageMs,float drift,float noise,int flags);
void sendInertia(AppContext& ctx,float rate,float predicted,float desired,float floor,int flags,float confidence=0.f);
void sendImpulse(AppContext& ctx,int phase,float elapsed,float forward,float yaw,float pulses,float pulseMs);
void sendVisualBearing(AppContext& ctx,float x,float reference,float error,bool active,int version,float forwardRequest=0.f);
void sendVisualSupport(AppContext& ctx,float y,float error,float trim,float demand,float yawCommand,int phase);
void sendMissionDecision(AppContext& ctx, int reason, float forwardRequest, float yawErrorDeg, int waypoint, float stableMs, bool bench);
void sendCore(AppContext& ctx);
void sendNicla(AppContext& ctx);
void sendNiclaStatus(AppContext& ctx);
void sendActuators(AppContext& ctx);
void sendFlow(AppContext& ctx);
void sendControl(AppContext& ctx, float yawRef, float heightRef, float fxCmd);
void sendMission(AppContext& ctx, int state, int visited, int targetCount, float searchHeight, float visitScore, float elapsedSec);
}
