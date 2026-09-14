#include "../../Runtime/RuntimeTelemetryV2.h"
#include <cassert>
#include <iostream>
int main(){
    HwaTelemetryV2::ClockEstimate clock;
    // Independent constructed timestamps: server is +5 s, each path takes
    // 2 ms, server residence 3 ms. Expected RTT 4 ms and offset exactly 5 s.
    for(int i=1;i<=4;++i){HwaTelemetryV2::Message r;r.kind=2;r.server="server";
        r.t1=1000000000LL*i;r.t2=r.t1+5002000000LL;r.t3=r.t2+3000000LL;
        clock.observe(r,r.t1+7000000LL);
    }
    assert(clock.valid(4010000000LL));assert(clock.offsetNs==5000000000.0);assert(clock.rttMs==4.0);
    assert(clock.uncertaintyMs>=2.1&&clock.uncertaintyMs<3.0);
    assert(!clock.valid(9000000000LL)); // stale is unavailable, never retained as current
    clock.reset();assert(!clock.valid(0));
    HwaTelemetryV2::Message message;message.kind=3;message.acceptedHz=60;message.executedHz=30;message.queueWaitMs=12.5;
    auto bytes=HwaTelemetryV2::encode(message);HwaTelemetryV2::Message decoded;
    assert(HwaTelemetryV2::decode(reinterpret_cast<const char*>(bytes.data()),bytes.size(),decoded));
    assert(decoded.acceptedHz==60&&decoded.executedHz==30&&decoded.queueWaitMs==12.5);
    bytes[0]=99;assert(!HwaTelemetryV2::decode(reinterpret_cast<const char*>(bytes.data()),bytes.size(),decoded));
    std::cout<<"TelemetryV2 PASS independent four timestamps, uncertainty, expiry, reset, schema\n";
}
