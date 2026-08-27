#include "DdsVideoPublisher.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#if defined(HWASIMIR_HAS_ZRDDS)
#include "ZRDDSCppSimpleInterface.h"
using namespace DDS;
#endif

namespace
{
std::int64_t SteadyNowNs()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}
}

struct DdsVideoPublisher::Impl
{
	DdsVideoPublisherConfig config;
	std::atomic<bool> enabled{ false };
	std::atomic<bool> healthy{ true };
	std::atomic<bool> accepting{ false };
	std::atomic<bool> running{ false };
	std::thread worker;
	mutable std::mutex mutex;
	std::condition_variable dataCv;
	std::condition_variable spaceCv;
	std::condition_variable drainedCv;
	std::deque<std::vector<std::uint8_t> > queue;
	bool writeInProgress = false;
	std::string topic;
	std::string fatalError;
	DdsVideoPublisherStats stats;
	double writeMsTotal = 0.0;
	std::int64_t lastPerfLogNs = 0;
	std::ofstream audit;
#if defined(HWASIMIR_HAS_ZRDDS)
	DDS::DomainParticipantFactory* factory = nullptr;
	DDS::DomainParticipant* participant = nullptr;
	DDS::DataWriter* writer = nullptr;
#endif

	void logPerf(bool force)
	{
		if (!config.enablePerfLog) return;
		const std::int64_t now = SteadyNowNs();
		if (!force && now - lastPerfLogNs < 2000000000LL) return;
		DdsVideoPublisherStats snapshot;
		{
			std::lock_guard<std::mutex> lock(mutex);
			snapshot = stats;
			snapshot.queueDepth = queue.size();
			snapshot.writeMsAverage = snapshot.sentSamples
				? writeMsTotal / static_cast<double>(snapshot.sentSamples) : 0.0;
		}
		std::cout << std::fixed << std::setprecision(3)
			<< "[DdsVideoPerf]"
			<< " topic=" << topic
			<< " sentSamples=" << snapshot.sentSamples
			<< " sentBytes=" << snapshot.sentBytes
			<< " writeMsAvg=" << snapshot.writeMsAverage
			<< " writeMsMax=" << snapshot.writeMsMaximum
			<< " queueDepth=" << snapshot.queueDepth
			<< " maxQueueDepth=" << snapshot.maxQueueDepth
			<< " backpressureMs=" << snapshot.backpressureMs
			<< " writeErrors=" << snapshot.writeErrors
			<< " droppedSamples=0"
			<< std::endl;
		lastPerfLogNs = now;
	}

	void run()
	{
		while (true)
		{
			std::vector<std::uint8_t> payload;
			std::string topicSnapshot;
			{
				std::unique_lock<std::mutex> lock(mutex);
				dataCv.wait(lock, [this] { return !queue.empty() || !running.load(); });
				if (queue.empty() && !running.load()) break;
				payload = std::move(queue.front());
				queue.pop_front();
				writeInProgress = true;
				topicSnapshot = topic;
				stats.queueDepth = queue.size();
				spaceCv.notify_all();
			}

			const auto before = std::chrono::steady_clock::now();
			bool writeOk = false;
#if defined(HWASIMIR_HAS_ZRDDS)
			const DDS::ReturnCode_t result = DDSIF::BytesWrite(
				config.domainId,
				const_cast<char*>(topicSnapshot.c_str()),
				reinterpret_cast<const char*>(payload.data()),
				static_cast<DDS::Long>(payload.size()));
			writeOk = result == DDS::RETCODE_OK;
			if (!writeOk)
			{
				std::ostringstream out;
				out << "DDSIF::BytesWrite failed code=" << result;
				fatalError = out.str();
			}
#else
			fatalError = "DDS requested but binary lacks HWASIMIR_HAS_ZRDDS";
#endif
			const double writeMs = std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - before).count();
			{
				std::lock_guard<std::mutex> lock(mutex);
				if (writeOk)
				{
					if (audit.is_open() &&
						(config.auditMaxSamples == 0 || stats.sentSamples < config.auditMaxSamples))
					{
						audit.write(reinterpret_cast<const char*>(payload.data()),
							static_cast<std::streamsize>(payload.size()));
						if (!audit)
						{
							fatalError = "DDS audit file write failed";
							writeOk = false;
						}
					}
				}
				if (writeOk)
				{
					++stats.sentSamples;
					stats.sentBytes += payload.size();
					writeMsTotal += writeMs;
					stats.writeMsMaximum = (std::max)(stats.writeMsMaximum, writeMs);
				}
				else
				{
					++stats.writeErrors;
					healthy.store(false);
					accepting.store(false);
					std::cerr << "[DdsVideo][FATAL] codec=wire_bytes publishSkipped=1 reason="
						<< fatalError << std::endl;
				}
				writeInProgress = false;
				if (queue.empty()) drainedCv.notify_all();
			}
			logPerf(false);
		}
		logPerf(true);
	}
};

