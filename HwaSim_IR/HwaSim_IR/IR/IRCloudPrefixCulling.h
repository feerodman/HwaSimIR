#pragma once
#include "luse.h"
#include <array>
#include <cmath>

// 仅用过相机原点的四个侧面，不用near/far裁掉相机到透明源之间的云。
// 外接球覆盖完整椭球；边界留浮点裕量，宁可多算，不遗漏边缘光程。
inline std::array<LVector3f,4> IRPrefixViewPlanes(const std::array<LVector3f,4>& rays){
    std::array<LVector3f,4> planes;
    LVector3f inside(0);for(const auto& ray:rays)inside+=ray;
    for(int i=0;i<4;++i){
        planes[i]=rays[i].cross(rays[(i+1)%4]);
        const float length=planes[i].length();
        if(length>1e-12f)planes[i]/=length;else planes[i]=LVector3f(0);
        if(planes[i].dot(inside)<0)planes[i]=-planes[i];
    }
    return planes;
}
inline bool IRPrefixSphereIntersectsView(const LPoint3f& center,float radius,const std::array<LVector3f,4>& planes){
    const float margin=1e-4f*(1.f+center.length()+radius);
    for(const auto& plane:planes)if(plane.dot(center)<-radius-margin)return false;
    return true;
}
