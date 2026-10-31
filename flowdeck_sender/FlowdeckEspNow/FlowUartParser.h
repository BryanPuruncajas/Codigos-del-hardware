#pragma once
#include "FlowProtocol.h"
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>

// Current source: FLOW,sequence,dx,dy,z_mm,squal,motion.
// Motion is retained only as input validation; its undocumented bit meaning
// must not be confused with a quality/validity flag.
inline bool parsePrefixedFlow(const char* text,FlowProtocol::Packet& output){
    long long v[6];const char* cursor=text+5;
    for(unsigned i=0;i<6;++i){
        while(isspace(static_cast<unsigned char>(*cursor)))++cursor;
        errno=0;char* end=nullptr;v[i]=strtoll(cursor,&end,10);
        if(cursor==end || errno==ERANGE)return false;
        cursor=end;while(isspace(static_cast<unsigned char>(*cursor)))++cursor;
        if(i<5){if(*cursor!=',')return false;++cursor;}
        else if(*cursor!='\0')return false;
    }
    if(v[0]<0 || v[0]>UINT32_MAX || v[1]<INT16_MIN || v[1]>INT16_MAX ||
       v[2]<INT16_MIN || v[2]>INT16_MAX || v[3]<-1 || v[3]>UINT16_MAX ||
       v[4]<0 || v[4]>UINT8_MAX || v[5]<0 || v[5]>UINT8_MAX)return false;
    FlowProtocol::Packet packet{};packet.magic=FlowProtocol::MAGIC;packet.version=FlowProtocol::VERSION;
    packet.sequence=uint32_t(v[0]);packet.dx=int16_t(v[1]);packet.dy=int16_t(v[2]);
    packet.quality=uint8_t(v[4]);packet.flags=FlowProtocol::FLOW_VALID;
    if(v[3]>0 && v[3]<=4000){packet.range_mm=uint16_t(v[3]);packet.flags|=FlowProtocol::RANGE_VALID;}
    output=packet;return true;
}

// Input: dx,dy,z_mm. No quality/status was supplied by this UART source.
// FLOW_VALID means a complete, representable input sample was received; the
// source must omit invalid flow measurements. It is not measured confidence.
inline bool parseFlowLine(const char* text, uint32_t sequence, FlowProtocol::Packet& output) {
    while(isspace(static_cast<unsigned char>(*text)))++text;
    if(strncmp(text,"FLOW,",5)==0)return parsePrefixedFlow(text,output);
    long values[3];
    const char* cursor=text;
    for(unsigned i=0;i<3;++i){
        while(isspace(static_cast<unsigned char>(*cursor)))++cursor;
        errno=0;char* end=nullptr;
        values[i]=strtol(cursor,&end,10);
        if(cursor==end || errno==ERANGE)return false;
        cursor=end;
        while(isspace(static_cast<unsigned char>(*cursor)))++cursor;
        if(i<2){if(*cursor!=',')return false;++cursor;}
        else if(*cursor!='\0')return false;
    }
    if(values[0]<INT16_MIN || values[0]>INT16_MAX ||
       values[1]<INT16_MIN || values[1]>INT16_MAX ||
       values[2]<-1 || values[2]>UINT16_MAX)return false;
    FlowProtocol::Packet packet{};
    packet.magic=FlowProtocol::MAGIC;
    packet.version=FlowProtocol::VERSION;
    packet.flags=FlowProtocol::FLOW_VALID;
    packet.sequence=sequence;
    packet.dx=static_cast<int16_t>(values[0]);
    packet.dy=static_cast<int16_t>(values[1]);
    // -1, zero and 65535 are treated as unavailable range, never a real height.
    if(values[2]>0 && values[2]<UINT16_MAX){
        packet.range_mm=static_cast<uint16_t>(values[2]);
        packet.flags|=FlowProtocol::RANGE_VALID;
    }
    packet.quality=0; // unknown: not present in the three-column input
    output=packet;
    return true;
}