DdsVideoPublisher::DdsVideoPublisher()
	: m_impl(new Impl())
{
}

DdsVideoPublisher::~DdsVideoPublisher()
{
	shutdown();
}

bool DdsVideoPublisher::start(const DdsVideoPublisherConfig& config, std::string& error)
{
	if (m_impl->running.load()) return true;
	m_impl->config = config;
	m_impl->enabled.store(config.enabled);
	if (!config.enabled) return true;
#if !defined(HWASIMIR_HAS_ZRDDS)
	error = "[DdsVideo][FATAL] Enable=true but binary lacks HWASIMIR_HAS_ZRDDS";
	m_impl->healthy.store(false);
	std::cerr << error << std::endl;
	return false;
#else
	m_impl->factory = DDSIF::Init(config.qosFile.c_str(), "hwasimir_factory");
	if (!m_impl->factory)
	{
		error = "DDSIF::Init failed qos=" + config.qosFile;
		m_impl->healthy.store(false);
		std::cerr << "[DdsVideo][FATAL] " << error << std::endl;
		return false;
	}
	m_impl->participant = DDSIF::CreateDP(config.domainId, "hwasimir_tcp");
	if (!m_impl->participant)
	{
		error = "DDSIF::CreateDP failed domain=" + std::to_string(config.domainId);
		m_impl->healthy.store(false);
		DDSIF::Finalize();
		m_impl->factory = nullptr;
		std::cerr << "[DdsVideo][FATAL] " << error << std::endl;
		return false;
	}
	if (!config.auditPath.empty())
	{
		m_impl->audit.open(config.auditPath.c_str(), std::ios::binary | std::ios::trunc);
		if (!m_impl->audit)
		{
			error = "cannot open DDS audit file: " + config.auditPath;
			m_impl->healthy.store(false);
			DDSIF::Finalize();
			m_impl->factory = nullptr;
			m_impl->participant = nullptr;
			return false;
		}
	}
	m_impl->healthy.store(true);
	m_impl->accepting.store(true);
	m_impl->running.store(true);
	m_impl->worker = std::thread(&Impl::run, m_impl.get());
	std::cout << "[DdsVideo] initialized=1 initCount=1 domain=" << config.domainId
		<< " qos=" << config.qosFile
		<< " queueMaxFrames=" << config.queueMaxFrames
		<< " blockWhenQueueFull=" << (config.blockWhenQueueFull ? "1" : "0")
		<< " wireType=DDS::Bytes" << std::endl;
	if (!config.topic.empty())
	{
		bool changed = false;
		return configureTopic(config.topic, &changed, error);
	}
	return true;
#endif
}

