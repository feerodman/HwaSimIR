#include "DdsRuntime.h"
#include "ZRDDSCppSimpleInterface.h"
#include <QDebug>

DDS::DomainParticipantFactory* DdsRuntime::m_factory = nullptr;

bool DdsRuntime::init(const char* qosFile)
{
    if (!m_factory)
    {
        // 全进程唯一的 Init：只加载 QoS/准备工厂，不创建业务 Participant。
        m_factory = DDS::DDSIF::Init(qosFile, "hwasimir_factory");
        qInfo() << "[DDS] Factory init:" << m_factory;
    }
    return m_factory != nullptr;
}

DDS::DomainParticipantFactory* DdsRuntime::factory()
{
    return m_factory;
}

bool DdsRuntime::shutdown()
{
    if (!m_factory)
        return true;
    // 必须等两套业务对象析构后调用；业务发布/订阅类不能 Finalize 工厂。
    const DDS::ReturnCode_t result = DDS::DDSIF::Finalize();
    m_factory = nullptr;
    qInfo() << "[DDS] Factory finalize result=" << result;
    return result == DDS::RETCODE_OK;
}
