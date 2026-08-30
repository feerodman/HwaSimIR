#include "DdsRuntimeManager.h"
#include "DdsStimClient.h"
#include "HwaSimIRProtocolEndpoint.h"
#include "ProtocolRoute.h"

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

struct Options
{
    std::string mode;
    std::string qos = "Config/DDS/ZRDDS_PROTOCOL_QOS.xml";
    int domain = 150;
    int platID = 1001;
    int sensorID = 1;
    bool acceptBroadcast = true;
    int discoveryWaitMs = 3000;
    int timeoutMs = 30000;
    int expectedCallbacks = 12;
    int expectedBroadcastAcks = 3;
};

static bool Parse(int argc, char** argv, Options& o)
{
    for (int i = 1; i < argc; ++i)
    {
        if (i + 1 >= argc) return false;
        const std::string arg(argv[i]);
        const std::string value(argv[++i]);
        if (arg == "--mode") o.mode = value;
        else if (arg == "--qos") o.qos = value;
        else if (arg == "--domain") o.domain = std::atoi(value.c_str());
        else if (arg == "--plat-id") o.platID = std::atoi(value.c_str());
        else if (arg == "--sensor-id") o.sensorID = std::atoi(value.c_str());
        else if (arg == "--accept-broadcast") o.acceptBroadcast = std::atoi(value.c_str()) != 0;
        else if (arg == "--discovery-wait-ms") o.discoveryWaitMs = std::atoi(value.c_str());
        else if (arg == "--timeout-ms") o.timeoutMs = std::atoi(value.c_str());
        else if (arg == "--expected-callbacks") o.expectedCallbacks = std::atoi(value.c_str());
        else if (arg == "--expected-broadcast-acks") o.expectedBroadcastAcks = std::atoi(value.c_str());
        else return false;
    }
    return o.mode == "reader" || o.mode == "sender";
}

static BYHWICD::InitP2cObjectTrackingCmd MakeInit(int platID, int sensorID,
    int width, int height, double pixelAngle)
{
    BYHWICD::InitP2cObjectTrackingCmd value = {};
    value.flag = 0x36; value.JB = 1; value.platID = platID; value.sensorID = sensorID;
    value.trackingInit.simMode = 2; value.trackingInit.videoFps = 60;
    value.trackingInit.trackerSensor[0].trackerSensorWidth = width;
    value.trackingInit.trackerSensor[0].trackerSensorHeight = height;
    value.trackingInit.trackerSensor[0].trackerSensorPixelAngle = pixelAngle;
    value.trackingInit.trackerSensor[0].h264En = true;
    return value;
}

