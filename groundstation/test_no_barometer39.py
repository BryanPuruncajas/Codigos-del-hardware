"""Compile the real SensorSuite against IMU/battery stubs, with NO BMP header."""
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parent.parent
SENSE=ROOT/'firmware/lib/BlimpSwarm/src/sense'
header=(SENSE/'SensorSuite.h').read_text()
source=(SENSE/'SensorSuite.cpp').read_text()
assert 'Barometer390.h' not in header and 'barometer.' not in source

with tempfile.TemporaryDirectory(prefix='blimp_flowonly39_') as temp:
    folder=Path(temp)
    (folder/'SensorSuite.h').write_text(header)
    (folder/'SensorSuite.cpp').write_text(source)
    (folder/'VerticalDiagnostics.h').write_text((SENSE/'VerticalDiagnostics.h').read_text())
    (folder/'SensorInterface.h').write_text('''#pragma once
struct SensorInterface {virtual ~SensorInterface(){};virtual void startup()=0;virtual bool update()=0;
virtual float* readValues(int&)=0;virtual void getPreferences(){};};
''')
    (folder/'BNO85.h').write_text('''#pragma once
#include "SensorInterface.h"
struct BNO85 {static int starts;float values[7]={3,4,5,6,7,8,9};
void startup(){++starts;}bool update(){return true;}void getPreferences(){}
float* readValues(int& n){n=7;return values;}};
''')
    (folder/'BatterySensor.h').write_text('''#pragma once
struct WeightedBatterySensor {static int starts;float value[1]={4};
void startup(){++starts;}bool update(){return true;}void getPreferences(){}
float* readValues(int& n){n=1;return value;}};
''')
    (folder/'test.cpp').write_text('''#include "SensorSuite.h"
#include <assert.h>
#include <stdio.h>
int BNO85::starts=0;int WeightedBatterySensor::starts=0;
int main(){SensorSuite suite;suite.startup();assert(BNO85::starts==1 && WeightedBatterySensor::starts==1);
int n=0;suite.update();float* values=suite.readValues(n);assert(n==11 && values[1]==0 && !suite.verticalDiagnostics().healthy);
VerticalDiagnostics flow{.8f,.8f,-.03f,-.03f,10,true,1000000,40000,200};
suite.publishFlowVertical(flow);suite.update();values=suite.readValues(n);
assert(values[0]==0 && values[1]==.8f && values[2]==-.03f && values[3]==3 && values[10]==4);
assert(suite.verticalDiagnostics().healthy && suite.verticalDiagnostics().sampleTimeUs==1000000);
flow.healthy=false;suite.publishFlowVertical(flow);values=suite.readValues(n);
assert(values[1]==0 && values[2]==0 && !suite.verticalDiagnostics().healthy);
puts("PASS real SensorSuite39: no BMP header/object/calls, IMU+battery retained, vertical slots populated only from Flowdeck, invalid sample cleared");}
''')
    binary=folder/'test'
    subprocess.run(['g++','-std=c++11','-Wall','-Wextra','-Werror','-I'+str(folder),
        str(folder/'SensorSuite.cpp'),str(folder/'test.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