bool DdsVideoPublisher::configureTopic(const std::string& topic, bool* changed, std::string& error)
{
	if (changed) *changed = false;
	if (!m_impl->enabled.load()) return true;
	if (topic.empty()) { error = "DDS topic is empty"; return false; }
	{
		std::lock_guard<std::mutex> lock(m_impl->mutex);
		if (m_impl->topic == topic &&
#if defined(HWASIMIR_HAS_ZRDDS)
			m_impl->writer != nullptr
#else
			false
#endif
		) return true;
	}
	if (!flush((m_impl->config.ackTimeoutSec + 5) * 1000, error)) return false;
#if defined(HWASIMIR_HAS_ZRDDS)
	if (m_impl->writer)
	{
		DDS::Duration_t timeout;
		timeout.sec = m_impl->config.ackTimeoutSec;
		timeout.nanosec = 0;
		const DDS::ReturnCode_t ack = m_impl->writer->wait_for_acknowledgments(timeout);
		if (ack != DDS::RETCODE_OK)
		{
			error = "wait_for_acknowledgments failed during topic switch";
			return false;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(m_impl->config.shutdownDrainMs));
		DDSIF::UnPubTopic(m_impl->writer);
		m_impl->writer = nullptr;
	}
	m_impl->writer = DDSIF::PubTopic(m_impl->participant, topic.c_str(),
		DDS::BytesTypeSupport::get_instance(), "hwasimir_reliable_writer", nullptr);
	if (!m_impl->writer)
	{
		error = "DDSIF::PubTopic failed topic=" + topic;
		m_impl->healthy.store(false);
		return false;
	}
	{
		std::lock_guard<std::mutex> lock(m_impl->mutex);
		m_impl->topic = topic;
	}
	if (changed) *changed = true;
	std::cout << "[DdsVideo] writerCreated=1 topic=" << topic
		<< " profile=hwasimir_reliable_writer requestIdr=1" << std::endl;
	return true;
#else
	error = "DDS requested but binary lacks HWASIMIR_HAS_ZRDDS";
	return false;
#endif
}

bool DdsVideoPublisher::publishBytes(const std::uint8_t* data, std::size_t size,
	double* backpressureMs, std::string& error)
{
	if (backpressureMs) *backpressureMs = 0.0;
	if (!m_impl->enabled.load()) return true;
	if (!data || size == 0) { error = "DDS payload is empty"; return false; }
	if (!m_impl->healthy.load() || !m_impl->accepting.load())
	{
		error = m_impl->fatalError.empty() ? "DDS publisher is not accepting" : m_impl->fatalError;
		return false;
	}
	const auto waitBegin = std::chrono::steady_clock::now();
	std::unique_lock<std::mutex> lock(m_impl->mutex);
	if (m_impl->topic.empty()) { error = "DDS writer topic is not configured"; return false; }
	if (m_impl->queue.size() >= m_impl->config.queueMaxFrames)
	{
		if (!m_impl->config.blockWhenQueueFull)
		{
			error = "DDS queue full and BlockWhenQueueFull=false; dropping is forbidden";
			m_impl->healthy.store(false);
			m_impl->accepting.store(false);
			return false;
		}
		m_impl->spaceCv.wait(lock, [this] {
			return m_impl->queue.size() < m_impl->config.queueMaxFrames ||
				!m_impl->running.load() || !m_impl->healthy.load();
		});
	}
	if (!m_impl->running.load() || !m_impl->healthy.load())
	{
		error = "DDS publisher stopped while applying backpressure";
		return false;
	}
	const double waited = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - waitBegin).count();
	m_impl->queue.emplace_back(data, data + size);
	++m_impl->stats.acceptedSamples;
	m_impl->stats.backpressureMs += waited;
	m_impl->stats.queueDepth = m_impl->queue.size();
	m_impl->stats.maxQueueDepth = (std::max)(m_impl->stats.maxQueueDepth, m_impl->queue.size());
	if (backpressureMs) *backpressureMs = waited;
	lock.unlock();
	m_impl->dataCv.notify_one();
	return true;
}

bool DdsVideoPublisher::flush(int timeoutMs, std::string& error)
{
	if (!m_impl->enabled.load() || !m_impl->running.load()) return true;
	std::unique_lock<std::mutex> lock(m_impl->mutex);
	const bool drained = m_impl->drainedCv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
		[this] { return m_impl->queue.empty() && !m_impl->writeInProgress; });
	if (!drained)
	{
		error = "DDS application queue flush timeout";
		return false;
	}
	if (!m_impl->healthy.load())
	{
		error = m_impl->fatalError;
		return false;
	}
	if (m_impl->audit.is_open()) m_impl->audit.flush();
	return true;
}

