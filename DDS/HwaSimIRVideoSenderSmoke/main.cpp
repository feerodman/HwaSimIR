#include "ZRDDSCppSimpleInterface.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace DDS;

namespace {

struct Options {
    int domain = 150;
    std::string topic = "HwaSimIR.Video.precise.RawGray8";
    std::string mode = "payload4k";
    std::string qos = "Config/ZRDDS_QOS_PROFILES.xml";
    std::string input;
    std::string auLengths;
    std::string sourceOutput;
    std::uint64_t frames = 1000;
    int startupMs = 3000;
    int intervalMs = 0;
    int ackTimeoutSec = 60;
    int drainMs = 5000;
};

void Usage() {
    std::cout << "Usage: HwaSimIRVideoSenderSmoke --domain N --topic NAME --qos FILE\n"
              << "  --mode payload4k|payload1m|raw_gray8|h264 --frames N\n"
              << "  [--input annexb.h264 --au-lengths au_lengths.txt]\n"
              << "  [--source-output FILE] [--startup-ms N] [--interval-ms N]\n"
              << "  [--ack-timeout-sec N] [--drain-ms N]\n";
}

std::uint64_t Unsigned(const char* text, const char* option) {
    char* end = NULL;
    const unsigned long long value = std::strtoull(text, &end, 10);
    if (!text[0] || (end && *end)) throw std::runtime_error(std::string("invalid value for ") + option);
    return static_cast<std::uint64_t>(value);
}

Options Parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--help" || arg == "-h") { Usage(); std::exit(0); }
        if (i + 1 >= argc) throw std::runtime_error("missing value for " + arg);
        const char* value = argv[++i];
        if (arg == "--domain") o.domain = static_cast<int>(Unsigned(value, "--domain"));
        else if (arg == "--topic") o.topic = value;
        else if (arg == "--mode") o.mode = value;
        else if (arg == "--qos") o.qos = value;
        else if (arg == "--input") o.input = value;
        else if (arg == "--au-lengths") o.auLengths = value;
        else if (arg == "--source-output") o.sourceOutput = value;
        else if (arg == "--frames") o.frames = Unsigned(value, "--frames");
        else if (arg == "--startup-ms") o.startupMs = static_cast<int>(Unsigned(value, "--startup-ms"));
        else if (arg == "--interval-ms") o.intervalMs = static_cast<int>(Unsigned(value, "--interval-ms"));
        else if (arg == "--ack-timeout-sec") o.ackTimeoutSec = static_cast<int>(Unsigned(value, "--ack-timeout-sec"));
        else if (arg == "--drain-ms") o.drainMs = static_cast<int>(Unsigned(value, "--drain-ms"));
        else throw std::runtime_error("unknown option: " + arg);
    }
    if (o.domain < 0 || o.domain > 232) throw std::runtime_error("--domain must be 0..232");
    if (o.frames == 0) throw std::runtime_error("--frames must be greater than zero");
    if (o.mode != "payload4k" && o.mode != "payload1m" && o.mode != "raw_gray8" && o.mode != "h264") {
        throw std::runtime_error("unsupported --mode");
    }
    if (o.mode == "h264" && (o.input.empty() || o.auLengths.empty())) {
        throw std::runtime_error("h264 requires --input and --au-lengths");
    }
    return o;
}

std::vector<char> ReadFile(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input) throw std::runtime_error("cannot open input: " + path);
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    input.seekg(0, std::ios::beg);
    if (size < 0) throw std::runtime_error("cannot determine input size");
    std::vector<char> bytes(static_cast<std::size_t>(size));
    if (!bytes.empty()) input.read(&bytes[0], static_cast<std::streamsize>(bytes.size()));
    if (!input) throw std::runtime_error("cannot read input: " + path);
    return bytes;
}

std::vector<std::size_t> ReadLengths(const std::string& path) {
    std::ifstream input(path.c_str());
    if (!input) throw std::runtime_error("cannot open AU lengths: " + path);
    std::vector<std::size_t> lengths;
    std::string line;
    while (std::getline(input, line)) {
        const std::string::size_type comment = line.find('#');
        if (comment != std::string::npos) line.erase(comment);
        std::istringstream parser(line);
        unsigned long long value = 0;
        if (parser >> value) {
            if (value == 0) throw std::runtime_error("AU length must be greater than zero");
            lengths.push_back(static_cast<std::size_t>(value));
        }
    }
    if (lengths.empty()) throw std::runtime_error("AU lengths file is empty");
    return lengths;
}

