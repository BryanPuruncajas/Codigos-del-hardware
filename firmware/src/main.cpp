/**
 * BICOPTER with altitude control
 * This code runs a bicopter with altitude control using the feedback from a barometer.
 * For this example, your robot needs a barometer sensor.
 */

#include "BlimpSwarm.h"
#include "robot/RobotFactory.h"
#include "state/nicla/NiclaConfig.h"
#include "comm/BaseCommunicator.h"
#include "comm/LLC_ESPNow.h"
#include "util/Print.h"
#include "sense/SensorSuite.h"
#include <Arduino.h>
#include <ESP32Servo.h>


// Robot
FullBicopter* myRobot = nullptr;

//sensor
NiclaSuite* nicla = nullptr;

nicla_t terms; 
hist_t* hist;

// Communication
BaseCommunicator* baseComm = nullptr;

// Control input from base station
ControlInput behave;
ControlInput cmd;
ReceivedData rcv;

// Data storage for the sensors
float senses[myRobot->MAX_SENSORS];

const int TIME_STEP_MICRO = 4000;

int dt = 1000;
unsigned long clockTime;
unsigned long printTime;

int niclaOffset = 11;
int old_flag = 0;
float forward_force = 0.0;

// ==================== MODO BUSQUEDA AUTONOMA (flag=3) ====================
// Maquina de estados adaptada de la referencia de simulacion (demopid.py),
// simplificada porque el hardware real no tiene posicion X/Y absoluta
// (no hay GPS/mocap), solo camara + IMU (yaw) + barometro (altura relativa).
enum SearchState { SEARCHING, APPROACHING, CAPTURED_TURN, FINISHED };
SearchState searchState = SEARCHING;
int balloonsFound = 0;
const int TOTAL_BALLOONS = 4;
unsigned long searchStateTimer = 0;

// Que tan cerca debe verse el globo (ancho del blob en pixeles) para
// contarlo como "capturado". AJUSTAR segun pruebas reales.
const float CAPTURE_AREA_THRESHOLD = 60.0;
// Velocidad de giro mientras busca sin ver nada (rad/s aprox, va directo a tz)
const float SEARCH_YAW_RATE = 0.35;
// Cuanto tiempo (ms) gira "a ciegas" tras capturar, antes de volver a buscar
// (evita re-detectar el mismo globo de inmediato)
const unsigned long CAPTURED_TURN_MS = 2000;
// ============================================================================

// Forward declarations (necesarias en .cpp; en .ino el IDE las genera solo)
void recieveCommands();
void paramUpdate();
void fixClockRate();


void setup() {
    Serial.begin(115200);
    Serial.println("Start!");

    // El pin RST del BNO085 (breakout) esta cableado a D2 segun el PCB.
    // El firmware original nunca lo tocaba, dejandolo flotando -> el sensor
    // quedaba en reset intermitente (por eso el I2C scanner lo detectaba
    // una vez y luego dejaba de responder). Lo ponemos en HIGH para
    // liberarlo del reset ANTES de inicializar cualquier sensor I2C.
    pinMode(D2, OUTPUT);
    digitalWrite(D2, HIGH);
    delay(50);  // le damos tiempo al sensor para arrancar tras salir de reset

    clockTime = micros();
    printTime = micros();
    // init communication
    baseComm = new BaseCommunicator(new LLC_ESPNow());
    baseComm->setMainBaseStation();

    // init robot with new parameters
    myRobot = new FullBicopter();
    myRobot->startup();
    nicla = &(myRobot->sensorsuite);
    paramUpdate();
    // NOTA: se removió nicla->changeNiclaMode(0x80) — ese comando en realidad
    // saca a la Nicla del modo balloon y la manda a modo goal (aros), que no
    // usamos en este proyecto (solo visitamos globos). La Nicla ya arranca
    // en modo balloon (0) por defecto en perception_subsystem.py.
    // updates the ground altitude for the ground feedback
    // TODO: make some way to access the actual ground height from robot
    int numSenses = myRobot->sense(senses);

}

float nicla_yaw = 0;
float z_estimator = 0;

