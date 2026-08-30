#include "DdsStimClient.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <sstream>

struct SensorSpec
{
    int sensorID = 0;
    int width = 0;
    int height = 0;
    double pixelAngle = 0.0;
};

struct Options
{
    int domain = 150, platID = 1, sensorID = 1, simMode = 2, videoFps = 60;
    int width = 800, height = 800, duration = 5, realtimeHz = 60, rounds = 1;
    int startSettleMs = 250;
    int discoveryWaitMs = 2000, ackTimeoutMs = 30000, shutdownDrainMs = 5000;
    int interRoundWaitMs = 6000;
    bool h264En = true, saveMP4En = false, realtimeAnnotation = false;
    bool broadcastInit = false, broadcastRealtime = false;
    std::vector<SensorSpec> sensors;
    std::string qos = "Config/DDS/ZRDDS_PROTOCOL_QOS.xml";
};

static bool ReadValue(int& i, int argc, char** argv, const char* name, std::string& value)
{
    if (std::string(argv[i]) != name || i + 1 >= argc) return false;
    value = argv[++i]; return true;
}
static bool Parse(int argc, char** argv, Options& o)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string value;
        if (ReadValue(i, argc, argv, "--qos", value)) o.qos = value;
        else if (ReadValue(i, argc, argv, "--domain", value)) o.domain = std::atoi(value.c_str());
        else if (ReadValue(i, argc, argv, "--plat-id", value)) o.platID = std::atoi(value.c_str());
        else if (ReadValue(i, argc, argv, "--sensor-id", value)) o.sensorID = std::atoi(value.c_str());
        else if (ReadValue(i, argc, argv, "--sim-mode", value)) o.simMode = std::atoi(value.c_str());
        else if (ReadValue(i, argc, argv, "--video-fps", value)) o.videoFps = std::atoi(value.c_str());
        else if (ReadValue(i, argc, argv, "--width", value)) o.width = std::atoi(value.c_str());
        else if (ReadValue(i, argc, argv, "--height", value)) o.height = std::atoi(value.c_str());
        else if (ReadValue(i, argc, argv, "--duration", value)) o.duration = std::atoi(value.c_str());
        else if (ReadValue(i, argc, argv, "--realtime-hz", value)) o.realtimeHz = std::atoi(value.c_str());
        else if (ReadValue(i, argc, argv, "--start-settle-ms", value)) o.startSettleMs = std::atoi(value.c_str());
        else if (ReadValue(i, argc, argv, "--rounds", value)) o.rounds = std::atoi(value.c_str());
        else if (ReadValue(i, argc, argv, "--discovery-wait-ms", value)) o.discoveryWaitMs = std::atoi(value.c_str());
        else if (ReadValue(i, argc, argv, "--ack-timeout-ms", value)) o.ackTimeoutMs = std::atoi(value.c_str());
        else if (ReadValue(i, argc, argv, "--shutdown-drain-ms", value)) o.shutdownDrainMs = std::atoi(value.c_str());
        else if (ReadValue(i, argc, argv, "--inter-round-wait-ms", value)) o.interRoundWaitMs = std::atoi(value.c_str());
        else if (ReadValue(i, argc, argv, "--h264", value)) o.h264En = std::atoi(value.c_str()) != 0;
        else if (ReadValue(i, argc, argv, "--save-mp4", value)) o.saveMP4En = std::atoi(value.c_str()) != 0;
        else if (ReadValue(i, argc, argv, "--realtime-annotation", value)) o.realtimeAnnotation = std::atoi(value.c_str()) != 0;
        else if (ReadValue(i, argc, argv, "--broadcast-init", value)) o.broadcastInit = std::atoi(value.c_str()) != 0;
        else if (ReadValue(i, argc, argv, "--broadcast-realtime", value)) o.broadcastRealtime = std::atoi(value.c_str()) != 0;
        else if (ReadValue(i, argc, argv, "--sensor", value))
        {
            SensorSpec spec;
            char colon1 = 0, times = 0, colon2 = 0;
            std::istringstream input(value);
            if (!(input >> spec.sensorID >> colon1 >> spec.width >> times >> spec.height >>
                    colon2 >> spec.pixelAngle) ||
                colon1 != ':' || (times != 'x' && times != 'X') || colon2 != ':' ||
                input.peek() != std::char_traits<char>::eof() ||
                spec.sensorID < 0 || spec.width <= 0 || spec.height <= 0)
            {
                std::cerr << "invalid --sensor, expected id:WIDTHxHEIGHT:pixelAngle" << std::endl;
                return false;
            }
            o.sensors.push_back(spec);
        }
        else { std::cerr << "unknown/missing option: " << argv[i] << std::endl; return false; }
    }
    return o.realtimeHz > 0 && o.duration >= 0 && o.rounds > 0 && o.width > 0 && o.height > 0 &&
        o.startSettleMs >= 0 &&
        o.discoveryWaitMs >= 0 && o.ackTimeoutMs > 0 && o.shutdownDrainMs >= 0 &&
        o.interRoundWaitMs >= 0;
}

