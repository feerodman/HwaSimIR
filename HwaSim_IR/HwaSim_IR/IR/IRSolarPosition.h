#pragma once

#include <string>

struct IRSolarPositionInput
{
	double latitudeDeg = 0.0;
	double longitudeDeg = 0.0;
	double altitudeM = 0.0;
	int utcYear = 2000;
	int utcMonth = 1;
	int utcDay = 1;
	double utcHour = 12.0;
};

struct IRSolarPositionOutput
{
	bool valid = false;
	double azimuthDeg = 0.0;   // clockwise from geodetic north
	double elevationDeg = -90.0;
	double zenithDeg = 180.0;
	double east = 0.0;
	double north = 0.0;
	double up = -1.0;
	std::string coordinateFrame = "ENU/Panda(X=East,Y=North,Z=Up)";
	std::string fallbackReason = "not_evaluated";
};

class IRSolarPosition
{
public:
	IRSolarPositionOutput evaluate(const IRSolarPositionInput& input) const;
	static bool parseUtcDate(const std::string& text, int& year, int& month, int& day);
	static bool validUtcDate(int year, int month, int day);
};
