#include "ShapeType.h"
#include "ShapeTypeDataReader.h"
#include "ShapeTypeDataWriter.h"
#include "ShapeTypeTypeSupport.h"
#include "ZRDDSCppSimpleInterface.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace DDS;

struct Options
{
    std::string role, mode = "direct";
    std::string qos = "Config/DDS/ZRDDS_PROTOCOL_QOS.xml";
    std::string topic = "HwaSimIR.Benchmark.Video";
    int domain = 150, fps = 60, durationSec = 10, timeoutSec = 30, shutdownDrainMs = 2000;
    std::uint64_t payloadBytes = 640000, frames = 0;
};

static Options Parse(int argc, char** argv)
{
    Options o;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a(argv[i]);
        if (i + 1 >= argc) throw std::runtime_error("missing value for " + a);
        const std::string v(argv[++i]);
        if (a == "--role") o.role = v; else if (a == "--mode") o.mode = v;
        else if (a == "--qos") o.qos = v; else if (a == "--topic") o.topic = v;
        else if (a == "--domain") o.domain = std::atoi(v.c_str());
        else if (a == "--fps") o.fps = std::atoi(v.c_str());
        else if (a == "--duration-sec") o.durationSec = std::atoi(v.c_str());
        else if (a == "--timeout-sec") o.timeoutSec = std::atoi(v.c_str());
        else if (a == "--shutdown-drain-ms") o.shutdownDrainMs = std::atoi(v.c_str());
        else if (a == "--payload-bytes") o.payloadBytes = std::strtoull(v.c_str(), 0, 10);
        else if (a == "--frames") o.frames = std::strtoull(v.c_str(), 0, 10);
        else throw std::runtime_error("unknown option " + a);
    }
    if (o.role != "pub" && o.role != "sub") throw std::runtime_error("--role pub|sub required");
    if (o.mode != "direct" && o.mode != "shape1k" && o.mode != "shape32k" &&
        o.mode != "chunk64k" && o.mode != "chunk32k") throw std::runtime_error("invalid --mode");
    if (!o.frames) o.frames = static_cast<std::uint64_t>(o.fps) * o.durationSec;
    if (o.shutdownDrainMs < 0) throw std::runtime_error("--shutdown-drain-ms must be >= 0");
    return o;
}

#pragma pack(push, 1)
struct ChunkHeader { std::uint32_t magic, frame, chunk, chunks, bytes; };
#pragma pack(pop)
static const std::uint32_t kChunkMagic = 0x314B4E43u;
static bool IsShape(const Options& o) { return o.mode.find("shape") == 0; }
static std::size_t ChunkBytes(const Options& o)
{
    if (o.mode == "shape1k") return 1024;
    if (o.mode == "shape32k" || o.mode == "chunk32k") return 32768;
    if (o.mode == "chunk64k") return 65536;
    return static_cast<std::size_t>(o.payloadBytes);
}

struct ReceiverStats
{
    std::mutex mutex; std::condition_variable ready;
    std::uint64_t samples = 0, frames = 0, bytes = 0, errors = 0;
    std::uint32_t activeFrame = 0, activeChunks = 0, expectedChunks = 0;
    std::uint64_t activeBytes = 0;
    bool started = false;
    std::chrono::steady_clock::time_point first, last;
    void accept(std::uint32_t frame, std::uint32_t chunk, std::uint32_t chunks,
                const unsigned char* data, std::size_t size, std::uint64_t expectedFrameBytes)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!started) { started = true; first = std::chrono::steady_clock::now(); }
        last = std::chrono::steady_clock::now(); ++samples; bytes += size;
        if (!data && size) ++errors;
        if (chunks == 1) ++frames;
        else
        {
            if (chunk == 0) { activeFrame = frame; activeChunks = 0; activeBytes = 0; expectedChunks = chunks; }
            if (frame != activeFrame || chunk != activeChunks || chunks != expectedChunks) ++errors;
            ++activeChunks; activeBytes += size;
            if (chunk + 1 == chunks)
            {
                if (activeBytes != expectedFrameBytes) ++errors;
                ++frames;
            }
        }
        ready.notify_all();
    }
};

