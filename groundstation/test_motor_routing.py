"""Compile the REAL RawBicopter output function bodies against fake actuators.

No serial/network/hardware. Covers both normal actuation and direct P00/mission
commands, including disarm, servo-calibration lock and clipping before routing.
"""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parent.parent
source=(root/'firmware/lib/BlimpSwarm/src/robot/RawBicopter.cpp').read_text(encoding='utf-8')

def body(signature):
    start=source.index(signature)
    opening=source.index('{',start)
    depth=1;end=opening+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}')
        end+=1
    return source[start:end]

assert '#define THRUST1 D9' in source and '#define THRUST2 D10' in source
assert '#define SERVO1 D0' in source and '#define SERVO2 D1' in source
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include "robot/MotorRouting.h"
template<class T> T constrain(T x,T lo,T hi){return x<lo?lo:(x>hi?hi:x);}
struct Output {float value=-1;unsigned writes=0;
 void act(float x){value=x;++writes;}
 void actMicroseconds(int x){value=x;++writes;}};
struct RawBicopter {
 bool actuatorsArmed=true;int servoCalibrationSelection=0;float motorPowerLimit=.25f;
 Output *motor1,*motor2,*servo1,*servo2,*led;
 void actuate(const float[],int);
 void commandMotorPowerTest(float,float,int,int);
};
'''
main=r'''
int main(){
 Output d9,d10,s1,s2,led;RawBicopter r;
 r.motor1=&d9;r.motor2=&d10;r.servo1=&s1;r.servo2=&s2;r.led=&led;
 r.commandMotorPowerTest(.10f,.20f,1000,1800);
 assert(d9.value==.20f && d10.value==.10f && s1.value==1000 && s2.value==1800);
 r.commandMotorPowerTest(.8f,-.1f,800,2200);
 assert(d9.value==0 && d10.value==.25f && s1.value==900 && s2.value==2100);
 float values[]={.10f,.20f,35,95,1};r.actuate(values,5);
 assert(d9.value==.20f && d10.value==.10f && s1.value==35 && s2.value==95);
 values[0]=.8f;values[1]=-.1f;r.actuate(values,5);
 assert(d9.value==0 && d10.value==.25f);
 r.commandMotorPowerTest(.18f,.18f,1400,1600);
 assert(d9.value==.18f && d10.value==.18f); //swapping equal powers cannot reverse travel
 const auto writes=d9.writes;r.actuatorsArmed=false;
 r.actuate(values,5);r.commandMotorPowerTest(.2f,.2f,1400,1600);
 assert(d9.writes==writes);
 r.actuatorsArmed=true;r.servoCalibrationSelection=1;
 r.commandMotorPowerTest(.2f,.2f,1400,1600);assert(d9.writes==writes);
 puts("PASS actual output routing: logical M1->D10/M2->D9, unchanged servos/pins, both actuation paths, clipping/disarm/calibration guards");
}
'''
with tempfile.TemporaryDirectory(prefix='blimp_motor_routing_') as folder:
    cpp=Path(folder)/'routing.cpp';binary=Path(folder)/'routing'
    cpp.write_text(prefix+body('void RawBicopter::actuate(')+'\n'+
        body('void RawBicopter::commandMotorPowerTest(')+main,encoding='utf-8')
    subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-Wno-unused-parameter',
        '-I'+str(root/'firmware/lib/BlimpSwarm/src'),str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
