#pragma once
#include "VideoEncoder.h"
#include <algorithm>
#include <cmath>
#include <sstream>

// Conservative Annex A checks for the supported progressive 8-bit 4:2:0,
// B=0, one-reference configuration. We never rewrite SPS bytes to fake a level.
inline bool ResolveH264Level(const VideoEncoderConfig& c,bool mpp,int& level,std::string& error) {
    if(c.h264Profile!="baseline" && c.h264Profile!="main" && c.h264Profile!="high") {
        error="unsupported H264Profile (baseline/main/high)";return false;
    }
    if(c.h264ReferenceFrames!=1 || !c.h264RepeatHeaders) {
        error="this build supports H264ReferenceFrames=1 and H264RepeatHeaders=true only";return false;
    }
    if(c.h264RateControl!="backend_default" && c.h264RateControl!="cbr") {
        error="unsupported H264RateControl (backend_default/cbr)";return false;
    }
    if(c.width<=0 || c.height<=0 || c.fps<=0 || c.bitrateKbps<1) {error="invalid H264 dimensions/fps/bitrate";return false;}
    struct Limit {int id,mbps,fs,dpb,kbps;};
    static const Limit limits[]={
        {30,40500,1620,8100,10000},{31,108000,3600,18000,14000},
        {32,216000,5120,20480,20000},{40,245760,8192,32768,20000},
        {41,245760,8192,32768,50000},{42,522240,8704,34816,50000},
        {50,589824,22080,110400,135000},{51,983040,36864,184320,240000},
        {52,2073600,36864,184320,240000}};
    const long long w=(c.width+15)/16,h=(c.height+15)/16,fs=w*h,mbps=fs*c.fps;
    int requested=0;
    if(c.h264Level!="auto") {
        for(const auto& v:limits)if(c.h264Level==std::to_string(v.id/10)+"."+std::to_string(v.id%10))requested=v.id;
        if(!requested){error="unsupported H264Level; supported auto,3.0..5.2 table entries";return false;}
    }
    for(const auto& v:limits) {
        if(requested && v.id!=requested)continue;
        // Preserve the existing MPP default request of 4.0 when auto suffices.
        if(!requested && mpp && v.id<40)continue;
        const double factor=c.h264Profile=="high"?1.25:1.0;
        const double peakKbps=c.bitrateKbps*(mpp?17.0/16.0:1.0);
        if(fs<=v.fs && mbps<=v.mbps && fs*c.h264ReferenceFrames<=v.dpb &&
           w*w<=8LL*v.fs && h*h<=8LL*v.fs && peakKbps<=v.kbps*factor) {level=v.id;return true;}
    }
    std::ostringstream msg;msg<<"H264Level capacity exceeded: size="<<c.width<<"x"<<c.height
        <<" fps="<<c.fps<<" macroblocks="<<fs<<" MBPS="<<mbps<<" requested="<<c.h264Level;
    error=msg.str();return false;
}

inline void AddH264Aud(std::vector<std::uint8_t>& bytes) {
    const std::uint8_t aud[]={0,0,0,1,9,0xf0}; // primary_pic_type=7, rbsp_stop_one_bit.
    bytes.insert(bytes.begin(),aud,aud+sizeof(aud));
}
