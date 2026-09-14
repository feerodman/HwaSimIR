#include "IRAutoMapping.h"
#include <cassert>
#include <cmath>
#include <iostream>
int main(){
    using namespace IRAutoMapping;
    const auto a=target(.4,.4,.02,.12,.78,.25,8,-8,8);
    assert(a.gain==1. && a.offset==0.);
    const auto b=target(.4,.42,.02,.12,.78,.25,8,-8,8);
    // Independent arithmetic: 0.66/0.02 capped at 8; offset 0.12 - 8*0.4.
    assert(std::abs(b.gain-8)<1.e-10 && std::abs(b.offset+3.08)<1.e-10);
    auto previous=target(.4,.4,.02,.12,.78,.25,8,-8,8);
    for(int i=1;i<=4000;++i){
        auto current=target(.4,.4+i*.00001,.02,.12,.78,.25,8,-8,8);
        assert(current.gain>0.); // fixed mappings remain strictly monotone
        assert(std::abs((current.gain*.4+current.offset)-(previous.gain*.4+previous.offset))<.001);
        previous=current;
    }
    const double once=smoothing(.15,5,1.);
    double accumulated=0.;for(int i=0;i<60;++i)accumulated+=(1-accumulated)*smoothing(.15,5,1./60);
    assert(std::abs(once-accumulated)<1.e-12);
    assert(std::abs(once-(1-std::pow(.85,5)))<1.e-12);
    assert(smoothing(.15,5,0)==0 && smoothing(.15,5,-1)==0);
    std::cout<<"PASS continuous threshold, exact established mapping, monotonicity, constant field and time integration\n";
}
