#ifndef BLIMPSWARM_RAWBICOPTER_H
#define BLIMPSWARM_RAWBICOPTER_H
#include "Robot.h"
#include "act/BLMotor.h"
#include "act/AServo.h"
#include "act/LED.h"
#include <Preferences.h>
#include "util/DataTypes.h"

class RawBicopter : public Robot {
public:
    RawBicopter();
    void startup() override;
    int sense(float sensors[MAX_SENSORS]) override;
    void actuate(const float actuators[],int size) override;
    void control(float sensors[MAX_SENSORS],float controls[],int size) override;
    void calibrate() override;
    void arm() override;
    void getPreferences() override;
    float adjustAngle(float angle);
    float clamp(float val,float minVal,float maxVal);

    void setActuatorsArmed(bool armed);

    // P00: 0=ninguno, 1=S1, 2=S2, 3=ambos. Motores siempre deshabilitados.
    void setServoCalibrationSelection(int selection);
    void setServoCalibrationEnabled(bool enabled); // compatibilidad: true=ambos
    bool servoCalibrationIsEnabled() const { return servoCalibrationSelection != 0; }
    int getServoCalibrationSelection() const { return servoCalibrationSelection; }
    void commandServoCalibrationUs(int servo1Us, int servo2Us);

    // P00M: potencia directa 0..1, sin PID ni mixer.
    void commandMotorPowerTest(float motor1Power, float motor2Power, int servo1Us, int servo2Us);

    bool actuatorsAreArmed() const { return actuatorsArmed; }
    float getMotorPowerLimit() const { return motorPowerLimit; }
    feedback_t PDterms;

protected:
    float servoDiff=0;
    BLMotor* motor1=nullptr;
    BLMotor* motor2=nullptr;
    AServo* servo1=nullptr;
    AServo* servo2=nullptr;
    LED* led=nullptr;
    bool actuatorsArmed=false;
    int servoCalibrationSelection=0;
    float motorPowerLimit=1.00f;
};
#endif