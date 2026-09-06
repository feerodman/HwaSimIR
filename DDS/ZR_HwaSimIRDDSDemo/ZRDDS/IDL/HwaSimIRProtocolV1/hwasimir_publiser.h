#pragma once

#include <QObject>
#include "DomainParticipantFactory.h"
#include "DomainParticipant.h"
#include "HwaSimIRProtocolV1DataWriter.h"

class HwaSimIR_publiser : public QObject
{
public:
    explicit HwaSimIR_publiser(DDS::DomainParticipantFactory* factory,
        uint domainId, QObject* parent = nullptr);
    ~HwaSimIR_publiser() override;

    bool isReady() const;
    // 仅供同一 HwaSimIR 模块的 InitAck Reader 使用，不与 ZR 模块共享。
    DDS::DomainParticipant* participant() const { return m_participant; }
    // 借用调用方样本；Initialize/Finalize 均由调用方负责。
    bool pubControl(const HwaSimIRDds::ControlCommandV1& data);
    bool pubInit(const HwaSimIRDds::InitCommandV1& data);
    bool pubRealtime(const HwaSimIRDds::RealtimeDataV1& data);

private:
    DDS::DomainParticipantFactory* m_factory;
    DDS::DomainParticipant* m_participant = nullptr;
    HwaSimIRDds::ControlCommandV1DataWriter* m_controlWriter = nullptr;
    HwaSimIRDds::InitCommandV1DataWriter* m_initWriter = nullptr;
    HwaSimIRDds::RealtimeDataV1DataWriter* m_realtimeWriter = nullptr;
};
