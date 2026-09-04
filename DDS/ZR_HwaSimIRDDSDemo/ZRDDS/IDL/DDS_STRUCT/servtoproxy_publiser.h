#ifndef SERVTOPROXY_PUBLISER_H
#define SERVTOPROXY_PUBLISER_H

#include <QObject>
#include "DomainParticipantFactory.h"
#include "DomainParticipant.h"
#include "DefaultQos.h"
#include "Publisher.h"
#include "DataWriter.h"
#include "DDS_Struct.h"
//#include "DDS_StructDataReader.h"
#include "DDS_StructDataWriter.h"
#include "DDS_StructTypeSupport.h"
#include "ZRSleep.h"
#include <stdio.h>
#include <string.h>
#include <QDebug>

class ServToProxy_publiser : public QObject
{
    Q_OBJECT
public:
    explicit ServToProxy_publiser(uint dId, const QString &topicName = "TOPIC_ABREQ_CMD", QObject *parent = nullptr);
    virtual ~ServToProxy_publiser();

    void pubData(ServToProxy data);
signals:

private:
    uint m_did;
    QString m_topic;
    DDS::DomainParticipant *participant = nullptr;
    ServToProxyDataWriter *m_writer = nullptr;
};


#endif // SERVTOPROXY_PUBLISER_H
