#include "mission/InertialHeight.h"
#include <stdio.h>
int main(){
    InertialHeight c;c.learningAllowed=false;float trim=.15f,h,v;unsigned us,phase;
    while(scanf("%u,%u,%f,%f",&us,&phase,&h,&v)==4){
#ifdef REFERENCE_V29
        const float p=c.update(us/1000,trim,0,0,false,false,true,v,.20f,0,0,true,us);
#else
        const float p=c.update(us/1000,trim,0,0,false,false,true,v,.20f,0,0,true,us,h);
#endif
        printf("%u,%u,%d,%.6f,%.6f\n",us,phase,c.velocityTrusted?1:0,c.controlRate,p-trim);
    }
}
