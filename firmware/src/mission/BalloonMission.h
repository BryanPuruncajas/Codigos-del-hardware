#pragma once
#include "app/AppContext.h"

class BalloonMission {
public:
    enum State { SEARCH=0, APPROACH=1, VISIT_CONFIRM=2, ESCAPE=3, WAIT_TARGET_LOST=4, DONE=5 };
    void configure(int targetCount, bool stopAfterVisit=false);
    void reset(AppContext& ctx);
    void update(AppContext& ctx);
    State state() const { return state_; }
    int visited() const { return visited_; }
private:
    int targetCount_ = 1;
    bool stopAfterVisit_ = false;
    State state_ = SEARCH;
    int visited_ = 0;
    int closeFrames_ = 0;
    int lostFrames_ = 0;
    float yawRef_ = 0;
    float searchHeight_ = 0;
    float initialHeight_ = 0;
    float lastSearchYaw_ = 0;
    float searchYawAccum_ = 0;
    bool searchInitialized_ = false;
    bool altitudeCorrectionActive_ = false;
    unsigned long altitudePauseStartMs_ = 0;
    unsigned long stateStartMs_ = 0;
    unsigned long missionStartMs_ = 0;
    void enter(AppContext& ctx, State s);
    bool detected(const AppContext& ctx) const;
    float visitScore(const AppContext& ctx) const;
};