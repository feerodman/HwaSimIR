#pragma once

#include "HwaSimIRProtocolV1.h"

#include <memory>
#include <string>

class HwaSimIRSimpleDdsClient
{
public:
    HwaSimIRSimpleDdsClient();
    ~HwaSimIRSimpleDdsClient();

    bool init(int domainId, const std::string& qosFile);
    bool sendControl(const HwaSimIRDds::ControlCommandV1& sample);
    bool sendInit(const HwaSimIRDds::InitCommandV1& sample);
    bool sendRealtime(const HwaSimIRDds::RealtimeDataV1& sample);
    bool waitForInitAck(int expectedPlatID, int expectedSensorID, int timeoutMs,
        HwaSimIRDds::InitAckV1& ack);
    void shutdown();

private:
    HwaSimIRSimpleDdsClient(const HwaSimIRSimpleDdsClient&);
    HwaSimIRSimpleDdsClient& operator=(const HwaSimIRSimpleDdsClient&);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};
