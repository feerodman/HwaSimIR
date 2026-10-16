#pragma once
#include "CustomerVideoContract.h"
#include "Generated/CustomerTxVideo.h"
#include "Generated/CustomerTxVideoDataWriter.h"
#include "Generated/CustomerTxVideoTypeSupport.h"
#include "ZRDDSCppSimpleInterface.h"
#include <atomic>

namespace CustomerTX {
static_assert(sizeof(DDS_Long)==4 && sizeof(DDS_ULong)==4,"Customer IDL requires 32-bit long");
class Writer {
public:
    ~Writer(){close();}
    bool open(DDS::DomainParticipant* participant,bool precise,bool compressed,std::string& error) {
        close();m_precise=precise;m_compressed=compressed;
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
        return true;
    }
    void close(){if(m_writer){DDS::DDSIF::UnPubTopic(m_writer);m_writer=nullptr;}}
    bool drain(int timeoutMs) {
        if(!m_writer)return true;
        DDS::Duration_t timeout;timeout.sec=timeoutMs/1000;timeout.nanosec=(timeoutMs%1000)*1000000;
        return m_writer->wait_for_acknowledgments(timeout)==DDS::RETCODE_OK;
    }
    bool send(const std::uint8_t* data,std::size_t bytes,std::int32_t frameId,std::uint64_t& samples,std::uint64_t& sentBytes,const std::atomic<bool>& cancel) {
        if(!m_writer || !data || !bytes)return false;
        if(m_compressed) {
            if(m_precise)return fragments<HD264Stream>(data,bytes,[&](const HD264Stream& sample){
                if(cancel.load())return false;
                const bool ok=static_cast<HD264StreamDataWriter*>(m_writer)->write(sample,DDS::HANDLE_NIL_NATIVE)==DDS::RETCODE_OK;
                if(ok){++samples;sentBytes+=sample.DataLength;}return ok;
            });
            return fragments<LD264Stream>(data,bytes,[&](const LD264Stream& sample){
                if(cancel.load())return false;
                const bool ok=static_cast<LD264StreamDataWriter*>(m_writer)->write(sample,DDS::HANDLE_NIL_NATIVE)==DDS::RETCODE_OK;
                if(ok){++samples;sentBytes+=sample.DataLength;}return ok;
            });
        }
        if(bytes>RawCapacity || frameId<=0 || cancel.load())return false;
        bool ok=false;
        if(m_precise) {
            HDDecoder sample;HDDecoderInitialize(&sample);sample.frameId=frameId;
            if(!sample.stream.length(static_cast<DDS::ULong>(bytes))){HDDecoderFinalize(&sample);return false;}
            std::memcpy(sample.stream.get_contiguous_buffer(),data,bytes);
            ok=static_cast<HDDecoderDataWriter*>(m_writer)->write(sample,DDS::HANDLE_NIL_NATIVE)==DDS::RETCODE_OK;
            HDDecoderFinalize(&sample);
        } else {
            LDDecoder sample;LDDecoderInitialize(&sample);sample.frameId=frameId;
            if(!sample.stream.length(static_cast<DDS::ULong>(bytes))){LDDecoderFinalize(&sample);return false;}
            std::memcpy(sample.stream.get_contiguous_buffer(),data,bytes);
            ok=static_cast<LDDecoderDataWriter*>(m_writer)->write(sample,DDS::HANDLE_NIL_NATIVE)==DDS::RETCODE_OK;
            LDDecoderFinalize(&sample);
        }
        if(ok){++samples;sentBytes+=bytes;}return ok;
    }
private:
    DDS::DataWriter* m_writer=nullptr;
    bool m_precise=true,m_compressed=true;
};
}
