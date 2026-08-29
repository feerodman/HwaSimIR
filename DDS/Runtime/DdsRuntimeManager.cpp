#include "DdsRuntimeManager.h"

#include <iostream>
#include <sstream>

DdsRuntimeManager::DdsRuntimeManager() = default;

DdsRuntimeManager::~DdsRuntimeManager()
{
    shutdown();
}

bool DdsRuntimeManager::start(const DdsRuntimeConfig& config, std::string& error)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running.load())
    {
        if (m_config.domainId != config.domainId || m_config.qosFile != config.qosFile ||
            m_config.factoryProfile != config.factoryProfile ||
            m_config.participantProfile != config.participantProfile)
        {
            error = "DDS Runtime already started with a different configuration";
            return false;
        }
        return true;
    }
#if !defined(HWASIMIR_HAS_ZRDDS)
    (void)config;
    error = "DDS requested but binary lacks HWASIMIR_HAS_ZRDDS";
    return false;
#else
    m_factory = DDS::DDSIF::Init(config.qosFile.c_str(), config.factoryProfile.c_str());
    if (!m_factory)
    {
        error = "DDSIF::Init failed qos=" + config.qosFile;
        return false;
    }
    ++m_initCount;
    m_participant = DDS::DDSIF::CreateDP(config.domainId, config.participantProfile.c_str());
    if (!m_participant)
    {
        error = "DDSIF::CreateDP failed domain=" + std::to_string(config.domainId);
        DDS::DDSIF::Finalize();
        m_factory = nullptr;
        return false;
    }
    m_config = config;
    m_running.store(true);
    std::cout << "[DdsRuntime] initialized=1 initCount=" << m_initCount.load()
              << " domain=" << config.domainId
              << " qos=" << config.qosFile
              << " participant=" << config.participantProfile << std::endl;
    return true;
#endif
}

void DdsRuntimeManager::shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running.load()) return;
#if defined(HWASIMIR_HAS_ZRDDS)
    const DDS::ReturnCode_t result = DDS::DDSIF::Finalize();
    std::cout << "[DdsRuntime] shutdown=1 finalizeCount=1 result="
              << static_cast<int>(result) << std::endl;
    m_factory = nullptr;
    m_participant = nullptr;
#endif
    m_running.store(false);
}

bool DdsRuntimeManager::running() const
{
    return m_running.load();
}

int DdsRuntimeManager::initCount() const
{
    return m_initCount.load();
}

DdsRuntimeConfig DdsRuntimeManager::config() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

#if defined(HWASIMIR_HAS_ZRDDS)
DDS::DomainParticipantFactory* DdsRuntimeManager::factory() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_factory;
}

DDS::DomainParticipant* DdsRuntimeManager::participant() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_participant;
}
#endif
