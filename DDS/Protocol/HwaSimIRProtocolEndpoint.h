#pragma once

#include "CommonData.h"

#include <functional>
#include <memory>
#include <string>

class DdsRuntimeManager;

struct DdsProtocolConfig
{
    bool enabled = false;
    int domainId = 150;
    std::string topicControl = "HwaSimIR.Control";
    std::string topicInit = "HwaSimIR.Init";
    std::string topicRealtime = "HwaSimIR.Realtime";
    std::string topicInitAck = "HwaSimIR.InitAck";
    std::string topicVideoStatus = "HwaSimIR.VideoStatus";
    std::string readerProfile = "hwasimir_protocol_reader";
    std::string writerProfile = "hwasimir_protocol_writer";
    std::string statusWriterProfile = "hwasimir_status_writer";
};

struct DdsVideoStatus
{
    int platID = 0;
    int sensorID = 0;
    std::string channel;
    bool running = false;
    std::string codec;
    std::string pixelFormat;
    std::string videoTopic;
    int width = 0;
    int height = 0;
    int fps = 0;
    int bitrateKbps = 0;
    int gopFrames = 0;
    bool compressed = false;
    int currentRound = 0;
};

struct DdsProtocolCallbacks
{
    std::function<void(const BYHWICD::ControlP2cX1ObjTrackingCmd&)> control;
    std::function<void(const BYHWICD::InitP2cObjectTrackingCmd&)> init;
    std::function<void(const BYHWICD::DisplayC2cObjTrackingData&)> realtime;
};

// HwaSim_IR server-side typed protocol endpoint. ZRDDS callbacks only copy,
// adapt and enqueue through these callbacks; they never touch Panda3D.
class HwaSimIRProtocolEndpoint
{
public:
    HwaSimIRProtocolEndpoint();
    ~HwaSimIRProtocolEndpoint();

    bool start(const std::shared_ptr<DdsRuntimeManager>& runtime,
               const DdsProtocolConfig& config,
               const DdsProtocolCallbacks& callbacks,
               std::string& error);
    bool publishInitAck(const BYHWICD::InitAckC2pObjectTrackingCmd& ack, std::string& error);
    bool publishVideoStatus(const DdsVideoStatus& status, std::string& error);
    void shutdown();
    bool running() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
