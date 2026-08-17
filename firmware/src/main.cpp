#include <Arduino.h>
#include "BlimpSwarm.h"
#include "comm/BaseCommunicator.h"
#include "comm/LLC_ESPNow.h"
#include "state/nicla/NiclaConfig.h"
#include "app/AppConfig.h"
#include "app/AppContext.h"
#include "app/HardwareSafety.h"
#include "tests/TestModes.h"

AppContext app;
unsigned long loopClock=0;

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
    if (armCmd > 0.5f) {
        if (requestedMode==AppConfig::P00_SERVO_CALIBRATION) {
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
    app.sensorCount=app.robot->sense(app.sensors);
    TestModes::run(app);

    while ((uint32_t)(micros()-loopClock) < AppConfig::CONTROL_LOOP_US) { yield(); }
    loopClock=micros();
}