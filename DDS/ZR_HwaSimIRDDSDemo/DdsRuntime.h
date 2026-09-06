#pragma once

#include "DomainParticipantFactory.h"

// 进程只共用这个工厂；各业务模块保留自己的 Domain、Participant 和 QoS。
class DdsRuntime
{
public:
    static bool init(const char* qosFile);
    static DDS::DomainParticipantFactory* factory();
    static bool shutdown();

private:
    static DDS::DomainParticipantFactory* m_factory;
};
