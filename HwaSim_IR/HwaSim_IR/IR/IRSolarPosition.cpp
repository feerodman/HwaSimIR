#include "IRSolarPosition.h"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace
{
const double kPi = 3.14159265358979323846;
double Radians(double degrees) { return degrees * kPi / 180.0; }
double Degrees(double radians) { return radians * 180.0 / kPi; }
double Wrap360(double value)
{
	value = std::fmod(value, 360.0);
	return value < 0.0 ? value + 360.0 : value;
}
double Wrap180(double value)
{
	value = Wrap360(value);
	return value > 180.0 ? value - 360.0 : value;
}
double JulianDay(const IRSolarPositionInput& input)
{
	int year = input.utcYear;
	int month = input.utcMonth;
	if (month <= 2) { --year; month += 12; }
	const int a = year / 100;
	const int b = 2 - a + a / 4;
	const double day = static_cast<double>(input.utcDay) + input.utcHour / 24.0;
	return std::floor(365.25 * (year + 4716)) + std::floor(30.6001 * (month + 1)) + day + b - 1524.5;
}
}

IRSolarPositionOutput IRSolarPosition::evaluate(const IRSolarPositionInput& input) const
{
	IRSolarPositionOutput output;
	if (!std::isfinite(input.latitudeDeg) || !std::isfinite(input.longitudeDeg) ||
		!std::isfinite(input.altitudeM) || !std::isfinite(input.utcHour) ||
		input.latitudeDeg < -90.0 || input.latitudeDeg > 90.0 ||
		input.longitudeDeg < -180.0 || input.longitudeDeg > 180.0 ||
		input.utcHour < 0.0 || input.utcHour >= 24.0 ||
		!validUtcDate(input.utcYear, input.utcMonth, input.utcDay))
	{
		output.fallbackReason = "invalid_input";
		return output;
	}

	const double jd = JulianDay(input);
	const double n = jd - 2451545.0;
	const double meanLongitude = Wrap360(280.460 + 0.9856474 * n);
	const double meanAnomaly = Wrap360(357.528 + 0.9856003 * n);
	const double eclipticLongitude = Wrap360(meanLongitude + 1.915 * std::sin(Radians(meanAnomaly)) +
		0.020 * std::sin(2.0 * Radians(meanAnomaly)));
	const double obliquity = 23.439 - 0.0000004 * n;
	const double lambda = Radians(eclipticLongitude);
	const double epsilon = Radians(obliquity);
	const double rightAscension = std::atan2(std::cos(epsilon) * std::sin(lambda), std::cos(lambda));
	const double declination = std::asin(std::sin(epsilon) * std::sin(lambda));
	const double gmst = Wrap360(280.46061837 + 360.98564736629 * n);
	const double hourAngle = Radians(Wrap180(gmst + input.longitudeDeg - Degrees(rightAscension)));
	const double latitude = Radians(input.latitudeDeg);
	const double sinElevation = std::sin(latitude) * std::sin(declination) +
		std::cos(latitude) * std::cos(declination) * std::cos(hourAngle);
	const double elevation = std::asin(std::max(-1.0, std::min(1.0, sinElevation)));
	const double azimuth = Wrap360(Degrees(std::atan2(
		std::sin(hourAngle), std::cos(hourAngle) * std::sin(latitude) -
		std::tan(declination) * std::cos(latitude))) + 180.0);

	output.valid = true;
	output.azimuthDeg = azimuth;
	output.elevationDeg = Degrees(elevation);
	output.zenithDeg = 90.0 - output.elevationDeg;
	const double az = Radians(azimuth);
	const double cosEl = std::cos(elevation);
	output.east = cosEl * std::sin(az);
	output.north = cosEl * std::cos(az);
	output.up = std::sin(elevation);
	output.fallbackReason = "none";
	return output;
}

bool IRSolarPosition::parseUtcDate(const std::string& text, int& year, int& month, int& day)
{
	char dash1 = 0;
	char dash2 = 0;
	std::istringstream stream(text);
	if (!(stream >> year >> dash1 >> month >> dash2 >> day) || dash1 != '-' || dash2 != '-' ||
		stream.peek() != std::char_traits<char>::eof()) return false;
	return validUtcDate(year, month, day);
}

bool IRSolarPosition::validUtcDate(int year, int month, int day)
{
	if (year < 1900 || year > 2200 || month < 1 || month > 12 || day < 1) return false;
	static const int days[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
	int maxDay = days[month - 1];
	const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
	if (month == 2 && leap) maxDay = 29;
	return day <= maxDay;
}
