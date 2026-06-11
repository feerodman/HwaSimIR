#include "IRSensorModel.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr int kFallbackSensorWidth = 800;
constexpr int kFallbackSensorHeight = 800;
constexpr int kMinSensorDimension = 256;
constexpr int kMaxSensorDimension = 4096;
constexpr double kFallbackNearClipM = 1.0;
constexpr double kFallbackFarClipM = 100000.0;
constexpr double kFallbackPixelAngleUrad = 20.0;
constexpr double kMaxNormalPixelAngleUrad = 200.0;
constexpr double kPi = 3.14159265358979323846;

int ValidateSensorDimension(int value, int fallback, bool& usedFallback)
{
	if (value < kMinSensorDimension || value > kMaxSensorDimension)
	{
		usedFallback = true;
		return fallback;
	}
	usedFallback = false;
	return value;
}
}

IRSensorDisplayConfig IRSensorModel::BuildSensorDisplayConfig(
	int trackerSensorWidth,
	int trackerSensorHeight,
	int trackerSensorViewMin,
	int trackerSensorViewMax,
	double trackerSensorPixelAngle) const
{
	IRSensorDisplayConfig config;
	config.requestedWidth = trackerSensorWidth;
	config.requestedHeight = trackerSensorHeight;
	config.width = ValidateSensorDimension(trackerSensorWidth, kFallbackSensorWidth, config.widthFallback);
	config.height = ValidateSensorDimension(trackerSensorHeight, kFallbackSensorHeight, config.heightFallback);

	config.requestedViewMinM = static_cast<double>(trackerSensorViewMin);
	config.requestedViewMaxM = static_cast<double>(trackerSensorViewMax);
	if (trackerSensorViewMin <= 0)
	{
		config.nearFallback = true;
		config.viewMinM = kFallbackNearClipM;
	}
	else
	{
		config.viewMinM = static_cast<double>(trackerSensorViewMin);
	}

	if (trackerSensorViewMax <= config.viewMinM)
	{
		config.farFallback = true;
		config.viewMaxM = kFallbackFarClipM;
	}
	else
	{
		config.viewMaxM = static_cast<double>(trackerSensorViewMax);
	}
	config.nearClipM = config.viewMinM;
	config.farClipM = config.viewMaxM;

	config.requestedPixelAngleUrad = trackerSensorPixelAngle;
	if (trackerSensorPixelAngle <= 0.0)
	{
		config.pixelAngleFallback = true;
		config.pixelAngleUrad = kFallbackPixelAngleUrad;
	}
	else if (trackerSensorPixelAngle > kMaxNormalPixelAngleUrad)
	{
		config.pixelAngleRangeWarning = true;
		config.pixelAngleClamped = true;
		config.pixelAngleUrad = kMaxNormalPixelAngleUrad;
	}
	else
	{
		config.pixelAngleUrad = trackerSensorPixelAngle;
		config.pixelAngleRangeWarning = trackerSensorPixelAngle < 1.0;
	}

	config.pixelAngleRad = config.pixelAngleUrad * 1.0e-6;
	const double horizontalFovRad = 2.0 * std::atan(static_cast<double>(config.width) * std::tan(config.pixelAngleRad / 2.0));
	const double verticalFovRad = 2.0 * std::atan(static_cast<double>(config.height) * std::tan(config.pixelAngleRad / 2.0));
	config.horizontalFovDeg = horizontalFovRad * 180.0 / kPi;
	config.verticalFovDeg = verticalFovRad * 180.0 / kPi;
	config.fovSource = "pixelAngle_urad_per_pixel";
	return config;
}