class BytesListener : public SimpleDataReaderListener<Bytes, BytesSeq, BytesDataReader>
{
public:
    BytesListener(const Options& o, ReceiverStats& s) : options(o), stats(s) {}
    void on_process_sample(DataReader*, const Bytes& sample, const SampleInfo&) override
    {
        const unsigned char* data = sample.value.get_contiguous_buffer();
        const std::size_t size = sample.value.length();
        if (options.mode == "direct") stats.accept(static_cast<std::uint32_t>(stats.frames), 0, 1, data, size, options.payloadBytes);
        else if (size < sizeof(ChunkHeader)) stats.accept(0, 0, 1, 0, 0, options.payloadBytes);
        else
        {
            ChunkHeader h; std::memcpy(&h, data, sizeof(h));
            if (h.magic != kChunkMagic || h.bytes + sizeof(h) != size) stats.accept(h.frame, h.chunk, h.chunks, 0, 0, options.payloadBytes);
            else stats.accept(h.frame, h.chunk, h.chunks, data + sizeof(h), h.bytes, options.payloadBytes);
        }
    }
private: const Options& options; ReceiverStats& stats;
};

class ShapeListener : public SimpleDataReaderListener<ShapeType, ShapeTypeSeq, ShapeTypeDataReader>
{
public:
    ShapeListener(const Options& o, ReceiverStats& s) : options(o), stats(s) {}
    void on_process_sample(DataReader*, const ShapeType& sample, const SampleInfo&) override
    {
        stats.accept(static_cast<std::uint32_t>(sample.x), static_cast<std::uint32_t>(sample.sn),
            static_cast<std::uint32_t>(sample.cmd), sample.data.get_contiguous_buffer(),
            static_cast<std::size_t>(sample.len), options.payloadBytes);
    }
private: const Options& options; ReceiverStats& stats;
};

static int RunSubscriber(const Options& o, DomainParticipant* participant)
{
    ReceiverStats stats; BytesListener bytes(o, stats); ShapeListener shape(o, stats);
    DataReader* reader = IsShape(o)
        ? DDSIF::SubTopic(participant, o.topic.c_str(), ShapeTypeTypeSupport::get_instance(), "hwasimir_protocol_reader", &shape)
        : DDSIF::SubTopic(participant, o.topic.c_str(), BytesTypeSupport::get_instance(), "hwasimir_reliable_reader", &bytes);
    if (!reader) throw std::runtime_error("SubTopic failed");
    std::unique_lock<std::mutex> lock(stats.mutex);
    const bool complete = stats.ready.wait_for(lock, std::chrono::seconds(o.timeoutSec), [&] { return stats.frames >= o.frames; });
    const double seconds = stats.started ? std::chrono::duration<double>(stats.last - stats.first).count() : 0.0;
    std::cout << std::fixed << std::setprecision(3)
        << "[BenchmarkReceiver] mode=" << o.mode << " receiverSamples=" << stats.samples
        << " receiverFrames=" << stats.frames << " receiverMiBPerSec="
        << (seconds > 0 ? stats.bytes / 1048576.0 / seconds : 0.0)
        << " errors=" << stats.errors << " drops=" << (o.frames > stats.frames ? o.frames - stats.frames : 0)
        << " complete=" << (complete ? 1 : 0) << std::endl;
    lock.unlock(); DDSIF::UnSubTopic(reader);
    return complete && stats.errors == 0 && stats.frames == o.frames ? 0 : 8;
}

