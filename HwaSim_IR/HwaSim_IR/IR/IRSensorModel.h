#pragma once

#include <string>

struct IRSensorDisplayConfig
{
	int requestedWidth = 800;
	int requestedHeight = 800;
	int width = 800;
	int height = 800;

	double requestedViewMinM = 1.0;
	double requestedViewMaxM = 100000.0;
	double viewMinM = 1.0;
	double viewMaxM = 100000.0;
	double nearClipM = 1.0;
	double farClipM = 100000.0;

	double requestedPixelAngleUrad = 20.0;
	double pixelAngleUrad = 20.0;
	double pixelAngleRad = 20.0e-6;
	double horizontalFovDeg = 0.0;
	double verticalFovDeg = 0.0;
	std::string fovSource = "pixelAngle_urad_per_pixel";

	bool widthFallback = false;
	bool heightFallback = false;
	bool nearFallback = false;
	bool farFallback = false;
	bool pixelAngleFallback = false;
	bool pixelAngleRangeWarning = false;
	bool pixelAngleClamped = false;
};

class IRSensorModel
{
public:
	IRSensorDisplayConfig BuildSensorDisplayConfig(
		int trackerSensorWidth,
		int trackerSensorHeight,
		int trackerSensorViewMin,
		int trackerSensorViewMax,
		double trackerSensorPixelAngle) const;
};
