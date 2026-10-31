"""Compile and exercise the actual P02 controller without any hardware."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent.parent
files = {
    'Arduino.h': r'''#pragma once
#include <cmath>
#include <cstdint>
#define PI 3.14159265358979323846f
enum {D0,D1,D2,D3,D4,D5,D6,D7,D8,D9,D10};
template<class T> T constrain(T x,T lo,T hi){return x<lo?lo:(x>hi?hi:x);}
''',
    'app/AppContext.h': r'''#pragma once
#include "app/AppConfig.h"
struct Robot {
 bool armed=true;float limit=1,servo_old1=0,servo_old2=0,motor_power1=0,motor_power2=0;
 float m1=-1,m2=-1;int s1=0,s2=0;unsigned writes=0;
 bool actuatorsAreArmed(){return armed;}
 float getMotorPowerLimit(){return limit;}
 void commandMotorPowerTest(float a,float b,int c,int d){m1=a;m2=b;s1=c;s2=d;++writes;}
};
struct AppContext {Robot* robot;struct {float params[13]={};} command;};
''',
    'tests/TestRunners.h': r'''#pragma once
#include "app/AppContext.h"
namespace TestRunners {void p02Manual(AppContext&);}
''',
    'app/Telemetry.h': r'''#pragma once
#include "app/AppContext.h"
namespace Telemetry {inline void sendControl(AppContext&,float,float,float){}}
''',
    'main.cpp': r'''#include "tests/TestRunners.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
int main(){
 Robot robot;AppContext c{};c.robot=&robot;
 c.command.params[AppConfig::PARAM_FZ]=.12f;
 TestRunners::p02Manual(c);
 assert(fabsf(robot.m1-.30f)<.00001f && fabsf(robot.m2-.30f)<.00001f);
 assert(robot.s1==1250 && robot.s2==1850); // existing 35/95 degree geometry
 c.command.params[AppConfig::PARAM_AUX0]=35;TestRunners::p02Manual(c);
 assert(fabsf(robot.m1-.35f)<.00001f);
 c.command.params[AppConfig::PARAM_AUX0]=100;TestRunners::p02Manual(c);
 assert(fabsf(robot.m1-.50f)<.00001f);
 robot.limit=.25f;TestRunners::p02Manual(c);
 assert(fabsf(robot.m1-.25f)<.00001f && fabsf(robot.motor_power1-.25f)<.00001f);
 robot.limit=1;
 for(float invalid : {-1.f, float(NAN)}){
   c.command.params[AppConfig::PARAM_AUX0]=invalid;TestRunners::p02Manual(c);
   assert(robot.m1==0 && robot.m2==0);
 }
 c.command.params[AppConfig::PARAM_AUX0]=30;
 c.command.params[AppConfig::PARAM_FZ]=0;TestRunners::p02Manual(c);
 assert(robot.m1==0 && robot.m2==0); // release is neutral at any configured power
 c.command.params[AppConfig::PARAM_FZ]=-.12f;TestRunners::p02Manual(c);
 assert(robot.m1==0 && robot.m2==0); // no downward thrust
 c.command.params[AppConfig::PARAM_FZ]=.12f;
 robot.armed=false;const auto before=robot.writes;TestRunners::p02Manual(c);
 assert(robot.writes==before && robot.motor_power1==0 && robot.motor_power2==0);
 puts("PASS real P02: 30% legacy default, AUX0 percent, 50% cap, hardware limit, unchanged geometry, invalid input, neutral/descent zero, no output or false power while disarmed");
}
''',
}
with tempfile.TemporaryDirectory(prefix='blimp_manual_power_') as folder:
    tmp = Path(folder)
    for name, source in files.items():
        path = tmp/name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(source, encoding='utf-8')
    binary = tmp/'manual'
    subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-Wno-unused-parameter',
        '-I'+str(tmp),'-I'+str(root/'firmware/src'),
        str(root/'firmware/src/tests/P02ManualControl.cpp'),str(tmp/'main.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
