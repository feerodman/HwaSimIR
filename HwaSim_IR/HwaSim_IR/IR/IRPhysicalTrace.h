#pragma once
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <cstdint>

// Separate, buffered JSONL file: stdout interleaving cannot corrupt evidence.
// Enable only for numerical validation with HWASIMIR_PHYSICAL_TRACE=<new path>.
class IRPhysicalTrace
{
public:
	struct Component { double value; std::string unit, status; };
	struct Record
	{
		std::uint64_t sourceSeq = 0, generation = 0;
		double utcMs = 0.0, relativeSec = 0.0;
		bool timeValid = false;
		std::string targetKey, band, datasetId, cellId, sourceCases, cameraStatus, error, overall;
		std::map<std::string, double> input;
		std::map<std::string, Component> components;
	};
	static IRPhysicalTrace& instance() { static IRPhysicalTrace trace; return trace; }
	bool enabled() const { return m_enabled; }
	void flush()
	{
		if (!m_enabled) return;
		std::lock_guard<std::mutex> guard(m_mutex);
		m_file.flush();
		if (!m_file) throw std::runtime_error("Physical evidence flush failed");
	}
	static std::string quote(const std::string& s)
	{
		std::ostringstream out; out << '"';
		for (unsigned char c : s)
		{
			if (c == '"' || c == '\\') out << '\\' << c;
			else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec;
			else out << c;
		}
		out << '"'; return out.str();
	}
	void write(const Record& r)
	{
		if (!m_enabled) return;
		if (r.sourceSeq == 0 || r.targetKey.empty() || r.band.empty() || r.datasetId.empty())
			throw std::runtime_error("Invalid physical evidence identity");
		std::ostringstream o; o << std::setprecision(17);
		o << "{\"schema\":\"HwaSimIR.PhysicalTrace.1\",\"runId\":" << quote(m_run)
			<< ",\"sourceSeq\":" << r.sourceSeq << ",\"generation\":" << r.generation
			<< ",\"simUtcMs\":" << (r.timeValid ? number(r.utcMs) : "null")
			<< ",\"simRelativeSec\":" << (r.timeValid ? number(r.relativeSec) : "null")
			<< ",\"timeValid\":" << (r.timeValid ? "true" : "false")
			<< ",\"targetKey\":" << quote(r.targetKey) << ",\"band\":" << quote(r.band)
			<< ",\"datasetId\":" << quote(r.datasetId) << ",\"cellId\":" << quote(r.cellId)
			<< ",\"sourceCases\":" << quote(r.sourceCases) << ",\"cameraStatus\":" << quote(r.cameraStatus)
			<< ",\"overall\":" << quote(r.overall) << ",\"error\":" << quote(r.error)
			<< ",\"inputSnapshotId\":" << quote(m_run+":"+std::to_string(r.generation)+":"+std::to_string(r.sourceSeq))
			<< ",\"input\":{";
		bool comma = false;
		for (const auto& item : r.input) { if (comma) o << ','; comma = true; o << quote(item.first) << ':' << number(item.second); }
		o << "},\"components\":{"; comma = false;
		for (const auto& item : r.components)
		{
			if (comma) o << ','; comma = true;
			const bool valid = item.second.status == "VALID" || item.second.status == "ENGINEERING_APPROX" || item.second.status == "ANALYTIC_BOUNDARY" || item.second.status == "OCCLUDED_PHYSICAL_ZERO";
			if (valid && !std::isfinite(item.second.value))
				throw std::runtime_error("Nonfinite valid physical component");
			o << quote(item.first) << ":{\"value\":" << (valid ? number(item.second.value) : "null")
				<< ",\"unit\":" << quote(item.second.unit) << ",\"status\":" << quote(item.second.status) << '}';
		}
		o << "}}\n";
		std::lock_guard<std::mutex> guard(m_mutex);
		m_file << o.str();
		if (!m_file) throw std::runtime_error("Physical evidence write failed");
	}
private:
	static std::string number(double value)
	{
		if (!std::isfinite(value)) return "null";
		std::ostringstream o; o << std::setprecision(17) << value; return o.str();
	}
	IRPhysicalTrace()
	{
		const std::string path = environment("HWASIMIR_PHYSICAL_TRACE");
		if (path.empty()) return;
		std::ifstream previous(path);
		if (previous.good()) throw std::runtime_error("Physical evidence path already exists");
		const std::string run = environment("HWASIMIR_RUN_ID");
		if (run.empty()) throw std::runtime_error("Physical evidence requires HWASIMIR_RUN_ID");
		m_run = run;
		m_file.open(path, std::ios::out | std::ios::binary);
		if (!m_file) throw std::runtime_error("Cannot open physical evidence file");
		m_enabled = true;
	}
	static std::string environment(const char* key)
	{
#if defined(_MSC_VER)
		char* value = nullptr; size_t size = 0;
		if (_dupenv_s(&value, &size, key) != 0 || value == nullptr) return "";
		const std::string result(value); std::free(value); return result;
#else
		const char* value = std::getenv(key);
		return value ? value : "";
#endif
	}
	bool m_enabled = false;
	std::string m_run;
	std::ofstream m_file;
	std::mutex m_mutex;
};