static int RunPublisher(const Options& o, DomainParticipant* participant)
{
    DataWriter* base = IsShape(o)
        ? DDSIF::PubTopic(participant, o.topic.c_str(), ShapeTypeTypeSupport::get_instance(), "hwasimir_protocol_writer", 0)
        : DDSIF::PubTopic(participant, o.topic.c_str(), BytesTypeSupport::get_instance(), "hwasimir_reliable_writer", 0);
    if (!base) throw std::runtime_error("PubTopic failed");
    ShapeTypeDataWriter* shapeWriter = dynamic_cast<ShapeTypeDataWriter*>(base);
    std::vector<unsigned char> source(static_cast<std::size_t>(o.payloadBytes));
    for (std::size_t i = 0; i < source.size(); ++i) source[i] = static_cast<unsigned char>((i * 131u + 17u) & 0xffu);
    const std::size_t chunkBytes = ChunkBytes(o);
    std::uint64_t samples = 0, errors = 0, sourceBytes = 0;
    double copyMs = 0, writeMs = 0, writeMaxMs = 0;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    for (std::uint64_t frame = 0; frame < o.frames; ++frame)
    {
        const std::uint32_t chunks = static_cast<std::uint32_t>((source.size() + chunkBytes - 1) / chunkBytes);
        for (std::uint32_t chunk = 0; chunk < chunks; ++chunk)
        {
            const std::size_t offset = static_cast<std::size_t>(chunk) * chunkBytes;
            const std::size_t count = (std::min)(chunkBytes, source.size() - offset);
            const std::chrono::steady_clock::time_point copyStart = std::chrono::steady_clock::now();
            ReturnCode_t rc = RETCODE_ERROR;
            if (IsShape(o))
            {
                ShapeType sample; ShapeTypeInitialize(&sample); sample.x = static_cast<long>(frame);
                sample.sn = chunk; sample.cmd = chunks; sample.type = o.mode == "shape1k" ? 1 : 32;
                sample.len = static_cast<long>(count); sample.data.from_array(source.data() + offset, count);
                copyMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - copyStart).count();
                const std::chrono::steady_clock::time_point writeStart = std::chrono::steady_clock::now();
                rc = shapeWriter->write(sample, HANDLE_NIL_NATIVE);
                const double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - writeStart).count();
                writeMs += elapsed; writeMaxMs = (std::max)(writeMaxMs, elapsed); ShapeTypeFinalize(&sample);
            }
            else
            {
                std::vector<unsigned char> wire;
                if (o.mode == "direct") wire.assign(source.begin(), source.end());
                else
                {
                    ChunkHeader h = { kChunkMagic, static_cast<std::uint32_t>(frame), chunk, chunks, static_cast<std::uint32_t>(count) };
                    wire.resize(sizeof(h) + count); std::memcpy(wire.data(), &h, sizeof(h));
                    std::memcpy(wire.data() + sizeof(h), source.data() + offset, count);
                }
                copyMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - copyStart).count();
                const std::chrono::steady_clock::time_point writeStart = std::chrono::steady_clock::now();
                rc = DDSIF::BytesWrite(o.domain, const_cast<char*>(o.topic.c_str()),
                    reinterpret_cast<const char*>(wire.data()), static_cast<DDS_Long>(wire.size()));
                const double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - writeStart).count();
                writeMs += elapsed; writeMaxMs = (std::max)(writeMaxMs, elapsed);
            }
            ++samples; if (rc != RETCODE_OK) ++errors;
        }
        sourceBytes += source.size();
        if (o.fps > 0) std::this_thread::sleep_until(begin + std::chrono::nanoseconds((frame + 1) * 1000000000LL / o.fps));
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    std::cout << std::fixed << std::setprecision(3)
        << "[BenchmarkPublisher] mode=" << o.mode << " sourceFrames=" << o.frames << " sourceBytes=" << sourceBytes
        << " appCopyMs=" << copyMs << " enqueueMs=0 queueWaitMs=0 ddsWriteMs=" << writeMs
        << " ddsWriteMsMax=" << writeMaxMs << " samplesPerSec=" << (samples / seconds)
        << " MiBPerSec=" << (sourceBytes / 1048576.0 / seconds) << " errors=" << errors << " drops=0" << std::endl;
    Duration_t ackTimeout;
    ackTimeout.sec = o.timeoutSec;
    ackTimeout.nanosec = 0;
    const ReturnCode_t ackResult = base->wait_for_acknowledgments(ackTimeout);
    std::cout << "[BenchmarkDrain] ackResult=" << ackResult
        << " boundedDrainMs=" << o.shutdownDrainMs << std::endl;
    if (o.shutdownDrainMs > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(o.shutdownDrainMs));
    DDSIF::UnPubTopic(base);
    return errors == 0 && ackResult == RETCODE_OK ? 0 : 9;
}

int main(int argc, char** argv)
{
    try
    {
        const Options o = Parse(argc, argv);
        if (!DDSIF::Init(o.qos.c_str(), "hwasimir_factory")) throw std::runtime_error("DDSIF::Init failed");
        DomainParticipant* p = DDSIF::CreateDP(o.domain, "hwasimir_tcp");
        if (!p) throw std::runtime_error("CreateDP failed");
        const int result = o.role == "pub" ? RunPublisher(o, p) : RunSubscriber(o, p);
        DDSIF::Finalize(); return result;
    }
    catch (const std::exception& e) { std::cerr << "benchmark_error=" << e.what() << std::endl; return 1; }
}