static int RunSender(const Options& o)
{
    DdsStimConfig config; config.domainId = o.domain; config.qosFile = o.qos;
    DdsStimClient client;
    std::string error;
    if (!client.start(config, error)) { std::cerr << error << std::endl; return 3; }
    std::this_thread::sleep_for(std::chrono::milliseconds(o.discoveryWaitMs));
    BYHWICD::ControlP2cX1ObjTrackingCmd control = {};
    control.flag = 0x41; control.platID = o.platID; control.simCommand = 2;
    control.currentRound = 1; control.roundCut = 1;
    if (!client.sendControl(control, error)) return 4;
    const int ids[] = { 1, 2, 3 };
    const int widths[] = { 640, 1280, 800 };
    const int heights[] = { 512, 1024, 800 };
    const double angles[] = { 11.1, 22.2, 33.3 };
    for (int i = 0; i < 3; ++i)
    {
        const BYHWICD::InitP2cObjectTrackingCmd init = MakeInit(
            o.platID, ids[i], widths[i], heights[i], angles[i]);
        if (!client.sendInit(init, error)) return 5;
        BYHWICD::InitAckC2pObjectTrackingCmd ack = {};
        if (!client.waitForInitAck(o.timeoutMs, ack) || ack.sensorID != ids[i]) return 6;
    }
    const BYHWICD::InitP2cObjectTrackingCmd broadcast = MakeInit(
        o.platID, 255, 800, 800, 44.4);
    if (!client.sendInit(broadcast, error)) return 7;
    std::vector<BYHWICD::InitAckC2pObjectTrackingCmd> acks;
    if (!client.waitForInitAcks(o.timeoutMs,
            static_cast<std::size_t>(o.expectedBroadcastAcks), acks)) return 8;
    std::set<int> ackSensors;
    for (std::size_t i = 0; i < acks.size(); ++i) ackSensors.insert(acks[i].sensorID);
    if (ackSensors.size() != static_cast<std::size_t>(o.expectedBroadcastAcks) ||
        ackSensors.count(255) != 0) return 9;
    for (int i = 0; i < 3; ++i)
    {
        BYHWICD::DisplayC2cObjTrackingData realtime = {};
        realtime.flag = 0x38; realtime.platID = o.platID; realtime.sensorID = ids[i];
        realtime.time = 100.0 + i;
        if (!client.sendRealtime(realtime, error)) return 10;
    }
    BYHWICD::DisplayC2cObjTrackingData realtimeBroadcast = {};
    realtimeBroadcast.flag = 0x38; realtimeBroadcast.platID = o.platID;
    realtimeBroadcast.sensorID = 255; realtimeBroadcast.time = 200.0;
    if (!client.sendRealtime(realtimeBroadcast, error)) return 11;
    BYHWICD::ControlP2cX1ObjTrackingCmd wrongControl = control;
    wrongControl.platID = o.platID + 1;
    if (!client.sendControl(wrongControl, error)) return 12;
    const BYHWICD::InitP2cObjectTrackingCmd wrongInit = MakeInit(
        o.platID + 1, 1, 320, 240, 99.9);
    if (!client.sendInit(wrongInit, error)) return 13;
    BYHWICD::DisplayC2cObjTrackingData wrongRealtime = realtimeBroadcast;
    wrongRealtime.platID = o.platID + 1;
    wrongRealtime.sensorID = 1;
    if (!client.sendRealtime(wrongRealtime, error)) return 14;
    client.waitForAcknowledgments(o.timeoutMs, error);
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    std::cout << "[FanoutSenderSummary] controlWriters=1 initWriters=1 realtimeWriters=1"
              << " exactInit=3 broadcastInit=1 broadcastAck=" << acks.size()
              << " ackSensors=";
    for (std::set<int>::const_iterator it = ackSensors.begin(); it != ackSensors.end(); ++it)
    {
        if (it != ackSensors.begin()) std::cout << ',';
        std::cout << *it;
    }
    std::cout << " exactRealtime=3 broadcastRealtime=1 platMismatchSamples=3 errors=0" << std::endl;
    client.shutdown();
    return 0;
}

class ReceiverState
{
public:
    explicit ReceiverState(const Options& options) : o(options) {}
    void setEndpoint(HwaSimIRProtocolEndpoint* value) { endpoint = value; }

    void route(const char* type, int packetPlatID, int packetSensorID,
        bool hasSensorID, const BYHWICD::InitP2cObjectTrackingCmd* init)
    {
        const ProtocolRouteResult result = EvaluateProtocolRoute(o.platID, o.sensorID,
            o.acceptBroadcast, packetPlatID, packetSensorID, hasSensorID);
        const bool accepted = ProtocolRouteAccepted(result);
        {
            std::lock_guard<std::mutex> lock(mutex);
            ++ddsCallbackSamples;
            if (accepted) ++routeAccepted; else ++routeRejected;
            if (accepted && init)
            {
                const BYHWICD::trackerSensorParam& sensor = init->trackingInit.trackerSensor[0];
                lastWidth = sensor.trackerSensorWidth;
                lastHeight = sensor.trackerSensorHeight;
                lastPixelAngle = sensor.trackerSensorPixelAngle;
                if (init->sensorID == o.sensorID) ++exactInitAccepted;
                if (init->sensorID == 255) ++broadcastInitAccepted;
            }
            changed.notify_all();
        }
        std::cout << "[ProtocolRoute] transport=dds type=" << type
                  << " accepted=" << (accepted ? 1 : 0)
                  << " localPlatID=" << o.platID << " localSensorID=" << o.sensorID
                  << " packetPlatID=" << packetPlatID << " packetSensorID=";
        if (hasSensorID) std::cout << packetSensorID; else std::cout << "na";
        std::cout << " reason=" << ProtocolRouteReason(result) << std::endl;
        if (accepted && init && endpoint)
        {
            BYHWICD::InitAckC2pObjectTrackingCmd ack = {};
            ack.flag = 0x37; ack.platID = o.platID; ack.sensorID = o.sensorID;
            ack.trackingReady = true;
            std::string error;
            if (!endpoint->publishInitAck(ack, error))
            {
                std::lock_guard<std::mutex> lock(mutex); ++errors;
            }
        }
    }

