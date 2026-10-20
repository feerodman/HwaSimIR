#pragma once
#include "IRSimulationUtc.h"
#include <cstdint>
#include <cmath>
#include <string>

// Ordered physical-state clock. Wall/steady time is deliberately absent.
// Random access LUT queries need no instance of this history-bearing clock.
class IRSimulationClock
{
public:
	struct State
	{
		bool valid = false;
		double utcMs = 0.0;
		double relativeSec = 0.0;
		double dtSec = 0.0;
		std::uint64_t sourceSeq = 0;
		std::string status = "NO_PROTOCOL_TIME";
	};
	void reset() { m_utc.reset(); m_state = State(); m_firstMs = m_lastMs = 0.0; m_lastSeq = 0; m_started = false; }
	const State& state() const { return m_state; }
	const State& accept(std::uint64_t seq, double ms, const std::string& mode, const std::string& date)
	{
		m_state.valid = false;
		m_state.dtSec = 0.0;
		m_state.sourceSeq = seq;
		if (seq == 0 || seq <= m_lastSeq) { m_state.status = "DUPLICATE_OR_REVERSED_SEQUENCE"; return m_state; }
		// Validate using a copy so an invalid ordering cannot corrupt rollover.
		IRSimulationUtc nextUtc = m_utc;
		const IRSimulationUtc::Result utc = nextUtc.resolve(ms, mode, date);
		if (!utc.valid) { m_state.status = utc.reason; return m_state; }
		double absolute = ms;
		if (mode == "UtcMillisecondsOfDay")
		{
			long long days = 0;
			for (int y = utc.year; y < 1970; ++y)
				days -= IRSolarPosition::validUtcDate(y, 2, 29) ? 366 : 365;
			for (int y = 1970; y < utc.year; ++y)
				days += IRSolarPosition::validUtcDate(y, 2, 29) ? 366 : 365;
			for (int month = 1; month < utc.month; ++month)
				for (int day = 1; day <= 31; ++day)
					if (IRSolarPosition::validUtcDate(utc.year, month, day)) ++days;
			absolute = (days + utc.day - 1)*86400000.0 + utc.hour*3600000.0;
		}
		if (m_started && absolute < m_lastMs)
		{
			m_state.status = "REVERSED_PROTOCOL_TIME";
			return m_state;
		}
		if (m_started && absolute-m_lastMs > 3600000.0)
		{
			m_state.status = "LARGE_TIME_JUMP_REQUIRES_RESET";
			return m_state;
		}
		m_utc = nextUtc;
		if (!m_started) m_firstMs = absolute;
		m_state.dtSec = !m_started ? 0.0 : (absolute-m_lastMs)*.001;
		m_state.relativeSec = (absolute-m_firstMs)*.001;
		m_state.utcMs = absolute;
		m_state.status = !m_started ? "FIRST_SAMPLE" : (absolute == m_lastMs ? "DUPLICATE_TIME_HOLD" : "ADVANCED");
		m_lastMs = absolute; m_lastSeq = seq;
		m_started = true;
		m_state.valid = true;
		return m_state;
	}
private:
	IRSimulationUtc m_utc;
	State m_state;
	double m_firstMs = 0.0, m_lastMs = 0.0;
	bool m_started = false;
	std::uint64_t m_lastSeq = 0;
};
