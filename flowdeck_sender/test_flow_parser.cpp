#include "FlowUartParser.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

int main(){
    FlowProtocol::Packet p{};
    assert(parseFlowLine("FLOW,4294967295,-10,20,900,75,128",0,p));
    assert(p.sequence==UINT32_MAX && p.dx==-10 && p.dy==20 && p.range_mm==900 && p.quality==75 && p.flags==3);
    assert(parseFlowLine("FLOW,0,0,0,65535,0,0",0,p));assert(p.flags==1 && p.range_mm==0);
    const char* unavailable[]={"FLOW,1,0,0,-1,40,0","FLOW,1,0,0,0,40,0","FLOW,1,0,0,4001,40,0"};
    for(const char* line:unavailable){
        assert(parseFlowLine(line,0,p));assert(p.flags==1 && p.range_mm==0);
    }
    assert(parseFlowLine("FLOW,1,0,0,4000,40,0",0,p));assert(p.flags==3 && p.range_mm==4000);
    assert(!parseFlowLine("FLOW,0,0,0,800,300,0",0,p));
    assert(!parseFlowLine("FLOW,4294967296,0,0,800,30,0",0,p));
    assert(!parseFlowLine("FLOW,1,32768,0,800,30,0",0,p));
    assert(!parseFlowLine("FLOW,1,0,0,800,30,0,extra",0,p));
    assert(sizeof(p)==16 && offsetof(FlowProtocol::Packet,sequence)==4 && offsetof(FlowProtocol::Packet,range_mm)==12);
    assert(parseFlowLine("12,-7,800",42,p));
    assert(p.magic==0xB17F && p.version==1 && p.sequence==42);
    assert(p.dx==12 && p.dy==-7 && p.range_mm==800 && p.flags==3 && p.quality==0 && p.reserved==0);
    const unsigned char* bytes=reinterpret_cast<const unsigned char*>(&p);
    assert(bytes[0]==0x7f && bytes[1]==0xb1 && bytes[2]==1 && bytes[3]==3);
    assert(parseFlowLine(" -32768 , 32767 , 65534 \r",UINT32_MAX,p));
    assert(p.dx==-32768 && p.dy==32767 && p.range_mm==65534 && p.sequence==UINT32_MAX);
    const char* invalidRange[]={"0,0,-1","0,0,0","0,0,65535"};
    for(const char* line:invalidRange){assert(parseFlowLine(line,0,p));assert(p.flags==1 && p.range_mm==0);}
    const char* bad[]={"","vivo...","1,2","1,2,3,4","1,2,3garbage","32768,0,5","0,-32769,5",
        "0,0,65536","0,0,-2","999999999999999999999999999,0,1","1.2,0,5",",,","1,,2"};
    for(const char* line:bad){const auto before=p;assert(!parseFlowLine(line,0,p));assert(memcmp(&p,&before,sizeof(p))==0);}
    puts("PASS flow parser: 16-byte receiver format, sequence/flags/range, unknown quality, bounds, malformed input, unchanged output on rejection");
}
