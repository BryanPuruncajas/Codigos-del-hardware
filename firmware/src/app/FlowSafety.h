#pragma once
#include "app/AppConfig.h"

// Direct manual/raw tests remain available. Every automatic mode that applies
// lift or uses a height setpoint requires a qualified Flowdeck range.
inline bool flowControlRequired(int mode){
    return mode==AppConfig::P03_YAW_CONTROL || mode==AppConfig::P04_ALTITUDE_CONTROL ||
        mode==AppConfig::P05_YAW_ALTITUDE || mode==AppConfig::P07_VISUAL_CENTER ||
        mode==AppConfig::P08_ONE_BALLOON || mode==AppConfig::P09_VISIT_ESCAPE ||
        mode==AppConfig::P10_TWO_BALLOONS || mode==AppConfig::P11_FOUR_BALLOONS ||
        mode==AppConfig::P11_INFINITY;
}