    int waitAndSummary()
    {
        std::unique_lock<std::mutex> lock(mutex);
        const bool complete = changed.wait_for(lock, std::chrono::milliseconds(o.timeoutMs),
            [this] { return ddsCallbackSamples >= o.expectedCallbacks; });
        const int expectedAccepted = o.acceptBroadcast ? 5 : 3;
        std::cout << "[FanoutReaderSummary] localPlatID=" << o.platID
                  << " localSensorID=" << o.sensorID
                  << " acceptBroadcast=" << (o.acceptBroadcast ? 1 : 0)
                  << " ddsCallbackSamples=" << ddsCallbackSamples
                  << " routeAccepted=" << routeAccepted
                  << " routeRejected=" << routeRejected
                  << " exactInitAccepted=" << exactInitAccepted
                  << " broadcastInitAccepted=" << broadcastInitAccepted
                  << " finalWidth=" << lastWidth << " finalHeight=" << lastHeight
                  << " finalPixelAngle=" << lastPixelAngle
                  << " errors=" << errors << std::endl;
        return complete && routeAccepted == expectedAccepted && errors == 0 ? 0 : 12;
    }

private:
    const Options& o;
    HwaSimIRProtocolEndpoint* endpoint = nullptr;
    std::mutex mutex;
    std::condition_variable changed;
    int ddsCallbackSamples = 0, routeAccepted = 0, routeRejected = 0;
    int exactInitAccepted = 0, broadcastInitAccepted = 0, errors = 0;
    int lastWidth = 0, lastHeight = 0;
    double lastPixelAngle = 0.0;
};

static int RunReader(const Options& o)
{
    std::shared_ptr<DdsRuntimeManager> runtime(new DdsRuntimeManager());
    DdsRuntimeConfig runtimeConfig; runtimeConfig.domainId = o.domain; runtimeConfig.qosFile = o.qos;
    std::string error;
    if (!runtime->start(runtimeConfig, error)) { std::cerr << error << std::endl; return 3; }
    ReceiverState state(o);
    HwaSimIRProtocolEndpoint endpoint;
    state.setEndpoint(&endpoint);
    DdsProtocolCallbacks callbacks;
    callbacks.control = [&state](const BYHWICD::ControlP2cX1ObjTrackingCmd& v) {
        state.route("control", v.platID, -1, false, nullptr);
    };
    callbacks.init = [&state](const BYHWICD::InitP2cObjectTrackingCmd& v) {
        state.route("init", v.platID, v.sensorID, true, &v);
    };
    callbacks.realtime = [&state](const BYHWICD::DisplayC2cObjTrackingData& v) {
        state.route("realtime", v.platID, v.sensorID, true, nullptr);
    };
    DdsProtocolConfig config; config.enabled = true; config.domainId = o.domain;
    config.topicVideoMeta = "HwaSimIR.ProbeMeta." + std::to_string(o.sensorID);
    config.topicAnnotation = "HwaSimIR.ProbeAnnotation." + std::to_string(o.sensorID);
    if (!endpoint.start(runtime, config, callbacks, error)) { std::cerr << error << std::endl; return 4; }
    std::cout << "[FanoutReaderReady] localPlatID=" << o.platID
              << " localSensorID=" << o.sensorID
              << " acceptBroadcast=" << (o.acceptBroadcast ? 1 : 0) << std::endl;
    const int result = state.waitAndSummary();
    endpoint.shutdown(); runtime->shutdown();
    return result;
}

int main(int argc, char** argv)
{
    Options o;
    if (!Parse(argc, argv, o)) return 2;
    return o.mode == "sender" ? RunSender(o) : RunReader(o);
}
