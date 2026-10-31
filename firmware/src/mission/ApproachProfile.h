#pragma once
#include <stdint.h>
#include <math.h>

// Tunable one-waypoint approach. These are commands and short-memory bounds,
// not measured horizontal speed, range or physical contact.
namespace ApproachProfile {
constexpr float FORWARD=.12f,TILT_DEG=28.f,FORWARD_SIGN=-1.f,FORECAST_S=.8f;
constexpr float ENTRY_YAW_DEG=12.f,CONTINUE_YAW_DEG=18.f;
constexpr float ENTRY_RATE=.18f,CONTINUE_RATE=.25f;
constexpr float ENTRY_X_PX=40.f,CONTINUE_X_PX=50.f;
constexpr float ENTRY_Y_PX=68.f,CONTINUE_Y_PX=74.f;
constexpr float ADVANCE_HEIGHT_M=.30f;
constexpr unsigned REQUIRED_SAMPLES=3;
constexpr uint32_t CONFIRM_MS=150,SETTLE_MS=300,PUSH_MS=1800,COAST_MS=900;
constexpr uint32_t RETURN_VECTOR_MS=1000,OCCLUSION_MS=800;
struct Memory {
    bool ready=false,heightReady=false;
    uint32_t lastSeen=0;
    uint8_t epoch=0;
    float lastHeight=0;
    void reset(){*this=Memory();}
    void observe(uint32_t now,uint8_t association,float height,bool heightHealthy){
        ready=true;lastSeen=now;epoch=association;
        heightReady=heightHealthy && isfinite(height);
        lastHeight=heightReady?height:0;
    }
    bool canContinue(uint32_t now,uint8_t association,bool link,bool ambiguous,
                     float height,bool heightHealthy,float vz) const {
        return ready && link && !ambiguous && association==epoch &&
            uint32_t(now-lastSeen)<=OCCLUSION_MS && heightReady && heightHealthy &&
            isfinite(height) && fabsf(height-lastHeight)<=.25f &&
            isfinite(vz) && fabsf(vz)<=.30f;
    }
};
}
