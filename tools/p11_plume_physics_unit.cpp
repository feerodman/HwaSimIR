#include "IREnginePlumeModel.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
double planckReference(double wavelengthUm, double temperatureK)
{
	const double c1 = 1.191042e8;
	const double c2 = 1.4387752e4;
	const double lambda = std::max(0.1, std::min(100.0, wavelengthUm));
	const double temperature = std::max(1.0, std::min(6000.0, temperatureK));
	const double exponent = std::max(1.0e-9, std::min(700.0, c2 / (lambda * temperature)));
	return c1 / (std::pow(lambda, 5.0) * (std::exp(exponent) - 1.0));
}

double bandReference(IRBand band, double temperatureK)
{
	if (band != IRBand::ShortWaveInfrared && band != IRBand::MidWaveInfrared)
	{
		const double center = band == IRBand::NearInfrared ? 0.90 :
			(band == IRBand::LongWaveInfrared ? 10.0 : 0.55);
		return planckReference(center, temperatureK);
	}
	const double lowUm = band == IRBand::ShortWaveInfrared ? 1.1 : 3.0;
	const double highUm = band == IRBand::ShortWaveInfrared ? 2.5 : 5.0;
	const int intervals = 16384;
	const double step = (highUm - lowUm) / static_cast<double>(intervals);
	double sum = planckReference(lowUm, temperatureK) + planckReference(highUm, temperatureK);
	for (int index = 1; index < intervals; ++index)
	{
		sum += (index % 2 == 0 ? 2.0 : 4.0) *
			planckReference(lowUm + step * static_cast<double>(index), temperatureK);
	}
	return sum * step / 3.0 / (highUm - lowUm);
}

const char* bandName(IRBand band)
{
	return band == IRBand::ShortWaveInfrared ? "SWIR" :
		(band == IRBand::MidWaveInfrared ? "MWIR" : "OTHER");
}

std::string cleanDetail(const std::string& value)
{
	std::string result = value;
	std::replace(result.begin(), result.end(), ',', ';');
	return result;
}

class TestLog
{
public:
	explicit TestLog(const std::string& path)
		: m_stream(path.c_str()), m_failures(0)
	{
		m_stream << "test,band,temperature_K,actual,expected,relative_error,status,detail\n";
	}

	void numeric(const std::string& test, const std::string& band, double temperatureK,
		double actual, double expected, double relativeTolerance, double absoluteTolerance,
		const std::string& detail)
	{
		const double absError = std::fabs(actual - expected);
		const double relError = absError / std::max(std::fabs(expected), 1.0e-300);
		const bool finite = std::isfinite(actual) && std::isfinite(expected);
		const bool pass = finite && (absError <= absoluteTolerance || relError <= relativeTolerance);
		write(test, band, temperatureK, actual, expected, relError, pass, detail);
	}

	void boolean(const std::string& test, bool pass, const std::string& detail)
	{
		write(test, "NA", 0.0, pass ? 1.0 : 0.0, 1.0, pass ? 0.0 : 1.0, pass, detail);
	}

	int failures() const { return m_failures; }

private:
	void write(const std::string& test, const std::string& band, double temperatureK,
		double actual, double expected, double relativeError, bool pass,
		const std::string& detail)
	{
		m_stream << test << ',' << band << ',' << std::setprecision(12) << temperatureK << ','
			<< actual << ',' << expected << ',' << relativeError << ','
			<< (pass ? "PASS" : "FAIL") << ',' << cleanDetail(detail) << '\n';
		if (!pass)
		{
			++m_failures;
			std::cerr << "FAIL " << test << " band=" << band
				<< " actual=" << actual << " expected=" << expected
				<< " detail=" << detail << std::endl;
		}
	}

	std::ofstream m_stream;
	int m_failures;
};