void loop() {
  // Retrieves cmd.params from ground station and checks flags
  recieveCommands();

  // Get sensor values
  int numSenses = myRobot->sense(senses);

  // send values to ground station
  rcv.flag = 1;
  rcv.values[0] = senses[1];  //height
  rcv.values[1] = senses[5];  //yaw
  rcv.values[2] = senses[niclaOffset + 1];  //nicla x
  rcv.values[3] = senses[niclaOffset + 2];  //nicla y
  rcv.values[4] = senses[niclaOffset + 9];  //nicla w
  rcv.values[5] = senses[niclaOffset + 9];  //nicla h
  // rcv.values[6] = senses[niclaOffset + 9];  //nicla confidence
  bool sent = baseComm->sendMeasurements(&rcv);

  // DEBUG/CALIBRACION: manda actuadores (servos + motores) y orientacion
  // completa del IMU a la base por ESP-NOW, para verlo en vivo sin USB.
  ReceivedData actuatorDebug;
  actuatorDebug.flag = 3;
  actuatorDebug.values[0] = myRobot->servo_old1;   // angulo real servo 1 (grados)
  actuatorDebug.values[1] = myRobot->servo_old2;   // angulo real servo 2 (grados)
  actuatorDebug.values[2] = myRobot->motor_power1; // potencia motor 1 (0 a 1)
  actuatorDebug.values[3] = myRobot->motor_power2; // potencia motor 2 (0 a 1)
  actuatorDebug.values[4] = 0;
  actuatorDebug.values[5] = 0;
  baseComm->sendMeasurements(&actuatorDebug);

  ReceivedData orientationDebug;
  orientationDebug.flag = 4;
  orientationDebug.values[0] = senses[3];  // roll
  orientationDebug.values[1] = senses[4];  // pitch
  orientationDebug.values[2] = senses[5];  // yaw
  orientationDebug.values[3] = senses[6];  // rollrate
  orientationDebug.values[4] = senses[7];  // pitchrate
  orientationDebug.values[5] = senses[8];  // yawrate
  baseComm->sendMeasurements(&orientationDebug);

  // print sensor values every second
  // senses => [temperature, altitude, veloctity_in_altitude, roll, pitch, yaw, rollrate, pitchrate, yawrate, null, battery, nicla_flag, nicla_x, nicla_y, nicla_w, nicla_h, nicla...]
  if (micros() - printTime > 515106){
    
    for (int i = 0; i < numSenses-1; i++){
      Serial.print(senses[i]);
      Serial.print(",");
    }
    Serial.println(senses[numSenses-1]);
    printTime = micros();
  }



  // Nicla controller (when the incomming flag = 2)
  if (cmd.params[0] == 2) {
    int nicla_flag = (int)senses[niclaOffset + 0];
    float tracking_x = (float)senses[niclaOffset + 1];
    if (nicla_flag & 0x40) {
      // the second MSB of nicla flag is 1 for balloon detection
      if ((nicla_flag & 0b11) != (old_flag & 0b11)) {
        // the last two MSBs of the flag toggles between 0b01 and 0b10 for new detections,
        // and it toggles to 0b00 for new no-detection
        old_flag = nicla_flag;
        if (nicla_flag & 0b11) {
          // if a new detection is fed in
          float _yaw = senses[5];  
          float _height = senses[1];  
          float tracking_y = (float)senses[niclaOffset + 2];
          
          float x_cal = tracking_x / terms.n_max_x; // normalizes the pixles into a value between [0,1]
          float des_yaw = ((x_cal - 0.5)) * terms.x_strength; // normalizes the normal to between [-.5, .5] to act as an offset for yaw
          nicla_yaw = _yaw + des_yaw; // add the offset in yaw to the current yaw for movement.
          float y_cal = tracking_y / terms.n_max_y;
          if ( abs(x_cal - 0.5) < terms.fx_charge){// && terms.y_strength != 0) { // makes sure yaw is in center before making height adjustments
              z_estimator =  ( _height + terms.y_strength * (y_cal - terms.y_thresh)) ; // height control doenst work well when not 0 bouyant
              forward_force = terms.fx_togoal;
          } else {
              forward_force = 0.0;
          }
        } else {
          // if new readings from the detction is no-detection, we reset the forward force
          forward_force = 0.0;
          nicla_yaw = 0.0;
        }
      }
    }
    behave.params[0] = cmd.params[0]; // flag
    behave.params[1] = cmd.params[1] + forward_force; // fx ('meters'/second)
    behave.params[2] = cmd.params[2]; // fz (meters)
    behave.params[3] = 0; // tx (radians/second)
    behave.params[4] = nicla_yaw; // tz (radians)

  } else if (cmd.params[0] == 3) {
    // ================= MODO BUSQUEDA AUTONOMA (4 globos) =================
    int nicla_flag = (int)senses[niclaOffset + 0];
    float tracking_x = (float)senses[niclaOffset + 1];
    float tracking_y = (float)senses[niclaOffset + 2];
    float nicla_w = (float)senses[niclaOffset + 4];
    bool ballVisible = (nicla_flag & 0x40) && (nicla_flag & 0b11);

    float search_fx = 0.0;
    float search_tz = 0.0;
    float search_fz = cmd.params[2]; // altura la sigue mandando la base (usa zEn si quieres)

    switch (searchState) {

      case SEARCHING:
        // Sin globo a la vista: gira lento escaneando, sin avanzar.
        search_tz = SEARCH_YAW_RATE;
        search_fx = 0.0;
        if (ballVisible) {
          Serial.println("[BUSQUEDA] Globo detectado, pasando a ACOPLANDO.");
          searchState = APPROACHING;
        }
        break;

      case APPROACHING:
        // Reutiliza la misma logica de centrado/avance ya calibrada en flag=2.
        if (!ballVisible) {
          Serial.println("[BUSQUEDA] Se perdio el globo, volviendo a BUSCANDO.");
          searchState = SEARCHING;
          break;
        }
        {
          float x_cal = tracking_x / terms.n_max_x;
          float des_yaw = ((x_cal - 0.5)) * terms.x_strength;
          float _yaw = senses[5];
          search_tz = 0; // se maneja como offset absoluto de yaw, no tasa; ver nicla_yaw abajo
          nicla_yaw = _yaw + des_yaw;
          float y_cal = tracking_y / terms.n_max_y;
          if (abs(x_cal - 0.5) < terms.fx_charge) {
            search_fx = terms.fx_togoal;
          } else {
            search_fx = 0.0;
          }

          // Si el globo se ve grande (cerca), lo contamos como capturado.
          if (nicla_w > CAPTURE_AREA_THRESHOLD) {
            balloonsFound++;
            Serial.print("[BUSQUEDA] Globo capturado! Total: ");
            Serial.println(balloonsFound);
            if (balloonsFound >= TOTAL_BALLOONS) {
              searchState = FINISHED;
            } else {
              searchState = CAPTURED_TURN;
              searchStateTimer = millis();
            }
          }
        }
        break;

      case CAPTURED_TURN:
        // Gira "a ciegas" un rato para no re-detectar el mismo globo de inmediato.
        search_tz = SEARCH_YAW_RATE;
        search_fx = 0.0;
        if (millis() - searchStateTimer > CAPTURED_TURN_MS) {
          Serial.println("[BUSQUEDA] Reanudando busqueda del siguiente globo.");
          searchState = SEARCHING;
        }
        break;

      case FINISHED:
        // Ya encontro los 4, se detiene.
        search_fx = 0.0;
        search_tz = 0.0;
        break;
    }

    behave.params[0] = cmd.params[0]; // flag = 3
    behave.params[1] = search_fx;
    behave.params[2] = search_fz;
    behave.params[3] = 0;
    // IMPORTANTE: tz aqui se manda como torque/tasa "cruda", no como angulo
    // absoluto. Esto funciona bien mientras yawEn este en OFF (asi tz entra
    // directo como torque constante -> giro continuo estable). Si activas
    // yawEn, este valor se interpretaria como un ANGULO objetivo fijo y el
    // blimp giraria hasta ahi y se detendria, no seguiria girando -> para
    // este modo de busqueda, deja yawEn apagado.
    behave.params[4] = (searchState == APPROACHING) ? nicla_yaw : search_tz;
    // ========================================================================

  } else { // direct control with joystick if 'flag' is not 2 ni 3
    z_estimator = cmd.params[2];
    nicla_yaw = cmd.params[4]; // autoset for when switch occurs
    forward_force = 0;
    behave.params[0] = cmd.params[0]; //flag
    behave.params[1] = cmd.params[1]; //fx
    behave.params[2] = cmd.params[2]; //fz
    behave.params[3] = cmd.params[3]; //tx
    behave.params[4] = cmd.params[4]; //tz
  }

  // Send command to the actuators
  myRobot->control(senses, behave.params, 5);

  // makes the clock rate of the loop consistant.
  fixClockRate();
}

void recieveCommands(){
  if (baseComm->isNewMsgCmd()){
    // New command received
    cmd = baseComm->receiveMsgCmd();
    if (int(cmd.params[11]) == 1){
      paramUpdate();
    }
    // Print command
    Serial.print("Cmd arrived: ");
    printControlInput(cmd);
  }
}

void paramUpdate(){
    NiclaConfig::getInstance()->loadConfiguration();
    const nicla_t& config = NiclaConfig::getInstance()->getConfiguration();
    terms = config; // Copy configuration data
    hist = NiclaConfig::getInstance()->getDynamicHistory();
    myRobot->getPreferences();
    baseComm->setMainBaseStation();

}

void fixClockRate() {

  dt = (int)(micros()-clockTime);
  while (TIME_STEP_MICRO - dt > 0){
    dt = (int)(micros()-clockTime);
  }
  clockTime = micros();
}