#pragma once
#include "IRJson.h"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996)
#endif
#include <iostream>
#include <algorithm>
// Explicit ordinary-geometry test presets; never business heat-source parameters.
struct P6GameConfig {
    bool enabled=false,legacy=false,large=false,nativeFov=false,existingTargets=false;
    std::string scene,view="separated",vfx="both",root;
    std::string shapeTextures[4],sheetTexture;int sheetChannel=0;float sheetWorldSize=9000,sheetDepthScale=.55f;
    int count=2,disableMask=0,steps=8;
    float radius=200,verticalRadius=140,density=1.4f,opticalDepth=2.5f;
    float cloudNir=.48f,cloudMwir=.34f;
    float coreLength=4.5f,bodyLength=9.f,smokeLength=14.f;
    float coreRadius=.48f,bodyRadius=.95f,smokeRadius=1.8f;
    float coreGray=.95f,bodyGray=.65f,smokeGray=.24f;
    void load(const std::string& configRoot){
        const char* s=std::getenv("P6Scene");if(!s||!*s)return;
        enabled=true;scene=s;root=configRoot;
        auto env=[](const char* key,const char* fallback){const char* p=std::getenv(key);return p?p:fallback;};
        existingTargets=std::string(env("P6ExistingTargets","0"))=="1";
        view=env("P6View","separated");vfx=env("P6Vfx","both");
        count=std::max(0,std::min(4,std::atoi(env("P6CloudCount","2"))));
        disableMask=std::atoi(env("P6DisableCloudMask","0"));legacy=std::string(env("P6LegacyArt","0"))=="1";
        large=std::string(env("P6LargeClouds","0"))=="1";nativeFov=std::string(env("P6NativeFov","0"))=="1";
        IRJson::Document w;w.load(root+"/Weather/game_environment.json");w.integer("SchemaVersion",1,1);
        radius=float(w.number("Cloud.RadiusXYM",20,1000));verticalRadius=float(w.number("Cloud.RadiusZM",20,800));
        steps=w.integer("Cloud.RaySteps",2,16);density=float(w.number("Cloud.Density",.01,10));opticalDepth=float(w.number("Cloud.OpticalDepth",.01,10));
        cloudNir=float(w.number("Cloud.LinearSourceNIR",0,1));cloudMwir=float(w.number("Cloud.LinearSourceMWIR",0,1));
        for(int i=0;i<4;++i)shapeTextures[i]=w.string("Cloud.ShapeTextures.Template"+std::to_string(i));
        sheetTexture=w.string("Sheet.Texture");sheetChannel=w.string("Sheet.MaskChannel")=="alpha"?1:0;
        sheetWorldSize=float(w.number("Sheet.WorldSizeM",500,50000));sheetDepthScale=float(w.number("Sheet.OpticalDepthScale",.01,2));
        IRJson::Document f;f.load(root+"/GameVFX/ordinary_nozzle.json");f.integer("SchemaVersion",1,1);
        coreLength=float(f.number("Core.LengthM",.1,100));coreRadius=float(f.number("Core.RadiusM",.01,10));coreGray=float(f.number("Core.LinearSource",0,1));
        bodyLength=float(f.number("Body.LengthM",.1,100));bodyRadius=float(f.number("Body.RadiusM",.01,10));bodyGray=float(f.number("Body.LinearSource",0,1));
        smokeLength=float(f.number("Smoke.LengthM",.1,100));smokeRadius=float(f.number("Smoke.RadiusM",.01,10));smokeGray=float(f.number("Smoke.LinearSource",0,1));
        std::cout<<"[P6Preset] scope=ordinary_test_only scene="<<scene<<" view="<<view<<" clouds="<<count<<" disabledMask="<<disableMask
            <<" weatherHash="<<w.hash<<" vfxHash="<<f.hash<<" legacyArt="<<legacy<<" nativeFov="<<nativeFov<<std::endl;
    }
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

