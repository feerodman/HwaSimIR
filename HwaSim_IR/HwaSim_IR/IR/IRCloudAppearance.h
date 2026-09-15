#pragma once
#include "IRJson.h"
#include "IRWorldCloudStreaming.h"
#include <fstream>
#include <cmath>
#include <iomanip>
#include <sstream>

// Shared normal/test appearance resource description. No positions or camera.
struct IRCloudAppearance {
    struct Template {std::string key,path,sha256,fnv,source,sourceSha;};
    bool enabled=false,bounded=true,densityLighting=false;
    std::string root,revision,hash,buildVersion,sheetTexture;
    std::vector<Template> templates;
    double latitude=40,longitude=116,altitude=0;
    int size=96,nearSteps=12,mediumSteps=10,farSteps=8;
    double cloudNir=.48,cloudMwir=.34,opticalDepth=2.5;
    double nearDiameterPx=0,mediumDiameterPx=0;
    int visibleStepBudget=0;
    double sheetPeriod=9000,sheetCoverage=30000,sheetAltitude=2500,sheetDepth=2,sheetGrazingFadeDegrees=8;
    IRWorldCloudStreamingConfig geometry;
    static std::string fnvHash(const unsigned char* data,size_t size) {
        std::uint64_t h=1469598103934665603ULL;
        for(size_t i=0;i<size;++i){h^=data[i];h*=1099511628211ULL;}
        std::ostringstream s;s<<std::hex<<std::setw(16)<<std::setfill('0')<<h;return s.str();
    }
    void load(const std::string& configRoot,const std::string& preset,bool legacyReference=false) {
        enabled=false;
        if(preset!="Legacy"&&preset!="GameWorld"&&preset!="GameWorldLegacyArt"&&preset!="CustomerDemo")throw std::runtime_error("unknown weather appearance preset: "+preset);
        root=configRoot;IRJson::Document d;d.load(root+(preset=="CustomerDemo"?"/Weather/world_cloud_demo.json":"/Weather/world_cloud_game.json"));
        d.integer("SchemaVersion",1,1);revision=d.string("Revision");hash=d.hash;
        if(d.string("World.Frame")!="WGS84_ENU_tangent"||d.string("World.Animation")!="Static")
            throw std::runtime_error("unsupported cloud world frame/time definition");
        latitude=d.number("World.LatitudeDeg",-89,89);longitude=d.number("World.LongitudeDeg",-180,180);
        altitude=d.number("World.OriginEllipsoidAltitudeM",-1000,20000);
        geometry.cellSizeM=d.number("World.CellSizeM",100,50000);
        geometry.weatherSeed=std::uint64_t(d.integer("World.Seed",0,2147483647));
        geometry.minCloudAltitudeM=d.number("World.MinAltitudeM",0,20000);
        geometry.maxCloudAltitudeM=d.number("World.MaxAltitudeM",geometry.minCloudAltitudeM,20000);
        geometry.minRadiusXYM=d.number("World.MinRadiusXYM",10,5000);
        geometry.maxRadiusXYM=d.number("World.MaxRadiusXYM",geometry.minRadiusXYM,5000);
        geometry.minRadiusZM=d.number("World.MinRadiusZM",10,5000);
        geometry.maxRadiusZM=d.number("World.MaxRadiusZM",geometry.minRadiusZM,5000);
        size=d.integer("Appearance.DensitySize",16,128);
        nearSteps=d.integer("Appearance.NearSteps",2,64);mediumSteps=d.integer("Appearance.MediumSteps",2,nearSteps);
        farSteps=d.integer("Appearance.FarSteps",2,mediumSteps);
        nearDiameterPx=d.has("Appearance.NearDiameterPx")?d.number("Appearance.NearDiameterPx",1,10000):0;
        mediumDiameterPx=d.has("Appearance.MediumDiameterPx")?d.number("Appearance.MediumDiameterPx",1,std::max(1.0,nearDiameterPx)):0;
        visibleStepBudget=d.has("Appearance.VisibleStepBudget")?d.integer("Appearance.VisibleStepBudget",4,128):0;
        opticalDepth=d.number("Appearance.OpticalDepth",.01,10);
        cloudNir=d.number("Appearance.LinearSourceNIR",0,1);cloudMwir=d.number("Appearance.LinearSourceMWIR",0,1);
        const std::string light=d.has("Appearance.ArtLightEncoding")?d.string("Appearance.ArtLightEncoding"):"LegacyPNG";
        if(light!="LegacyPNG" && light!="DensityCoefficientGx1.4")throw std::runtime_error("unsupported cloud art light encoding");
        densityLighting=light=="DensityCoefficientGx1.4" && (preset=="GameWorld"||preset=="CustomerDemo") && !legacyReference;
        sheetTexture=d.string("Sheet.Texture");sheetPeriod=d.number("Sheet.PeriodM",100,50000);
        sheetCoverage=d.number("Sheet.CoverageM",sheetPeriod,200000);
        sheetAltitude=d.number("Sheet.AltitudeM",0,20000);sheetDepth=d.number("Sheet.OpticalDepthScale",.01,8);
        sheetGrazingFadeDegrees=d.number("Sheet.GrazingFadeDegrees",0,45);
        bounded=preset!="GameWorldLegacyArt";
        IRJson::Document m;m.load(root+"/"+(legacyReference?"Weather/Derived/cloud_manifest.json":bounded?d.string("Appearance.Manifest"):"Weather/Derived/cloud_manifest_p6a.json"));
        buildVersion=m.string("BuildVersion");
        if(densityLighting && buildVersion!="P6D-density-light-1" && buildVersion!="P7-asymmetric-alpha-puffs-2")throw std::runtime_error("cloud lighting/cache version mismatch");
        if(m.integer("Size",16,128)!=size)throw std::runtime_error("cloud cache size mismatch");
        int count=m.integer("Count",1,16);templates.clear();
        for(int i=0;i<count;++i){
            const std::string p="Templates.T"+std::to_string(i)+".";Template t;
            t.key=m.string(p+"Key");t.path=m.string(p+"Path");t.sha256=m.string(p+"SHA256");t.fnv=m.string(p+"FNV64");
            t.source=m.string(p+"Source");t.sourceSha=m.string(p+"SourceSHA256");
            for(const auto& other:templates)if(t.key==other.key)throw std::runtime_error("duplicate cloud template key");
            templates.push_back(t);
        }
        enabled=preset!="Legacy";
    }
    std::vector<unsigned char> readTemplate(int index) const {
        const auto& t=templates.at(index);std::ifstream f((root+"/"+t.path).c_str(),std::ios::binary);
        std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());
        if(bytes.size()!=size_t(size)*size*size*4||fnvHash(bytes.data(),bytes.size())!=t.fnv)
            throw std::runtime_error("cloud cache content mismatch: "+t.path);
        return bytes;
    }
};
