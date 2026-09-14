#include "IRWorldCloudStreaming.h"
#include <iostream>
#include <iomanip>
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    const std::string weather=argv[1];
    if(weather!="Rain" && weather!="Snow")return 2;
    IRWorldCloudStreaming generator;IRWorldCloudStreamingConfig config;
    config.maxVisibleVolumes=2;generator.setConfig(config);
    std::cout<<std::setprecision(17);
    for(int y=-5;y<=5;++y)for(int x=-5;x<=5;++x) {
        const auto d=generator.descriptorForCell(x,y,0,weather,weather=="Rain"?.72:.58,weather=="Rain"?1.3:1.0,4);
        if(!d.hasCloud)continue;
        std::cout<<"{\"cloudId\":\""<<IRWorldCloudStreaming::cloudIdText(d.cloudId)<<"\",\"cell\":["<<x<<","<<y
            <<"],\"position\":["<<d.worldX<<","<<d.worldY<<","<<d.worldZ<<"],\"radius\":["<<d.radiusX<<","<<d.radiusY<<","<<d.radiusZ
            <<"],\"rotation\":"<<d.rotationDeg<<",\"template\":"<<d.densityTemplate<<"}\n";
    }
}
