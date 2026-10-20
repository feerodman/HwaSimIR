#pragma once

#include "IRSolarPosition.h"
#include <cmath>
#include <string>

// The transport's double "time" has no unit/epoch tag. The session must select
// its contract explicitly; never infer an epoch from the magnitude of a value.
class IRSimulationUtc
{
public:
	struct Result
	{
		bool valid = false;
		int year = 2000, month = 1, day = 1;
		double hour = 0.0;
		std::string source;
		std::string reason = "not_evaluated";
	};

	void reset() { m_previousMs = -1.0; m_rolloverDays = 0; }

	Result resolve(double valueMs, const std::string& mode, const std::string& dateAnchor)
	{
		Result result;
		result.source = mode;
		if (!std::isfinite(valueMs) || valueMs < 0.0)
		{
			result.reason = "invalid_time_value";
			return result;
		}
		long long days = 0;
		double withinDay = 0.0;
		if (mode == "UnixEpochMs")
		{
			// Bound before conversion to integer and independent of time_t width.
			if (valueMs > 7258118399999.0)
			{
				result.reason = "epoch_out_of_supported_range";
				return result;
			}
			days = static_cast<long long>(std::floor(valueMs / 86400000.0));
			withinDay = valueMs - static_cast<double>(days) * 86400000.0;
			result.year = 1970;
			result.month = result.day = 1;
		}
		else if (mode == "UtcMillisecondsOfDay")
		{
			if (valueMs >= 86400000.0 ||
				!IRSolarPosition::parseUtcDate(dateAnchor, result.year, result.month, result.day))
			{
				result.reason = "day_time_requires_explicit_valid_utc_date";
				return result;
			}
			if (m_previousMs >= 0.0 && valueMs < m_previousMs)
			{
				if (m_previousMs >= 64800000.0 && valueMs <= 21600000.0) ++m_rolloverDays;
				else
				{
					result.reason = "time_reversed_without_midnight";
					return result;
				}
			}
			m_previousMs = valueMs;
			days = m_rolloverDays;
			withinDay = valueMs;
		}
		else
		{
			result.reason = "unsupported_time_contract";
			return result;
		}
		// Skip whole calendar months, independent of the host timezone.
		while (days > 0)
		{
			int lastDay = 31;
			while (!IRSolarPosition::validUtcDate(result.year, result.month, lastDay) && lastDay > 27) --lastDay;
			const long long remaining = lastDay - result.day;
			if (days <= remaining)
			{
				result.day += static_cast<int>(days);
				break;
			}
			days -= remaining + 1;
			result.day = 1;
			if (++result.month > 12)
			{
				result.month = 1;
				++result.year;
			}
		}
		if (!IRSolarPosition::validUtcDate(result.year, result.month, result.day))
		{
			result.reason = "utc_date_out_of_supported_range";
			return result;
		}
		result.hour = withinDay / 3600000.0;
		result.valid = true;
		result.reason = "none";
		return result;
	}

private:
	double m_previousMs = -1.0;
	long long m_rolloverDays = 0;
};
