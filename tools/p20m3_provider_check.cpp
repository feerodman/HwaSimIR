#include "../Shared/EngineeringAtmosphere.h"
#include <iostream>
#include <random>
#include <iomanip>
int main(int argc,char** argv){
    if(argc!=3)return 2;HwaAtmosphere::Provider p;if(!p.load(argv[1],argv[2]))return 3;
    std::mt19937 rng(20261021);std::uniform_real_distribution<double> u(0,1);
    unsigned tested=0,failed=0;double maxSplit=0;
    for(int i=0;i<2400;++i){
        double h=3000+17000*u(rng),lat=-89+178*u(rng),lon=-180+360*u(rng);
        HwaAtmosphere::Geo a(lat,lon,h),b=a;
        if(i%3==0)b.alt=3000+17000*u(rng);
        else {double d=49900*u(rng);b.lat+=d/6378137*180/HwaAtmosphere::pi;}
        if(i<11){double distances[]={0,.1,.5,1,10,100,1000,2400,5000,20000,49999};a=HwaAtmosphere::Geo(0,0,3000);b=a;b.lon=distances[i]/(6378137+3000)*180/HwaAtmosphere::pi;}
        double vis=6+17*u(rng),rh=30+55*u(rng),elevation=-90+180*u(rng);
        const auto sun=HwaAtmosphere::sunEcef(a,360*u(rng),elevation);
        for(int band:{0,2}){
            const auto v=p.query(band,a,b,vis,rh,sun);++tested;
            if(!v.usableForRendering||!std::isfinite(v.tau)||v.tau<0||v.tau>1+1e-14||
              !std::isfinite(v.pathThermalRadiance)||v.pathThermalRadiance<0||!std::isfinite(v.pathScatteringRadiance)||v.pathScatteringRadiance<0||
              !std::isfinite(v.directSolarIrradiance)||v.directSolarIrradiance<0||!std::isfinite(v.skyDiffuseIrradiance)||v.skyDiffuseIrradiance<0)++failed;
            if(i==0&&(std::abs(v.tau-1)>1e-14||v.pathThermalRadiance!=0||v.pathScatteringRadiance!=0))++failed;
            if(i<100){const auto v2=p.query(band,a,b,vis,rh,sun,true,1,false,64);maxSplit=std::max(maxSplit,std::abs(v2.tau-v.tau));}
        }
    }
    unsigned boundaryQueries=0; double maxAdjacentTau=0, maxUniformSplit=0;
    for(double h:{3000.,20000.})for(double d:{.1,.5,1.})for(int band:{0,2}){
        HwaAtmosphere::Geo a(0,0,h),b=a;b.lon=2*std::asin(d/(2*(6378137+h)))*180/HwaAtmosphere::pi;
        const auto sun=HwaAtmosphere::sunEcef(a,0,40);
        auto one=p.query(band,a,b,6,85,sun,true,1,false,1),many=p.query(band,a,b,6,85,sun,true,1,false,64);
        double error=std::max(std::abs(one.tau-many.tau),std::max(std::abs(one.pathThermalRadiance-many.pathThermalRadiance),std::abs(one.pathScatteringRadiance-many.pathScatteringRadiance)));
        maxUniformSplit=std::max(maxUniformSplit,error);if(error>1e-11)++failed;
        auto zero=p.query(band,b,b,6,85,sun);if(zero.directSolarIrradiance!=many.directSolarIrradiance||zero.heatingDirectWm2!=many.heatingDirectWm2)++failed;
    }
    for(double h:{3000.,11000.,20000.})for(double sza:{0.,73.,88.,90.,92.,96.,100.,120.,180.})for(int band:{0,2}){
        HwaAtmosphere::Geo a(0,0,h),b=a;double previous=1;
        for(int n=0;n<=500;++n){double d=n*100.;b.lon=2*std::asin(d/(2*(6378137+h)))*180/HwaAtmosphere::pi;
            auto v=p.query(band,a,b,6,85,HwaAtmosphere::sunEcef(a,0,90-sza));++boundaryQueries;
            if(!v.usableForRendering||!std::isfinite(v.tau)||!std::isfinite(v.pathThermalRadiance)||!std::isfinite(v.pathScatteringRadiance))++failed;
            maxAdjacentTau=std::max(maxAdjacentTau,std::abs(v.tau-previous));previous=v.tau;
        }
        for(double ht:{3000.,11000.,20000.}){b=HwaAtmosphere::Geo(0,0,ht);auto v=p.query(band,a,b,23,30,HwaAtmosphere::sunEcef(a,0,90-sza));++boundaryQueries;if(!v.usableForRendering)++failed;}
        for(double pole:{-90.,90.}){a=HwaAtmosphere::Geo(pole,180,h);b=HwaAtmosphere::Geo(pole,-180,h);auto v=p.query(band,a,b,23,30,HwaAtmosphere::sunEcef(a,0,90-sza));++boundaryQueries;if(!v.usableForRendering||!std::isfinite(v.directSolarIrradiance))++failed;}
    }
    auto invalid=p.query(0,HwaAtmosphere::Geo(91,0,3000),HwaAtmosphere::Geo(0,0,3000),6,60,HwaAtmosphere::Vec(1,0,0));if(invalid.usableForRendering)++failed;
    std::cout<<std::setprecision(17)<<"{\"schema\":\"P20M3.ProviderScan.1\",\"seed\":20261021,\"states\":2400,\"band_queries\":"<<tested<<",\"failures\":"<<failed<<",\"max_tau_32_vs_64_segments\":"<<maxSplit<<",\"quality\":\"ENGINEERING_APPROX\",\"strict_accuracy\":\"PENDING\"}"<<std::endl;
    std::cerr<<"boundaryQueries="<<boundaryQueries<<" maxAdjacentTau100m="<<maxAdjacentTau<<" maxUniformSplit1vs64="<<maxUniformSplit<<" failures="<<failed<<std::endl;
    return failed?1:0;
}
