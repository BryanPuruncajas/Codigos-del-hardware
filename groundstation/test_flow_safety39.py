"""Execute the real mode-dispatch function with fake actuators and telemetry."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parent.parent
source=(ROOT/'firmware/src/tests/TestModes.cpp').read_text()
start=source.index('void run(AppContext& ctx)');opening=source.index('{',start);end=opening+1;depth=1
while depth:
    depth+=(source[end]=='{')-(source[end]=='}');end+=1
body=source[start:end]
modes=['SAFE_STOP','P00_MOTOR_POWER_TEST','P00_SERVO_CALIBRATION','P01_SENSOR_INTEGRATION','P02_MANUAL_CONTROL',
    'P03_YAW_CONTROL','P04_ALTITUDE_CONTROL','P05_YAW_ALTITUDE','P06_NICLA_RAW','P07_VISUAL_CENTER',
    'P08_ONE_BALLOON','P09_VISIT_ESCAPE','P10_TWO_BALLOONS','P11_FOUR_BALLOONS','P11_INFINITY','P12_VISION_BENCH']
values=[0,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22]
with tempfile.TemporaryDirectory(prefix='blimp_safety39_') as temp:
    folder=Path(temp);(folder/'app').mkdir()
    (folder/'app/AppConfig.h').write_text('#pragma once\nnamespace AppConfig {enum {'+
        ','.join(f'{key}={value}' for key,value in zip(modes,values))+'};}\n')
    prefix='''#include <assert.h>
#include <stdio.h>
#include "app/FlowSafety.h"
int dispatched=0;struct Robot{bool armed=true;bool actuatorsAreArmed(){return armed;}void setActuatorsArmed(bool x){armed=x;}};
struct AppContext{Robot* robot;int mode=0;struct{bool valid=false;}flowHeight;};
void neutral(AppContext&){};
namespace Telemetry {
'''+''.join('void '+name+'(AppContext&){}\n' for name in ['sendCore','sendNicla','sendNiclaStatus','sendActuators','sendFlow','sendFlowControl'])+'''}
namespace TestRunners {
'''+''.join('void '+name+'(AppContext&){++dispatched;}\n' for name in ['p00MotorPower','p00ServoCalibration','p01Sensors','p02Manual','p03Yaw','p04Altitude','p05YawAltitude','p06NiclaRaw','p07VisualCenter'])+'''}
int pausedMode=-1;unsigned long pausedAt=0;
bool autonomous(int m){return m>=17 && m<=21;}
struct Mission{void update(AppContext&){++dispatched;}void reportPaused(AppContext&,unsigned long){}}mission;
'''
    main='''int main(){Robot robot;AppContext c;c.robot=&robot;
for(int mode: {12,13,14,16,17,18,19,20,21}){robot.armed=true;c.mode=mode;c.flowHeight.valid=false;dispatched=0;run(c);
assert(!robot.armed && dispatched==0);}
c.mode=13;c.flowHeight.valid=true;robot.armed=true;dispatched=0;run(c);assert(robot.armed && dispatched==1);
c.mode=8;c.flowHeight.valid=false;robot.armed=true;dispatched=0;run(c);assert(robot.armed && dispatched==1);
puts("PASS real TestModes39: all automatic height modes disarm and skip actuation on invalid Flowdeck; raw bench remains available");}
'''
    cpp=folder/'test.cpp';cpp.write_text('#include <initializer_list>\n'+prefix+body+'\n'+main)
    binary=folder/'test';subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror',
        '-I'+str(folder),'-I'+str(ROOT/'firmware/src'),str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
