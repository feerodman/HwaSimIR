#pragma once
#include "CustomerVideoContract.h"
#include "Generated/CustomerTxVideo.h"
#include "Generated/CustomerTxVideoDataWriter.h"
#include "Generated/CustomerTxVideoTypeSupport.h"
#include "ZRDDSCppSimpleInterface.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <sstream>

namespace CustomerTX {
static_assert(sizeof(DDS_Long)==4 && sizeof(DDS_ULong)==4,"Customer IDL requires 32-bit long");
class Writer {
public:
    // API return codes are kept verbatim; vendor transport log codes are separate.
    struct Result {
        bool apiCalled=false;
        int code=0;
        const char* stage="not_called";
        std::int32_t frameId=0;
        std::size_t payloadBytes=0;
        std::uint64_t completedSamples=0;
        double elapsedMs=0;
        std::string describe() const {
            std::ostringstream s;
            s<<"stage="<<stage<<" apiCalled="<<apiCalled<<" sdkReturnCode="<<code
             <<" frameId="<<frameId<<" payloadBytes="<<payloadBytes
             <<" completedSamples="<<completedSamples<<" writeMs="<<elapsedMs;
            return s.str();
        }
    };
    const Result& result() const {return m_result;}
    ~Writer(){close();}
    bool open(DDS::DomainParticipant* participant,bool precise,bool compressed,std::string& error) {
        close();m_result=Result();m_precise=precise;m_compressed=compressed;m_accepted=m_failures=0;
        DDS::TypeSupport* support=nullptr;
        if(compressed)support=precise?static_cast<DDS::TypeSupport*>(HD264StreamTypeSupport::get_instance()):LD264StreamTypeSupport::get_instance();
        else support=precise?static_cast<DDS::TypeSupport*>(HDDecoderTypeSupport::get_instance()):LDDecoderTypeSupport::get_instance();
        m_writer=DDS::DDSIF::PubTopic(participant,topic(precise,compressed),support,
            compressed?"customer_tx_h264_writer":"customer_tx_raw_writer",nullptr);
        if(!m_writer){error="customer typed writer creation failed";return false;}
        DDS::DataWriterQos actual;
        if(m_writer->get_qos(actual)!=DDS::RETCODE_OK || actual.reliability.kind!=DDS::RELIABLE_RELIABILITY_QOS ||
            actual.history.kind!=DDS::KEEP_ALL_HISTORY_QOS || actual.reliability.max_blocking_time.sec!=0 ||
            actual.reliability.max_blocking_time.nanosec!=100000000) {
            error="customer writer requires RELIABLE KEEP_ALL finite 100ms max_blocking_time";close();return false;
        }
        const int sampleLimit=compressed?4096:16;
        if(actual.resource_limits.max_samples<1 || actual.resource_limits.max_samples>sampleLimit ||
           actual.resource_limits.max_samples_per_instance<1 || actual.resource_limits.max_samples_per_instance>sampleLimit) {
            error="customer writer requires bounded sample history (raw<=16, H264<=4096)";close();return false;
        }
        snapshot("open");return true;
    }
    void close(){if(m_writer){DDS::DDSIF::UnPubTopic(m_writer);m_writer=nullptr;}}
    bool drain(int timeoutMs) {
        if(!m_writer)return true;
#if defined(_ZRDDS_INCLUDE_BATCH)
        // SDK-supported flush before ACK wait. A successful write is not proof
        // that a small final TCP sample has left the middleware send buffer.
        DDS::DataWriterQos flushQos;
        if(m_writer->get_qos(flushQos)==DDS::RETCODE_OK && flushQos.batch.enable) {
            m_result=Result();m_result.stage="DataWriter_flush";m_result.apiCalled=true;
            m_result.code=int(m_writer->flush());
            std::cout<<"[CustomerTXFlush] batchEnabled=1 sdkReturnCode="<<m_result.code<<std::endl;
            if(m_result.code!=int(DDS::RETCODE_OK)){snapshot("flush_failure");return false;}
        }
#endif
        DDS::Duration_t timeout;timeout.sec=timeoutMs/1000;timeout.nanosec=(timeoutMs%1000)*1000000;
        m_result=Result();m_result.stage="wait_for_acknowledgments";m_result.apiCalled=true;
        const auto begin=std::chrono::steady_clock::now();
        m_result.code=int(m_writer->wait_for_acknowledgments(timeout));
        m_result.elapsedMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
        if(m_result.code!=int(DDS::RETCODE_OK))snapshot("ack_failure");
        return m_result.code==int(DDS::RETCODE_OK);
    }
    bool send(const std::uint8_t* data,std::size_t bytes,std::int32_t frameId,std::uint64_t& samples,std::uint64_t& sentBytes,const std::atomic<bool>& cancel) {
        m_result=Result();m_result.frameId=frameId;m_result.payloadBytes=bytes;
        if(!m_writer || !data || !bytes){m_result.stage="application_validation";return false;}
        if(m_compressed) {
            if(m_precise)return fragments<HD264Stream>(data,bytes,[&](const HD264Stream& sample){
                return write(static_cast<HD264StreamDataWriter*>(m_writer),sample,sample.DataLength,samples,sentBytes,cancel);
            });
            return fragments<LD264Stream>(data,bytes,[&](const LD264Stream& sample){
                return write(static_cast<LD264StreamDataWriter*>(m_writer),sample,sample.DataLength,samples,sentBytes,cancel);
            });
        }
        if(bytes>RawCapacity || frameId<=0 || cancel.load()){m_result.stage="application_validation_or_cancel";return false;}
        bool ok=false;
        if(m_precise) {
            HDDecoder sample;HDDecoderInitialize(&sample);sample.frameId=frameId;
            if(!sample.stream.length(static_cast<DDS::ULong>(bytes))){HDDecoderFinalize(&sample);m_result.stage="sequence_allocation";return false;}
            std::memcpy(sample.stream.get_contiguous_buffer(),data,bytes);
            ok=write(static_cast<HDDecoderDataWriter*>(m_writer),sample,bytes,samples,sentBytes,cancel);
            HDDecoderFinalize(&sample);
        } else {
            LDDecoder sample;LDDecoderInitialize(&sample);sample.frameId=frameId;
            if(!sample.stream.length(static_cast<DDS::ULong>(bytes))){LDDecoderFinalize(&sample);m_result.stage="sequence_allocation";return false;}
            std::memcpy(sample.stream.get_contiguous_buffer(),data,bytes);
            ok=write(static_cast<LDDecoderDataWriter*>(m_writer),sample,bytes,samples,sentBytes,cancel);
            LDDecoderFinalize(&sample);
        }
        return ok;
    }
    void snapshot(const char* reason) {
        if(!m_writer)return;
        DDS::DataWriterQos q;
        const auto qr=m_writer->get_qos(q);
        DDS::PublicationMatchedStatus matched={};
        const auto mr=m_writer->get_publication_matched_status(matched);
        DDS::OfferedIncompatibleQosStatus incompatible={};
        const auto ir=m_writer->get_offered_incompatible_qos_status(incompatible);
        DDS::LivelinessLostStatus lost={};
        const auto lr=m_writer->get_liveliness_lost_status(lost);
        std::cout<<"[CustomerTXSdk] reason="<<reason<<" topic="<<topic(m_precise,m_compressed)
            <<" type="<<type(m_precise,m_compressed)<<" acceptedWrites="<<m_accepted<<" failedWrites="<<m_failures
            <<" getQosRc="<<int(qr)<<" matchedRc="<<int(mr)<<" matchedReaders="<<matched.current_count
            <<" matchTotal="<<matched.total_count<<" matchChange="<<matched.current_count_change
            <<" incompatibleRc="<<int(ir)<<" incompatibleTotal="<<incompatible.total_count
            <<" lastPolicy="<<incompatible.last_policy_id<<" livelinessRc="<<int(lr)<<" livelinessLost="<<lost.total_count;
        if(qr==DDS::RETCODE_OK)std::cout<<" reliability="<<int(q.reliability.kind)<<" history="<<int(q.history.kind)
            <<" maxSamples="<<q.resource_limits.max_samples<<" maxInstances="<<q.resource_limits.max_instances
            <<" maxPerInstance="<<q.resource_limits.max_samples_per_instance<<" durability="<<int(q.durability.kind)
            <<" blockingSec="<<q.reliability.max_blocking_time.sec<<" blockingNs="<<q.reliability.max_blocking_time.nanosec;
        std::cout<<" "<<m_result.describe()<<std::endl;
        auto* publisher=m_writer->get_publisher();
        if(publisher) {
            DDS::PublisherQos publisherQos;
            const auto pr=publisher->get_qos(publisherQos);
            auto* participant=publisher->get_participant();
            std::cout<<"[CustomerTXPartition] getQosRc="<<int(pr)<<" domain="<<(participant?participant->get_domain_id():-1);
            if(pr==DDS::RETCODE_OK){
                std::cout<<" count="<<publisherQos.partition.name._length;
                for(unsigned i=0;i<publisherQos.partition.name._length;++i)std::cout<<" partition="<<publisherQos.partition.name[i];
                if(!publisherQos.partition.name._length)std::cout<<" partition=<default_empty>";
            }
            std::cout<<std::endl;
            if(participant){DDS::DomainParticipantQos dp;
                const auto dr=participant->get_qos(dp);
                if(dr==DDS::RETCODE_OK){
                    std::cout<<"[CustomerTXParticipant] receiveBufferBytes="<<dp.receiver_thread_config.receive_buffer_length
                        <<" zeroMeansTransportDefault=1 singleSampleCapacity=640000_cdr_overhead_additional"<<std::endl;
                    for(unsigned i=0;i<dp.usertraffic_receive_addresses.addresses._length;++i)
                        std::cout<<"[CustomerTXReceiveAddress] configuredEffective="<<dp.usertraffic_receive_addresses.addresses[i]<<std::endl;
                }
            }
        }
        DDS::PublicationSendStatusSeq transports;
        const auto tr=m_writer->get_send_status(transports);
        if(tr==DDS::RETCODE_OK) {
            for(unsigned i=0;i<transports._length;++i)
            for(unsigned j=0;j<transports[i].send_locators._length;++j) {
                const auto& locator=transports[i].send_locators[j];
                std::cout<<"[CustomerTXLocator] transport="<<int(locator.locator_type)
                    <<" status="<<int(locator.locator_status)<<" transportReturnCode="<<locator.locator_return_code
                    <<" peer="<<int(locator.locator_addr[0])<<'.'<<int(locator.locator_addr[1])<<'.'
                    <<int(locator.locator_addr[2])<<'.'<<int(locator.locator_addr[3])<<':'<<locator.locator_port
                    <<" codeNamespace=SDK_transport_not_errno deliveryNotImpliedByApiOk=1"<<std::endl;
            }
        } else std::cout<<"[CustomerTXLocator] unavailable=1 apiReturnCode="<<int(tr)<<std::endl;
        DDS::InstanceHandleSeq handles;
        if(m_writer->get_matched_subscriptions(handles)==DDS::RETCODE_OK) {
            for(DDS::ULong i=0;i<handles.length();++i){
                DDS::SubscriptionBuiltinTopicData info;
                if(m_writer->get_matched_subscription_data(info,handles[i])!=DDS::RETCODE_OK)continue;
                std::cout<<"[CustomerTXReader] guid="<<std::hex<<info.key[0]<<":"<<info.key[1]<<":"<<info.key[2]<<":"<<info.key[3]<<std::dec
                    <<" type="<<info.type_name<<" topic="<<info.topic_name<<" reliability="<<int(info.reliability.kind)
                    <<" locator=NOT_EXPOSED_BY_SUBSCRIPTION_BUILTIN_API"<<std::endl;
            }
        }
    }
private:
    template<class W,class S>bool write(W* writer,const S& sample,std::size_t bytes,std::uint64_t& samples,std::uint64_t& sentBytes,const std::atomic<bool>& cancel) {
        if(cancel.load()){m_result.stage="application_cancel";return false;}
        const auto begin=std::chrono::steady_clock::now();
        m_result.apiCalled=true;m_result.stage="typed_DataWriter_write";
        m_result.code=int(writer->write(sample,DDS::HANDLE_NIL_NATIVE));
        m_result.elapsedMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
        if(m_result.code!=int(DDS::RETCODE_OK)){++m_failures;snapshot("write_failure");return false;}
        ++samples;sentBytes+=bytes;++m_result.completedSamples;++m_accepted;
        if(m_accepted==1 || m_accepted%600==0)snapshot("periodic");
        return true;
    }
    Result m_result;
    std::uint64_t m_accepted=0,m_failures=0;
    DDS::DataWriter* m_writer=nullptr;
    bool m_precise=true,m_compressed=true;
};
}
