#pragma once

#include <string>

#include "IRModtranRadianceLut.h"
#include "IRTypes.h"

struct IRModtranTauQuery
{
	IRBand band;
	double observerAltKm;
	double targetAltKm;
	double rangeKm;
	double visibilityKm;
	double solarZenithDeg;
	std::string fallbackInput;

	IRModtranTauQuery();
};

struct IRModtranTauResult
{
	bool found;
	bool usedNearest;
	double tauUp;
	double tauDown;
	std::string interpolationMode;
	std::string fallbackState;

	IRModtranTauResult();
};

// Compatibility facade for the Stage3 atmosphere API. The formal M1 LUT has
// one LOS tau_up field; parsing and interpolation are delegated to the same
// SI-only LUT used by the radiance chain.
class IRModtranTauLut
{
public:
	bool load(const std::string& filePath);
	bool empty() const;
	const std::string& loadedPath() const;
	IRModtranTauResult query(const IRModtranTauQuery& query) const;

private:
	IRModtranRadianceLut m_formalSiLut;
};
