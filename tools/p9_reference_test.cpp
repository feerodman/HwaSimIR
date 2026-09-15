#include "../DDS/Runtime/RuntimeTelemetryV2.h"
#include "../DDS/Runtime/StopDrainOperation.h"
#include "../HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/VideoPixelFit.h"
#include <cassert>
#include <iostream>
int main(){
    HwaStopDrainOperation stop;stop.receiveNs=1000000000;stop.fps=60;
    assert(!stop.draining.load());stop.beginDrain(1005000000);
    assert(stop.draining.load()&&stop.executeNs-stop.receiveNs==5000000&&stop.quietRequiredNs==50000000);
    struct Example {int w,h,aw,ah,ew,eh;};
    const Example examples[]={{640,480,1800,1400,640,480},{800,800,1800,1400,800,800},
        {1024,1024,1800,1400,1024,1024},{1280,1024,1800,1400,1024,819},
        {800,800,600,900,600,600},{640,480,400,290,386,290},{800,800,800,800,800,800}};
    for(const auto& e:examples){const auto v=HwaVideoFit::fit(e.w,e.h,e.aw,e.ah);assert(v.width==e.ew&&v.height==e.eh);
        std::cout<<"physical_fit "<<e.w<<"x"<<e.h<<" area="<<e.aw<<"x"<<e.ah<<" result="<<v.width<<"x"<<v.height<<" PASS\n";}
    const int delays[]={5,10,20};const double expected[]={200,100,50};
    HwaTelemetryV2::Counters c;
    for(int i=0;i<3;++i){c.control(i+1,1,1000000000LL,1000000000LL+delays[i]*1000000LL);
        auto m=c.controlSnapshot();assert(HwaTelemetryV2::controlResponseMs(m)==delays[i]);assert(HwaTelemetryV2::controlEquivalentHz(m)==expected[i]);
        const auto bytes=HwaTelemetryV2::encode(m);HwaTelemetryV2::Message decoded;
        assert(HwaTelemetryV2::decode(reinterpret_cast<const char*>(bytes.data()),bytes.size(),decoded));
        assert(decoded.version==3&&decoded.controlExecuteNs==m.controlExecuteNs&&decoded.controlCommand==i+1);
        c.reset();c.accept(2000000000);c.execute(2005000000,2000000000);c.finish();
        assert(c.controlSnapshot().controlSequence==m.controlSequence&&c.controlSnapshot().controlReceiveNs==m.controlReceiveNs);
        std::cout<<"control "<<delays[i]<<"ms = "<<expected[i]<<" equivalent_Hz; V3 roundtrip and RESET/Realtime persistence PASS\n";
    }
    c.control(1,1,1000000,1000000);assert(!HwaTelemetryV2::controlTimeValid(c.controlSnapshot()));
    c.control(2,1,1000000,1000050);assert(!HwaTelemetryV2::controlTimeValid(c.controlSnapshot()));
    HwaTelemetryV2::Message old;old.kind=3;auto v2=HwaTelemetryV2::encode(old);
    HwaTelemetryV2::Message decoded;assert(HwaTelemetryV2::decode(reinterpret_cast<const char*>(v2.data()),v2.size(),decoded)&&decoded.version==2&&decoded.controlSequence==0);
    auto newer=HwaTelemetryV2::encode(c.controlSnapshot());newer.pop_back();assert(!HwaTelemetryV2::decode(reinterpret_cast<const char*>(newer.data()),newer.size(),decoded));
    std::cout<<"zero/subresolution, V2 compatibility, truncated V3 rejection PASS\n";
}