std::vector<char> Deterministic(std::size_t bytes, std::uint64_t frame) {
    std::vector<char> payload(bytes);
    for (std::size_t i = 0; i < bytes; ++i) {
        payload[i] = static_cast<char>((frame * 131u + i * 17u + (i >> 8u)) & 0xffu);
    }
    return payload;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const Options o = Parse(argc, argv);
        std::ofstream source;
        if (!o.sourceOutput.empty()) {
            source.open(o.sourceOutput.c_str(), std::ios::binary | std::ios::trunc);
            if (!source) throw std::runtime_error("cannot open source output: " + o.sourceOutput);
        }

        std::vector<char> h264;
        std::vector<std::size_t> auLengths;
        std::vector<std::size_t> auOffsets;
        if (o.mode == "h264") {
            h264 = ReadFile(o.input);
            auLengths = ReadLengths(o.auLengths);
            std::size_t offset = 0;
            for (std::size_t i = 0; i < auLengths.size(); ++i) {
                auOffsets.push_back(offset);
                if (auLengths[i] > h264.size() - offset) throw std::runtime_error("AU lengths exceed input size");
                offset += auLengths[i];
            }
            if (offset != h264.size()) throw std::runtime_error("AU lengths do not exactly cover input");
            if (o.frames > auLengths.size()) throw std::runtime_error("--frames exceeds indexed AU count");
        }

        DomainParticipantFactory* factory = DDSIF::Init(o.qos.c_str(), "hwasimir_factory");
        if (factory == NULL) { std::cerr << "DDSIF::Init failed writeErrors=1\n"; return 2; }
        DomainParticipant* participant = DDSIF::CreateDP(o.domain, "hwasimir_tcp");
        if (participant == NULL) { std::cerr << "DDSIF::CreateDP failed writeErrors=1\n"; DDSIF::Finalize(); return 3; }
        DataWriter* writer = DDSIF::PubTopic(participant, o.topic.c_str(),
            BytesTypeSupport::get_instance(), "hwasimir_reliable_writer", NULL);
        if (writer == NULL) { std::cerr << "DDSIF::PubTopic failed writeErrors=1\n"; DDSIF::Finalize(); return 4; }

        std::cout << "senderReady=1 domain=" << o.domain << " topic=" << o.topic
                  << " mode=" << o.mode << " frames=" << o.frames << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(o.startupMs));
        const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
        std::uint64_t sent = 0, bytes = 0, errors = 0, minBytes = 0, maxBytes = 0;
        double maxWriteMs = 0.0, totalWriteMs = 0.0;
        for (std::uint64_t frame = 0; frame < o.frames; ++frame) {
            std::vector<char> generated;
            const char* data = NULL;
            std::size_t size = 0;
            if (o.mode == "h264") {
                data = &h264[auOffsets[static_cast<std::size_t>(frame)]];
                size = auLengths[static_cast<std::size_t>(frame)];
            } else {
                const std::size_t payloadBytes = o.mode == "payload4k" ? 4096u
                    : (o.mode == "payload1m" ? 1024u * 1024u : 800u * 800u);
                generated = Deterministic(payloadBytes, frame);
                data = &generated[0];
                size = generated.size();
            }
            if (source.is_open()) source.write(data, static_cast<std::streamsize>(size));

            const std::chrono::steady_clock::time_point beforeWrite = std::chrono::steady_clock::now();
            const ReturnCode_t result = DDSIF::BytesWrite(o.domain,
                const_cast<char*>(o.topic.c_str()), data, static_cast<Long>(size));
            const double writeMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - beforeWrite).count();
            totalWriteMs += writeMs;
            maxWriteMs = (std::max)(maxWriteMs, writeMs);
            if (result != RETCODE_OK) {
                ++errors;
                std::cerr << "write_error sample=" << frame << " code=" << result << "\n";
                break;
            }
            ++sent;
            bytes += size;
            minBytes = sent == 1 ? size : (std::min)(minBytes, static_cast<std::uint64_t>(size));
            maxBytes = (std::max)(maxBytes, static_cast<std::uint64_t>(size));
            if (o.intervalMs) std::this_thread::sleep_for(std::chrono::milliseconds(o.intervalMs));
        }
        if (source.is_open()) { source.flush(); if (!source) ++errors; }
        Duration_t ackTimeout;
        ackTimeout.sec = o.ackTimeoutSec;
        ackTimeout.nanosec = 0;
        const std::chrono::steady_clock::time_point beforeAck = std::chrono::steady_clock::now();
        const ReturnCode_t ackResult = writer->wait_for_acknowledgments(ackTimeout);
        const double ackWaitMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - beforeAck).count();
        if (ackResult != RETCODE_OK) {
            ++errors;
            std::cerr << "wait_for_acknowledgments_error code=" << ackResult << "\n";
        }
        // ZRDDS 2.4.4's tcpv4 wait_for_acknowledgments returns immediately even
        // while large samples are still draining. Keep the writer alive for a
        // bounded grace period so Finalize cannot tear down the last reliable
        // transmission. This never overwrites or drops an application sample.
        std::this_thread::sleep_for(std::chrono::milliseconds(o.drainMs));
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        const ReturnCode_t finalizeResult = DDSIF::Finalize();
        if (finalizeResult != RETCODE_OK) ++errors;
        const double sampleAvg = sent ? static_cast<double>(bytes) / sent : 0.0;
        const double sampleRate = elapsed > 0.0 ? sent / elapsed : 0.0;
        const double mbps = elapsed > 0.0 ? bytes / elapsed / (1024.0 * 1024.0) : 0.0;
        std::cout << std::fixed << std::setprecision(3)
                  << "sentSamples=" << sent << " sentBytes=" << bytes
                  << " sampleBytesMin=" << (sent ? minBytes : 0)
                  << " sampleBytesAvg=" << sampleAvg << " sampleBytesMax=" << maxBytes
                  << " samplesPerSec=" << sampleRate << " throughputMiBps=" << mbps
                  << " writeMsAvg=" << (sent ? totalWriteMs / sent : 0.0)
                  << " writeMsMax=" << maxWriteMs << " writeErrors=" << errors
                  << " ackResult=" << ackResult << " ackWaitMs=" << ackWaitMs
                  << " drainMs=" << o.drainMs
                  << " droppedSamples=0 elapsedSec=" << elapsed << "\n";
        return sent == o.frames && errors == 0 ? 0 : 8;
    } catch (const std::exception& error) {
        std::cerr << "sender_error=" << error.what() << " writeErrors=1\n";
        Usage();
        return 1;
    }
}
