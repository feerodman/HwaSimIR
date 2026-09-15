// Independent executable linked to the production parser; no rendering/targets.
#include "../HwaSim_IR/HwaSim_IR/IR/IRConfig.h"
#include "../HwaSim_IR/HwaSim_IR/IR/IRJson.h"
#include <iostream>
#include <set>
int main(int argc,char** argv){
    if(argc!=4){std::cerr<<"usage: p10_profile_check file protocolBand output.json\n";return 64;}
    const int protocol=std::atoi(argv[2]);const IRBand band=IRBandFromProtocol(protocol);
    IRSensorProfileDatabase db;const bool valid=db.checkProfileFile(band,argv[1]);const auto& p=db.profileForBand(band);
    cv::FileStorage out(argv[3],cv::FileStorage::WRITE|cv::FileStorage::FORMAT_JSON);
    out<<"file"<<argv[1]<<"requestedProtocolBand"<<protocol<<"fixedBand"<<IRBandName(band)
       <<"rangeToleranceUm"<<1.e-4<<"loaderValid"<<int(valid)<<"effectiveBand"<<(db.supportsProductionProtocolBand(protocol)?IRBandName(band):"unavailable")
       <<"productionSupported"<<int(db.supportsProductionProtocolBand(protocol))<<"loadError"<<p.loadError
       <<"contentFingerprint"<<p.contentHash<<"fallbackUsedAsRequestedBand"<<0
       <<"activationScope"<<"profile eligibility only; valid interface geometry takes precedence; this standalone tool does not execute rendering modules";
    bool parsed=false;
    try{
        IRJson::Document d;d.load(argv[1]);const std::string prefix="Systems.SensorConfigurationSystem.";
        parsed=true;
        out<<"jsonParsed"<<1;
        const double lo=d.number(prefix+"SpectralResponseRangeLow",.1,100),hi=d.number(prefix+"SpectralResponseRangeHigh",.1,100);
        const auto expected=IRDefaultRangeForBand(band);
        const std::string unit=d.has("HwaSimIR.SpectralUnit")?d.string("HwaSimIR.SpectralUnit"):"um";
        out<<"low"<<lo<<"high"<<hi<<"unit"<<unit<<"unitSource"<<(d.has("HwaSimIR.SpectralUnit")?"explicit":"legacy_loader_contract")
           <<"expectedLowUm"<<expected.lowUm<<"expectedHighUm"<<expected.highUm
           <<"rangeMatches"<<int(unit=="um"&&std::abs(lo-expected.lowUm)<=1.e-4&&std::abs(hi-expected.highUm)<=1.e-4)
           <<"schemaVersion"<<(d.has("HwaSimIR.SchemaVersion")?d.integer("HwaSimIR.SchemaVersion",1,1):0)
           <<"displayStatus"<<(valid&&p.schemaVersion==1?"PASS":d.has("HwaSimIR.Display")?"not_validated_due_to_profile_failure":"unsupported_legacy_schema")
           <<"defaultPreset"<<p.defaultDisplayPreset<<"fieldConsumption"<<"[";
        const std::set<std::string> defaults={"Width","Height","FOVH"};
        const std::set<std::string> logs={"FOVV","FocalLength","DetectorPitch","LensFnumber","NoiseEquivalentTemperatureDifference","ADCBitNumber","BlackHot"};
        const std::set<std::string> display={"Gamma","Gain","OffsetGray","WhiteHot","Mode","ToneMap","Statistics.Method","Statistics.Size","Statistics.UpdateHz","Statistics.LowPercentile","Statistics.HighPercentile","Statistics.SmoothingAlpha","Statistics.MinimumInputSpan","Mapping.TargetLow","Mapping.TargetHigh","Mapping.MinGain","Mapping.MaxGain","Mapping.MinOffset","Mapping.MaxOffset"};
        for(const auto& f:d.types){
            if(f.second=='o'||f.second=='a'||f.first.empty())continue;
            const auto path=f.first;const auto tail=path.substr(path.find_last_of('.')+1);
            std::string state="unimplemented_not_consumed",consumer="none";
            if(path==prefix+tail){
                if(defaults.count(tail)){state="interface_invalid_defaults_only";consumer="sensor geometry fallback";}
                if(logs.count(tail)){state="stored_log_only";consumer="LogActiveIRSensorProfile";}
                if(tail=="SpectralResponseRangeLow"||tail=="SpectralResponseRangeHigh"){state="validation_and_diagnostic";consumer="fixed band gate; debug band range";}
            }
            if(path=="Systems.DisplaySystem.DisplayBits"){state="stored_log_only";consumer="actual transport remains RGB8";}
            const std::string presets="HwaSimIR.Display.Presets.";
            std::string presetName;
            if(path.find(presets)==0){const auto rest=path.substr(presets.size());const auto dot=rest.find('.');
                if(dot!=std::string::npos && display.count(rest.substr(dot+1))){presetName=rest.substr(0,dot);state="public_display";consumer="ApplyStage6DisplayConfig / final shader / sampled statistics";}}
            if(path=="HwaSimIR.Display.DefaultPreset"){state="public_display";consumer="default selection";}
            if(path=="HwaSimIR.SchemaVersion"||path=="HwaSimIR.Band"||path=="HwaSimIR.Revision"||path=="HwaSimIR.SpectralUnit"){state="validation_or_provenance";consumer="IRConfig";}
            out<<"{"<<"path"<<path<<"type"<<std::string(1,f.second)<<"contract"<<state<<"consumer"<<consumer
               <<"eligibleForThisProfileRequest"<<int(valid&&state!="unimplemented_not_consumed"&&(presetName.empty()||presetName==p.defaultDisplayPreset))
               <<"presetName"<<presetName<<"}";
        }
        out<<"]";
    }catch(const std::exception& e){out<<"auditError"<<e.what();}
    if(!parsed)out<<"jsonParsed"<<0;
    out.release();return valid&&db.supportsProductionProtocolBand(protocol)?0:2;
}
