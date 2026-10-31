#include <Arduino.h>
#include "BlimpSwarm.h"
#include "comm/BaseCommunicator.h"
#include "comm/LLC_ESPNow.h"
#include "state/nicla/NiclaConfig.h"
#include "app/AppConfig.h"
#include "app/AppContext.h"
#include "app/HardwareSafety.h"
#include "tests/TestModes.h"
#include "sense/VerticalProfile.h"
#include "app/PilotWatchdog.h"
#include "app/SensorMap.h"

AppContext app;
unsigned long loopClock=0;

void updateFlowSensors() {
    FlowProtocol::Packet packet{};uint32_t ageMs=0;
    const bool received=readLatestFlowPacket(packet,ageMs);
    const float angular=fmaxf(fabsf(app.sensors[SensorMap::ROLL_RATE]),
        fmaxf(fabsf(app.sensors[SensorMap::PITCH_RATE]),fabsf(app.sensors[SensorMap::YAW_RATE])));
    app.flowHeight.update(millis(),received,packet,ageMs,
        app.sensors[SensorMap::ROLL],app.sensors[SensorMap::PITCH],angular,
        !app.robot->actuatorsAreArmed());
    const auto& f=app.flowHeight;
    const VerticalDiagnostics d{f.rangeHeight,f.height,f.rangeVz,f.vz,
        f.valid?uint32_t(millis()-f.lastTrustedAt):UINT32_MAX,f.valid,
        f.sampleTimeUs,f.samplePeriodUs,f.windowSpanMs};
    app.robot->sensorsuite.publishFlowVertical(d);
    app.sensors[SensorMap::TEMPERATURE]=0;
    app.sensors[SensorMap::ALTITUDE]=f.valid?f.height:0.f;
    app.sensors[SensorMap::VERTICAL_VELOCITY]=f.valid?f.vz:0.f;
}

void reportFlowLink() {
    static uint32_t lastReportMs = 0;
    const uint32_t now = millis();
    if (now - lastReportMs < 500) return;
    lastReportMs = now;

    FlowProtocol::Packet packet{};
    uint32_t ageMs = 0;
    if (!readLatestFlowPacket(packet, ageMs)) {
        Serial.println("[FLOW] esperando primer paquete ESP-NOW");
        return;
    }

    const bool fresh = ageMs <= FlowProtocol::RX_TIMEOUT_MS;
    const bool flowValid = (packet.flags & FlowProtocol::FLOW_VALID) != 0;
    const bool rangeValid = (packet.flags & FlowProtocol::RANGE_VALID) != 0;
    Serial.printf("[FLOW] %s seq=%lu age=%lu ms dx=%d dy=%d range=%u mm quality=%u flowValid=%u rangeValid=%u\n",
                  fresh ? "FRESCO" : "VENCIDO",
                  (unsigned long)packet.sequence,
                  (unsigned long)ageMs,
                  packet.dx,
                  packet.dy,
                  packet.range_mm,
                  packet.quality,
                  flowValid,
                  rangeValid);
}

void reloadPreferences() {
    NiclaConfig::getInstance()->loadConfiguration();
    // Este proyecto solo usa globos: selecciona explícitamente parámetros balloon.
    NiclaConfig::getInstance()->getDynamicHistory()->nicla_desired = 0;
    app.robot->getPreferences();
    app.comm->setMainBaseStation();
}

void processCommands() {
    if (!app.comm->isNewMsgCmd()) return;
    app.command=app.comm->receiveMsgCmd();

    const int requestedMode=(int)app.command.params[AppConfig::PARAM_MODE];
    const float armCmd=app.command.params[AppConfig::PARAM_ARM];
    const int special=(int)app.command.params[AppConfig::PARAM_RESET];
    if(special==2){
        if(!TestModes::resumePausedMission(app,requestedMode)){
            app.robot->setActuatorsArmed(false);TestModes::onModeEnter(app,AppConfig::SAFE_STOP);
        }
        app.lastCommandMs=millis();return;
    }
    if(special==3){
        if(requestedMode==AppConfig::P02_MANUAL_CONTROL && armCmd==0)
            TestModes::completePausedWaypoint(app);
        app.lastCommandMs=millis();return;
    }
    if (armCmd > 0.5f) {
        if (requestedMode==AppConfig::P12_VISION_BENCH) {
            app.robot->setActuatorsArmed(false);
            Serial.println("[SAFE] ARM IGNORADO: banco visual exclusivamente desarmado.");
        } else if (requestedMode==AppConfig::P00_SERVO_CALIBRATION) {
            Serial.println("[SAFE] ARM IGNORADO: P00 permite SOLO servos; brushless bloqueados.");
        } else {
            app.robot->setActuatorsArmed(true);
            Serial.println("[SAFE] ACTUADORES ARMADOS por comando explicito.");
        }
    } else if (armCmd < -0.5f) {
        app.robot->setActuatorsArmed(false);
        Serial.println("[SAFE] ACTUADORES DESARMADOS.");
    }

    if ((int)app.command.params[AppConfig::PARAM_RELOAD] == 1) reloadPreferences();
    if ((int)app.command.params[AppConfig::PARAM_RESET] == 1) TestModes::reset(app);

    int requested=requestedMode;
    if (requested != app.mode) {
        // Entrar a SAFE_STOP desarma físicamente las salidas PWM.
        if (requested==AppConfig::SAFE_STOP) app.robot->setActuatorsArmed(false);
        TestModes::onModeEnter(app,requested);
        Serial.printf("[MODE] %d\n",requested);
    }
    // ESC arming blocks ~3.7 s. Start the pilot deadline AFTER this command
    // finishes, so that the arming sequence does not expire itself.
    app.lastCommandMs=millis();
}

void setup() {
    Serial.begin(115200);
    delay(50);
    HardwareSafety::preInit();
    Serial.println("\n=== BLIMP TESIS / REFRACTOR DE PRUEBAS ===");
    Serial.println("SAFE BOOT: actuadores desarmados; D2=HIGH (RST BNO); potencia motor original 0..100%.");

    app.comm=new BaseCommunicator(new LLC_ESPNow());
    app.comm->setMainBaseStation();

    app.robot=new FullBicopter();
    app.robot->startup();              // NO arma actuadores en esta versión
    app.robot->sensorsuite.changeNiclaMode(AppConfig::NICLA_BALLOON_MODE);
    reloadPreferences();

    app.sensorCount=app.robot->sense(app.sensors);
    TestModes::onModeEnter(app,AppConfig::SAFE_STOP);
    loopClock=micros();
}

void loop() {
    processCommands();
    if(PilotWatchdog::expired(millis(),app.lastCommandMs,
       (int)app.command.params[AppConfig::PARAM_RELOAD]==AppConfig::PILOT_HEARTBEAT_TOKEN,
       app.robot->actuatorsAreArmed())){
        app.robot->setActuatorsArmed(false);
        TestModes::onModeEnter(app,AppConfig::SAFE_STOP);
        Serial.println("[SAFE] Xbox: comandos vencidos");
    }
    app.sensorCount=app.robot->sense(app.sensors);
    updateFlowSensors();
    TestModes::run(app);
    reportFlowLink();

    while ((uint32_t)(micros()-loopClock) < AppConfig::CONTROL_LOOP_US) { yield(); }
    loopClock=micros();
}
