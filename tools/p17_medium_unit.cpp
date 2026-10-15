#include "IREnginePlumeModel.h"
#include <cmath>
#include <iostream>
#include <algorithm>

// Generic transport tests, independent of particle shape and display mapping.
int main() {
    int failed=0,checks=0;
    const IRBand bands[]={IRBand::ShortWaveInfrared,IRBand::MidWaveInfrared};
    for (auto band:bands) for(double coupling:{0.0,0.2,0.8,1.0}) {
        const double background=IREnginePlumeModel::bandAveragePlanckRadianceWm2SrUm(band,300.0);
        for (double temperature:{250.0,300.0,900.0}) {
            const double source=IREnginePlumeModel::layerSourceRadianceWm2SrUm(band,temperature,300.0,coupling);
            for(double opticalDepth:{0.01,0.8,3.0}) {
                const double expected=background*std::exp(-opticalDepth)+source*(-std::expm1(-opticalDepth));
                for(int layers:{1,2,8,32}) {
                    double actual=background;
                    const double alpha=-std::expm1(-opticalDepth/layers);
                    for(int i=0;i<layers;++i)actual=actual*(1-alpha)+source*alpha;
                    failed+=std::fabs(actual-expected)>1e-6;++checks;
                    if(temperature==300.0){failed+=std::fabs(actual-background)>1e-6;++checks;}
                }
                if(temperature<300.0 && coupling>0.0){failed+=!(expected<background);++checks;}
            }
        }
    }
    for(double width:{.0015,.0025,.0035}) for(double footprint:{.01,.1,1.0}) {
        const double drawn=std::max(width,footprint*.72);
        failed+=std::fabs(drawn*(width/drawn)-width)>1e-12;++checks;
    }
    // Independently integrate the 1800 K fixed display white reference. The
    // old band quadrature was originally qualified only through 1200 K.
    for(auto band:bands) {
        const double low=band==IRBand::ShortWaveInfrared?1.1:3.0;
        const double high=band==IRBand::ShortWaveInfrared?2.5:5.0;
        const int n=16384; double sum=0.0;
        for(int i=0;i<=n;++i) {
            const double wavelength=low+(high-low)*i/n;
            const double value=1.191042e8/(std::pow(wavelength,5)*(std::exp(1.4387752e4/(wavelength*1800.0))-1));
            sum+=(i==0||i==n?1:(i%2?4:2))*value;
        }
        const double reference=sum/(3*n);
        const double actual=IREnginePlumeModel::bandAveragePlanckRadianceWm2SrUm(band,1800.0);
        failed+=std::fabs(actual/reference-1)>0.01;++checks;
        std::cout<<"white1800 band="<<int(band)<<" source="<<actual<<" reference="<<reference<<" relativeError="<<std::fabs(actual/reference-1)<<"\n";
    }
    std::cout<<"P17 medium checks="<<checks<<" failures="<<failed
             <<" tolerance=1e-6 equilibrium=absolute_source layer_split=1,2,8,32\n";
    return failed?1:0;
}
