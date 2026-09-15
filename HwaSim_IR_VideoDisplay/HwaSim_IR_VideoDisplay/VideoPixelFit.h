#pragma once
#include <algorithm>
#include <cmath>
// All arguments/results are physical pixels. Qt DPR is applied only at the UI boundary.
namespace HwaVideoFit {
struct Size { int width, height; };
inline Size fit(int width, int height, double availableWidth, double availableHeight) {
    if(width<=0 || height<=0 || availableWidth<1 || availableHeight<1)return {0,0};
    const double scale=std::min(1.0,std::min(std::min(1024.0/width,1024.0/height),
        std::min(availableWidth/width,availableHeight/height)));
    return {std::max(1,int(std::floor(width*scale))),std::max(1,int(std::floor(height*scale)))};
}
}
