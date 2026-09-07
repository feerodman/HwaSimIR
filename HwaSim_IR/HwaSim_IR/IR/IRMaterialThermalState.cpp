#include "IRMaterialThermalState.h"

#include <algorithm>
#include <cmath>

namespace
{
const double kSigma = 5.670374419e-8;
double Clamp(double v,double lo,double hi){return std::max(lo,std::min(hi,v));}
}

IRMaterialThermalOutput IRMaterialThermalModel::update(const IRMaterialThermalInput& input,
	const IRMaterialThermalProperties& p,const IRMaterialThermalOptions& o,IRMaterialThermalState* state)const
{
	IRMaterialThermalOutput out;
	if(state==nullptr){out.fallbackReason="state_missing";return out;}
	if(!o.enabled){out.solarDeltaK=state->solarDeltaK;out.fallbackReason="disabled";return out;}
	const double thickness=p.effectiveThicknessM>1.0e-6?p.effectiveThicknessM:o.defaultEffectiveThicknessM;
	const double capacity=p.densityKgM3*p.specificHeatJkgK*thickness;
	if(!std::isfinite(capacity)||capacity<=1.0){out.fallbackReason="invalid_areal_heat_capacity";return out;}
	const double dt=Clamp(input.dtSec,0.0,1.0);
	const double baseline=std::max(1.0,input.baseTempK+input.aeroDeltaK);
	const double h=std::max(0.0,o.convectionBaseWm2K+o.convectionSpeedCoeffWm2KPerSqrtMps*std::sqrt(std::max(0.0,input.targetSpeedMps)));
	const std::array<std::array<double,3>,6> normals={{{{1,0,0}},{{-1,0,0}},{{0,1,0}},{{0,-1,0}},{{0,0,1}},{{0,0,-1}}}};
	double sunLen=std::sqrt(input.sunDirectionLocal[0]*input.sunDirectionLocal[0]+input.sunDirectionLocal[1]*input.sunDirectionLocal[1]+input.sunDirectionLocal[2]*input.sunDirectionLocal[2]);
	if(sunLen<1.0e-9)sunLen=1.0;
	for(size_t i=0;i<6;++i)
	{
		const double ndotl=std::max(0.0,(normals[i][0]*input.sunDirectionLocal[0]+normals[i][1]*input.sunDirectionLocal[1]+normals[i][2]*input.sunDirectionLocal[2])/sunLen);
		const double qSolar=Clamp(p.solarAbsorptivity,0.0,1.0)*(std::max(0.0,input.directIrradianceWm2)*ndotl*Clamp(input.sunVisibilityThermal,0.0,1.0)+std::max(0.0,input.diffuseIrradianceWm2)*Clamp(o.skyFactor,0.0,1.0));
		const double temp=std::max(1.0,baseline+state->solarDeltaK[i]);
		const double qConv=h*(temp-input.airTempK);
		const double qRad=Clamp(p.thermalEmissivity,0.0,1.0)*kSigma*(std::pow(temp,4.0)-std::pow(std::max(1.0,input.environmentTempK),4.0));
		const double conductionLimit=std::max(0.0,o.coreRelaxationWm2K);
		const double qCond=std::min(std::max(0.0,p.conductivityWmK/thickness),conductionLimit)*(temp-baseline);
		state->solarDeltaK[i]=Clamp(state->solarDeltaK[i]+dt*(qSolar-qConv-qRad-qCond)/capacity,-50.0,std::max(0.0,o.maxSolarDeltaK));
		out.peakSolarFluxWm2=std::max(out.peakSolarFluxWm2,qSolar);
		out.finalTempK[i]=baseline+state->solarDeltaK[i];
	}
	state->initialized=true;out.solarDeltaK=state->solarDeltaK;out.representativeSolarDeltaK=*std::max_element(state->solarDeltaK.begin(),state->solarDeltaK.end());out.arealHeatCapacityJm2K=capacity;out.convectionCoefficientWm2K=h;out.valid=true;out.fallbackReason="none";return out;
}
