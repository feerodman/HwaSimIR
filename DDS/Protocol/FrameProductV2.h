#pragma once

// Optional H.264 user_data_unregistered SEI. It belongs to the same access unit
// as the picture, and leaves CommonData and all V1 DDS topics unchanged.
// Integers are little endian, doubles are IEEE754 binary64. No native struct
// padding or host ABI is transmitted. Unknown versions are never guessed.
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <random>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include "CommonData.h"

namespace HwaFrameV2 {
static const unsigned char uuid[16] = {0x48,0x77,0x61,0x53,0x69,0x6d,0x49,0x52,0x9a,0x72,0x4f,0x10,0x81,0x6d,0x02,0x01};
struct Product {
    std::string session, channel, annotation;
    std::uint64_t generation=0, run=0, frameSeq=0, sourceSeq=0;
    std::int64_t ptsMs=0, acceptedNs=0, executeNs=0, captureNs=0, encodeNs=0;
    std::int64_t writerSubmitNs=0;
    int platID=0, sensorID=0, round=0;
    bool annotationEnabled=false;
    bool saveRequested=false;
    BYHWICD::DisplayC2cObjTrackingData realtime = {};
};
inline std::string newSession() {
    std::random_device random;
    std::ostringstream out; out << std::hex << std::setfill('0');
    for(int i=0;i<4;++i) out << std::setw(8) << random();
    return out.str();
}
inline const std::string& processSession() { static const std::string session=newSession();return session; }
struct Writer {
    std::vector<std::uint8_t> bytes;
    void number(std::uint64_t x,int n) { for(int i=0;i<n;++i) bytes.push_back(static_cast<std::uint8_t>(x>>(8*i))); }
    void value(int& x) { number(static_cast<std::uint32_t>(x),4); }
    void value(bool& x) { number(x?1:0,1); }
    void value(std::uint64_t& x) { number(x,8); }
    void value(std::int64_t& x) { number(static_cast<std::uint64_t>(x),8); }
    void value(double& x) { std::uint64_t v; static_assert(sizeof(x)==8,"binary64 required"); std::memcpy(&v,&x,8);number(v,8); }
    void value(std::string& x) { number(x.size(),4);bytes.insert(bytes.end(),x.begin(),x.end()); }
};
struct Reader {
    const std::uint8_t* p; std::size_t size, pos=0;
    Reader(const std::uint8_t* data,std::size_t count):p(data),size(count){}
    std::uint64_t number(int n) { if(pos+static_cast<std::size_t>(n)>size) throw std::runtime_error("truncated product"); std::uint64_t v=0;for(int i=0;i<n;++i)v|=std::uint64_t(p[pos++])<<(8*i);return v; }
    void value(int& x) { x=static_cast<std::int32_t>(number(4)); }
    void value(bool& x) { const auto v=number(1);if(v>1)throw std::runtime_error("invalid bool");x=v!=0; }
    void value(std::uint64_t& x) { x=number(8); }
    void value(std::int64_t& x) { x=static_cast<std::int64_t>(number(8)); }
    void value(double& x) { const auto v=number(8);std::memcpy(&x,&v,8); }
    void value(std::string& x) { auto n=number(4);if(n>1024*1024||n>size-pos)throw std::runtime_error("invalid product string");x.assign(reinterpret_cast<const char*>(p+pos),static_cast<std::size_t>(n));pos+=static_cast<std::size_t>(n); }
};
template<class IO> inline void spatial(IO& io,BYHWICD::SpatialState& s) {
    io.value(s.lat);io.value(s.lon);io.value(s.alt);io.value(s.yaw);io.value(s.pitch);io.value(s.roll);io.value(s.speed);
}
template<class IO> inline void fields(IO& io,Product& p) {
    io.value(p.session);io.value(p.channel);io.value(p.generation);io.value(p.run);
    io.value(p.frameSeq);io.value(p.sourceSeq);io.value(p.ptsMs);
    io.value(p.acceptedNs);io.value(p.executeNs);io.value(p.captureNs);io.value(p.encodeNs);io.value(p.writerSubmitNs);
    io.value(p.platID);io.value(p.sensorID);io.value(p.round);io.value(p.annotationEnabled);io.value(p.saveRequested);io.value(p.annotation);
    auto& d=p.realtime;
    io.value(d.flag);io.value(d.platID);io.value(d.sensorID);io.value(d.time);spatial(io,d.platLoc);
    auto& w=d.weaponState;
    io.value(w.targetType);io.value(w.targetPlatID);io.value(w.targetID);
    for(auto& a:w.xxOutAng)io.value(a);
    io.value(w.lookatEn);io.value(w.illuminatorEn);for(auto& a:w.offsetAng)io.value(a);
    io.value(w.viewValid);io.value(w.damageFlag);io.value(w.strikeFlag);io.value(w.strikePart);
    io.value(d.targetNumValid);
    for(auto& t:d.targetState) {
        io.value(t.targetType);io.value(t.targetPlatID);io.value(t.targetID);io.value(t.engineState);io.value(t.viewValid);
        spatial(io,t.targetLoc);io.value(t.targetState);
    }
}
inline std::vector<std::uint8_t> serialize(Product p) { Writer w;w.number(2,4);fields(w,p);return w.bytes; }
inline bool deserialize(const std::uint8_t* data,std::size_t size,Product& product) {
    try { Reader r(data,size);if(r.number(4)!=2)return false;Product p;fields(r,p);
        if(r.pos!=size||p.session.empty()||p.frameSeq==0||p.generation==0)return false;product=std::move(p);return true;
    } catch(const std::exception&) { return false; }
}
inline std::vector<std::uint8_t> sei(const Product& product) {
    const auto data=serialize(product);std::vector<std::uint8_t> rbsp;rbsp.push_back(5);
    std::size_t size=data.size()+16;while(size>=255){rbsp.push_back(255);size-=255;}rbsp.push_back(static_cast<std::uint8_t>(size));
    rbsp.insert(rbsp.end(),uuid,uuid+16);rbsp.insert(rbsp.end(),data.begin(),data.end());rbsp.push_back(0x80);
    std::vector<std::uint8_t> nal={0,0,0,1,6};int zeros=0;
    for(auto b:rbsp){if(zeros>=2&&b<=3){nal.push_back(3);zeros=0;}nal.push_back(b);zeros=b==0?zeros+1:0;}return nal;
}
inline std::size_t startCode(const std::uint8_t* data,std::size_t n,std::size_t i) {
    if(i+3<=n&&data[i]==0&&data[i+1]==0){if(data[i+2]==1)return 3;if(i+4<=n&&data[i+2]==0&&data[i+3]==1)return 4;}return 0;
}
// Extract exactly one recognized product. Duplicate/malformed identities fail
// closed, without disturbing compatibility decoding of the original AU.
inline bool extract(const std::uint8_t* data,std::size_t n,Product& result) {
    bool found=false;
    for(std::size_t i=0;i<n;) {
        const auto prefix=startCode(data,n,i);if(!prefix){++i;continue;}
        const auto begin=i+prefix;std::size_t end=begin+1;while(end<n&&!startCode(data,n,end))++end;i=end;
        if(begin>=n||(data[begin]&31)!=6)continue;
        std::vector<std::uint8_t> rbsp;int zeros=0;
        for(std::size_t j=begin+1;j<end;++j){auto b=data[j];if(zeros>=2&&b==3){zeros=0;continue;}rbsp.push_back(b);zeros=b==0?zeros+1:0;}
        for(std::size_t j=0;j<rbsp.size();) {
            std::size_t type=0,size=0;while(j<rbsp.size()&&rbsp[j]==255){type+=255;++j;}if(j>=rbsp.size())break;type+=rbsp[j++];
            while(j<rbsp.size()&&rbsp[j]==255){size+=255;++j;}if(j>=rbsp.size())break;size+=rbsp[j++];
            if(size>rbsp.size()-j)break;
            if(type==5&&size>=16&&std::memcmp(rbsp.data()+j,uuid,16)==0) {
                if(found||!deserialize(rbsp.data()+j+16,size-16,result))return false;found=true;
            }j+=size;
        }
    }return found;
}
inline void insert(std::vector<std::uint8_t>& au,const Product& product) {
    const auto nal=sei(product);
    // Insert before the first VCL, after AUD/SPS/PPS; retains one AU/sample.
    std::size_t at=au.size();
    for(std::size_t i=0;i<au.size();++i){auto s=startCode(au.data(),au.size(),i);if(s&&i+s<au.size()) {auto type=au[i+s]&31;if(type>=1&&type<=5){at=i;break;}i+=s;}}
    au.insert(au.begin()+at,nal.begin(),nal.end());
}
inline bool stampWriter(std::vector<std::uint8_t>& au,std::int64_t submitNs,Product& product) {
    if(!extract(au.data(),au.size(),product))return false;
    for(std::size_t i=0;i<au.size();) {
        const auto prefix=startCode(au.data(),au.size(),i);if(!prefix){++i;continue;}
        auto end=i+prefix+1;while(end<au.size()&&!startCode(au.data(),au.size(),end))++end;
        Product candidate;
        if(extract(au.data()+i,end-i,candidate)) { au.erase(au.begin()+i,au.begin()+end);break; }i=end;
    }
    product.writerSubmitNs=submitNs;insert(au,product);return true;
}
}
