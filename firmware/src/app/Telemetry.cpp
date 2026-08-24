#include "app/Telemetry.h"
#include "app/SensorMap.h"

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
void sendCore(AppContext& ctx) {
    send(ctx.comm, 1,
        ctx.sensors[SensorMap::ALTITUDE], ctx.sensors[SensorMap::YAW],
        ctx.sensors[SensorMap::ROLL], ctx.sensors[SensorMap::PITCH],
        ctx.sensors[SensorMap::BATTERY], ctx.sensors[SensorMap::VERTICAL_VELOCITY]);
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
void sendActuators(AppContext& ctx) {
    send(ctx.comm, 3,
        ctx.robot ? ctx.robot->servo_old1 : 0,
        ctx.robot ? ctx.robot->servo_old2 : 0,
        ctx.robot ? ctx.robot->motor_power1 : 0,
        ctx.robot ? ctx.robot->motor_power2 : 0,
        (ctx.robot && ctx.robot->actuatorsAreArmed()) ? 1.0f : 0.0f,
        (float)ctx.mode);
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