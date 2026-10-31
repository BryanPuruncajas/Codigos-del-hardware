#pragma once
#include "BlimpSwarm.h"
#include "robot/FullBicopter.h"
#include "comm/BaseCommunicator.h"
#include "util/DataTypes.h"
#include "app/AppConfig.h"

#include "mission/FlowHeight.h"

struct AppContext {
    FlowHeight flowHeight;
    FullBicopter* robot = nullptr;
    BaseCommunicator* comm = nullptr;
    float sensors[Robot::MAX_SENSORS] = {0};
    int sensorCount = 0;
    ControlInput command{};
    ControlInput behavior{};
    int mode = AppConfig::SAFE_STOP;
    bool resetRequested = false;
    unsigned long modeEnteredMs = 0;
    uint32_t lastCommandMs=0;
};
