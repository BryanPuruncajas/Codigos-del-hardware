#pragma once
#include "app/AppContext.h"

namespace Telemetry {
void sendCore(AppContext& ctx);
void sendNicla(AppContext& ctx);
void sendActuators(AppContext& ctx);
void sendControl(AppContext& ctx, float yawRef, float heightRef, float fxCmd);
void sendMission(AppContext& ctx, int state, int visited, int targetCount, float searchHeight, float visitScore, float elapsedSec);
}
