#pragma once

#include <atomic>
#include <mutex>
#include <string>

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
// that process borrow the same participant; only this class calls Init,
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
#endif

private:
    mutable std::mutex m_mutex;
    DdsRuntimeConfig m_config;
    std::atomic<bool> m_running{ false };
    std::atomic<int> m_initCount{ 0 };
#if defined(HWASIMIR_HAS_ZRDDS)
    DDS::DomainParticipantFactory* m_factory = nullptr;
    DDS::DomainParticipant* m_participant = nullptr;
#endif
};
