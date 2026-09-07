#include "IRActiveIlluminator.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
struct Row
{
	std::string test;
	std::string caseName;
	double value;
	double expected;
	std::string unit;
	bool pass;
	double geometricIrradianceWm2;
	double tauOutbound;
	double tauInbound;
	double targetIrradianceWm2;
	double activeSurfaceRadianceWm2SrUm;
	double activeSensorRadianceWm2SrUm;
};

bool near(double a, double b, double relative = 1.0e-10)
{
	return std::fabs(a - b) <= relative * std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
}

IRActiveIlluminatorInput baseInput()
{
	IRActiveIlluminatorInput input;
	input.protocolEnabled = true;
	input.sensorBand = IRBand::NearInfrared;
	input.sensorLowUm = 0.70;
	input.sensorHighUm = 1.10;
	input.protocolAngleMrad = 5.0;
	input.protocolSpotRadiance = 1.0;
	input.rangeM = 1000.0;
	input.beamAngleRad = 0.0;
	input.surfaceNdotL = 1.0;
	input.tauInbound = 0.9;
	input.tauInboundValid = true;
	input.tauFallbackReason = "none";
	input.activeVisibility = 1.0;
	input.bandReflectance = 0.5;
	input.reflectanceSource = "solar_absorptivity_fallback";
	return input;
}
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		std::cerr << "usage: l2_active_illuminator_unit <output.csv>\n";
		return 2;
	}
	IRActiveIlluminator model;
	IRActiveIlluminatorConfig config;
	config.enabled = true;
	config.referenceRangeM = 1000.0;
	config.legacyMaxReferenceIrradianceWm2 = 1.0;
	std::vector<Row> rows;

	IRActiveIlluminatorInput input = baseInput();
	input.protocolEnabled = false;
	IRActiveIlluminatorOutput off1 = model.evaluate(config, input);
	rows.push_back(Row{"enable", "off", off1.activeSensorRadianceWm2SrUm, 0.0, "W/(m^2 sr um)", off1.activeSensorRadianceWm2SrUm == 0.0});
	input.protocolEnabled = true;
	IRActiveIlluminatorOutput on = model.evaluate(config, input);
	rows.push_back(Row{"enable", "on", on.activeSensorRadianceWm2SrUm, 0.0, "W/(m^2 sr um)", on.activeSensorRadianceWm2SrUm > 0.0});
	input.protocolEnabled = false;
	IRActiveIlluminatorOutput off2 = model.evaluate(config, input);
	rows.push_back(Row{"enable", "off_again", off2.activeSensorRadianceWm2SrUm, 0.0, "W/(m^2 sr um)", off2.activeSensorRadianceWm2SrUm == 0.0});

	double lastRadius = -1.0;
	const double angles[] = {1.0, 2.0, 5.0};
	for (size_t i = 0; i < 3; ++i)
	{
		input = baseInput();
		input.protocolAngleMrad = angles[i];
		const IRActiveIlluminatorOutput out = model.evaluate(config, input);
		const bool pass = out.spotRadiusM > lastRadius;
		rows.push_back(Row{"angle", std::to_string(static_cast<int>(angles[i])) + "mrad", out.spotRadiusM,
			input.rangeM * std::tan(angles[i] * 1.0e-3 * 0.5), "m", pass});
		lastRadius = out.spotRadiusM;
	}

	const double rangesKm[] = {1.0, 2.0, 5.0, 10.0};
	const double nirTau[] = {0.9961376869, 0.9932903544, 0.9860404126, 0.9755341991};
	double lastRadiance = 1.0e100;
	for (size_t i = 0; i < 4; ++i)
	{
		input = baseInput();
		input.rangeM = rangesKm[i] * 1000.0;
		input.tauInbound = nirTau[i];
		const IRActiveIlluminatorOutput out = model.evaluate(config, input);
		const double expected = 0.5 / 3.14159265358979323846 / 0.05 *
			std::pow(1000.0 / input.rangeM, 2.0) * nirTau[i] * nirTau[i];
		const bool pass = out.activeSensorRadianceWm2SrUm < lastRadiance && near(out.activeSensorRadianceWm2SrUm, expected);
		Row row = Row{"range_two_way_tau", std::to_string(static_cast<int>(rangesKm[i])) + "km",
			out.activeSensorRadianceWm2SrUm, expected, "W/(m^2 sr um)", pass};
		row.geometricIrradianceWm2 = out.geometricIrradianceWm2;
		row.tauOutbound = out.tauOutbound;
		row.tauInbound = out.tauInbound;
		row.targetIrradianceWm2 = out.targetIrradianceWm2;
		row.activeSurfaceRadianceWm2SrUm = out.activeSurfaceRadianceWm2SrUm;
		row.activeSensorRadianceWm2SrUm = out.activeSensorRadianceWm2SrUm;
		rows.push_back(row);
		lastRadiance = out.activeSensorRadianceWm2SrUm;
	}
	input = baseInput();
	input.protocolSpotRadiance = 999.0;
	config.intensityMode = IRActiveIntensityMode::BandIrradianceAtReference;
	config.referenceIrradianceWm2 = 2.0;
	IRActiveIlluminatorOutput physicalIntensity = model.evaluate(config, input);
	rows.push_back(Row{"intensity", "physical_reference_ignores_protocol_raw_value",
		physicalIntensity.referenceIrradianceWm2, 2.0, "W/m^2", near(physicalIntensity.referenceIrradianceWm2, 2.0)});
	config.intensityMode = IRActiveIntensityMode::LegacyNormalized;

	input = baseInput();
	input.sensorBand = IRBand::MidWaveInfrared;
	input.sensorLowUm = 3.0;
	input.sensorHighUm = 5.0;
	IRActiveIlluminatorOutput mismatch = model.evaluate(config, input);
	rows.push_back(Row{"band", "NIR_source_MWIR_sensor", mismatch.activeSensorRadianceWm2SrUm, 0.0,
		"W/(m^2 sr um)", !mismatch.spectralOverlap && mismatch.activeSensorRadianceWm2SrUm == 0.0});
	config.centerWavelengthUm = 4.0;
	config.bandwidthUm = 0.5;
	IRActiveIlluminatorOutput semanticMismatch = model.evaluate(config, input);
	rows.push_back(Row{"band", "NIR_label_with_MWIR_wavelength_is_rejected", semanticMismatch.activeSensorRadianceWm2SrUm, 0.0,
		"W/(m^2 sr um)", !semanticMismatch.spectralOverlap && semanticMismatch.activeSensorRadianceWm2SrUm == 0.0});
	config.band = IRActiveIlluminatorBand::MidWaveInfrared;
	config.centerWavelengthUm = 4.0;
	config.bandwidthUm = 0.5;
	IRActiveIlluminatorOutput mwir = model.evaluate(config, input);
	rows.push_back(Row{"band", "MWIR_source_MWIR_sensor", mwir.activeSensorRadianceWm2SrUm, 0.0,
		"W/(m^2 sr um)", mwir.spectralOverlap && mwir.activeSensorRadianceWm2SrUm > 0.0});
	config.band = IRActiveIlluminatorBand::FollowSensor;
	IRActiveIlluminatorOutput followMwir = model.evaluate(config, input);
	rows.push_back(Row{"band", "FollowSensor_MWIR", followMwir.activeSensorRadianceWm2SrUm, 0.0,
		"W/(m^2 sr um)", followMwir.spectralOverlap && followMwir.activeSensorRadianceWm2SrUm > 0.0});
	input = baseInput();
	IRActiveIlluminatorOutput followNir = model.evaluate(config, input);
	rows.push_back(Row{"band", "FollowSensor_NIR", followNir.activeSensorRadianceWm2SrUm, 0.0,
		"W/(m^2 sr um)", followNir.spectralOverlap && followNir.activeSensorRadianceWm2SrUm > 0.0});

	config = IRActiveIlluminatorConfig();
	config.enabled = true;
	input = baseInput();
	const double half = input.protocolAngleMrad * 1.0e-3 * 0.5;
	const double coneAngles[] = {0.0, half, half * 1.01};
	const char* coneCases[] = {"center", "edge", "outside"};
	for (size_t i = 0; i < 3; ++i)
	{
		input.beamAngleRad = coneAngles[i];
		const IRActiveIlluminatorOutput out = model.evaluate(config, input);
		const bool pass = i < 2 ? out.activeSensorRadianceWm2SrUm > 0.0 : out.activeSensorRadianceWm2SrUm == 0.0;
		rows.push_back(Row{"cone", coneCases[i], out.beamFactor, i < 2 ? 1.0 : 0.0, "dimensionless", pass});
	}
	input = baseInput();
	input.activeVisibility = 0.0;
	const IRActiveIlluminatorOutput occluded = model.evaluate(config, input);
	rows.push_back(Row{"occlusion", "target_cast", occluded.activeSensorRadianceWm2SrUm, 0.0,
		"W/(m^2 sr um)", occluded.activeSensorRadianceWm2SrUm == 0.0});
	input = baseInput();
	input.tauInboundValid = false;
	input.tauFallbackReason = "out_of_range";
	const IRActiveIlluminatorOutput invalidTau = model.evaluate(config, input);
	rows.push_back(Row{"atmosphere", "invalid_tau_zero_fallback", invalidTau.activeSensorRadianceWm2SrUm, 0.0,
		"W/(m^2 sr um)", invalidTau.activeSensorRadianceWm2SrUm == 0.0 &&
			invalidTau.fallbackReason == "invalid_modtran_tau:out_of_range"});

	const char* materialNames[] = {"aluminum", "paint", "glass"};
	const double materialRho[] = {0.70, 0.50, 0.27};
	double previous = 1.0e100;
	for (size_t i = 0; i < 3; ++i)
	{
		input = baseInput();
		input.bandReflectance = materialRho[i];
		const IRActiveIlluminatorOutput out = model.evaluate(config, input);
		rows.push_back(Row{"material", materialNames[i], out.activeSensorRadianceWm2SrUm,
			materialRho[i], "W/(m^2 sr um)", out.activeSensorRadianceWm2SrUm < previous});
		previous = out.activeSensorRadianceWm2SrUm;
	}

	std::ofstream csv(argv[1]);
	csv << "test,case,value,expected,unit,status,Egeom_W_m2,tau_outbound,tau_inbound,Etarget_W_m2,active_surface_W_m2_sr_um,active_sensor_W_m2_sr_um\n";
	bool allPass = true;
	for (size_t i = 0; i < rows.size(); ++i)
	{
		csv << rows[i].test << ',' << rows[i].caseName << ',' << rows[i].value << ','
			<< rows[i].expected << ',' << rows[i].unit << ',' << (rows[i].pass ? "PASS" : "FAIL") << ','
			<< rows[i].geometricIrradianceWm2 << ',' << rows[i].tauOutbound << ','
			<< rows[i].tauInbound << ',' << rows[i].targetIrradianceWm2 << ','
			<< rows[i].activeSurfaceRadianceWm2SrUm << ',' << rows[i].activeSensorRadianceWm2SrUm << '\n';
		allPass = allPass && rows[i].pass;
	}
	std::cout << "L2 active illuminator unit rows=" << rows.size()
		<< " result=" << (allPass ? "PASS" : "FAIL") << " output=" << argv[1] << '\n';
	return allPass ? 0 : 1;
}
