#include "IRAeroThermalModel.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
int failures = 0;

void Check(const std::string& name, bool passed, double actual, const std::string& expectation)
{
	std::cout << name << "," << (passed ? "PASS" : "FAIL") << "," << actual << "," << expectation << "\n";
	if (!passed) ++failures;
}

IRAeroSpatialInput Sample(double x, double y, double z, double nx, double ny, double nz)
{
	IRAeroSpatialInput input;
	input.localPosition = {{x, y, z}};
	input.localNormal = {{nx, ny, nz}};
	input.boundsCenter = {{0.0, 0.0, 0.0}};
	input.boundsHalfExtent = {{1.0, 1.0, 1.0}};
	input.forwardAxis = {{0.0, 1.0, 0.0}};
	input.noseDeltaK = 30.0;
	input.edgeDeltaK = 20.0;
	input.rearDeltaK = 5.0;
	return input;
}
}

int main()
{
	std::cout << "check,status,actual,expectation\n";
	const IRAeroSpatialOutput bodyCenter = IRAeroThermalModel::evaluateSpatial(
		Sample(0.0, 0.0, 0.0, 1.0, 0.0, 0.0));
	Check("body_center_unheated", std::fabs(bodyCenter.deltaK) < 1.0e-12,
		bodyCenter.deltaK, "0K_no_whole_body_offset");

	const IRAeroSpatialOutput nose = IRAeroThermalModel::evaluateSpatial(
		Sample(0.0, 1.0, 0.0, 0.0, 1.0, 0.0));
	Check("nose_local_peak", std::fabs(nose.deltaK - 30.0) < 1.0e-9,
		nose.deltaK, "30K");
	Check("nose_mask_local", nose.noseMask > 0.999 && nose.edgeMask < 1.0e-12 && nose.rearMask < 1.0e-12,
		nose.noseMask, "nose=1_edge=rear=0");

	const IRAeroSpatialOutput rear = IRAeroThermalModel::evaluateSpatial(
		Sample(0.0, -1.0, 0.0, 0.0, -1.0, 0.0));
	Check("rear_local_peak", std::fabs(rear.deltaK - 5.0) < 1.0e-9,
		rear.deltaK, "5K");

	const IRAeroSpatialOutput edge = IRAeroThermalModel::evaluateSpatial(
		Sample(1.0, 0.9, 1.0, 1.0, 0.0, 0.0));
	Check("front_edge_local", edge.edgeMask > 0.99 && edge.deltaK > 19.9,
		edge.deltaK, "approximately_20K");

	const IRAeroSpatialOutput middleRoof = IRAeroThermalModel::evaluateSpatial(
		Sample(0.0, 0.0, 1.0, 0.0, 0.0, 1.0));
	Check("middle_roof_unheated", std::fabs(middleRoof.deltaK) < 1.0e-12,
		middleRoof.deltaK, "0K_locality_guard");

	IRAeroSpatialInput zero = Sample(0.0, 1.0, 0.0, 0.0, 1.0, 0.0);
	zero.noseDeltaK = zero.edgeDeltaK = zero.rearDeltaK = 0.0;
	const IRAeroSpatialOutput disabled = IRAeroThermalModel::evaluateSpatial(zero);
	Check("zero_speed_candidate_no_heat", std::fabs(disabled.deltaK) < 1.0e-12,
		disabled.deltaK, "0K");

	return failures == 0 ? 0 : 1;
}
