#pragma once
#include <atomic>
#include <cstdint>
#include <algorithm>
// Control executor arms the STOP drain barrier. Rendering owns completion.
// Fields are immutable once draining is published with release semantics.
struct HwaStopDrainOperation {
    std::atomic<bool> draining{false};
    std::int64_t receiveNs=0,executeNs=0,quietRequiredNs=0;
    int round=0,fps=60;
    void beginDrain(std::int64_t now){
        executeNs=now;
        quietRequiredNs=static_cast<std::int64_t>(std::max(50.0,2000.0/std::max(1,fps))*1.e6);
        draining.store(true,std::memory_order_release);
    }
};
