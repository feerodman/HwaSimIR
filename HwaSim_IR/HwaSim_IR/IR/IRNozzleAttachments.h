#pragma once
#include "IRJson.h"
#include <map>

// Authored from the shipped OBJ nozzle rims. Lengths are game-art choices.
// Only render transforms are changed; the engine/thermal output is untouched.
struct IRNozzleAttachment {
    int count=1;
    LPoint3f position[2];
    float radiusX=.1f,radiusZ=.1f,coreLength=1.f,haloLength=2.f;
};
inline const IRNozzleAttachment* FindNozzleAttachment(const std::string& name){
    // Explicit before/after fixture only. Normal rendering uses the asset map.
    const char* old=std::getenv("P6LegacyNozzleAttachment");
    if(old&&std::string(old)=="1")return nullptr;
    static const std::map<std::string,IRNozzleAttachment> database=[](){
        std::map<std::string,IRNozzleAttachment> out;
        IRJson::Document doc;
        try{
            doc.load("Config/GameVFX/nozzle_attachments.json");doc.integer("SchemaVersion",1,1);
            for(const char* key:{"F35","F22","AIM120D","AIM9X"}){
                const std::string p=std::string("Assets.")+key+".";
                IRNozzleAttachment a;a.count=doc.integer(p+"NozzleCount",1,2);
                for(int n=0;n<a.count;++n){
                    const auto q=p+"Nozzle"+std::to_string(n)+".";
                    a.position[n]=LPoint3f(float(doc.number(q+"X",-20,20)),float(doc.number(q+"Y",-30,30)),float(doc.number(q+"Z",-20,20)));
                }
                a.radiusX=float(doc.number(p+"RadiusX",.01,5));a.radiusZ=float(doc.number(p+"RadiusZ",.01,5));
                a.coreLength=float(doc.number(p+"CoreLength",.05,30));a.haloLength=float(doc.number(p+"HaloLength",.05,50));
                out[key]=a;
                std::cout<<"[NozzleAttachment] asset="<<key<<" mesh="<<doc.string(p+"Mesh")<<" count="<<a.count
                    <<" nozzle0="<<a.position[0]<<" radius="<<a.radiusX<<","<<a.radiusZ
                    <<" hash="<<doc.hash<<" source=mesh_local_geometry thermalParametersChanged=0"<<std::endl;
            }
        }catch(const std::exception& e){
            out.clear();std::cerr<<"[NozzleAttachment][ERROR] fallback=legacy_geometry reason="<<e.what()<<std::endl;
        }
        return out;
    }();
    auto it=database.find(name);return it==database.end()?nullptr:&it->second;
}
