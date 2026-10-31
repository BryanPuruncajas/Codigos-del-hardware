#pragma once
#include "app/AppContext.h"
#include "mission/AdvanceGate.h"
#include "mission/VisualHeight.h"
#include "app/VisualSupport.h"
#include "app/VisualBearing.h"
#include "app/SupportYaw.h"
#include "mission/ImpulseApproach.h"
#include "mission/VisualSearch.h"

class BalloonMission {
public:
    enum State { SEARCH=0, APPROACH=1, VISIT_CONFIRM=2, TURN_TO_NEXT=3, ACQUIRE_NEXT=4, DONE=5, TURN_FAILED=6 };
    void configure(int targetCount, bool stopAfterVisit=false, int track=1);
    void reset(AppContext& ctx);
    void update(AppContext& ctx);
    State state() const { return state_; }
    int visited() const { return visited_; }
    bool resumeFromManual(AppContext& ctx,uint32_t pausedMs);
    bool manualWaypointClear(AppContext& ctx);
    void reportPaused(AppContext& ctx,uint32_t pausedAt);
private:
    VisualSupport::Loop approachSupport_;
    VisualBearing approachBearing_;
    SupportYaw::Mixer approachMixer_;
    ControlCommon::YawState approachYaw_;
    uint32_t approachUs_=0,approachGeneration_=0;
    bool approachHadFull_=false;
    bool approachNearMemory_=false;
    uint8_t approachAssociationEpoch_=0;
    bool approachReacquiring_=false;
    uint32_t approachLastReset_=0,approachHeartbeat_=0;
    ImpulseApproach impulse_;
    ApproachProfile::Memory approachMemory_;
    VisualSearch initialSearch_;
    float approachBase_=AdaptiveLift::START_TRIM;
    uint32_t initialHeartbeat_=0,initialReset_=0;
    void updateN03Search(AppContext& ctx);
    void updateN03Approach(AppContext& ctx);
    VisualHeight visualHeight_;
    bool visualHeightSeen_ = false;
    uint32_t visualHeightLastSeenMs_ = 0;
    float visualHeightAnchor_ = 0;
    float visualHeightPower_ = 0;
    uint32_t visualHeightLastUs_ = 0;
    float recoveryYaw_ = 0;
    AdvanceGate advanceGate_;
    bool largeTargetRejected_ = false;
    uint32_t largeTargetRejectedMs_ = 0;
    void reportDecision(AppContext& ctx, int reason, float forwardRequest);
    int targetCount_ = 1;
    bool stopAfterVisit_ = false;
    State state_ = SEARCH;
    int visited_ = 0;
    int closeFrames_ = 0;
    int track_ = 1;
    float courseYaw_ = 0;
    float routeYawRef_ = 0;
    float lastVisitedArea_ = 0;
    unsigned long turnSettledMs_ = 0;
    bool turnSettled_ = false;
    uint32_t lastVisionGeneration_ = 0;
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
    void holdRouteYaw(AppContext& ctx, float altitudeDemand, float maxPower,
                      float minPower, float altitudeMax, float& servo1,
                      float& servo2, float& motorPower);
};
