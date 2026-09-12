#include "IRConfig.h"
#include "IRJson.h"
#include <iostream>
namespace {
const IRBand bands[]={IRBand::Visible,IRBand::NearInfrared,IRBand::ShortWaveInfrared,IRBand::MidWaveInfrared,IRBand::LongWaveInfrared};
std::string join(const std::string& d,const std::string& f){return d+"/"+f;}
bool exists(const std::string& p){return bool(std::ifstream(p.c_str()));}
}
IRSensorProfileDatabase::IRSensorProfileDatabase():m_loaded(false){resetToFallbacks();}
void IRSensorProfileDatabase::resetToFallbacks(){
    m_profiles.clear();for(auto b:bands)m_profiles[b]=IRDefaultSensorProfile(b);
    m_loaded=false;m_loadedDirectory.clear();
}
bool IRSensorProfileDatabase::loadFromDirectoryCandidates(const std::vector<std::string>& directories){
    resetToFallbacks();
    for(const auto& dir:directories){
        bool contains=false;for(auto b:bands)contains=contains||exists(join(dir,IRSensorProfileFileName(b)));
        if(!contains)continue;
        // One release root. Never fill invalid/missing profiles from another version.
        m_loadedDirectory=dir;
        for(auto b:bands)loadProfileFromFile(b,join(dir,IRSensorProfileFileName(b)));
        m_loaded=m_profiles[IRBand::NearInfrared].loadedFromFile&&m_profiles[IRBand::MidWaveInfrared].loadedFromFile;
        std::cout<<"[SensorProfiles] requiredBands=NIR,MWIR allRequiredValid="<<m_loaded<<" root="<<dir<<" mixedRoots=0"<<std::endl;
        return m_loaded;
    }
    std::cerr<<"[SensorProfiles][ERROR] no_profile_root"<<std::endl;return false;
}
const IRSensorProfile& IRSensorProfileDatabase::profileForBand(IRBand b)const{
    auto i=m_profiles.find(b);return i==m_profiles.end()?m_profiles.find(IRBand::MidWaveInfrared)->second:i->second;
}
const IRSensorProfile& IRSensorProfileDatabase::profileForProtocolBand(int b)const{return profileForBand(IRBandFromProtocol(b));}
std::vector<IRSensorProfile> IRSensorProfileDatabase::allProfiles()const{std::vector<IRSensorProfile> v;for(auto b:bands)v.push_back(profileForBand(b));return v;}
bool IRSensorProfileDatabase::loaded()const{return m_loaded;}
const std::string& IRSensorProfileDatabase::loadedDirectory()const{return m_loadedDirectory;}
bool IRSensorProfileDatabase::loadProfileFromFile(IRBand band,const std::string& path){
    IRSensorProfile p=IRDefaultSensorProfile(band);p.sourcePath=path;
    try{
        IRJson::Document d;d.load(path);p.contentHash=d.hash;
        const std::string s="Systems.SensorConfigurationSystem.";
        p.width=d.integer(s+"Width",256,4096);p.height=d.integer(s+"Height",256,4096);
        p.spectralLowUm=d.number(s+"SpectralResponseRangeLow",.1,100);
        p.spectralHighUm=d.number(s+"SpectralResponseRangeHigh",.1,100);
        const auto range=IRDefaultRangeForBand(band);
        if(std::abs(p.spectralLowUm-range.lowUm)>1.e-4||std::abs(p.spectralHighUm-range.highUm)>1.e-4)
            throw std::runtime_error("spectral_range_conflicts_with_fixed_band");
        auto number=[&](const char* key,double& target,double lo,double hi){if(d.has(s+key))target=d.number(s+key,lo,hi);};
        number("FOVH",p.fovHDeg,.001,179);number("FOVV",p.fovVDeg,.001,179);
        number("FocalLength",p.focalLengthMm,.001,100000);number("DetectorPitch",p.detectorPitchMm,.000001,100);
        number("LensFnumber",p.lensFNumber,.01,1000);number("NoiseEquivalentTemperatureDifference",p.netdK,0,100000);
        if(d.has(s+"ADCBitNumber"))p.adcBits=d.integer(s+"ADCBitNumber",1,32);
        if(d.has("Systems.DisplaySystem.DisplayBits"))p.displayBits=d.integer("Systems.DisplaySystem.DisplayBits",1,32);
        if(d.has(s+"BlackHot"))p.blackHot=d.boolean(s+"BlackHot");
        if(d.has("HwaSimIR")){
            p.schemaVersion=d.integer("HwaSimIR.SchemaVersion",1,1);
            if(d.string("HwaSimIR.Band")!=IRBandName(band))throw std::runtime_error("profile_band_mismatch");
            p.revision=d.string("HwaSimIR.Revision");p.defaultDisplayPreset=d.string("HwaSimIR.Display.DefaultPreset");
            const std::string base="HwaSimIR.Display.Presets";d.require(base,'o');
            for(auto it=d.at(base).begin();it!=d.at(base).end();++it){
                const std::string name=(*it).name(),q=base+"."+name+".";IRDisplayPreset v;
                v.gamma=d.number(q+"Gamma",.1,5);v.gain=d.number(q+"Gain",.001,100);
                v.offsetGray=d.number(q+"OffsetGray",-255,255);v.whiteHot=d.boolean(q+"WhiteHot");
                const std::string mode=d.string(q+"Mode");
                if(mode!="Fixed"&&mode!="Auto")throw std::runtime_error("unknown_display_mode: "+q);
                v.automatic=mode=="Auto";p.displayPresets[name]=v;
            }
            if(!p.displayPresets.count(p.defaultDisplayPreset))throw std::runtime_error("default_display_preset_missing");
        }
        p.usedFields="Systems.SensorConfigurationSystem.SpectralResponseRangeLow/High (validated fixed band),HwaSimIR.Display";
        p.fallbackFields="Systems.SensorConfigurationSystem.Width,Height,FOVH only when interface geometry invalid";
        p.ignoredPresagisFields="legacy gain/MTF/noise/intensifier/QE/integration/well capacity not implemented";
        p.loadedFromFile=true;
    }catch(const cv::Exception& e){p=IRDefaultSensorProfile(band);p.sourcePath=path;p.loadError="opencv_json: "+e.err;}
    catch(const std::exception& e){p=IRDefaultSensorProfile(band);p.sourcePath=path;p.loadError=e.what();}
    m_profiles[band]=p;
    std::cout<<"[SensorProfile] Band="<<IRBandName(band)<<" valid="<<p.loadedFromFile<<" file="<<path
        <<" version="<<p.schemaVersion<<" revision="<<p.revision<<" hash="<<p.contentHash
        <<" width="<<p.width<<" height="<<p.height<<" geometrySource=profile_defaults_only"
        <<" DisplayBitsPath=Systems.DisplaySystem.DisplayBits unsupportedLegacySystems=1 error="<<p.loadError<<std::endl;
    return p.loadedFromFile;
}
