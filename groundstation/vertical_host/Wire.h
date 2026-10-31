#pragma once
#include <stdint.h>
#include <stddef.h>
struct TwoWire {
 void begin(){}
 void beginTransmission(uint8_t){}
 size_t write(uint8_t){return 1;}
 size_t write(const uint8_t*,size_t n){return n;}
 uint8_t endTransmission(bool=true){return 0;}
 uint8_t requestFrom(uint8_t,uint8_t n){return n;}
 int read(){return 0;}
};
extern TwoWire Wire;
