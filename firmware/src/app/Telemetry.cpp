#include "app/Telemetry.h"
#include "sense/VerticalProfile.h"
#include "mission/VerticalMotionEvidence.h"
#include "app/SensorMap.h"
#include "comm/LLC_ESPNow.h"

namespace {
void send(BaseCommunicator* comm, int flag, float a, float b, float c, float d, float e, float f) {
    if (!comm) return;
    ReceivedData msg{};
    msg.flag = flag;
    msg.values[0]=a; msg.values[1]=b; msg.values[2]=c;
    msg.values[3]=d; msg.values[4]=e; msg.values[5]=f;
    comm->sendMeasurements(&msg);
}
}

namespace Telemetry {
void sendFlowControl(AppContext& ctx){
    static uint32_t lastMs=0;const uint32_t now=millis();
    if(uint32_t(now-lastMs)<200)return;lastMs=now;
    const auto& f=ctx.flowHeight;
    send(ctx.comm,18,f.height,f.vz,float(f.source),f.rangeHeight,f.rangeVz,float(f.flags()));
    send(ctx.comm,19,f.opticalValid?f.rateX:0.f,f.opticalValid?f.rateY:0.f,f.motionScore,
        f.packetAge==UINT32_MAX?-1.f:float(f.packetAge),float(f.quality),float(f.lastSequence));
}
void sendFixedHeight(AppContext& ctx,float target,float estimate,float bias,float offset,int flags,float pitch){
    static uint32_t lastMs=0;const uint32_t now=millis();
    if(uint32_t(now-lastMs)<200)return;
    lastMs=now;send(ctx.comm,17,target,estimate,bias,offset,float(flags),pitch);
}
void sendHeightReference(AppContext& ctx,float reference,float error,float ageMs,float drift,float noise,int flags){
    static uint32_t lastMs=0;const uint32_t now=millis();
    if(uint32_t(now-lastMs)<200)return;
    lastMs=now;send(ctx.comm,16,reference,error,ageMs,drift,noise,float(flags));
}
void sendInertia(AppContext& ctx,float rate,float predicted,float desired,float floor,int flags,float confidence){
    static uint32_t lastMs=0;const uint32_t now=millis();
    if(uint32_t(now-lastMs)<200)return;
    lastMs=now;send(ctx.comm,13,rate,predicted,desired,floor,float(flags),confidence);
}
void sendImpulse(AppContext& ctx,int phase,float elapsed,float forward,float yaw,float pulses,float pulseMs){
    static uint32_t lastMs=0;const uint32_t now=millis();
    if(uint32_t(now-lastMs)<200)return;
    lastMs=now;send(ctx.comm,12,float(phase),elapsed,forward,yaw,pulses,pulseMs);
}
void sendVisualBearing(AppContext& ctx,float x,float reference,float error,bool active,int version,float forwardRequest){
    static uint32_t lastMs=0;const uint32_t now=millis();
    if(uint32_t(now-lastMs)<200)return;
    lastMs=now;
    send(ctx.comm,11,x,reference,error,active?1.f:0.f,float(version),forwardRequest);
}
void sendVisualSupport(AppContext& ctx,float y,float error,float trim,float demand,float yawCommand,int phase){
    static uint32_t lastMs=0;
    const uint32_t now=millis();
    if(uint32_t(now-lastMs)<200)return;
    lastMs=now;
    send(ctx.comm,10,y,error,trim,demand,yawCommand,float(phase));
}
void sendMissionDecision(AppContext& ctx, int reason, float forwardRequest, float yawErrorDeg, int waypoint, float stableMs, bool bench) {
    static uint32_t lastMs = 0;
    const uint32_t now = millis();
    if (uint32_t(now - lastMs) < 200) return;
    lastMs = now;
    send(ctx.comm, 8, float(reason), forwardRequest, yawErrorDeg, float(waypoint), stableMs, bench ? 1.0f : 0.0f);
}
void sendCore(AppContext& ctx) {
    send(ctx.comm, 1,
        ctx.sensors[SensorMap::ALTITUDE], ctx.sensors[SensorMap::YAW],
        ctx.sensors[SensorMap::ROLL], ctx.sensors[SensorMap::PITCH],
        ctx.sensors[SensorMap::BATTERY], ctx.sensors[SensorMap::VERTICAL_VELOCITY]);
    static uint32_t lastVerticalMs=0;const uint32_t now=millis();
    if(uint32_t(now-lastVerticalMs)>=200){
        lastVerticalMs=now;const auto& f=ctx.flowHeight;
        // F9/F15 pressure diagnostics are intentionally no longer sent.
        send(ctx.comm,14,float(f.samplePeriodUs)*.001f,float(f.windowSpanMs),1.f,
            f.valid?f.vz:0.f,f.valid?float(uint32_t(now-f.lastTrustedAt)):-1.f,40.f);
    }
}
void sendNicla(AppContext& ctx) {
    // La Nicla manda DOS pares de coordenadas por trama:
    //
    //   NICLA_X / Y / W / H              -> ROI de SEGUIMIENTO
    //   NICLA_X_VALUE / Y_ / W_ / H_     -> DETECCION real del objeto
    //
    // El ROI es la ventana de busqueda del tracker: crece y encoge por su
    // propia logica, es cuadrada por construccion (por eso W y H salian
    // siempre identicos) y no representa al globo. Medido el 23/08 con el
    // globo INMOVIL: NICLA_W recorria de 38 a 219 px y NICLA_X de 5 a 132.
    //
    // Aqui se envian los valores de DETECCION, que es lo que hay que usar
    // para centrar (P07) y para decidir si un globo esta visitado (P08+).
    send(ctx.comm, 2,
        ctx.sensors[SensorMap::NICLA_FLAG], ctx.sensors[SensorMap::NICLA_X_VALUE],
        ctx.sensors[SensorMap::NICLA_Y_VALUE], ctx.sensors[SensorMap::NICLA_W_VALUE],
        ctx.sensors[SensorMap::NICLA_H_VALUE], ctx.sensors[SensorMap::NICLA_DISTANCE]);
}
void sendNiclaStatus(AppContext& ctx) {
    if (!ctx.robot) return;
    static uint32_t lastMs = 0;
    const uint32_t now = millis();
    if ((uint32_t)(now-lastMs) < 200) return;
    lastMs = now;
    const auto& f = ctx.robot->sensorsuite.niclaFeedback();
    send(ctx.comm, 7, f.fresh ? 1.0f : 0.0f, (float)f.state,
         (float)f.sample, (float)f.protocol, f.resetPending ? 1.0f : 0.0f,
         (float)f.resetAck);
}
void sendActuators(AppContext& ctx) {
    send(ctx.comm, 3,
        ctx.robot ? ctx.robot->servo_old1 : 0,
        ctx.robot ? ctx.robot->servo_old2 : 0,
        ctx.robot ? ctx.robot->motor_power1 : 0,
        ctx.robot ? ctx.robot->motor_power2 : 0,
        (ctx.robot && ctx.robot->actuatorsAreArmed()) ? 1.0f : 0.0f,
        (float)ctx.mode);
}
void sendFlow(AppContext& ctx) {
    FlowProtocol::Packet packet{};
    uint32_t ageMs = 0;
    const bool received = readLatestFlowPacket(packet, ageMs);

    // F6 conserva los datos del enlace Flow sin mezclarlos con los indices
    // historicos de sensores ni con F1-F5.
    send(ctx.comm, 6,
        received ? (float)packet.dx : 0.0f,
        received ? (float)packet.dy : 0.0f,
        received ? (float)packet.range_mm : 0.0f,
        received ? (float)packet.quality : 0.0f,
        received ? (float)ageMs : -1.0f,
        received ? (float)packet.flags : 0.0f);
}
void sendControl(AppContext& ctx, float yawRef, float heightRef, float fxCmd) {
    send(ctx.comm, 4,
        ctx.sensors[SensorMap::ROLL_RATE], ctx.sensors[SensorMap::PITCH_RATE],
        ctx.sensors[SensorMap::YAW_RATE], yawRef, heightRef, fxCmd);
}
void sendMission(AppContext& ctx, int state, int visited, int targetCount, float searchHeight, float visitScore, float elapsedSec) {
    send(ctx.comm, 5, (float)state, (float)visited, (float)targetCount, searchHeight, visitScore, elapsedSec);
}
}
