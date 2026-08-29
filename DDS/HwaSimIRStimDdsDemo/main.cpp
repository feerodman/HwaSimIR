#include "DdsStimClient.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

struct Options
{
    int domain = 150, platID = 1, sensorID = 1, simMode = 2, videoFps = 60;
    int width = 800, height = 800, duration = 5, realtimeHz = 60;
    int discoveryWaitMs = 2000, ackTimeoutMs = 30000, shutdownDrainMs = 5000;
    bool h264En = true, saveMP4En = false;
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
        else if (ReadValue(i, argc, argv, "--discovery-wait-ms", value)) o.discoveryWaitMs = std::atoi(value.c_str());
        else if (ReadValue(i, argc, argv, "--ack-timeout-ms", value)) o.ackTimeoutMs = std::atoi(value.c_str());
        else if (ReadValue(i, argc, argv, "--shutdown-drain-ms", value)) o.shutdownDrainMs = std::atoi(value.c_str());
        else if (ReadValue(i, argc, argv, "--h264", value)) o.h264En = std::atoi(value.c_str()) != 0;
        else if (ReadValue(i, argc, argv, "--save-mp4", value)) o.saveMP4En = std::atoi(value.c_str()) != 0;
        else { std::cerr << "unknown/missing option: " << argv[i] << std::endl; return false; }
    }
    return o.realtimeHz > 0 && o.duration >= 0 && o.width > 0 && o.height > 0 &&
        o.discoveryWaitMs >= 0 && o.ackTimeoutMs > 0 && o.shutdownDrainMs >= 0;
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

    BYHWICD::ControlP2cX1ObjTrackingCmd control = {};
    control.flag = 0x41; control.JB = 1; control.platID = options.platID;
    control.roundCut = 1; control.currentRound = 1; control.simCommand = 1;
    if (!client.sendControl(control, error)) { std::cerr << error << std::endl; return 4; }

    BYHWICD::InitP2cObjectTrackingCmd init = {};
    init.flag = 0x36; init.JB = 1; init.platID = options.platID; init.sensorID = options.sensorID;
    init.platParamInit.id = options.platID; init.platParamInit.type = 1;
    init.trackingInit.simMode = options.simMode; init.trackingInit.videoFps = options.videoFps;
    init.trackingInit.trackerSensor[0].h264En = options.h264En;
    init.trackingInit.trackerSensor[0].saveMP4En = options.saveMP4En;
    init.trackingInit.trackerSensor[0].trackerSensorWidth = options.width;
    init.trackingInit.trackerSensor[0].trackerSensorHeight = options.height;
    init.trackingInit.trackerSensor[0].trackerSensorBand = 2;
    if (!client.sendInit(init, error)) { std::cerr << error << std::endl; return 5; }
    BYHWICD::InitAckC2pObjectTrackingCmd ack = {};
    if (!client.waitForInitAck(options.ackTimeoutMs, ack))
    {
        std::cerr << "[Stim][FATAL] InitAck timeoutMs=" << options.ackTimeoutMs << std::endl;
        return 6;
    }
    std::cout << "[StimInitAck] received=1 platID=" << ack.platID << " sensorID=" << ack.sensorID
              << " trackingReady=" << (ack.trackingReady ? 1 : 0) << std::endl;
    if (!ack.trackingReady) return 7;

    control.simCommand = 2;
    if (!client.sendControl(control, error)) return 8;
    const long long total = static_cast<long long>(options.duration) * options.realtimeHz;
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    for (long long index = 0; index < total; ++index)
    {
        BYHWICD::DisplayC2cObjTrackingData realtime = {};
        realtime.flag = 0x38; realtime.platID = options.platID; realtime.sensorID = options.sensorID;
        realtime.time = 1000.0 * static_cast<double>(index) / options.realtimeHz;
        realtime.platLoc.alt = 1000.0; realtime.platLoc.speed = 250.0;
        if (!client.sendRealtime(realtime, error)) { std::cerr << error << std::endl; return 9; }
        const std::chrono::steady_clock::time_point deadline = start +
            std::chrono::nanoseconds((index + 1) * 1000000000LL / options.realtimeHz);
        std::this_thread::sleep_until(deadline);
    }
    control.simCommand = 3;
    if (!client.sendControl(control, error)) return 10;
    if (!client.waitForAcknowledgments(options.ackTimeoutMs, error))
    {
        std::cerr << "[Stim][FATAL] drain " << error << std::endl;
        return 11;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(options.shutdownDrainMs));
    std::cout << "[StimSummary] runtimeInitCount=" << client.runtimeInitCount()
              << " reset=1 init=1 ack=1 start=1 realtime=" << total << " stop=1"
              << " shutdownDrainMs=" << options.shutdownDrainMs << " errors=0" << std::endl;
    client.shutdown();
    return 0;
}
