#include "IRSimulationUtc.h"
#include <iostream>
#include <limits>
#include <cstdlib>

void require(bool condition, const char* label)
{
	if (!condition) { std::cerr << "FAIL " << label << std::endl; std::exit(1); }
}

int main()
{
	IRSimulationUtc clock;
	auto t = clock.resolve(1792491147359.0, "UnixEpochMs", "2026-09-06");
	require(t.valid && t.year == 2026 && t.month == 10 && t.day == 20, "customer absolute date");
	t = clock.resolve(0, "UnixEpochMs", "");
	require(t.valid && t.year == 1970 && t.month == 1 && t.day == 1, "epoch origin");
	t = clock.resolve(1709251200000.0, "UnixEpochMs", "");
	require(t.valid && t.year == 2024 && t.month == 3 && t.day == 1, "leap date");
	require(!clock.resolve(100, "UtcMillisecondsOfDay", "").valid, "missing anchor rejected");
	t = clock.resolve(86399999, "UtcMillisecondsOfDay", "2024-02-28");
	require(t.valid && t.day == 28, "before midnight");
	t = clock.resolve(1, "UtcMillisecondsOfDay", "2024-02-28");
	require(t.valid && t.day == 29, "midnight leap rollover");
	require(!clock.resolve(0, "UtcMillisecondsOfDay", "2024-02-28").valid, "reversed time rejected");
	clock.reset();
	t = clock.resolve(1, "UtcMillisecondsOfDay", "2024-02-28");
	require(t.valid && t.day == 28, "reset clears rollover");
	require(!clock.resolve(10, "ReplayRelativeMs", "2026-10-21").valid, "unanchored relative rejected");
	require(!clock.resolve(std::numeric_limits<double>::infinity(), "UnixEpochMs", "").valid, "infinity rejected");
	IRSolarPosition solar;
	for (double lat : {-90.0, 90.0})
		for (double lon : {-180.0, 180.0})
		{
			IRSolarPositionInput input;
			input.latitudeDeg=lat; input.longitudeDeg=lon;
			auto sun=solar.evaluate(input);
			require(sun.valid && std::isfinite(sun.east) && std::isfinite(sun.north) && std::isfinite(sun.up), "polar finite vector");
		}
	std::cout << "PASS UTC contracts, source date, leap rollover, RESET, rejection, polar vectors" << std::endl;
}