IREnginePlumeOutput evaluateSteady(const std::string& profilePath,
	const std::string& platformName, IRBand band, bool engineState,
	float displayGain, float opacityScale)
{
	IREnginePlumeModel model;
	if (!model.load(profilePath))
	{
		throw std::runtime_error("profile load failed");
	}
	IREnginePlumeInput input;
	input.platformName = platformName;
	input.runtimeKey = std::string("unit-") + platformName + "-" + bandName(band);
	input.engineState = engineState;
	input.dtSec = 100.0f;
	input.ambientTempK = 300.0f;
	input.band = band;
	input.options.displayGain = displayGain;
	input.options.coreDisplayGain = 1.0f;
	input.options.haloDisplayGain = 1.0f;
	input.options.opacityScale = opacityScale;
	input.options.coreOpacityScale = 1.0f;
	input.options.haloOpacityScale = 1.0f;
	return model.update(input);
}
}

int main(int argc, char** argv)
{
	if (argc != 3)
	{
		std::cerr << "usage: p11_plume_physics_unit <profile.json> <output.csv>" << std::endl;
		return 2;
	}

	TestLog log(argv[2]);
	const IRBand bands[] = { IRBand::ShortWaveInfrared, IRBand::MidWaveInfrared };
	const double temperaturesK[] = { 250.0, 300.0, 500.0, 900.0, 1200.0, 1800.0 };
	for (size_t bandIndex = 0; bandIndex < 2; ++bandIndex)
	{
		for (size_t tempIndex = 0; tempIndex < 6; ++tempIndex)
		{
			const IRBand band = bands[bandIndex];
			const double temp = temperaturesK[tempIndex];
			const double actual = IREnginePlumeModel::bandAveragePlanckRadianceWm2SrUm(band, temp);
			const double expected = bandReference(band, temp);
			const double legacyCenter = planckReference(
				band == IRBand::ShortWaveInfrared ? 1.8 : 4.0, temp);
			const double legacyBiasPercent = 100.0 * (legacyCenter - expected) /
				std::max(expected, 1.0e-300);
			std::ostringstream detail;
			detail << "unit=W/(m^2 sr um); legacy_center=" << std::setprecision(12)
				<< legacyCenter << "; legacy_bias_percent=" << legacyBiasPercent;
			log.numeric("band_mean_vs_16384_simpson", bandName(band), temp,
				actual, expected, 0.01, 1.0e-12, detail.str());
		}
	}

	for (size_t bandIndex = 0; bandIndex < 2; ++bandIndex)
	{
		const IRBand band = bands[bandIndex];
		const IREnginePlumeOutput output = evaluateSteady(
			argv[1], "AIM120D", band, true, 1.0f, 1.0f);
		IREnginePlumeModel profileModel;
		profileModel.load(argv[1]);
		const IREnginePlumeProfile& profile = profileModel.profileForPlatform("AIM120D");
		const double coreExpected = IREnginePlumeModel::layerSourceRadianceWm2SrUm(
			band, output.coreTempK, std::max(300.0f, profile.ambientMixK),
			profile.core.bandEmissivity.forBand(band));
		const double haloExpected = IREnginePlumeModel::layerSourceRadianceWm2SrUm(
			band, output.haloTempK, std::max(300.0f, profile.ambientMixK),
			profile.halo.bandEmissivity.forBand(band));
		log.numeric("core_source_radiance", bandName(band), output.coreTempK,
			output.coreSourceRadianceWm2SrUm, coreExpected, 2.0e-6, 1.0e-8,
			"emissivity times band-mean Planck excess; opacity excluded");
		log.numeric("halo_source_radiance", bandName(band), output.haloTempK,
			output.haloSourceRadianceWm2SrUm, haloExpected, 2.0e-6, 1.0e-8,
			"emissivity times band-mean Planck excess; opacity excluded");
		log.numeric("core_opacity_applied_once", bandName(band), output.coreTempK,
			output.coreEmittedRadianceWm2SrUm,
			output.coreSourceRadianceWm2SrUm * output.coreOpacity, 2.0e-6, 1.0e-8,
			"emitted=source times opacity exactly once");
		log.numeric("halo_opacity_applied_once", bandName(band), output.haloTempK,
			output.haloEmittedRadianceWm2SrUm,
			output.haloSourceRadianceWm2SrUm * output.haloOpacity, 2.0e-6, 1.0e-8,
			"emitted=source times opacity exactly once");
		log.boolean(std::string("bounded_emissivity_") + bandName(band),
			output.coreEffectiveEmissivity >= 0.0f && output.coreEffectiveEmissivity <= 1.0f &&
			output.haloEffectiveEmissivity >= 0.0f && output.haloEffectiveEmissivity <= 1.0f,
			"physical emissivity range is 0..1");
		log.boolean(std::string("local_two_layer_geometry_") + bandName(band),
			output.coreLengthM > 0.0f && output.haloLengthM > output.coreLengthM &&
			output.coreRadiusRootM > 0.0f && output.haloRadiusTailM > output.coreRadiusTailM,
			"plume remains localized core/halo geometry");

		const IREnginePlumeOutput displayZero = evaluateSteady(
			argv[1], "AIM120D", band, true, 0.0f, 1.0f);
		log.numeric("display_gain_does_not_change_core_SI", bandName(band), output.coreTempK,
			displayZero.coreSourceRadianceWm2SrUm, output.coreSourceRadianceWm2SrUm,
			2.0e-6, 1.0e-8, "display preview is separate from physical radiance");
		log.boolean(std::string("display_zero_only_zeroes_preview_") + bandName(band),
			displayZero.coreGray == 0.0f && displayZero.coreNodeVisible,
			"formal node visibility follows emitted SI radiance");

		const IREnginePlumeOutput opacityHalf = evaluateSteady(
			argv[1], "AIM120D", band, true, 1.0f, 0.5f);
		log.numeric("opacity_scale_does_not_change_source", bandName(band), output.coreTempK,
			opacityHalf.coreSourceRadianceWm2SrUm, output.coreSourceRadianceWm2SrUm,
			2.0e-6, 1.0e-8, "opacity is not folded into source radiance");
		log.numeric("opacity_scale_changes_emitted_once", bandName(band), output.coreTempK,
			opacityHalf.coreEmittedRadianceWm2SrUm,
			output.coreEmittedRadianceWm2SrUm * 0.5, 2.0e-6, 1.0e-8,
			"halving opacity halves emitted contribution once");

		const IREnginePlumeOutput off = evaluateSteady(
			argv[1], "AIM120D", band, false, 1.0f, 1.0f);
		log.boolean(std::string("engine_off_disables_local_plume_") + bandName(band),
			!off.enabled && !off.nodeVisible && !off.coreEnabled && !off.haloEnabled,
			"engineState protocol semantics preserved; no whole-target heating");
		log.numeric("engine_off_core_radiance_zero", bandName(band), off.coreTempK,
			off.coreSourceRadianceWm2SrUm, 0.0, 0.0, 1.0e-8,
			"steady off state equals ambient mix temperature");

		IREnginePlumeModel transientModel;
		transientModel.load(argv[1]);
		IREnginePlumeInput transientInput;
		transientInput.platformName = "AIM120D";
		transientInput.runtimeKey = std::string("transient-") + bandName(band);
		transientInput.engineState = true;
		transientInput.dtSec = 100.0f;
		transientInput.ambientTempK = 300.0f;
		transientInput.band = band;
		transientModel.update(transientInput);
		transientInput.engineState = false;
		transientInput.dtSec = 0.0f;
		const IREnginePlumeOutput immediateOff = transientModel.update(transientInput);
		log.boolean(std::string("engine_off_immediate_visibility_gate_") + bandName(band),
			immediateOff.coreSourceRadianceWm2SrUm > 0.0f && !immediateOff.enabled &&
			!immediateOff.coreNodeVisible && !immediateOff.haloNodeVisible,
			"stored thermal state may cool but protocol off hides only local plume immediately");
	}

	IREnginePlumeModel civilProfileModel;
	log.boolean("civil_profile_load", civilProfileModel.load(argv[1]),
		"P11-CIVIL-VAN must be loaded explicitly rather than default fallback");
	const IREnginePlumeProfile& civilProfile =
		civilProfileModel.profileForPlatform("P11-CIVIL-VAN");
	const IREnginePlumeProfile& civilNormalizedAlias =
		civilProfileModel.profileForPlatform("p11 civil van");
	log.boolean("civil_profile_not_missile_default",
		std::fabs(civilProfile.core.temperatureK - 575.0f) < 1.0e-6f &&
		std::fabs(civilProfile.halo.temperatureK - 375.0f) < 1.0e-6f &&
		civilProfile.core.lengthM < 1.0f && civilProfile.halo.lengthM < 2.0f,
		"civil exhaust must not inherit 1520 K core or 5.2 m halo defaults");
	log.boolean("civil_profile_normalized_lookup",
		std::fabs(civilNormalizedAlias.core.temperatureK - civilProfile.core.temperatureK) < 1.0e-6f &&
		std::fabs(civilNormalizedAlias.localPos.x - civilProfile.localPos.x) < 1.0e-6f,
		"hyphen/space/case variants resolve through the full explicit profile key");
	log.boolean("civil_profile_tailpipe_alignment",
		std::fabs(civilProfile.localPos.x - 0.66f) < 1.0e-6f &&
		std::fabs(civilProfile.localPos.y + 2.88f) < 1.0e-6f &&
		std::fabs(civilProfile.localPos.z - 0.56f) < 1.0e-6f &&
		std::fabs(civilProfile.localDir.y + 1.0f) < 1.0e-6f,
		"profile origin matches the model tailpipe exit and points rearward");
	log.boolean("civil_profile_engine_gate_required",
		civilProfile.enabled && civilProfile.enabledByEngineState,
		"engineState=false is required to disable the civil exhaust");
	log.boolean("civil_profile_engineering_assumptions_frozen",
		std::fabs(civilProfile.core.opacity - 0.10f) < 1.0e-6f &&
		std::fabs(civilProfile.halo.opacity - 0.035f) < 1.0e-6f &&
		std::fabs(civilProfile.core.bandEmissivity.swir - 0.04f) < 1.0e-6f &&
		std::fabs(civilProfile.core.bandEmissivity.mwir - 0.16f) < 1.0e-6f &&
		std::fabs(civilProfile.halo.bandEmissivity.swir - 0.015f) < 1.0e-6f &&
		std::fabs(civilProfile.halo.bandEmissivity.mwir - 0.07f) < 1.0e-6f &&
		std::fabs(civilProfile.core.radiusRootM - 0.055f) < 1.0e-6f &&
		std::fabs(civilProfile.halo.radiusTailM - 0.30f) < 1.0e-6f,
		"small civil opacity/emissivity/geometry assumptions remain explicit and bounded");

	for (size_t bandIndex = 0; bandIndex < 2; ++bandIndex)
	{
		const IRBand band = bands[bandIndex];
		const IREnginePlumeOutput civilOn = evaluateSteady(
			argv[1], "P11-CIVIL-VAN", band, true, 1.0f, 1.0f);
		const IREnginePlumeOutput civilOff = evaluateSteady(
			argv[1], "P11-CIVIL-VAN", band, false, 1.0f, 1.0f);
		const double expectedCore = IREnginePlumeModel::layerSourceRadianceWm2SrUm(
			band, 575.0, 303.0, civilProfile.core.bandEmissivity.forBand(band));
		const double expectedHalo = IREnginePlumeModel::layerSourceRadianceWm2SrUm(
			band, 375.0, 303.0, civilProfile.halo.bandEmissivity.forBand(band));
		log.numeric("civil_core_source_radiance", bandName(band), civilOn.coreTempK,
			civilOn.coreSourceRadianceWm2SrUm, expectedCore, 2.0e-6, 1.0e-9,
			"small civil gas core; unit=W/(m^2 sr um)");
		log.numeric("civil_halo_source_radiance", bandName(band), civilOn.haloTempK,
			civilOn.haloSourceRadianceWm2SrUm, expectedHalo, 2.0e-6, 1.0e-9,
			"small civil gas halo; unit=W/(m^2 sr um)");
		log.numeric("civil_core_emitted_radiance", bandName(band), civilOn.coreTempK,
			civilOn.coreEmittedRadianceWm2SrUm, expectedCore * 0.10, 2.0e-6, 1.0e-9,
			"core opacity=0.10 applied once; unit=W/(m^2 sr um)");
		log.numeric("civil_halo_emitted_radiance", bandName(band), civilOn.haloTempK,
			civilOn.haloEmittedRadianceWm2SrUm, expectedHalo * 0.035, 2.0e-6, 1.0e-9,
			"halo opacity=0.035 applied once; unit=W/(m^2 sr um)");
		log.boolean(std::string("civil_engine_on_local_only_") + bandName(band),
			civilOn.enabled && civilOn.coreNodeVisible && civilOn.haloNodeVisible &&
			civilOn.coreLengthM == civilProfile.core.lengthM &&
			civilOn.haloLengthM == civilProfile.halo.lengthM,
			"only the explicit sub-metre/rear exhaust nodes are enabled");
		log.boolean(std::string("civil_engine_off_closed_") + bandName(band),
			!civilOff.enabled && !civilOff.nodeVisible && !civilOff.coreEnabled &&
			!civilOff.haloEnabled && civilOff.coreSourceRadianceWm2SrUm == 0.0f,
			"engineState=false closes civil gas plume at ambient steady state");

		IREnginePlumeModel bypassModel;
		bypassModel.load(argv[1]);
		IREnginePlumeInput bypassInput;
		bypassInput.platformName = "P11-CIVIL-VAN";
		bypassInput.runtimeKey = std::string("civil-bypass-") + bandName(band);
		bypassInput.engineState = false;
		bypassInput.dtSec = 100.0f;
		bypassInput.ambientTempK = 300.0f;
		bypassInput.band = band;
		bypassInput.options.useEngineState = false;
		bypassInput.options.forcePlumeVisible = true;
		const IREnginePlumeOutput bypassAttempt = bypassModel.update(bypassInput);
		log.boolean(std::string("civil_engine_off_rejects_diagnostic_bypass_") + bandName(band),
			!bypassAttempt.enabled && !bypassAttempt.nodeVisible &&
			bypassAttempt.coreSourceRadianceWm2SrUm == 0.0f,
			"civil engineState=false overrides force-visible and useEngineState bypasses");
	}

	const IREnginePlumeProfile& controlledSamplesProfile =
		civilProfileModel.profileForPlatform("P11-CONTROLLED-SAMPLES");
	log.boolean("controlled_samples_plume_profile_explicitly_disabled",
		!controlledSamplesProfile.enabled && !controlledSamplesProfile.core.enabled &&
		!controlledSamplesProfile.halo.enabled,
		"0x66 radiometric panels must never inherit the default missile plume");
	for (size_t bandIndex = 0; bandIndex < 2; ++bandIndex)
	{
		const IRBand band = bands[bandIndex];
		const IREnginePlumeOutput output = evaluateSteady(
			argv[1], "P11-CONTROLLED-SAMPLES", band, true, 1.0f, 1.0f);
		log.boolean(std::string("controlled_samples_no_plume_") + bandName(band),
			!output.profileEnabled && !output.enabled && !output.nodeVisible &&
			output.coreSourceRadianceWm2SrUm == 0.0f &&
			output.haloSourceRadianceWm2SrUm == 0.0f,
			"even engineState=true cannot add a non-sample heat source");
	}

	log.numeric("zero_emissivity_zero_source", "SWIR", 1200.0,
		IREnginePlumeModel::layerSourceRadianceWm2SrUm(
			IRBand::ShortWaveInfrared, 1200.0, 300.0, 0.0),
		0.0, 0.0, 1.0e-12, "emissivity is a physical coefficient not a gain");

	if (log.failures() != 0)
	{
		std::cerr << "P11 plume physics FAIL count=" << log.failures() << std::endl;
		return 1;
	}
	std::cout << "P11 plume physics PASS" << std::endl;
	return 0;
}