int main(int argc, char** argv)
{
    Options options;
    if (!Parse(argc, argv, options)) return 2;
    DdsStimConfig config;
    config.domainId = options.domain;
    config.qosFile = options.qos;
    DdsStimClient client;
    std::string error;
    if (!client.start(config, error)) { std::cerr << "[Stim][FATAL] " << error << std::endl; return 3; }
    if (options.discoveryWaitMs > 0)
    {
        std::cout << "[StimDiscovery] waitMs=" << options.discoveryWaitMs << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(options.discoveryWaitMs));
    }

    const long long total = static_cast<long long>(options.duration) * options.realtimeHz;
    long long totalRealtime = 0;
    for (int round = 1; round <= options.rounds; ++round)
    {
        BYHWICD::ControlP2cX1ObjTrackingCmd control = {};
        control.flag = 0x41; control.JB = 1; control.platID = options.platID;
        control.roundCut = options.rounds; control.currentRound = round; control.simCommand = 1;
        if (!client.sendControl(control, error)) { std::cerr << error << std::endl; return 4; }

        const auto makeInit = [&](int sensorID, int width, int height, double pixelAngle) {
            BYHWICD::InitP2cObjectTrackingCmd init = {};
            init.flag = 0x36; init.JB = 1; init.platID = options.platID; init.sensorID = sensorID;
            init.platParamInit.id = options.platID; init.platParamInit.type = 1;
            init.trackingInit.simMode = options.simMode; init.trackingInit.videoFps = options.videoFps;
            init.trackingInit.trackerSensor[0].h264En = options.h264En;
            init.trackingInit.trackerSensor[0].saveMP4En = options.saveMP4En;
            init.trackingInit.trackerSensor[0].realtimeAnnotation = options.realtimeAnnotation;
            init.trackingInit.trackerSensor[0].trackerSensorWidth = width;
            init.trackingInit.trackerSensor[0].trackerSensorHeight = height;
            init.trackingInit.trackerSensor[0].trackerSensorPixelAngle = pixelAngle;
            init.trackingInit.trackerSensor[0].trackerSensorBand = 2;
            return init;
        };
        if (options.sensors.empty())
        {
            const BYHWICD::InitP2cObjectTrackingCmd init = makeInit(
                options.sensorID, options.width, options.height, 0.0);
            if (!client.sendInit(init, error)) { std::cerr << error << std::endl; return 5; }
            BYHWICD::InitAckC2pObjectTrackingCmd ack = {};
            if (!client.waitForInitAck(options.ackTimeoutMs, ack)) return 6;
            std::cout << "[StimInitAck] received=1 round=" << round
                      << " platID=" << ack.platID << " sensorID=" << ack.sensorID
                      << " trackingReady=" << (ack.trackingReady ? 1 : 0) << std::endl;
            if (!ack.trackingReady) return 7;
        }
        else
        {
            for (std::size_t index = 0; index < options.sensors.size(); ++index)
            {
                const SensorSpec& spec = options.sensors[index];
                const BYHWICD::InitP2cObjectTrackingCmd init = makeInit(
                    spec.sensorID, spec.width, spec.height, spec.pixelAngle);
                if (!client.sendInit(init, error)) return 5;
                BYHWICD::InitAckC2pObjectTrackingCmd ack = {};
                if (!client.waitForInitAck(options.ackTimeoutMs, ack)) return 6;
                std::cout << "[StimExactInitAck] requestedSensorID=" << spec.sensorID
                          << " ackSensorID=" << ack.sensorID << " width=" << spec.width
                          << " height=" << spec.height << " pixelAngle=" << spec.pixelAngle << std::endl;
                if (ack.sensorID != spec.sensorID || !ack.trackingReady) return 7;
            }
            if (options.broadcastInit)
            {
                const BYHWICD::InitP2cObjectTrackingCmd init = makeInit(
                    255, options.width, options.height, 0.0);
                if (!client.sendInit(init, error)) return 5;
                std::vector<BYHWICD::InitAckC2pObjectTrackingCmd> acks;
                if (!client.waitForInitAcks(options.ackTimeoutMs,
                    options.sensors.size(), acks)) return 6;
                std::cout << "[StimBroadcastInitAck] requestedSensorID=255 received="
                          << acks.size() << " sensors=";
                for (std::size_t i = 0; i < acks.size(); ++i)
                    std::cout << (i ? "," : "") << acks[i].sensorID;
                std::cout << std::endl;
            }
        }

        control.simCommand = 2;
        if (!client.sendControl(control, error)) return 8;
        if (options.startSettleMs > 0)
        {
            std::cout << "[StimStartSettle] round=" << round
                      << " waitMs=" << options.startSettleMs << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(options.startSettleMs));
        }
        const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        for (long long index = 0; index < total; ++index)
        {
            const auto sendRealtime = [&](int sensorID) {
                BYHWICD::DisplayC2cObjTrackingData realtime = {};
                realtime.flag = 0x38; realtime.platID = options.platID; realtime.sensorID = sensorID;
                realtime.time = 1000.0 * static_cast<double>(index) / options.realtimeHz;
                realtime.platLoc.alt = 1000.0; realtime.platLoc.speed = 250.0;
                if (!client.sendRealtime(realtime, error)) return false;
                ++totalRealtime;
                return true;
            };
            if (options.sensors.empty())
            {
                if (!sendRealtime(options.sensorID)) return 9;
            }
            else if (options.broadcastRealtime)
            {
                if (!sendRealtime(255)) return 9;
            }
            else
            {
                for (std::size_t sensorIndex = 0; sensorIndex < options.sensors.size(); ++sensorIndex)
                    if (!sendRealtime(options.sensors[sensorIndex].sensorID)) return 9;
            }
            const std::chrono::steady_clock::time_point deadline = start +
                std::chrono::nanoseconds((index + 1) * 1000000000LL / options.realtimeHz);
            std::this_thread::sleep_until(deadline);
        }
        control.simCommand = 3;
        if (!client.sendControl(control, error)) return 10;
        // STOP has no business ACK. HwaSim_IR must drain video/meta/annotation
        // and close recording before the next RESET. Keep this bounded pause
        // explicit so a test driver cannot overtake the STOP lifecycle.
        if (round < options.rounds && options.interRoundWaitMs > 0)
        {
            std::cout << "[StimRoundDrain] round=" << round
                      << " waitMs=" << options.interRoundWaitMs << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(options.interRoundWaitMs));
        }
    }
    if (!client.waitForAcknowledgments(options.ackTimeoutMs, error))
    {
        std::cerr << "[Stim][FATAL] drain " << error << std::endl;
        return 11;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(options.shutdownDrainMs));
    std::cout << "[StimSummary] runtimeInitCount=" << client.runtimeInitCount()
              << " rounds=" << options.rounds << " reset=" << options.rounds
              << " init=" << options.rounds << " ack=" << options.rounds
              << " start=" << options.rounds << " realtime=" << totalRealtime
              << " stop=" << options.rounds
              << " shutdownDrainMs=" << options.shutdownDrainMs << " errors=0" << std::endl;
    client.shutdown();
    return 0;
}
