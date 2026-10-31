#include "mission/InertialHeight.h"
#include <stdio.h>
#include <math.h>
// Input: elapsed_us, recorded filtered_vz. Replay is downsampled F9(~5Hz),
// not the25Hz acquisition stream. No visual observation, no actuator output.
int main(){
    InertialHeight c;c.learningAllowed=false;float trim=.15f,velocity;
    unsigned us,n=0,trusted=0;double energy=0,maxCorrection=0,totalVariation=0;float last=trim;
    while(scanf("%u,%f",&us,&velocity)==2){
        float p=c.update(us/1000,trim,0,0,false,false,true,velocity,.20f,0,0,true,us);
        float delta=p-trim;energy+=delta*delta;maxCorrection=fmax(maxCorrection,fabs(delta));
        totalVariation+=fabs(p-last);last=p;++n;if(c.velocityTrusted)++trusted;
    }
    printf("{\"samples\":%u,\"trusted\":%u,\"power_correction_rms\":%.8f,\"max_correction\":%.8f,\"power_total_variation\":%.8f}\n",
        n,trusted,n?sqrt(energy/n):0,maxCorrection,totalVariation);
}
