#pragma once
#include <algorithm>
#include <cmath>

namespace IRAutoMapping {
struct Target { double gain, offset, confidence; };
inline double clamp(double x,double lo,double hi){return (std::max)(lo,(std::min)(hi,x));}
// Near-constant samples provide progressively less evidence for expansion.
// Blend the complete affine mapping with identity, continuously, in its signed
// input domain. At MinimumInputSpan the established percentile mapping is exact.
inline Target target(double low,double high,double minimumSpan,double targetLow,double targetHigh,
                     double minGain,double maxGain,double minOffset,double maxOffset){
    const double span=(std::max)(0.0,high-low);
    const double t=clamp(span/(std::max)(minimumSpan,1.e-8),0.0,1.0);
    const double confidence=t*t*(3.0-2.0*t);
    const double expandedGain=clamp((targetHigh-targetLow)/(std::max)(span,1.e-8),minGain,maxGain);
    const double expandedOffset=clamp(targetLow-expandedGain*low,minOffset,maxOffset);
    return {1.0+confidence*(expandedGain-1.0),confidence*expandedOffset,confidence};
}
// Existing smoothingAlpha denotes one nominal statistics interval. Integrate
// its time constant over simulation time, not over the number of rendered AUs.
inline double smoothing(double alpha,double hz,double elapsedSeconds){
    alpha=clamp(alpha,0.,1.);
    if(elapsedSeconds<=0.)return 0.;
    return 1.-std::pow(1.-alpha,(std::max)(0.1,hz)*elapsedSeconds);
}
}
