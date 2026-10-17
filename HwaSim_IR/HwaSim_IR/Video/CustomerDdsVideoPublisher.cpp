#include "CustomerDdsVideoPublisher.h"
#include "CustomerVideoContract.h"
#include "Sha256File.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#if defined(HWASIMIR_HAS_ZRDDS)
#include "DdsRuntimeManager.h"
#include "CustomerTxWriter.h"
#endif
namespace {
std::int64_t nowNs(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
bool compressed(const CustomerDdsVideoConfig& c,bool requested){return c.mode=="h264" || (c.mode=="auto" && requested);}
}
struct CustomerDdsVideoPublisher::Impl {
    struct Frame {std::vector<std::uint8_t> bytes;int width=0,height=0;bool clean=false,flip=false;
        std::int64_t pts=0,queuedNs=0;std::uint64_t seq=0,source=0;};
    CustomerDdsVideoConfig config;std::shared_ptr<DdsRuntimeManager> runtime;
    std::mutex mutex;std::condition_variable cv,drained;std::deque<Frame> queue;std::thread worker;
    bool running=false,accepting=false,inFlight=false,failed=false,precise=true,h264=false;
    std::atomic<bool> cancel{false};
    bool diagnosticFailureConsumed=false;
    std::string failure;int round=0;VideoEncoderConfig encoderConfig;std::string backend;
    std::unique_ptr<IVideoEncoder> encoder;bool encoderReady=false;
    std::uint64_t accepted=0,sent=0,samples=0,bytes=0,aborted=0,rejected=0,maxDepth=0;
    std::ofstream audit,payload;HwaHash::Sha256 streamHash;
#if defined(HWASIMIR_HAS_ZRDDS)
    CustomerTX::Writer writer;
    DDS::DomainParticipant* participant=nullptr;
#endif
    void failLocked(const std::string& reason) {
        if(!failed)std::cerr<<"[CustomerTX][ERROR] reason="<<reason<<" round="<<round
            <<" role="<<(precise?"precise":"coarse")<<" queueDepth="<<queue.size()
            <<" accepted="<<accepted<<" sent="<<sent<<" aborted="<<aborted<<" rejected="<<rejected
            <<" action=abort_customer_round old_branch_continues=1"<<std::endl;
        failure=reason;failed=true;cancel.store(true);accepting=false;aborted+=queue.size();queue.clear();cv.notify_all();drained.notify_all();
    }
    void run() {
        for(;;) {
            Frame frame;
            {std::unique_lock<std::mutex> lock(mutex);cv.wait(lock,[&]{return !running || !queue.empty();});
                if(!running && queue.empty())break;
                frame=std::move(queue.front());queue.pop_front();inFlight=true;}
            bool ok=true;std::string error;std::vector<std::uint8_t> converted;EncodedVideoFrame encoded;
            std::uint64_t frameSamples=0,frameBytes=0;const auto begin=nowNs();double convertMs=0,encodeMs=0;
            const std::vector<std::uint8_t>* wire=&frame.bytes;
            try {
                if(!frame.clean) {
                    ok=CustomerTX::convertBgr(frame.bytes.data(),frame.width,frame.height,frame.width*3,frame.flip,
                        config.pixelFormat=="gray8"?CustomerTX::Pixels::Gray8:CustomerTX::Pixels::RGB24,converted);
                    convertMs=(nowNs()-begin)/1e6;wire=&converted;
                    if(ok && h264) {
                        if(!encoderReady) {
#if defined(HWASIMIR_HAS_RKMPP)
                            if(backend=="auto" || backend=="mpp")encoder.reset(new H264MppEncoder());
                            else
#endif
                            encoder.reset(new H264FfmpegEncoder());
                            encoderConfig.width=frame.width;encoderConfig.height=frame.height;
                            encoderReady=encoder->configure(encoderConfig,error);
                            if(encoderReady)encoder->requestKeyFrame();
                        }
                        // Use the existing RGB -> limited-range YUV path, with
                        // neutral RGB for gray. MPP's legacy Gray8 fast path
                        // copies full-range Y without declaring its range.
                        std::vector<std::uint8_t> neutral;
                        if(config.pixelFormat=="gray8") {
                            neutral.resize(converted.size()*3);
                            for(std::size_t i=0;i<converted.size();++i)neutral[3*i]=neutral[3*i+1]=neutral[3*i+2]=converted[i];
                        }
                        RawVideoFrame raw;raw.data=neutral.empty()?converted.data():neutral.data();raw.width=frame.width;raw.height=frame.height;
                        raw.pixelFormat=RawVideoPixelFormat::Rgb24;
                        raw.stride=frame.width*3;raw.ptsMs=frame.pts;
                        ok=encoderReady && encoder->encode(raw,encoded,error);encodeMs=encoded.encodeMs;wire=&encoded.payload;
                    }
                }
                if(ok && (wire->empty() || (!h264 && wire->size()>CustomerTX::RawCapacity))) {ok=false;error="invalid_payload_size";}
                const auto writeBegin=nowNs();
#if defined(HWASIMIR_HAS_ZRDDS)
                if(ok) {
                    ok=writer.send(wire->data(),wire->size(),static_cast<std::int32_t>(frame.seq),frameSamples,frameBytes,cancel);
                    if(!ok)error=writer.result().describe();
                }
#else
                ok=false;error="ZRDDS_unavailable";
#endif
                const auto end=nowNs();
                if(ok) {
                    streamHash.update(wire->data(),wire->size());
                    if(payload.is_open())payload.write(reinterpret_cast<const char*>(wire->data()),static_cast<std::streamsize>(wire->size()));
                    if(audit.is_open()) {
                        HwaHash::Sha256 hash;hash.update(wire->data(),wire->size());
                        audit<<"{\"round\":"<<round<<",\"frameSeq\":"<<frame.seq<<",\"sourceSeq\":"<<frame.source
                            <<",\"bytes\":"<<wire->size()<<",\"samples\":"<<frameSamples<<",\"sha256\":\""<<hash.finishHex()
                            <<"\",\"queuedNs\":"<<frame.queuedNs<<",\"workerNs\":"<<begin<<",\"writeBeginNs\":"<<writeBegin
                            <<",\"writeEndNs\":"<<end<<",\"queueWaitMs\":"<<(begin-frame.queuedNs)/1e6
                            <<",\"convertMs\":"<<convertMs<<",\"encodeMs\":"<<encodeMs<<",\"writeMs\":"<<(end-writeBegin)/1e6
                            <<",\"sharedCleanAu\":"<<(frame.clean?"true":"false")
                            <<",\"diagnosticColorCard\":"<<(config.diagnosticColorCard?"true":"false")<<"}\n";
                    }
                    if((payload.is_open() && !payload.good()) || (audit.is_open() && !audit.good())) {ok=false;error="audit_storage_failed";}
                }
            } catch(const std::exception& e){ok=false;error=e.what();}
            {std::lock_guard<std::mutex> lock(mutex);samples+=frameSamples;bytes+=frameBytes;
                if(ok)++sent;else {++aborted;failLocked(error.empty()?"typed_write_failed_or_timeout":error);}
                inFlight=false;drained.notify_all();}
        }
    }
};
CustomerDdsVideoPublisher::CustomerDdsVideoPublisher():m(new Impl){}
CustomerDdsVideoPublisher::~CustomerDdsVideoPublisher(){shutdown();}
bool CustomerDdsVideoPublisher::validate(const CustomerDdsVideoConfig& c,int w,int h,bool requested,std::string& error) {
    if(!c.enabled)return true;
    if((c.mode!="auto" && c.mode!="raw" && c.mode!="h264") || (c.pixelFormat!="gray8" && c.pixelFormat!="rgb24") ||
        c.queueMaxFrames<1 || c.queueMaxFrames>256 || c.drainMs<100 || c.drainMs>10000 || c.discoverySettleMs<0 || c.discoverySettleMs>5000) {
        error="invalid CustomerDdsVideo configuration";return false;
    }
    std::size_t count=0;
    if(!CustomerTX::imageBytes(w,h,c.pixelFormat=="gray8"?CustomerTX::Pixels::Gray8:CustomerTX::Pixels::RGB24,count)) {error="invalid image geometry";return false;}
    if(!compressed(c,requested) && count>CustomerTX::RawCapacity) {
        error="raw_capacity_exceeded required="+std::to_string(count)+" allowed=640000";return false;
    }
    return true;
}
bool CustomerDdsVideoPublisher::start(const CustomerDdsVideoConfig& c,const std::shared_ptr<DdsRuntimeManager>& runtime,bool precise,std::string& error) {
    m->config=c;m->precise=precise;m->runtime=runtime;
    if(!c.enabled)return true;
    if(!validate(c,1,1,false,error))return false;
#if !defined(HWASIMIR_HAS_ZRDDS)
    error="customer requires ZRDDS build";return false;
#else
    if(!runtime || !runtime->running()){error="customer requires existing DDS runtime";return false;}
    m->config.domainId=c.domainId<0?runtime->config().domainId:c.domainId;
    m->participant=runtime->participantForDomain(m->config.domainId,error);
    if(!m->participant)return false;
    m->running=true;m->worker=std::thread(&Impl::run,m.get());
    std::cout<<"[CustomerTX] enabled=1 sharedRuntime=1 runtimeInitCount="<<runtime->initCount()
        <<" domain="<<m->config.domainId<<" qos="<<runtime->config().qosFile<<" mode="<<c.mode
        <<" pixelFormat="<<c.pixelFormat<<" queueMaxFrames="<<c.queueMaxFrames<<" writeTimeoutMs=100 drainMs="<<c.drainMs<<std::endl;
    return true;
#endif
}
bool CustomerDdsVideoPublisher::beginRound(int round,bool requested,const VideoEncoderConfig& config,const std::string& backend,std::string& error) {
    if(!enabled())return true;
    endRound(error);
    std::lock_guard<std::mutex> lock(m->mutex);
    if(m->inFlight || !m->queue.empty()){error="previous customer round still draining";return false;}
    std::cout<<"[CustomerTXState] previous="<<(m->failed?"Failed":"Ready")<<" next=Recovering globalReinit=0"<<std::endl;
    error.clear();m->cancel.store(false);
    m->round=round;m->h264=compressed(m->config,requested);m->encoderConfig=config;m->backend=backend;
    m->encoder.reset();m->encoderReady=false;m->failed=false;m->failure.clear();
    m->accepted=m->sent=m->samples=m->bytes=m->aborted=m->rejected=m->maxDepth=0;m->streamHash=HwaHash::Sha256();
#if defined(HWASIMIR_HAS_ZRDDS)
    if(!m->writer.open(m->participant,m->precise,m->h264,error)){m->failLocked(error);return false;}
#endif
    if(!m->config.auditPrefix.empty()) {
        const std::string prefix=m->config.auditPrefix+"_round"+std::to_string(round);
        m->audit.open(prefix+".jsonl",std::ios::out|std::ios::trunc);
        if(m->h264)m->payload.open(prefix+".h264",std::ios::out|std::ios::binary|std::ios::trunc);
        if(!m->audit || (m->h264 && !m->payload)){error="cannot open customer audit";m->failLocked(error);return false;}
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(m->config.discoverySettleMs));
    m->accepting=true;
#if defined(HWASIMIR_HAS_ZRDDS)
    std::cout<<"[CustomerTXState] next=Running runtimeInitCount="<<m->runtime->initCount()<<std::endl;
#endif
    std::cout<<"[CustomerTXRound] round="<<round<<" topic="<<CustomerTX::topic(m->precise,m->h264)<<" type="<<CustomerTX::type(m->precise,m->h264)
        <<" requestedH264="<<requested<<" actualH264="<<m->h264<<" frameIdReset=1 privateSei=0"<<std::endl;
    return true;
}
bool CustomerDdsVideoPublisher::submit(const RawVideoFrame& raw,std::uint64_t seq,std::uint64_t source,const std::vector<std::uint8_t>* clean) {
    if(!enabled())return true;
    std::lock_guard<std::mutex> lock(m->mutex);
    if(!m->accepting || m->failed){++m->rejected;return false;}
    if(m->config.diagnosticFailAfterFramesOnce>0 && !m->diagnosticFailureConsumed &&
       m->accepted>=static_cast<std::uint64_t>(m->config.diagnosticFailAfterFramesOnce)) {
        m->diagnosticFailureConsumed=true;++m->rejected;
        m->failLocked("diagnostic_application_failure_once_NOT_SDK_RETURN_CODE");return false;
    }
    if(!seq || seq>2147483647ULL || m->queue.size()>=static_cast<std::size_t>(m->config.queueMaxFrames)) {
        ++m->rejected;m->failLocked("frame_id_or_queue_capacity_exceeded");return false;
    }
    std::string error;
    if(!validate(m->config,raw.width,raw.height,m->h264,error)){++m->rejected;m->failLocked(error);return false;}
    if(m->h264 && m->config.pixelFormat=="rgb24" && !m->config.diagnosticColorCard && clean && clean->empty()) {
        ++m->rejected;m->failLocked("shared_encoder_failed");return false;
    }
    try {
    Impl::Frame f;f.width=raw.width;f.height=raw.height;f.flip=raw.flipVertical;f.pts=raw.ptsMs;f.seq=seq;f.source=source;f.queuedNs=nowNs();
    f.clean=m->h264 && m->config.pixelFormat=="rgb24" && !m->config.diagnosticColorCard && clean && !clean->empty();
    if(f.clean)f.bytes=*clean;
    else {
        if(!raw.data || raw.pixelFormat!=RawVideoPixelFormat::Bgr24 || raw.stride<raw.width*3){++m->rejected;m->failLocked("invalid_bgr_input");return false;}
        f.bytes.resize(static_cast<std::size_t>(raw.width)*raw.height*3);
        for(int y=0;y<raw.height;++y)std::memcpy(f.bytes.data()+static_cast<std::size_t>(y)*raw.width*3,raw.data+static_cast<std::size_t>(y)*raw.stride,static_cast<std::size_t>(raw.width)*3);
    }
    if(m->config.diagnosticColorCard) {
        // Explicit customer-branch-only diagnostic. This is never an IR scene.
        const unsigned char rgb[6][3]={{255,0,0},{0,255,0},{0,0,255},{0,0,0},{128,128,128},{255,255,255}};
        f.flip=false;
        for(int y=0;y<f.height;++y)for(int x=0;x<f.width;++x){
            const int stripe=(std::min)(5,x*6/f.width);auto* p=f.bytes.data()+(static_cast<std::size_t>(y)*f.width+x)*3;
            const int factor=y<f.height/2?1:2;p[0]=rgb[stripe][2]/factor;p[1]=rgb[stripe][1]/factor;p[2]=rgb[stripe][0]/factor;
        }
    }
    m->queue.push_back(std::move(f));++m->accepted;m->maxDepth=(std::max)(m->maxDepth,static_cast<std::uint64_t>(m->queue.size()));m->cv.notify_one();return true;
    } catch(const std::exception& e) {++m->rejected;m->failLocked(std::string("copy_failed: ")+e.what());return false;}
}
bool CustomerDdsVideoPublisher::endRound(std::string& error) {
    if(!enabled())return true;
    std::unique_lock<std::mutex> lock(m->mutex);m->accepting=false;const auto start=nowNs();
    if(!m->drained.wait_for(lock,std::chrono::milliseconds(m->config.drainMs),[&]{return m->queue.empty() && !m->inFlight;})) {
        m->failLocked("drain_timeout");error=m->failure;return false;
    }
#if defined(HWASIMIR_HAS_ZRDDS)
    if(m->round) {
        if(!m->failed && m->h264 && m->sent && m->encoderConfig.h264StandardEnd) {
            // Real normal end only. H.264 end_of_seq RBSP is empty; this is
            // a standard NAL, not a customer application EOS sample/header.
            const std::uint8_t endSequence[]={0,0,0,1,10};
            std::uint64_t endSamples=0,endBytes=0;
            const auto endBegin=nowNs();
            if(!m->writer.send(endSequence,sizeof(endSequence),0,endSamples,endBytes,m->cancel))
                m->failLocked(m->writer.result().describe());
            else {
                m->samples+=endSamples;m->bytes+=endBytes;
                m->streamHash.update(endSequence,sizeof(endSequence));
                if(m->payload.is_open())m->payload.write(reinterpret_cast<const char*>(endSequence),sizeof(endSequence));
                std::cout<<"[CustomerTXEndSequence] round="<<m->round<<" nalType=10 bytes="<<endBytes
                    <<" writeBeginNs="<<endBegin<<" writeEndNs="<<nowNs()<<" pictureCountAdded=0"<<std::endl;
            }
        }
        const int remaining=(std::max)(0,m->config.drainMs-static_cast<int>((nowNs()-start)/1000000));
        if(!m->failed && !m->writer.drain(remaining))m->failLocked(m->writer.result().describe());
    }
#endif
    if(m->round)std::cout<<"[CustomerTXDrain] round="<<m->round<<" accepted="<<m->accepted<<" sentFrames="<<m->sent<<" samples="<<m->samples
        <<" bytes="<<m->bytes<<" aborted="<<m->aborted<<" rejected="<<m->rejected<<" maxQueueDepth="<<m->maxDepth
        <<" drainMs="<<(nowNs()-start)/1e6<<" streamSha256="<<m->streamHash.finishHex()<<" failed="<<m->failed<<std::endl;
    m->audit.close();m->payload.close();m->round=0;
#if defined(HWASIMIR_HAS_ZRDDS)
    // Keep one healthy Writer through STOP. TCP Readers in this SDK can report
    // BEST_EFFORT despite the requested RELIABLE profile, so ACK completion is
    // not a delivery proof. Immediate UnPubTopic can discard the final sample.
    // Failed entities are released here; beginRound safely rebuilds either case.
    if(m->failed)m->writer.close();
#endif
    error=m->failure;return !m->failed;
}
void CustomerDdsVideoPublisher::shutdown() {
    if(!m->running)return;
    std::string error;endRound(error);
    {std::lock_guard<std::mutex> lock(m->mutex);m->running=false;m->accepting=false;m->aborted+=m->queue.size();m->queue.clear();m->cv.notify_all();}
    if(m->worker.joinable())m->worker.join();
#if defined(HWASIMIR_HAS_ZRDDS)
    m->writer.close();
#endif
}
bool CustomerDdsVideoPublisher::enabled()const{return m->config.enabled;}
bool CustomerDdsVideoPublisher::wantsSharedH264(bool requested)const{return enabled() && compressed(m->config,requested) && m->config.pixelFormat=="rgb24" && !m->config.diagnosticColorCard;}
