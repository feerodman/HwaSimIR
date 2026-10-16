#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// Engineering conventions for the unchanged customer IDL. No wire header.
namespace CustomerTX {
static const std::size_t ChunkCapacity=1400, RawCapacity=640000;
enum class Pixels { Gray8, RGB24 };
inline bool imageBytes(int width,int height,Pixels format,std::size_t& bytes) {
    bytes=0;
    if(width<=0 || height<=0)return false;
    const std::size_t w=static_cast<std::size_t>(width),h=static_cast<std::size_t>(height),bpp=format==Pixels::Gray8?1:3;
    if(w>(std::numeric_limits<std::size_t>::max)()/h || w*h>(std::numeric_limits<std::size_t>::max)()/bpp)return false;
    bytes=w*h*bpp;return true;
}
inline const char* topic(bool precise,bool compressed) {
    return compressed?(precise?"TX/HD264":"TX/LD264"):(precise?"TX/HDecode":"TX/LDecode");
}
inline const char* type(bool precise,bool compressed) {
    return compressed?(precise?"HD264Stream":"LD264Stream"):(precise?"HDDecoder":"LDDecoder");
}
template<class Sample,class Send>
bool fragments(const std::uint8_t* data,std::size_t size,Send send) {
    if(!data || !size)return false;
    for(std::size_t offset=0;offset<size;) {
        Sample sample={};
        const std::size_t count=(std::min)(ChunkCapacity,size-offset);
        sample.DataLength=static_cast<std::uint32_t>(count);
        std::memcpy(sample.VedioStream,data+offset,count);
        if(!send(sample))return false;
        offset+=count;
    }
    return true;
}
inline bool validLength(std::uint32_t n) { return n>0 && n<=ChunkCapacity; }
// BGR input is tightly packed or padded; output is top-to-bottom, no padding.
inline bool convertBgr(const std::uint8_t* data,int width,int height,int stride,bool flip,Pixels format,std::vector<std::uint8_t>& out) {
    std::size_t bytes;
    if(!data || !imageBytes(width,height,format,bytes) || width>(std::numeric_limits<int>::max)()/3 || stride<width*3)return false;
    out.resize(bytes);
    const std::size_t row=static_cast<std::size_t>(width)*(format==Pixels::Gray8?1:3);
    for(int y=0;y<height;++y) {
        const auto* src=data+static_cast<std::size_t>(flip?height-1-y:y)*stride;
        auto* dst=out.data()+static_cast<std::size_t>(y)*row;
        for(int x=0;x<width;++x) {
            if(format==Pixels::Gray8)dst[x]=static_cast<std::uint8_t>((29u*src[3*x]+150u*src[3*x+1]+77u*src[3*x+2]+128u)>>8);
            else {dst[3*x]=src[3*x+2];dst[3*x+1]=src[3*x+1];dst[3*x+2]=src[3*x];}
        }
    }
    return true;
}
}
