#ifndef SERVTOPROXY_SUBDCRIBER_H
#define SERVTOPROXY_SUBDCRIBER_H

#include <QObject>
#include "DomainParticipantFactory.h"
#include "DomainParticipant.h"
#include "DefaultQos.h"
#include "Subscriber.h"
#include "DataReader.h"
#include "Topic.h"
#include "DataReaderListener.h"
#include "DDS_Struct.h"
#include "DDS_StructDataReader.h"
#include "DDS_StructTypeSupport.h"
#include <stdio.h>
#include <string.h>
#include <functional>

namespace ServToProxy_RX {
    typedef std::function<void(const ServToProxy *data, uint did, const QString &topicName)> ProcessDataCallBack;
}

class MylistenerServToProxy;

class ServToProxy_subscriber : public QObject
{
    Q_OBJECT
public:
    explicit ServToProxy_subscriber(uint dId, ServToProxy_RX::ProcessDataCallBack callBack, const QString &topicName = "ServToProxy", QObject *parent = nullptr);
    virtual ~ServToProxy_subscriber();
private:
    ServToProxyDataReader *m_reader = nullptr;
    MylistenerServToProxy *m_listener = nullptr;
    DDS::DomainParticipant *participant = nullptr;
signals:
};

#endif // SERVTOPROXY_SUBDCRIBER_H
