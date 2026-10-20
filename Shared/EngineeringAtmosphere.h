#pragma once
// P20M3 engineering model. Rectangular band mean SI quantities, NOT a MODTRAN LUT.
#include "Sha256File.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>

namespace HwaAtmosphere {
static const double pi = 3.14159265358979323846;
struct Vec {
    double x,y,z;
    Vec(double a=0,double b=0,double c=0):x(a),y(b),z(c){}
    Vec operator+(const Vec& b)const{return Vec(x+b.x,y+b.y,z+b.z);}
    Vec operator-(const Vec& b)const{return Vec(x-b.x,y-b.y,z-b.z);}
    Vec operator*(double s)const{return Vec(x*s,y*s,z*s);}
    double dot(const Vec& b)const{return x*b.x+y*b.y+z*b.z;}
    double norm()const{return std::sqrt(dot(*this));}
};
struct Geo { double lat,lon,alt; Geo(double a=0,double b=0,double c=0):lat(a),lon(b),alt(c){} };
inline bool legal(const Geo& g){return std::isfinite(g.lat)&&std::isfinite(g.lon)&&std::isfinite(g.alt)&&std::abs(g.lat)<=90&&std::abs(g.lon)<=180;}
inline Vec ecef(const Geo& g){double p=g.lat*pi/180,l=g.lon*pi/180,s=std::sin(p),c=std::cos(p),n=6378137/std::sqrt(1-0.0066943799901413165*s*s);return Vec((n+g.alt)*c*std::cos(l),(n+g.alt)*c*std::sin(l),(n*(1-0.0066943799901413165)+g.alt)*s);}
inline Vec sunEcef(const Geo& g,double az,double el){double p=g.lat*pi/180,l=g.lon*pi/180,a=az*pi/180,e=el*pi/180;return Vec(-std::sin(l),std::cos(l),0)*(std::cos(e)*std::sin(a))+Vec(-std::sin(p)*std::cos(l),-std::sin(p)*std::sin(l),std::cos(p))*(std::cos(e)*std::cos(a))+Vec(std::cos(p)*std::cos(l),std::cos(p)*std::sin(l),std::sin(p))*std::sin(e);}
inline double range(const Geo& a,const Geo& b){return (ecef(a)-ecef(b)).norm();}
inline double planck(double um,double t){double l=um*1e-6;return 1.191042972e-16/(std::pow(l,5)*std::expm1(0.01438776877/(l*t)))*1e-6;}
inline double meanPlanck(double lo,double hi,double t){double sum=0;for(int i=0;i<12;++i)sum+=planck(lo+(hi-lo)*(i+.5)/12,t);return sum/12;}
struct Result {
    bool usableForRendering=false;
    std::string runtimeStatus="INVALID",quality="ENGINEERING_APPROX",source="P20M3_GREY_GAS_1",parameterHash,inputError;
    std::string radianceUnit="W/(m^2 sr um)",irradianceUnit="W/(m^2 um)";
    double tau=1,pathThermalRadiance=0,pathScatteringRadiance=0,directSolarIrradiance=0,skyDiffuseIrradiance=0;
    double heatingDirectWm2=0,heatingDiffuseWm2=0,rangeM=0;
};
class Provider {
    struct Band {double lo,hi,k[3],w[3];} bands[2];
    bool ready_=false;
    std::string hash_;
    static double airT(double h){return 288.15-.0065*std::min(11000.,h);}
    static double scale(double h,double rh){return .15*std::exp(-h/8000)+.85*(rh/60)*std::exp(-h/2000);}
    static double aerosol(double h,double vis,double wave){return 3.912/(vis*1000)*std::pow(.55/wave,1.3)*std::exp(-h/1200);}
    // Spherical straight solar ray with geometric Earth occultation. No refraction claim.
    // Atmospheric column uses nonuniform midpoint quadrature out to 100 km.
    static void columns(double h,double mu,double& dryWet,double& aer,double rh){
        const double r=6371000+h, top=6471000;
        double length=-r*mu+std::sqrt(r*r*mu*mu+top*top-r*r);
        dryWet=0;aer=0;
        for(int i=0;i<24;++i){double t0=double(i)/24,t1=double(i+1)/24;
            double s0=length*t0*t0,s1=length*t1*t1,s=(s0+s1)/2;
            double z=std::sqrt(r*r+s*s+2*r*s*mu)-6371000;
            if(z<0){dryWet=1e12;aer=1e12;return;}
            dryWet+=scale(z,rh)*(s1-s0);aer+=std::exp(-z/1200)*(s1-s0);
        }
    }
    void illumination(const Band& b,double h,double mu,double vis,double rh,double distanceFactor,
                      double& direct,double& sky,double& heatDirect,double& heatDiffuse)const{
        double c,a;columns(h,mu,c,a,rh);
        const double wave=(b.lo+b.hi)/2, ae=aerosol(0,vis,wave);
        double tr=0;for(int j=0;j<3;++j)tr+=b.w[j]*std::exp(-b.k[j]*c-ae*a);
        const double solarSolidAngle=1361/(5.670374419e-8*std::pow(5772.,4)/pi);
        double toa=meanPlanck(b.lo,b.hi,5772)*solarSolidAngle*distanceFactor;
        // Broad isotropic single-scatter closure with continuous twilight tail.
        double elevation=std::asin(std::max(-1.,std::min(1.,mu)))*180/pi;
        double day=.5*(mu+std::sqrt(mu*mu+.0004));
        double twilight=std::exp(-std::pow(std::min(0.,elevation)/3.,2));
        double diffuseFraction=.12*(1-std::exp(-ae*1200))*twilight;
        direct=toa*tr;
        sky=toa*diffuseFraction*(day+.05)+pi*meanPlanck(b.lo,b.hi,airT(h))*(1-std::exp(-b.k[1]*8000*scale(h,rh)));
        double heatingToa=meanPlanck(.30,2.50,5772)*2.20*solarSolidAngle*distanceFactor;
        heatDirect=heatingToa*std::exp(-.00008*c-aerosol(0,vis,.7)*a);
        heatDiffuse=heatingToa*.12*(1-std::exp(-aerosol(0,vis,.7)*1200))*twilight*(day+.05);
    }
public:
    bool load(const std::string& path,const std::string& expected){
        ready_=false;try{hash_=HwaHash::Sha256File(path);}catch(const std::exception&){return false;}if(expected.size()!=64||hash_!=expected)return false;
        std::ifstream f(path);std::string version;f>>version;if(version!="P20M3_GREY_GAS_1")return false;
        for(int i=0;i<2;++i){Band& b=bands[i];f>>b.lo>>b.hi;for(int j=0;j<3;++j)f>>b.k[j];for(int j=0;j<3;++j)f>>b.w[j];
            double sum=0;for(int j=0;j<3;++j){if(!std::isfinite(b.k[j])||b.k[j]<0||!std::isfinite(b.w[j])||b.w[j]<0)return false;sum+=b.w[j];}
            if(!f||std::abs(sum-1)>1e-12||b.lo!=(i==0?1.1:3.)||b.hi!=(i==0?2.5:5.))return false;}
        std::string extra;if(f>>extra)return false;ready_=true;return true;
    }
    bool ready()const{return ready_;} const std::string& hash()const{return hash_;}
    Result query(int band,const Geo& observer,const Geo& target,double vis,double rh,const Vec& sun,
                 bool solarValid=true,double distanceFactor=1,bool background=false,int segments=32)const{
        Result o;o.parameterHash=hash_;
        if(!ready_){o.inputError="parameter_identity";return o;}
        if((band!=0&&band!=2)||!legal(observer)||!legal(target)||!std::isfinite(vis)||vis<=0||!std::isfinite(rh)||rh<0||rh>100||!std::isfinite(distanceFactor)||distanceFactor<=0){o.inputError="invalid_input";return o;}
        Vec end=ecef(observer),start=ecef(target),delta=end-start;double r=delta.norm();o.rangeM=r;
        if(!background&&(observer.alt<3000-1e-6||observer.alt>20000+1e-6||target.alt<3000-1e-6||target.alt>20000+1e-6||r>50000+1e-6)){o.inputError="outside_declared_target_domain";return o;}
        if(observer.alt<0||target.alt<0||observer.alt>100000||target.alt>100000||segments<1||segments>128){o.inputError="invalid_ray";return o;}
        if(!solarValid||!std::isfinite(sun.norm())||std::abs(sun.norm()-1)>1e-6){o.inputError="solar_utc_or_vector_invalid";return o;}
        const Band& b=bands[band==0?0:1];
        Vec up=start*(1/start.norm());double direct,sky,hd,hf;
        illumination(b,target.alt,up.dot(sun),vis,rh,distanceFactor,direct,sky,hd,hf);
        o.directSolarIrradiance=direct;o.skyDiffuseIrradiance=sky;o.heatingDirectWm2=hd;o.heatingDiffuseWm2=hf;
        o.tau=0;
        // Local geocentric reference radii interpolate between WGS84 endpoints.
        // Chord sag is retained; avoids Earth-scale float subtraction.
        const double radius0=start.norm()-target.alt,radius1=end.norm()-observer.alt;
        double tau[3]={1,1,1},thermal[3]={0,0,0},scatter[3]={0,0,0};
        for(int i=0;i<segments && r>0;++i){double t=(i+.5)/segments;Vec p=start+delta*t;
            double h=p.norm()-(radius0*(1-t)+radius1*t);
            if(h<0){o.inputError="earth_intersection";return o;}
            double sc=aerosol(h,vis,(b.lo+b.hi)/2),s=scale(h,rh),bb=meanPlanck(b.lo,b.hi,airT(h));
            double localD,localSky,unusedA,unusedB;
            illumination(b,h,(p*(1/p.norm())).dot(sun),vis,rh,distanceFactor,localD,localSky,unusedA,unusedB);
            double phase=1/(4*pi); // declared isotropic single scattering, no second Shader phase factor
            for(int j=0;j<3;++j){double absorption=b.k[j]*s+.1*sc,ext=b.k[j]*s+sc,x=ext*r/segments;
                double a=std::exp(-x),length= x==0?r/segments:-std::expm1(-x)/ext;
                tau[j]*=a;thermal[j]=a*thermal[j]+absorption*bb*length;
                scatter[j]=a*scatter[j]+.9*sc*localD*phase*length;}
        }
        for(int j=0;j<3;++j){o.tau+=b.w[j]*tau[j];o.pathThermalRadiance+=b.w[j]*thermal[j];o.pathScatteringRadiance+=b.w[j]*scatter[j];}
        o.usableForRendering=true;o.runtimeStatus=r==0?"ENGINEERING_ANALYTIC_ZERO_LOS":"ENGINEERING_APPROX";return o;
    }
};
}