bool DdsVideoPublisher::endRound(std::string& error)
{
	const auto drainBegin = std::chrono::steady_clock::now();
	if (!flush((m_impl->config.ackTimeoutSec + 5) * 1000, error)) return false;
	const auto queueDrained = std::chrono::steady_clock::now();
	int ackReturnCode = 0;
#if defined(HWASIMIR_HAS_ZRDDS)
	if (m_impl->writer)
	{
		DDS::Duration_t timeout;
		timeout.sec = m_impl->config.ackTimeoutSec;
		timeout.nanosec = 0;
		const DDS::ReturnCode_t ackResult = m_impl->writer->wait_for_acknowledgments(timeout);
		ackReturnCode = static_cast<int>(ackResult);
		if (ackResult != DDS::RETCODE_OK)
		{
			error = "DDS endRound wait_for_acknowledgments failed";
			return false;
		}
	}
#endif
	const auto ackReturned = std::chrono::steady_clock::now();
	const int boundedDrainMs = std::max(0, m_impl->config.shutdownDrainMs);
	if (boundedDrainMs > 0)
		std::this_thread::sleep_for(std::chrono::milliseconds(boundedDrainMs));
	const auto drainComplete = std::chrono::steady_clock::now();
	const double queueDrainMs = std::chrono::duration<double, std::milli>(queueDrained - drainBegin).count();
	const double ackWaitMs = std::chrono::duration<double, std::milli>(ackReturned - queueDrained).count();
	m_impl->logPerf(true);
	std::cout << "[DdsVideoDrain] roundDrained=1 writerRetained=1 finalize=0"
		<< " queueDrainMs=" << std::fixed << std::setprecision(3) << queueDrainMs
		<< " ackReturn=" << ackReturnCode
		<< " ackWaitMs=" << ackWaitMs
		<< " boundedDrainMs=" << boundedDrainMs
		<< " totalDrainMs=" << std::chrono::duration<double, std::milli>(drainComplete - drainBegin).count()
		<< std::endl;
	return true;
}

void DdsVideoPublisher::shutdown()
{
	if (!m_impl || !m_impl->enabled.load()) return;
	std::string error;
	m_impl->accepting.store(false);
	flush((m_impl->config.ackTimeoutSec + 5) * 1000, error);
	m_impl->running.store(false);
	m_impl->dataCv.notify_all();
	m_impl->spaceCv.notify_all();
	if (m_impl->worker.joinable()) m_impl->worker.join();
#if defined(HWASIMIR_HAS_ZRDDS)
	if (m_impl->writer)
	{
		DDS::Duration_t timeout;
		timeout.sec = m_impl->config.ackTimeoutSec;
		timeout.nanosec = 0;
		m_impl->writer->wait_for_acknowledgments(timeout);
		std::this_thread::sleep_for(std::chrono::milliseconds(m_impl->config.shutdownDrainMs));
		DDSIF::UnPubTopic(m_impl->writer);
		m_impl->writer = nullptr;
	}
	DDSIF::Finalize();
	m_impl->factory = nullptr;
	m_impl->participant = nullptr;
#endif
	if (m_impl->audit.is_open()) m_impl->audit.close();
	m_impl->logPerf(true);
	std::cout << "[DdsVideo] shutdown=1 finalize=1" << std::endl;
	m_impl->enabled.store(false);
}

bool DdsVideoPublisher::enabled() const { return m_impl->enabled.load(); }
bool DdsVideoPublisher::healthy() const { return m_impl->healthy.load(); }
std::string DdsVideoPublisher::topic() const
{
	std::lock_guard<std::mutex> lock(m_impl->mutex);
	return m_impl->topic;
}
DdsVideoPublisherStats DdsVideoPublisher::stats() const
{
	std::lock_guard<std::mutex> lock(m_impl->mutex);
	DdsVideoPublisherStats value = m_impl->stats;
	value.queueDepth = m_impl->queue.size();
	value.writeMsAverage = value.sentSamples
		? m_impl->writeMsTotal / static_cast<double>(value.sentSamples) : 0.0;
	return value;
}
