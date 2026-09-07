#include "IRSolarHeatingLut.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>

namespace
{
const double kEps = 1.0e-8;
const double kAltitudeGridSnapKm = 1.0e-3;
std::string Trim(const std::string& v) { const size_t b=v.find_first_not_of(" \t\r\n\""); if(b==std::string::npos)return std::string(); const size_t e=v.find_last_not_of(" \t\r\n\""); return v.substr(b,e-b+1); }
std::vector<std::string> Split(const std::string& line) { std::vector<std::string> out; std::string v; std::istringstream s(line); while(std::getline(s,v,','))out.push_back(Trim(v)); return out; }
std::string Text(const std::vector<std::string>& v,const std::map<std::string,size_t>& c,const char* n){const auto i=c.find(n);return i==c.end()||i->second>=v.size()?std::string():v[i->second];}
double Number(const std::vector<std::string>& v,const std::map<std::string,size_t>& c,const char* n){try{return std::stod(Text(v,c,n));}catch(...){return std::numeric_limits<double>::quiet_NaN();}}
bool Same(double a,double b){return std::abs(a-b)<=kEps;}
std::string Merge(const std::string&a,const std::string&b){return a==b?a:"interpolated_multiple_cases";}
}

bool IRSolarHeatingLut::load(const std::string& filePath)
{
	std::ifstream file(filePath.c_str()); if(!file.is_open())return false;
	std::string line; if(!std::getline(file,line))return false;
	const std::vector<std::string> header=Split(line); std::map<std::string,size_t> columns;
	for(size_t i=0;i<header.size();++i)columns[header[i]]=i;
	const char* required[]={"band","atmosphere_model","aerosol_model","humidity_profile","target_alt_km","visibility_km","solar_zenith_deg","direct_shortwave_solar_irradiance_W_m2","diffuse_shortwave_down_irradiance_W_m2","irradiance_unit","source_case_ids","source_files"};
	for(size_t i=0;i<sizeof(required)/sizeof(required[0]);++i)if(columns.find(required[i])==columns.end())return false;
	std::vector<Entry> loaded;
	while(std::getline(file,line))
	{
		if(Trim(line).empty())continue; const std::vector<std::string> values=Split(line);
		if(Text(values,columns,"band")!="SOLAR_SHORTWAVE_0.30_2.50_UM"||Text(values,columns,"irradiance_unit")!="W/m^2")continue;
		Entry e; e.atmosphereModel=Text(values,columns,"atmosphere_model"); e.aerosolModel=Text(values,columns,"aerosol_model"); e.humidityProfile=Text(values,columns,"humidity_profile");
		e.targetAltKm=Number(values,columns,"target_alt_km"); e.visibilityKm=Number(values,columns,"visibility_km"); e.solarZenithDeg=Number(values,columns,"solar_zenith_deg");
		e.direct=Number(values,columns,"direct_shortwave_solar_irradiance_W_m2"); e.diffuse=Number(values,columns,"diffuse_shortwave_down_irradiance_W_m2"); e.sourceCaseIds=Text(values,columns,"source_case_ids"); e.sourceFiles=Text(values,columns,"source_files");
		if(std::isfinite(e.targetAltKm)&&std::isfinite(e.visibilityKm)&&e.visibilityKm>0.0&&std::isfinite(e.solarZenithDeg)&&std::isfinite(e.direct)&&e.direct>=0.0&&std::isfinite(e.diffuse)&&e.diffuse>=0.0)loaded.push_back(e);
	}
	if(loaded.empty())return false; m_entries.swap(loaded); m_loadedPath=filePath; return true;
}
bool IRSolarHeatingLut::empty()const{return m_entries.empty();}
const std::string& IRSolarHeatingLut::loadedPath()const{return m_loadedPath;}
size_t IRSolarHeatingLut::entryCount()const{return m_entries.size();}

