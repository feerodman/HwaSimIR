#include "../HwaSim_IR/HwaSim_IR/IR/IRRadianceModelV2.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
int main(){
    unsigned failures=0;double worstRelative=0,worstAbsolute=0,worstQuadratureRelative=0;
    for(IRBand band:{IRBand::ShortWaveInfrared,IRBand::MidWaveInfrared}){
        float table[8192];for(int i=0;i<8192;++i)table[i]=float(IRRadianceModelV2::bandAveragePlanckRadianceWm2SrUm(band,120.+i*.5));
        for(int n=0;n<=81900;++n){double t=120+n*.05;int i=std::min(8190,int((t-120)*2));float fraction=float((t-120)*2-i);
            double value=table[i]*(1.f-fraction)+table[i+1]*fraction;
            double reference=IRRadianceModelV2::bandAveragePlanckRadianceWm2SrUm(band,t),error=std::abs(value-reference);
            worstAbsolute=std::max(worstAbsolute,error);
            if(reference>1e-7)worstRelative=std::max(worstRelative,error/reference);
            // Frozen incremental table budget, separate from atmosphere accuracy.
            if(error>1e-10+.0025*std::abs(reference))++failures;
        }
        for(int n=0;n<=196;++n){double t=220+n*5+.5,lo=band==IRBand::ShortWaveInfrared?1.1:3.,hi=band==IRBand::ShortWaveInfrared?2.5:5.;
            double sum=0,step=(hi-lo)/16384;
            for(int j=0;j<=16384;++j)sum+=(j==0||j==16384?1:j%2?4:2)*IRRadianceModelV2::planckRadianceWm2SrUm(lo+j*step,t);
            double ref=sum*step/3/(hi-lo);int i=int((t-120)*2);double test=table[i];
            double relative=std::abs(test-ref)/ref;worstQuadratureRelative=std::max(worstQuadratureRelative,relative);
            if(relative>.01)++failures;
        }
    }
    std::cout<<std::setprecision(17)<<"{\"temperature_queries\":163802,\"temperature_domain_K\":[120,4215],\"table_step_K\":0.5,\"additional_error_budget\":\"1e-10 W/(m2 sr um) + 0.0025*reference\",\"max_absolute_error_W_m2_sr_um\":"<<worstAbsolute<<",\"max_relative_above_1e_minus_7\":"<<worstRelative<<",\"max_relative_vs_16384_interval_quadrature_220_to_1200K\":"<<worstQuadratureRelative<<",\"failures\":"<<failures<<"}"<<std::endl;
    return failures?1:0;
}
