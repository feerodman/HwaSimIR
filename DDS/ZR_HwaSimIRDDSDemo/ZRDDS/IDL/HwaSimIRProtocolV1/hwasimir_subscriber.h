#pragma once

#include <QObject>
#include <QString>
#include <functional>
#include "DomainParticipantFactory.h"
#include "DomainParticipant.h"
#include "HwaSimIRProtocolV1DataReader.h"

namespace HwaSimIR_RX {
    // data 为 DDS 借出的样本，只在本次回调内有效；业务若需保存应先复制。
    typedef std::function<void(const HwaSimIRDds::InitAckV1* data,
        uint domainId, const QString& topicName)> ProcessDataCallBack;
}

class MyListenerInitAck;

class HwaSimIR_subscriber : public QObject
{
public:
    // 借用 HwaSimIR 发布类的 Participant，保持三个 Writer 与一个 Reader 同域同 QoS。
    // 发布对象必须比此订阅对象活得更久；不借用原 ZR 模块的 Participant。
    explicit HwaSimIR_subscriber(DDS::DomainParticipantFactory* factory,
        DDS::DomainParticipant* participant, HwaSimIR_RX::ProcessDataCallBack callback,
        const QString& topicName = "HwaSimIR.InitAck", QObject* parent = nullptr);
    ~HwaSimIR_subscriber() override;
    bool isReady() const { return m_reader != nullptr; }

private:
    DDS::DomainParticipant* m_participant;
    DDS::Subscriber* m_subscriber = nullptr;
    DDS::Topic* m_topic = nullptr;
    HwaSimIRDds::InitAckV1DataReader* m_reader = nullptr;
    MyListenerInitAck* m_listener = nullptr;
};
