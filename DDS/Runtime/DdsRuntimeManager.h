#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <map>

#if defined(HWASIMIR_HAS_ZRDDS)
#include "ZRDDSCppSimpleInterface.h"
#endif

struct DdsRuntimeConfig
{
    int domainId = 150;
    std::string qosFile = "Config/DDS/ZRDDS_PROTOCOL_QOS.xml";
    std::string factoryProfile = "hwasimir_factory";
    std::string participantProfile = "hwasimir_tcp";
};

// One instance is owned by each process. All protocol and video endpoints in
// that process reuse one participant per domain; only this class calls Init,
// CreateDP and Finalize.
class DdsRuntimeManager
{
public:
    DdsRuntimeManager();
    ~DdsRuntimeManager();

    bool start(const DdsRuntimeConfig& config, std::string& error);
    void shutdown();
    bool running() const;
    int initCount() const;
    DdsRuntimeConfig config() const;

#if defined(HWASIMIR_HAS_ZRDDS)
    DDS::DomainParticipantFactory* factory() const;
    DDS::DomainParticipant* participant() const;
    // Borrowed until shutdown. Creating another domain never calls global Init.
    DDS::DomainParticipant* participantForDomain(int domainId, std::string& error);
#endif

private:
    mutable std::mutex m_mutex;
    DdsRuntimeConfig m_config;
    std::atomic<bool> m_running{ false };
    std::atomic<int> m_initCount{ 0 };
#if defined(HWASIMIR_HAS_ZRDDS)
    DDS::DomainParticipantFactory* m_factory = nullptr;
    DDS::DomainParticipant* m_participant = nullptr;
    std::map<int, DDS::DomainParticipant*> m_participants;
#endif
};
