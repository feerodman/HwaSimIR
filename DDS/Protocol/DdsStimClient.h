#pragma once

#include "CommonData.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>

class DdsRuntimeManager;

struct DdsStimConfig
{
    int domainId = 150;
    std::string qosFile = "Config/DDS/ZRDDS_PROTOCOL_QOS.xml";
    std::string topicControl = "HwaSimIR.Control";
    std::string topicInit = "HwaSimIR.Init";
    std::string topicRealtime = "HwaSimIR.Realtime";
    std::string topicInitAck = "HwaSimIR.InitAck";
    std::string writerProfile = "hwasimir_protocol_writer";
    std::string readerProfile = "hwasimir_protocol_reader";
};

// Shared by DataDrivenTestQT and the customer command-line stimulator. The
// caller still constructs the legacy BYHWICD structs, then this class performs
// explicit field conversion and typed DDS publication.
class DdsStimClient
{
public:
    DdsStimClient();
    ~DdsStimClient();

    bool start(const DdsStimConfig& config, std::string& error);
    void shutdown();
    bool sendControl(const BYHWICD::ControlP2cX1ObjTrackingCmd& value, std::string& error);
    bool sendInit(const BYHWICD::InitP2cObjectTrackingCmd& value, std::string& error);
    bool sendRealtime(const BYHWICD::DisplayC2cObjTrackingData& value, std::string& error);
    bool waitForInitAck(int timeoutMs, BYHWICD::InitAckC2pObjectTrackingCmd& value);
    bool waitForAcknowledgments(int timeoutMs, std::string& error);
    void setAckCallback(const std::function<void(const BYHWICD::InitAckC2pObjectTrackingCmd&)>& callback);
    unsigned long long ackCount() const;
    int runtimeInitCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
