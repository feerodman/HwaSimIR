#pragma once
#include <cstdlib>
#include <cstdint>
#include <string>
#include <sstream>

namespace HwaFrameChart {
inline bool enabled(){
    static const bool on=[](){
#if defined(_MSC_VER)
        char* value=nullptr;std::size_t length=0;
        if(_dupenv_s(&value,&length,"P7FrameIdentityChart")!=0)return false;
        const bool result=value&&std::string(value)=="1";std::free(value);return result;
#else
        const char* value=std::getenv("P7FrameIdentityChart");return value&&std::string(value)=="1";
#endif
    }();
    return on;
}
inline std::string annotate(std::string json,std::uint64_t seq,int width,int height){
    if(!enabled()||json.empty()||json.back()!='}')return json;
    // Explicit ordinary test chart, in original image pixel coordinates.
    // The validator independently recovers the raster code and rectangle.
    std::ostringstream chart;
    chart<<",\"diagnosticChart\":{\"synthetic\":true,\"sourceSeq\":"<<seq
        <<",\"code12\":"<<(seq%4096)<<",\"left\":"<<(100+(seq%4096)*3%560)
        <<",\"top\":300,\"width\":40,\"height\":60,\"imageWidth\":"<<width<<",\"imageHeight\":"<<height<<"}}";
    json.pop_back();return json+chart.str();
}
}
