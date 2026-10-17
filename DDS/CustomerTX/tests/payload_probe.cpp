// DDS payload probe: non-image lengths are deliberately not presented as images.
#include "CustomerTxWriter.h"
#include "Generated/CustomerTxVideoDataReader.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

int main(int argc,char** argv) {
    std::string qos="ZRDDS_PROTOCOL_QOS.xml",mode="send";
    int domain=1,bytes=102400,frames=60,hz=60,waitSeconds=3,holdSeconds=15;
    try {
        for(int i=1;i<argc;i+=2) {
            if(i+1>=argc)throw std::runtime_error("missing value");
            const std::string key=argv[i],value=argv[i+1];
            if(key=="--qos")qos=value;
            else if(key=="--domain")domain=std::stoi(value);
            else if(key=="--mode")mode=value;
            else if(key=="--bytes")bytes=std::stoi(value);
            else if(key=="--frames")frames=std::stoi(value);
            else if(key=="--hz")hz=std::stoi(value);
            else if(key=="--discovery-seconds")waitSeconds=std::stoi(value);
            else if(key=="--hold-seconds")holdSeconds=std::stoi(value);
            else throw std::runtime_error("unknown option");
        }
        if(domain<0 || bytes<1 || bytes>640000 || frames<1 || frames>100000 || hz<1 || hz>60 ||
           waitSeconds<0 || waitSeconds>30 || holdSeconds<1 || holdSeconds>60 || (mode!="send"&&mode!="hold"))throw std::runtime_error("invalid bounded probe");
        if(!DDS::DDSIF::Init(qos.c_str(),"hwasimir_factory"))throw std::runtime_error("Init failed");
        auto* participant=DDS::DDSIF::CreateDP(domain,"hwasimir_tcp");
        if(!participant){DDS::DDSIF::Finalize();throw std::runtime_error("CreateDP failed");}
        if(mode=="hold") {
            // Intentionally no listener/take: real DDS reader history pressure.
            auto* reader=DDS::DDSIF::SubTopic(participant,"TX/LDecode",LDDecoderTypeSupport::get_instance(),"customer_tx_raw_reader",nullptr);
            if(!reader){DDS::DDSIF::Finalize();throw std::runtime_error("hold reader creation failed");}
            DDS::DataReaderQos actual;const auto rc=reader->get_qos(actual);
            std::cout<<"[PayloadHold] domain="<<domain<<" getQosRc="<<int(rc)<<" maxSamples="<<actual.resource_limits.max_samples<<" seconds="<<holdSeconds<<std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(holdSeconds));
            DDS::DDSIF::UnSubTopic(reader);DDS::DDSIF::Finalize();return 0;
        }
        if(mode!="send")throw std::runtime_error("mode must be send/hold");
        CustomerTX::Writer writer;std::string error;
        if(!writer.open(participant,false,false,error)){DDS::DDSIF::Finalize();throw std::runtime_error(error);}
        std::this_thread::sleep_for(std::chrono::seconds(waitSeconds));
        std::vector<std::uint8_t> payload(bytes);
        for(int i=0;i<bytes;++i)payload[i]=static_cast<std::uint8_t>(i); // includes 00/80/FF.
        std::atomic<bool> cancel(false);std::uint64_t samples=0,sentBytes=0;
        int failureCode=0;bool failed=false;double maxWriteMs=0;
        const auto start=std::chrono::steady_clock::now();
        for(int i=1;i<=frames;++i) {
            const bool ok=writer.send(payload.data(),payload.size(),i,samples,sentBytes,cancel);
            maxWriteMs=(std::max)(maxWriteMs,writer.result().elapsedMs);
            if(!ok){failed=true;failureCode=writer.result().code;break;}
            std::this_thread::sleep_until(start+std::chrono::microseconds(1000000LL*i/hz));
        }
        const bool drained=writer.drain(1000);
        writer.snapshot("probe_complete");writer.close();DDS::DDSIF::Finalize();
        std::cout<<"{\"domain\":"<<domain<<",\"payloadBytes\":"<<bytes<<",\"hz\":"<<hz
                 <<",\"requestedFrames\":"<<frames<<",\"sentSamples\":"<<samples<<",\"sentBytes\":"<<sentBytes
                 <<",\"maxWriteMs\":"<<maxWriteMs<<",\"failed\":"<<(failed?"true":"false")
                 <<",\"actualWriteReturnCode\":"<<failureCode<<",\"ackDrain\":"<<(drained?"true":"false")<<"}"<<std::endl;
        return failed || !drained?2:0;
    }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
}
