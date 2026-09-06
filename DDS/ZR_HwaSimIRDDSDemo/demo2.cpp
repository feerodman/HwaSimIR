#include "demo2.h"
#include "DdsRuntime.h"
#include <functional>
#include <QDebug>

bool demo2::initDds()
{
    // 程序初始化位置调用：此时 demo 已先创建了原 ZR Participant。
    hwaSimIR_reader.reset();
    hwaSimIR_writer.reset(new HwaSimIR_publiser(DdsRuntime::factory(), HWA_SIMIR_DOMAIN_ID));
    HwaSimIR_RX::ProcessDataCallBack callback = std::bind(&demo2::recvInitAck,
        this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    hwaSimIR_reader.reset(new HwaSimIR_subscriber(DdsRuntime::factory(),
        hwaSimIR_writer->participant(), callback, "HwaSimIR.InitAck"));
    return hwaSimIR_writer->isReady() && hwaSimIR_reader->isReady();
}

void demo2::sendReset()
{
    // 需要 Reset 的业务位置调用；原协议 simCommand=1 表示复位。
    HwaSimIRDds::ControlCommandV1 command = {};
    HwaSimIRDds::ControlCommandV1Initialize(&command);
    command.flag = 0x41;
    command.JB = 1;
    command.platID = expectedPlatID;
    command.simCommand = 1;
    command.roundCut = 1;
    command.currentRound = 1;
    qInfo() << "Reset sent:" << hwaSimIR_writer->pubControl(command);
    HwaSimIRDds::ControlCommandV1Finalize(&command);
}

void demo2::sendInit()
{
    // 初始化参数准备完成后调用；以下为样例值，请在此填入实际业务参数。
    HwaSimIRDds::InitCommandV1 command = {};
    HwaSimIRDds::InitCommandV1Initialize(&command);
    command.flag = 0x36;
    command.JB = 1;
    command.platID = expectedPlatID;
    command.sensorID = expectedSensorID;
    command.platParamInit.id = expectedPlatID;
    command.platParamInit.type = 1;
    command.trackingInit.enable = true;
    command.trackingInit.videoFps = 30;
    command.trackingInit.trackerSensor[0].trackerSensorWidth = 640;
    command.trackingInit.trackerSensor[0].trackerSensorHeight = 512;
    command.trackingInit.trackerSensor[0].trackerSensorBand = 2;
    qInfo() << "Init sent:" << hwaSimIR_writer->pubInit(command);
    HwaSimIRDds::InitCommandV1Finalize(&command);
}

void demo2::recvInitAck(const HwaSimIRDds::InitAckV1* ack, uint domainId,
    const QString& topicName)
{
    // DDS 收包线程回调；这里只判断并记录，实际 GUI 操作需投递到 GUI 线程。
    // 原协议典型判断：platID、sensorID 匹配，且 trackingReady 为 true。
    const bool ready = ack->platID == expectedPlatID &&
        ack->sensorID == expectedSensorID && ack->trackingReady == true;
    qInfo() << "InitAck domain=" << domainId << "topic=" << topicName
            << "platID=" << ack->platID << "sensorID=" << ack->sensorID
            << "trackingReady=" << ack->trackingReady << "matchedReady=" << ready;
    // 客户在此通知业务层；由业务层决定是否调用 sendStart()，DDS 层不阻塞等待。
    // 同一 platID/sensorID 的多回合还需由业务层管理时序，不能只凭旧 Ack 再次 Start。
}

void demo2::sendStart()
{
    // 业务确认 InitAck 后，在仿真开始位置调用；simCommand=2 表示开始。
    HwaSimIRDds::ControlCommandV1 command = {};
    HwaSimIRDds::ControlCommandV1Initialize(&command);
    command.flag = 0x41;
    command.JB = 1;
    command.platID = expectedPlatID;
    command.simCommand = 2;
    command.roundCut = 1;
    command.currentRound = 1;
    qInfo() << "Start sent:" << hwaSimIR_writer->pubControl(command);
    HwaSimIRDds::ControlCommandV1Finalize(&command);
}

void demo2::sendRealtime(const HwaSimIRDds::RealtimeDataV1& realtimeData)
{
    // 每帧/每周期调用；调用方 Initialize，填写 flag=0x38、平台/传感器和实时状态，
    // 调用本函数后由调用方 Finalize。这里借用样本，不销毁调用方对象。
    hwaSimIR_writer->pubRealtime(realtimeData);
}

void demo2::sendStop()
{
    // 停止位置调用；原协议 simCommand=3 表示停止。
    HwaSimIRDds::ControlCommandV1 command = {};
    HwaSimIRDds::ControlCommandV1Initialize(&command);
    command.flag = 0x41;
    command.JB = 1;
    command.platID = expectedPlatID;
    command.simCommand = 3;
    command.roundCut = 1;
    command.currentRound = 1;
    qInfo() << "Stop sent:" << hwaSimIR_writer->pubControl(command);
    HwaSimIRDds::ControlCommandV1Finalize(&command);
}
