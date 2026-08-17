#pragma once
#include "app/AppContext.h"

namespace TestModes {
void onModeEnter(AppContext& ctx, int newMode);
void run(AppContext& ctx);
void reset(AppContext& ctx);
}
