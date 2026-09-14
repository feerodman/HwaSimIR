#pragma once
#include "CommonData.h"
#include <cstddef>
#include <cstring>
#include <cmath>
namespace HwaSensorFields {
enum Kind { Boolean, Integer, Real, Band };
struct Field { const char* name; const char* label; const char* unit; Kind kind; std::size_t offset; double initial, minimum, maximum; int decimals; };
inline const Field* fields() {
#define F(n,label,unit,kind,initial,min,max,dec) {#n,label,unit,kind,offsetof(BYHWICD::trackerSensorParam,n),initial,min,max,dec}
    static const Field list[] = {
        F(h264En,"H.264","",Boolean,1,0,1,0),
        F(noiseEn,"模糊与噪声","",Boolean,1,0,1,0),
        F(trackerSensorNoise,"噪声系数","系数",Real,.5,0,1000,6),
        F(realtimeAnnotation,"实时标注","",Boolean,1,0,1,0),
        F(saveMP4En,"保存 MP4","",Boolean,1,0,1,0),
        F(trackerSensorBand,"波段","协议值",Band,2,0,4,0),
        F(trackerSensorWidth,"编码宽度","px",Integer,800,1,8192,0),
        F(trackerSensorHeight,"编码高度","px",Integer,800,1,8192,0),
        F(trackerSensorViewMin,"最近距离","m",Integer,1,0,2000000,0),
        F(trackerSensorViewMax,"最远距离","m",Integer,200000,1,2000000,0),
        F(trackerSensorPixelAngle,"像元角度","μrad",Real,25,.000001,1000000,6),
        F(trackerX,"传感器 X","m",Real,0,-1000000,1000000,6),
        F(trackerY,"传感器 Y","m",Real,0,-1000000,1000000,6),
        F(trackerZ,"传感器 Z","m",Real,0,-1000000,1000000,6),
        F(trackerPitch,"传感器俯仰","°",Real,0,-360,360,6),
        F(trackerYaw,"传感器航向","°",Real,0,-360,360,6),
        F(trackerRoll,"传感器滚转","°",Real,0,-360,360,6),
        F(illuminatorX,"照明器 X","m",Real,0,-1000000,1000000,6),
        F(illuminatorY,"照明器 Y","m",Real,0,-1000000,1000000,6),
        F(illuminatorZ,"照明器 Z","m",Real,0,-1000000,1000000,6),
        F(illuminatorPitch,"照明器俯仰","°",Real,0,-360,360,6),
        F(illuminatorYaw,"照明器航向","°",Real,0,-360,360,6),
        F(illuminatorRoll,"照明器滚转","°",Real,0,-360,360,6),
        F(illuminatorAngle,"照明张角","mrad",Real,0,0,1000000,6),
        F(illuminatorSpotRad,"照明强度","模式原值",Real,0,0,1.e15,6),
        F(emitterSpotRadius,"发射器半径","px",Integer,0,0,8192,0),
        F(emitterSpotRad,"发射器亮度","接口原值",Real,0,0,1.e15,6)
    };
#undef F
    return list;
}
static const int count=27;
inline double get(const BYHWICD::trackerSensorParam& sensor,const Field& f) {
    const auto* p=reinterpret_cast<const unsigned char*>(&sensor)+f.offset;
    if(f.kind==Real){double v;std::memcpy(&v,p,sizeof(v));return v;}
    if(f.kind==Boolean){bool v;std::memcpy(&v,p,sizeof(v));return v?1:0;}
    int v;std::memcpy(&v,p,sizeof(v));return v;
}
inline bool set(BYHWICD::trackerSensorParam& sensor,const Field& f,double value) {
    if(!std::isfinite(value)||value<f.minimum||value>f.maximum||(f.kind!=Real&&std::floor(value)!=value))return false;
    auto* p=reinterpret_cast<unsigned char*>(&sensor)+f.offset;
    if(f.kind==Real)std::memcpy(p,&value,sizeof(value));
    else if(f.kind==Boolean){const bool v=value!=0;std::memcpy(p,&v,sizeof(v));}
    else {const int v=static_cast<int>(value);std::memcpy(p,&v,sizeof(v));}return true;
}
inline BYHWICD::trackerSensorParam defaults() {
    BYHWICD::trackerSensorParam sensor={};for(int i=0;i<count;++i)set(sensor,fields()[i],fields()[i].initial);return sensor;
}
}
