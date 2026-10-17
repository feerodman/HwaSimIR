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
    if (config.domainId < 0 || config.domainId > 232) { error = "DDS DomainId must be 0..232"; return false; }
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
    m_participants[config.domainId] = m_participant;
    DDS::DomainParticipantQos actualQos;
    const DDS::ReturnCode_t qosResult = m_participant->get_qos(actualQos);
    if (qosResult == DDS::RETCODE_OK)
        std::cout << "[DdsReceiveThreads] kind=" << static_cast<int>(actualQos.receiver_thread_config.kind)
                  << " receiveBufferBytes=" << actualQos.receiver_thread_config.receive_buffer_length
                  << " source=participant_get_qos enum=0_per_port,1_per_kind,2_all_ports" << std::endl;
    else
        std::cout << "[DdsReceiveThreads] result=unavailable returnCode=" << static_cast<int>(qosResult) << std::endl;
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
    m_participants.clear();
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

DDS::DomainParticipant* DdsRuntimeManager::participantForDomain(int domainId, std::string& error)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running.load()) { error = "DDS Runtime is not ready"; return nullptr; }
    if (domainId < 0 || domainId > 232) { error = "DDS DomainId must be 0..232"; return nullptr; }
    const auto found = m_participants.find(domainId);
    if (found != m_participants.end()) return found->second;
    DDS::DomainParticipant* participant = DDS::DDSIF::CreateDP(domainId, m_config.participantProfile.c_str());
    if (!participant) { error = "DDSIF::CreateDP failed domain=" + std::to_string(domainId); return nullptr; }
    m_participants[domainId] = participant;
    std::cout << "[DdsRuntime] participantCreated=1 domain=" << domainId
              << " participantCount=" << m_participants.size() << " initCount=" << m_initCount.load()
              << " qos=" << m_config.qosFile << std::endl;
    return participant;
}
#endif
