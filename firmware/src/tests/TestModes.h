#pragma once
#include "app/AppContext.h"

namespace TestModes {
void onModeEnter(AppContext& ctx, int newMode);
void run(AppContext& ctx);
void reset(AppContext& ctx);
bool resumePausedMission(AppContext& ctx,int mode);
bool completePausedWaypoint(AppContext& ctx);
}
