#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#define F(x) x
extern uint32_t fakeUs;
inline uint32_t micros(){return fakeUs;}
inline void delay(uint32_t ms){fakeUs+=ms*1000;}
inline void delayMicroseconds(uint32_t us){fakeUs+=us;}
struct SerialMock {
 template<class T>void print(const T&){}
 template<class T>void println(const T&){}
};
extern SerialMock Serial;
