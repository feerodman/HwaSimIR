#include "IR/IRConfig.h"
#include "IR/IRJson.h"
#include "IR/IRSensorModel.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
static void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
int main(int argc,char** argv){
    if(argc==4){
        IRSensorProfileDatabase db;require(!db.loadFromDirectoryCandidates({argv[1],argv[2]}),"invalid root unexpectedly valid");
        require(db.loadedDirectory()==argv[1],"mixed release roots");
        require(!db.profileForBand(IRBand::MidWaveInfrared).loadedFromFile,"invalid MWIR replaced from fallback directory");
        require(db.profileForBand(IRBand::NearInfrared).loadedFromFile,"valid NIR lost its own status");
        std::cout<<"[P6ConfigIsolation] PASS "<<argv[3]<<std::endl;return 0;
    }
    if(argc!=3)return 2;
    IRSensorProfileDatabase db;require(db.loadFromDirectoryCandidates({argv[1]}),"required profiles invalid");
    auto p=db.profileForBand(IRBand::MidWaveInfrared);
    require(p.loadedFromFile&&p.displayPresets.at("Legacy").gamma==1.0,"legacy display changed");
    IRSensorModel model;auto geom=model.BuildSensorDisplayConfig(800,800,1,100000,20);
    require(geom.width==800&&geom.height==800,"interface geometry overwritten");
    const std::string path=argv[2];int passed=0;
    const char* invalid[]={"{\"A\":1,}","{\"A\":1}x","{\"A\":1e999}","{\"A\":NaN}","{\"A\":1,\"A\":2}","{\"A\":01}","{\"A\":true}","{\"A\":\"2\"}","{\"A\":null}"};
    for(auto s:invalid){std::ofstream(path)<<s;bool rejected=false;try{IRJson::Document d;d.load(path);d.number("A",0,100);}catch(...){rejected=true;}require(rejected,s);++passed;}
    std::ofstream(path)<<"{\"Other\":{\"Width\":999},\"Systems\":{\"Width\":800},\"A\":false}";
    IRJson::Document d;d.load(path);require(d.integer("Systems.Width",1,4096)==800&&!d.boolean("A"),"path/type lookup wrong");
    std::cout<<"[P6ConfigCheck] PASS malformed_and_type_cases="<<passed<<" exact_path=1 geometry=800x800 legacyEquivalent=1"<<std::endl;
}