IRSolarHeatingResult IRSolarHeatingLut::query(const IRSolarHeatingQuery& query)const
{
	IRSolarHeatingResult r; if(m_entries.empty())return r;
	if(!std::isfinite(query.targetAltKm)||!std::isfinite(query.visibilityKm)||query.visibilityKm<=0.0||!std::isfinite(query.solarZenithDeg)){r.fallbackReason="invalid_query";return r;}
	std::vector<const Entry*> candidates;
	for(size_t i=0;i<m_entries.size();++i){const Entry&e=m_entries[i];if(e.atmosphereModel==query.atmosphereModel&&e.aerosolModel==query.aerosolModel&&e.humidityProfile==query.humidityProfile)candidates.push_back(&e);}
	if(candidates.empty()){r.fallbackReason="category_missing";return r;}
	Sample sample; if(!interpolate(candidates,query,0,sample,r))return r;
	r.valid=true;r.directIrradianceWm2=sample.direct;r.diffuseDownIrradianceWm2=sample.diffuse;r.interpolationMode="staged_linear_targetAlt_visibility_solarZenith";r.fallbackReason="none";r.sourceCaseIds=sample.cases;r.sourceFiles=sample.files;return r;
}

bool IRSolarHeatingLut::interpolate(const std::vector<const Entry*>& entries,const IRSolarHeatingQuery&q,size_t axis,Sample& sample,IRSolarHeatingResult& error)const
{
	if(axis>=3){if(entries.size()!=1){error.fallbackReason="cell_missing_or_duplicate";error.fallbackAxis="cell";return false;}sample.direct=entries[0]->direct;sample.diffuse=entries[0]->diffuse;sample.cases=entries[0]->sourceCaseIds;sample.files=entries[0]->sourceFiles;return true;}
	std::vector<double> values;for(size_t i=0;i<entries.size();++i){const double v=axisValue(*entries[i],axis);if(std::find_if(values.begin(),values.end(),[v](double x){return Same(x,v);})==values.end())values.push_back(v);}std::sort(values.begin(),values.end());
	const double requested=queryValue(q,axis);if(values.empty()||requested<values.front()-kEps||requested>values.back()+kEps){error.fallbackReason="out_of_range";error.fallbackAxis=axisName(axis);error.fallbackQuery=requested;error.fallbackMin=values.empty()?0.0:values.front();error.fallbackMax=values.empty()?0.0:values.back();return false;}
	double low=values.front(),high=values.back();for(size_t i=0;i<values.size();++i){const double snap=axis==0?kAltitudeGridSnapKm:kEps;if(std::abs(values[i]-requested)<=snap){low=high=values[i];break;}if(values[i]<requested)low=values[i];if(values[i]>requested){high=values[i];break;}}
	auto subset=[&](double selected){std::vector<const Entry*> out;for(size_t i=0;i<entries.size();++i)if(Same(axisValue(*entries[i],axis),selected))out.push_back(entries[i]);return out;};
	if(Same(low,high))return interpolate(subset(low),q,axis+1,sample,error);
	Sample a,b;if(!interpolate(subset(low),q,axis+1,a,error)||!interpolate(subset(high),q,axis+1,b,error))return false;const double t=(requested-low)/(high-low);sample.direct=a.direct+(b.direct-a.direct)*t;sample.diffuse=a.diffuse+(b.diffuse-a.diffuse)*t;sample.cases=Merge(a.cases,b.cases);sample.files=Merge(a.files,b.files);return true;
}
double IRSolarHeatingLut::axisValue(const Entry&e,size_t a){return a==0?e.targetAltKm:(a==1?e.visibilityKm:e.solarZenithDeg);}
double IRSolarHeatingLut::queryValue(const IRSolarHeatingQuery&q,size_t a){return a==0?q.targetAltKm:(a==1?q.visibilityKm:q.solarZenithDeg);}
const char* IRSolarHeatingLut::axisName(size_t a){return a==0?"targetAltKm":(a==1?"visibilityKm":"solarZenithDeg");}
