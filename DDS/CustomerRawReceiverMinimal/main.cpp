// 只依赖 C++11、ZRDDS 和客户原始 generated 类型；无需 Qt / FFmpeg / CommonData。
#include "CustomerTxVideoDataReader.h"
#include "CustomerTxVideoTypeSupport.h"
#include "ZRDDSCppSimpleInterface.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <deque>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

static std::atomic<bool> interrupted(false);
static void interrupt(int) { interrupted.store(true); }

class RawReceiver : public DDS::SimpleDataReaderListener<LDDecoder, LDDecoderSeq, LDDecoderDataReader> {
public:
    struct Frame { DDS_Long id; std::vector<unsigned char> bytes; };
    explicit RawReceiver(std::size_t expected, int delay) : expectedBytes(expected), delayMs(delay) {}
    ~RawReceiver() { stop(); }
    void start() { worker=std::thread(&RawReceiver::consume,this); }
    void stop() {
        { std::lock_guard<std::mutex> guard(mutex); stopping=true; cv.notify_all(); }
        if(worker.joinable()) worker.join();
    }
    void on_process_sample(DDS::DataReader*, const LDDecoder& sample, const DDS::SampleInfo&) override {
        // SDK SimpleDataReaderListener 已执行 take、过滤 valid_data，回调后归还 loan。
        // 不重复 take，不保存 stream 指针。sequence 是二进制，不可用 strlen/sizeof(sample)。
        const std::size_t length=sample.stream.length();
        if(sample.frameId<=0 || length!=expectedBytes || length>640000) { ++invalid; return; }
        try {
            std::lock_guard<std::mutex> guard(mutex);
            // 同时有样本数和字节上界：最多 16 帧，640000*16 字节。
            if(stopping || queue.size()>=16) { ++rejected; return; }
            Frame copy;
            copy.id=sample.frameId;
            const auto* data=reinterpret_cast<const unsigned char*>(sample.stream.get_contiguous_buffer());
            copy.bytes.assign(data,data+length);
            queue.push_back(std::move(copy));
            ++received; bytes+=length; cv.notify_one();
        } catch(...) { ++rejected; } // 异常不得逃出 SDK 回调，否则 loan 无法正常归还。
    }
    void on_sample_rejected(DDS::DataReader*,const DDS::SampleRejectedStatus& s) override {
        std::cerr<<"[RawSampleRejected] total="<<s.total_count<<" reason="<<int(s.last_reason)<<std::endl;
    }
    void on_sample_lost(DDS::DataReader*,const DDS::SampleLostStatus& s) override {
        std::cerr<<"[RawSampleLost] total="<<s.total_count<<std::endl;
    }
    void on_requested_incompatible_qos(DDS::DataReader*,const DDS::RequestedIncompatibleQosStatus& s) override {
        std::cerr<<"[RawIncompatibleQos] total="<<s.total_count<<" policy="<<s.last_policy_id<<std::endl;
    }
    void consume() {
        for(;;) {
            Frame frame;
            { std::unique_lock<std::mutex> guard(mutex);
              cv.wait(guard,[&]{return stopping || !queue.empty();});
              if(queue.empty()) return;
              frame=std::move(queue.front()); queue.pop_front(); }
            // 工作线程拥有副本。显示/录像可接在这里，不能移到 DDS callback 内。
            if(delayMs) std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            if(lastId && frame.id==lastId) ++duplicates;
            if(lastId && std::int64_t(frame.id)>std::int64_t(lastId)+1) gaps+=std::int64_t(frame.id)-lastId-1;
            if(lastId && frame.id<lastId) ++roundResets;
            lastId=frame.id; ++consumed;
            if(consumed==1 || consumed%60==0)
                std::cout<<"[RawFrame] frameId="<<frame.id<<" length="<<frame.bytes.size()
                         <<" firstPixel="<<unsigned(frame.bytes.front())<<" lastPixel="<<unsigned(frame.bytes.back())
                         <<" consumed="<<consumed<<std::endl;
        }
    }
    std::atomic<std::uint64_t> received{0},bytes{0},invalid{0},rejected{0};
    std::uint64_t consumed=0,duplicates=0,gaps=0,roundResets=0;
private:
    std::size_t expectedBytes;
    int delayMs;
    DDS_Long lastId=0;
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<Frame> queue;
    bool stopping=false;
    std::thread worker;
};

