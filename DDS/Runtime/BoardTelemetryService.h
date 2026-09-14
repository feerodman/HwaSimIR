#pragma once
#include "RuntimeTelemetryV2.h"
#include "DdsRuntimeManager.h"
#include <atomic>
#include <thread>
#include <iostream>
#if defined(HWASIMIR_HAS_ZRDDS)
#include "ZRDDSCppSimpleInterface.h"
#endif

// An independent versioned diagnostic channel. The render/input threads only
// append bounded timestamp counters; DDS writes happen on this service thread.
class BoardTelemetryService {
public:
    HwaTelemetryV2::Counters counters;
    const std::string session=HwaFrameV2::processSession();
#if defined(HWASIMIR_HAS_ZRDDS)
    struct Listener : DDS::SimpleDataReaderListener<DDS::Bytes,DDS::BytesSeq,DDS::BytesDataReader> {
        BoardTelemetryService* owner;
        explicit Listener(BoardTelemetryService* p):owner(p){}
        void on_process_sample(DDS::DataReader*,const DDS::Bytes& data,const DDS::SampleInfo&) override {
            const auto t2=HwaTelemetryV2::nowNs();HwaTelemetryV2::Message m;
            if(!HwaTelemetryV2::decode(reinterpret_cast<const char*>(data.value.get_contiguous_buffer()),data.value.length(),m)||m.kind!=1)return;
            m.t2=t2;m.kind=2;m.server=owner->session;
            std::lock_guard<std::mutex> lock(owner->mutex);
            if(owner->requests.size()<32)owner->requests.push_back(std::move(m));
        }
    } listener{this};
    DDS::DataReader* reader=nullptr;DDS::DataWriter* writer=nullptr;
    std::atomic<bool> running{false};std::thread thread;std::mutex mutex,sendMutex;
    std::deque<HwaTelemetryV2::Message> requests;std::string responseTopic;int domain=150;
    bool start(const std::shared_ptr<DdsRuntimeManager>& runtime,int domainId,int platID,int sensorID) {
        if(running.load())return true;domain=domainId;
        const auto base="HwaSimIR.Diagnostics.V2."+std::to_string(platID)+"."+std::to_string(sensorID);
        responseTopic=base+".Board";
        writer=DDS::DDSIF::PubTopic(runtime->participant(),responseTopic.c_str(),DDS::BytesTypeSupport::get_instance(),"hwasimir_protocol_writer",nullptr);
        reader=DDS::DDSIF::SubTopic(runtime->participant(),(base+".ClockRequest").c_str(),DDS::BytesTypeSupport::get_instance(),"hwasimir_protocol_reader",&listener);
        if(!writer||!reader)return false;
        running=true;thread=std::thread([this]{
            auto last=HwaTelemetryV2::nowNs();
            while(running.load()){
                std::deque<HwaTelemetryV2::Message> reply;
                {std::lock_guard<std::mutex> lock(mutex);reply.swap(requests);}
                for(auto& m:reply){m.t3=HwaTelemetryV2::nowNs();send(m);}
                const auto now=HwaTelemetryV2::nowNs();
                if(now-last>=250000000LL){auto m=counters.snapshot();m.server=session;send(m);last=now;}
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        });return true;
    }
    void send(const HwaTelemetryV2::Message& m){
        std::lock_guard<std::mutex> lock(sendMutex);auto bytes=HwaTelemetryV2::encode(m);
        const auto rc=DDS::DDSIF::BytesWrite(domain,const_cast<char*>(responseTopic.c_str()),reinterpret_cast<const char*>(bytes.data()),static_cast<DDS::Long>(bytes.size()));
        if(rc!=DDS::RETCODE_OK)std::cerr<<"[RuntimeTelemetryV2][ERROR] write code="<<rc<<" kind="<<m.kind<<std::endl;
    }
    void complete(){counters.finish();auto m=counters.snapshot();m.server=session;if(writer){send(m);DDS::Duration_t timeout;timeout.sec=2;timeout.nanosec=0;const auto rc=writer->wait_for_acknowledgments(timeout);if(rc!=DDS::RETCODE_OK)std::cerr<<"[RuntimeTelemetryV2][ERROR] end acknowledgement code="<<rc<<std::endl;}}
    void stop(){running=false;if(thread.joinable())thread.join();if(reader){DDS::DDSIF::UnSubTopic(reader);reader=nullptr;}if(writer){DDS::DDSIF::UnPubTopic(writer);writer=nullptr;}}
#else
    bool start(const std::shared_ptr<DdsRuntimeManager>&,int,int,int){return false;}
    void complete(){}
    void stop(){}
#endif
    ~BoardTelemetryService(){stop();}
};
