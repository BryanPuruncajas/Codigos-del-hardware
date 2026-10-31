#pragma once
#include <math.h>
#include <stdint.h>
#define PI 3.14159265358979323846f
extern uint32_t mockMs;
inline unsigned long millis(){return mockMs;}
inline unsigned long micros(){return mockMs*1000U;}
template<class T> T constrain(T x,T lo,T hi){return x<lo?lo:(x>hi?hi:x);}
struct SerialMock {
 template<class... T> void printf(const char*,T...){}
 void println(const char*){}
};
static SerialMock Serial;
