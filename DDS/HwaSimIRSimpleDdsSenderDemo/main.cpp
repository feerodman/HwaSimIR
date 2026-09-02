#include "HwaSimIRSimpleDdsClient.h"

#include <string>
#include <vector>

int RunOneRound(
    int domainId,
    const std::string& qosFile,
    const HwaSimIRDds::ControlCommandV1& resetCommand,
    const HwaSimIRDds::InitCommandV1& initCommand,
    const HwaSimIRDds::ControlCommandV1& startCommand,
    const std::vector<HwaSimIRDds::RealtimeDataV1>& realtimeSamples,
    const HwaSimIRDds::ControlCommandV1& stopCommand,
    int ackTimeoutMs)
{
    HwaSimIRSimpleDdsClient client;

    // 初始化 DDS，创建 Participant、Topic、Writer 和 Ack Reader
    if (!client.init(domainId, qosFile))
        return 1;

    // 复位
    if (!client.sendControl(resetCommand))
        return 2;

    // 发送一个初始化参数
    if (!client.sendInit(initCommand))
        return 3;

    // 等待与本次 platID/sensorID 匹配的初始化应答
    HwaSimIRDds::InitAckV1 ack = {};
    HwaSimIRDds::InitAckV1Initialize(&ack);
    const bool ackReady = client.waitForInitAck(
        initCommand.platID,
        initCommand.sensorID,
        ackTimeoutMs,
        ack);
    HwaSimIRDds::InitAckV1Finalize(&ack);
    if (!ackReady)
        return 4;

    // 初始化成功后开始本回合
    if (!client.sendControl(startCommand))
        return 5;

    // 按业务频率发送实时数据。
    for (std::vector<HwaSimIRDds::RealtimeDataV1>::const_iterator it =
             realtimeSamples.begin();
         it != realtimeSamples.end();
         ++it)
    {
        if (!client.sendRealtime(*it))
            return 6;
    }

    // 结束后停止本回合
    if (!client.sendControl(stopCommand))
        return 7;

    // 释放 DDS
    client.shutdown();
    return 0;
}

int main()
{
    return 0;
}
