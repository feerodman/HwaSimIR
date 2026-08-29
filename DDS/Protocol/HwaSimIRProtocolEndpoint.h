#pragma once

#include "CommonData.h"

#include <cstdint>
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
    std::string topicVideoMeta = "HwaSimIR.VideoMeta.precise";
    std::string topicAnnotation = "HwaSimIR.Annotation.precise";
    std::string readerProfile = "hwasimir_protocol_reader";
    std::string writerProfile = "hwasimir_protocol_writer";
    std::string statusWriterProfile = "hwasimir_status_writer";
};

struct DdsVideoFrameMeta
{
    int platID = 0;
    int sensorID = 0;
    std::string channel;
    std::uint32_t frameSeq = 0;
    int currentRound = 0;
    double ptsMs = 0.0;
    bool keyFrame = false;
    std::string codec;
    int width = 0;
    int height = 0;
};

struct DdsAnnotationFrame
{
    int platID = 0;
    int sensorID = 0;
    std::string channel;
    std::uint32_t frameSeq = 0;
    int currentRound = 0;
    double ptsMs = 0.0;
    std::string json;
};

struct DdsFrameAuxStats
{
    std::uint64_t metaCount = 0;
    std::uint64_t annotationCount = 0;
    std::uint64_t writeErrors = 0;
    double metaWriteMsTotal = 0.0;
    double annotationWriteMsTotal = 0.0;
    double metaWriteMsMax = 0.0;
    double annotationWriteMsMax = 0.0;
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
    bool publishVideoFrameMeta(const DdsVideoFrameMeta& meta, std::string& error);
    bool publishAnnotationFrame(const DdsAnnotationFrame& annotation, std::string& error);
    bool drainFrameOutputs(int ackTimeoutSec, int boundedDrainMs, std::string& error);
    void resetFrameStats();
    DdsFrameAuxStats frameStats() const;
    void shutdown();
    bool running() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
