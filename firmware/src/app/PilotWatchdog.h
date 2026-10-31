#pragma once
#include <stdint.h>
namespace PilotWatchdog {
constexpr uint32_t TIMEOUT_MS=750;
inline bool expired(uint32_t now,uint32_t lastCommand,bool optedIn,bool armed){
    return optedIn && armed && uint32_t(now-lastCommand)>TIMEOUT_MS;
}
}
