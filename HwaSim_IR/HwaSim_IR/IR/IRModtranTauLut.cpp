#include "IRModtranTauLut.h"

IRModtranTauQuery::IRModtranTauQuery()
	: band(IRBand::MidWaveInfrared), observerAltKm(10.0), targetAltKm(10.0),
	rangeKm(1.0), visibilityKm(23.0), solarZenithDeg(45.0), fallbackInput("none")
{
}

IRModtranTauResult::IRModtranTauResult()
	: found(false), usedNearest(false), tauUp(1.0), tauDown(1.0),
	interpolationMode("none"), fallbackState("modtran_si_lut_missing")
{
}

bool IRModtranTauLut::load(const std::string& filePath)
{
	return m_formalSiLut.load(filePath);
}

bool IRModtranTauLut::empty() const
{
	return m_formalSiLut.empty();
}

const std::string& IRModtranTauLut::loadedPath() const
{
	return m_formalSiLut.loadedPath();
}

IRModtranTauResult IRModtranTauLut::query(const IRModtranTauQuery& query) const
{
	IRModtranTauResult result;
	IRModtranRadianceQuery formalQuery;
	formalQuery.band = query.band;
	formalQuery.atmosphereModel = "Mid-Latitude Summer";
	formalQuery.aerosolModel = "Rural";
	formalQuery.humidityProfile = "default";
	formalQuery.observerAltKm = query.observerAltKm;
	formalQuery.targetAltKm = query.targetAltKm;
	formalQuery.rangeKm = query.rangeKm;
	formalQuery.visibilityKm = query.visibilityKm;
	formalQuery.solarZenithDeg = query.solarZenithDeg;
	const IRModtranRadianceResult formal = m_formalSiLut.query(formalQuery);
	if (!formal.valid)
	{
		result.fallbackState = formal.fallbackReason;
		if (!formal.fallbackAxis.empty() && formal.fallbackAxis != "none")
		{
			result.fallbackState += ":" + formal.fallbackAxis;
		}
		return result;
	}
	result.found = true;
	result.usedNearest = false;
	result.tauUp = formal.tauUp;
	// The formal M1 schema does not claim a downward LOS tau. This legacy
	// compatibility field is an explicit unavailable sentinel and is not used.
	result.tauDown = 1.0;
	result.interpolationMode = formal.interpolationMode;
	result.fallbackState = "formal_tau_up_only";
	return result;
}
