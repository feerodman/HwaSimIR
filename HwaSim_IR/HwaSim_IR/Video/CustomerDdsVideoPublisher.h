#pragma once
#include "VideoEncoder.h"
#include <memory>
#include <string>
class DdsRuntimeManager;
struct CustomerDdsVideoConfig {
    bool enabled=false;
    int domainId=-1; // -1 inherits the process primary domain for standalone callers.
    bool diagnosticColorCard=false;
    int diagnosticFailAfterFramesOnce=0; // Explicit application-failure test; never an SDK return code.
    std::string mode="auto",pixelFormat="gray8",auditPrefix;
    int queueMaxFrames=32,drainMs=5000,discoverySettleMs=1000;
};
// Both platform workers use this implementation. No HwaFrameV2 dependency.
class CustomerDdsVideoPublisher {
public:
    CustomerDdsVideoPublisher();
    ~CustomerDdsVideoPublisher();
    static bool validate(const CustomerDdsVideoConfig&,int width,int height,bool requestedH264,std::string& error);
    bool start(const CustomerDdsVideoConfig&,const std::shared_ptr<DdsRuntimeManager>&,bool precise,std::string& error);
    bool beginRound(int round,bool requestedH264,const VideoEncoderConfig&,const std::string& backend,std::string& error);
    // cleanAu is copied before returning, before the original branch injects SEI.
    bool submit(const RawVideoFrame&,std::uint64_t frameSeq,std::uint64_t sourceSeq,
        const std::vector<std::uint8_t>* cleanAu=nullptr);
    bool endRound(std::string& error);
    void shutdown();
    bool enabled() const;
    bool wantsSharedH264(bool requestedH264) const;
private:
    struct Impl;std::unique_ptr<Impl> m;
};
