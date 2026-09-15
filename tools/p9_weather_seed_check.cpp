// Offline selection of a static demo Weather seed; never used by production.
#include "../HwaSim_IR/HwaSim_IR/IR/IRWorldCloudStreaming.h"
#include <iostream>
#include <cmath>
#include <algorithm>
int main(){
    IRWorldCloudStreaming cloud;IRWorldCloudStreamingConfig cfg;cfg.minCloudAltitudeM=9400;cfg.maxCloudAltitudeM=10600;
    cfg.maxVisibleVolumes=2;
    // Approximate ordinary demo camera/scene corridor, from the unchanged 1.txt.
    // This only rejects seeds with no plausible cloud intersection; actual pixels must be checked on RK3588.
    for(int seed=1;seed<=64;++seed){
        cfg.weatherSeed=seed;cloud.setConfig(cfg);int counts[3]={};
        for(int k=0;k<3;++k){double cy=5+k*2000,cz=11000+k*300,tx=-853,ty=22240-k*2000,tz=10000-k*200;
            auto list=cloud.queryCandidates(tx,ty,0,"Cloudy",.35,1,5);if(list.size()>8)list.resize(8);
            for(auto& d:list){
                double t=(d.worldY-cy)/(ty-cy);if(t<0)continue;
                const double x=tx*t,z=cz+(tz-cz)*t;
                const double dx=(d.worldX-x)/std::min(d.radiusX,d.radiusY),dz=(d.worldZ-z)/d.radiusZ;
                if(dx*dx+dz*dz<.65)++counts[k];
            }
        }
        if(counts[0]>0)std::cout<<seed<<","<<counts[0]<<","<<counts[1]<<","<<counts[2]<<"\n";
    }
}
