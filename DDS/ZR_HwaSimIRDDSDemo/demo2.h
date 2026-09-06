#pragma once

#include <memory>
#include "hwasimir_publiser.h"
#include "hwasimir_subscriber.h"

// 当前 HwaSimIRRuntime.ini / DdsStimClient 默认值为 150；按实际对端 DomainID 修改。
static const int HWA_SIMIR_DOMAIN_ID = 150;

class demo2
{
public:
    bool initDds();
    void sendReset();
    void sendInit();
    void sendStart();
    void sendRealtime(const HwaSimIRDds::RealtimeDataV1& realtimeData);
    void sendStop();
    void recvInitAck(const HwaSimIRDds::InitAckV1* ack, uint domainId,
        const QString& topicName);

private:
    // 示例平台/传感器；修改时与实际 Init/Realtime 数据保持一致。
    const int expectedPlatID = 1;
    const int expectedSensorID = 1;
    // 声明顺序保证析构时先删除 Reader，再由 Writer 对象删除 HwaSimIR Participant。
    std::unique_ptr<HwaSimIR_publiser> hwaSimIR_writer;
    std::unique_ptr<HwaSimIR_subscriber> hwaSimIR_reader;
};