int main(int argc,char** argv) {
    static_assert(sizeof(DDS_Long)==4,"IDL long is 32 bit, including Linux AArch64");
    try {
        int domain=1,seconds=0,delay=0;
        std::size_t bytes=320*320;
        std::string qos="ZRDDS_PROTOCOL_QOS.xml";
        for(int i=1;i<argc;i+=2) {
            if(i+1>=argc) throw std::runtime_error("option requires value");
            const std::string key=argv[i],value=argv[i+1];
            if(key=="--domain") domain=std::stoi(value);
            else if(key=="--qos") qos=value;
            else if(key=="--seconds") seconds=std::stoi(value);
            else if(key=="--probe-bytes") bytes=std::stoul(value);
            else if(key=="--slow-ms") delay=std::stoi(value);
            else throw std::runtime_error("unknown option: "+key);
        }
        if(domain<0 || seconds<0 || delay<0 || delay>1000 || bytes<1 || bytes>640000)
            throw std::runtime_error("invalid bounded configuration");
        RawReceiver receiver(bytes,delay);
        struct DdsSession {
            RawReceiver& receiver;DDS::DataReader* reader=nullptr;bool initialized=false;
            explicit DdsSession(RawReceiver& r):receiver(r){}
            void close(){if(reader){DDS::DDSIF::UnSubTopic(reader);reader=nullptr;}receiver.stop();if(initialized){DDS::DDSIF::Finalize();initialized=false;}}
            ~DdsSession(){close();}
        } session(receiver);
        auto* factory=DDS::DDSIF::Init(qos.c_str(),"hwasimir_factory"); // 进程中仅一次。
        if(!factory) throw std::runtime_error("DDSIF::Init failed");
        session.initialized=true;
        auto* participant=DDS::DDSIF::CreateDP(domain,"hwasimir_tcp");
        // SubTopic 内部通过 TypeSupport 注册全局 LDDecoder，不能注册本机 native struct。
        auto* reader=participant?DDS::DDSIF::SubTopic(participant,"TX/LDecode",
            LDDecoderTypeSupport::get_instance(),"customer_tx_raw_reader",&receiver):nullptr;
        session.reader=reader;
        if(!reader) throw std::runtime_error("SubTopic failed");
        DDS::DataReaderQos actual;
        const auto rc=reader->get_qos(actual);
        std::cout<<"[RawReady] domain="<<domain<<" topic=TX/LDecode type=LDDecoder expectedBytes="<<bytes
                 <<" qos="<<qos<<" getQosRc="<<int(rc);
        if(rc==DDS::RETCODE_OK) std::cout<<" reliability="<<int(actual.reliability.kind)
            <<" history="<<int(actual.history.kind)<<" maxSamples="<<actual.resource_limits.max_samples;
        std::cout<<" mode="<<(bytes==102400?"320x320_Gray8":"DDS_payload_probe_not_image")<<std::endl;
        receiver.start();
        std::signal(SIGINT,interrupt);std::signal(SIGTERM,interrupt);
        const auto start=std::chrono::steady_clock::now();
        while(!interrupted && (!seconds || std::chrono::steady_clock::now()-start<std::chrono::seconds(seconds)))
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        // 先移除 Reader、停止新回调，再排空副本队列、join 工作线程，最后 Finalize。
        session.close();
        std::cout<<"{\"receivedSamples\":"<<receiver.received<<",\"bytes\":"<<receiver.bytes
                 <<",\"consumed\":"<<receiver.consumed<<",\"invalid\":"<<receiver.invalid
                 <<",\"rejected\":"<<receiver.rejected<<",\"duplicates\":"<<receiver.duplicates
                 <<",\"gaps\":"<<receiver.gaps<<",\"roundResets\":"<<receiver.roundResets<<"}"<<std::endl;
        return receiver.invalid || receiver.rejected || !receiver.received?2:0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<std::endl; return 1; }
}
