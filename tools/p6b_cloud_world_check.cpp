#include "IR/IRCloudAppearance.h"
#include "IR/IRCloudWorldFrame.h"
#include <iostream>
#include <map>
#include <cmath>
#include <algorithm>

int main(int argc,char** argv){
    try{
        if(argc<5)throw std::runtime_error("usage: cloud_world_check CONFIG_ROOT LOCAL_LAT LOCAL_LON LOCAL_ALT [reverse] [seed_delta]");
        IRCloudAppearance art;art.load(argv[1],"GameWorld");
        IRWorldCloudStreaming generator;auto cfg=art.geometry;
        if(argc>6)cfg.weatherSeed+=std::stoull(argv[6]);generator.setConfig(cfg);
        IRCloudWorldFrame frame;frame.setWorld(art.latitude,art.longitude,art.altitude);
        frame.setLocal(std::stod(argv[2]),std::stod(argv[3]),std::stod(argv[4]));
        std::map<std::uint64_t,IRWorldCloudDescriptor> baseline;
        std::vector<std::pair<int,int>> cells;
        for(int y=-4;y<=4;++y)for(int x=-4;x<=4;++x)cells.emplace_back(x,y);
        if(argc>5&&std::string(argv[5])=="reverse")std::reverse(cells.begin(),cells.end());
        double maxRoundTrip=0;
        for(auto cell:cells){
            auto d=generator.descriptorForCell(cell.first,cell.second,0,"Cloudy",.35,.88,int(art.templates.size()));
            if(!d.hasCloud)continue;
            baseline[d.cloudId]=d;
            ICD::CartesianCoordinate p={d.worldX,d.worldY,d.worldZ};auto q=frame.toWorld(frame.toLocal(p));
            maxRoundTrip=std::max(maxRoundTrip,std::max(std::abs(p.x-q.x),std::max(std::abs(p.y-q.y),std::abs(p.z-q.z))));
        }
        // Loading order and positive/negative boundary traversal cannot mutate identity.
        for(double center:{0.0,7501.0,-10001.0,15001.0,-2501.0,0.0}){
            for(const auto& d:generator.queryCandidates(center,center,0,"Cloudy",.35,.88,int(art.templates.size()))){
                auto old=baseline.find(d.cloudId);if(old!=baseline.end()){
                    const auto& b=old->second;
                    if(d.worldX!=b.worldX||d.worldY!=b.worldY||d.worldZ!=b.worldZ||d.densityTemplate!=b.densityTemplate)
                        throw std::runtime_error("descriptor changed on return");
                }
            }
        }
        if(maxRoundTrip>1.e-6)throw std::runtime_error("ENU round-trip exceeded one micrometre");
        std::cout<<std::setprecision(17);
        for(const auto& entry:baseline){const auto& d=entry.second;const auto& t=art.templates.at(d.densityTemplate);
            std::cout<<"{\"cloudId\":\""<<IRWorldCloudStreaming::cloudIdText(d.cloudId)<<"\",\"cell\":["<<d.cellX<<","<<d.cellY
                <<"],\"position\":["<<d.worldX<<","<<d.worldY<<","<<d.worldZ<<"],\"radius\":["<<d.radiusX<<","<<d.radiusY<<","<<d.radiusZ
                <<"],\"density\":"<<d.density<<",\"rotation\":"<<d.rotationDeg<<",\"template\":\""<<t.key<<"\",\"sha256\":\""<<t.sha256
                <<"\",\"animation\":\"static\"}"<<std::endl;
        }
        std::cerr<<"[CloudWorldCheck] PASS count="<<baseline.size()<<" maxRoundTripM="<<maxRoundTrip
            <<" crossPlatformToleranceM=0.000001 order="<<(argc>5?argv[5]:"forward")<<" traversedPositiveNegativeCells=1"<<std::endl;
        return 0;
    }catch(const std::exception& e){std::cerr<<"[CloudWorldCheck] FAIL "<<e.what()<<std::endl;return 1;}
}
