#pragma once
#include "RuntimeTelemetryV2.h"
#include "DdsRuntimeManager.h"
#include "StopDrainOperation.h"
#include <condition_variable>
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
    std::condition_variable wake;
    std::deque<std::shared_ptr<HwaStopDrainOperation>> stops;
    std::shared_ptr<HwaStopDrainOperation> requestStop(std::int64_t receiveNs,int round,int fps){
        if(!running.load())return {};
        auto op=std::make_shared<HwaStopDrainOperation>();op->receiveNs=receiveNs;op->round=round;op->fps=fps;
        {std::lock_guard<std::mutex> lock(mutex);stops.push_back(op);}
        wake.notify_one();return op;
    }
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
            std::uint64_t lastControl=0;
            while(running.load()){
                std::deque<HwaTelemetryV2::Message> reply;
                std::deque<std::shared_ptr<HwaStopDrainOperation>> work;
                {std::lock_guard<std::mutex> lock(mutex);reply.swap(requests);work.swap(stops);}
                for(auto& op:work){
                    op->beginDrain(HwaTelemetryV2::nowNs());
                    counters.control(3,op->round,op->receiveNs,op->executeNs);
                    std::cout<<"[ControlStopPhase] phase=draining executor=control_service receiveNs="<<op->receiveNs
                        <<" beginNs="<<op->executeNs<<" quietRequiredNs="<<op->quietRequiredNs<<std::endl;
                }
                for(auto& m:reply){m.t3=HwaTelemetryV2::nowNs();send(m);}
                const auto now=HwaTelemetryV2::nowNs();
                const bool periodic=now-last>=250000000LL;
                if(periodic){auto m=counters.snapshot();m.server=session;send(m);last=now;}
                auto control=counters.controlSnapshot();
                if(control.controlSequence && (periodic||control.controlSequence!=lastControl)){
                    control.server=session;send(control);lastControl=control.controlSequence;
                }
                std::unique_lock<std::mutex> waitLock(mutex);
                wake.wait_for(waitLock,std::chrono::milliseconds(2),[this]{return !running.load()||!stops.empty();});
            }
        });return true;
    }
    void send(const HwaTelemetryV2::Message& m){
        std::lock_guard<std::mutex> lock(sendMutex);auto bytes=HwaTelemetryV2::encode(m);
        const auto rc=DDS::DDSIF::BytesWrite(domain,const_cast<char*>(responseTopic.c_str()),reinterpret_cast<const char*>(bytes.data()),static_cast<DDS::Long>(bytes.size()));
        if(rc!=DDS::RETCODE_OK)std::cerr<<"[RuntimeTelemetryV2][ERROR] write code="<<rc<<" kind="<<m.kind<<std::endl;
    }
    void complete(){counters.finish();auto m=counters.snapshot();m.server=session;if(writer){send(m);DDS::Duration_t timeout;timeout.sec=2;timeout.nanosec=0;const auto rc=writer->wait_for_acknowledgments(timeout);if(rc!=DDS::RETCODE_OK)std::cerr<<"[RuntimeTelemetryV2][ERROR] end acknowledgement code="<<rc<<std::endl;}}
    void stop(){running=false;wake.notify_all();if(thread.joinable())thread.join();if(reader){DDS::DDSIF::UnSubTopic(reader);reader=nullptr;}if(writer){DDS::DDSIF::UnPubTopic(writer);writer=nullptr;}}
#else
    bool start(const std::shared_ptr<DdsRuntimeManager>&,int,int,int){return false;}
    void complete(){}
    void stop(){}
#endif
    ~BoardTelemetryService(){stop();}
};
