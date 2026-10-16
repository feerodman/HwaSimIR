// Independent customer receiver: only the four unchanged customer DDS types.
#include "CustomerVideoContract.h"
#include "Generated/CustomerTxVideoDataReader.h"
#include "Generated/CustomerTxVideoTypeSupport.h"
#include "ZRDDSCppSimpleInterface.h"
#include "CustomerAvSha256.h"
#include <QApplication>
#include <QLabel>
#include <QTimer>
#include <QDir>
#include <QImage>
#include <QPixmap>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
using Clock=std::chrono::steady_clock;
static long long ns(){return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count();}
struct Options {
    std::string topic,codec,pixels,out,qos,ffmpeg="ffmpeg";
    int width=0,height=0,fps=60,domain=150,idleMs=10000,timeoutMs=180000,slowMs=0,callbackBlockMs=0;
    bool display=true;std::size_t queueBytes=32*1024*1024;
};
static Options parse(int argc,char** argv) {
    Options o;
    for(int i=1;i<argc;i+=2) {
        if(i+1>=argc)throw std::runtime_error("missing option value");
        const std::string k=argv[i],v=argv[i+1];
        if(k=="--topic")o.topic=v;else if(k=="--codec")o.codec=v;
        else if(k=="--pixel-format")o.pixels=v;else if(k=="--output")o.out=v;
        else if(k=="--qos")o.qos=v;else if(k=="--width")o.width=std::stoi(v);
        else if(k=="--height")o.height=std::stoi(v);else if(k=="--fps")o.fps=std::stoi(v);
        else if(k=="--domain")o.domain=std::stoi(v);else if(k=="--idle-ms")o.idleMs=std::stoi(v);
        else if(k=="--timeout-ms")o.timeoutMs=std::stoi(v);else if(k=="--slow-ms")o.slowMs=std::stoi(v);
        else if(k=="--callback-block-ms")o.callbackBlockMs=std::stoi(v);
        else if(k=="--display")o.display=v=="1";else if(k=="--ffmpeg")o.ffmpeg=v;
        else if(k=="--queue-bytes")o.queueBytes=std::stoull(v);
        else throw std::runtime_error("unknown option "+k);
    }
    bool compressed=o.codec=="h264";
    if(o.codec!="raw" && !compressed)throw std::runtime_error("explicit codec raw/h264 required");
    if(o.topic!=CustomerTX::topic(true,compressed) && o.topic!=CustomerTX::topic(false,compressed))throw std::runtime_error("topic/codec mismatch");
    if(o.pixels!="gray8" && o.pixels!="rgb24")throw std::runtime_error("explicit gray8/rgb24 required; RGB order, top-down");
    std::size_t bytes=0;
    if(!CustomerTX::imageBytes(o.width,o.height,o.pixels=="gray8"?CustomerTX::Pixels::Gray8:CustomerTX::Pixels::RGB24,bytes) ||
       (!compressed && bytes>CustomerTX::RawCapacity) || o.width>4096 || o.height>4096 || o.fps<=0 || o.idleMs<=0 || o.timeoutMs<=0 ||
       o.slowMs<0 || o.slowMs>5000 || o.callbackBlockMs<0 || o.callbackBlockMs>5000 ||
       o.queueBytes<640000 || o.queueBytes>256*1024*1024 || o.out.empty() || o.qos.empty())throw std::runtime_error("invalid explicit geometry/capacity/configuration");
    return o;
}
struct Receiver {
    struct Sample {std::vector<std::uint8_t> data;int id=0;long long received=0;};
    Options o;std::mutex mutex,imageMutex;std::condition_variable cv;std::deque<Sample> queue;
    std::size_t queued=0,maxQueued=0;bool accepting=true,stopping=false;std::thread worker;
    std::atomic<bool> failed{false};std::atomic<long long> lastReceive{0};
    std::atomic<unsigned long long> received{0},receivedBytes{0},invalid{0},rejected{0},decoded{0},shown{0};
    unsigned long long processed=0,saved=0,decodeErrors=0,resets=0,recoverySkippedPackets=0;long long firstReceive=0,lastDecoded=0;
    bool waitingIdr=true;std::vector<uint8_t> sps,pps;
    QImage latest,lastImage;std::uint64_t latestId=0;std::ofstream wire,audit;
    CustomerAvSha256 hash;std::string streamHash;
    AVCodecParserContext* parser=nullptr;AVCodecContext* decoder=nullptr;AVFrame* frame=nullptr;SwsContext* sws=nullptr;
    explicit Receiver(const Options& options):o(options){}
    void error(const std::string& reason){failed=true;std::cerr<<"[CustomerRxError] "<<reason<<std::endl;}
    void push(const void* data,std::size_t bytes,int id) {
        const auto stamp=ns();std::size_t expected=static_cast<std::size_t>(o.width)*o.height*(o.pixels=="gray8"?1:3);
        if(!data || (o.codec=="h264"?!CustomerTX::validLength(static_cast<std::uint32_t>(bytes)):(bytes!=expected || bytes>640000 || id<=0))) {++invalid;failed=true;return;}
        std::lock_guard<std::mutex> lock(mutex);
        if(!accepting || failed || queued+bytes>o.queueBytes){++rejected;failed=true;accepting=false;cv.notify_all();return;}
        try {
            Sample sample;const auto* p=static_cast<const std::uint8_t*>(data);sample.data.assign(p,p+bytes);sample.id=id;sample.received=stamp;
            queue.push_back(std::move(sample));queued+=bytes;maxQueued=(std::max)(maxQueued,queued);
            ++received;receivedBytes+=bytes;lastReceive=stamp;if(!firstReceive)firstReceive=stamp;cv.notify_one();
        } catch(const std::exception&){++rejected;failed=true;accepting=false;}
    }
    void initDecoder(){
        waitingIdr=true;sps.clear();pps.clear();
        parser=av_parser_init(AV_CODEC_ID_H264);decoder=avcodec_alloc_context3(avcodec_find_decoder(AV_CODEC_ID_H264));frame=av_frame_alloc();
        if(!parser || !decoder || !frame)throw std::runtime_error("decoder allocation failed");
        decoder->thread_count=1;
        if(avcodec_open2(decoder,avcodec_find_decoder(AV_CODEC_ID_H264),nullptr)<0)throw std::runtime_error("decoder open failed");
    }
    void closeDecoder(){if(parser)av_parser_close(parser);parser=nullptr;avcodec_free_context(&decoder);av_frame_free(&frame);sws_freeContext(sws);sws=nullptr;}
    void image(const QImage& img,int rawId,long long receivedNs){
        ++decoded;lastDecoded=ns();lastImage=img.copy();
        if(decoded==1 && !img.save(QString::fromStdString(o.out+"/first.png")))error("first PNG write failed");
        CustomerAvSha256 pixelHash;for(int y=0;y<img.height();++y)pixelHash.update(img.constScanLine(y),static_cast<std::size_t>(img.width())*(img.format()==QImage::Format_Grayscale8?1:3));
        audit<<"{\"kind\":\"frame\",\"index\":"<<decoded<<",\"frameId\":"<<rawId<<",\"receivedNs\":"<<receivedNs<<",\"decodedNs\":"<<lastDecoded
             <<",\"pixelSha256\":\""<<pixelHash.finishHex()<<"\"}\n";
        {std::lock_guard<std::mutex> lock(imageMutex);latest=lastImage;latestId=decoded;}
    }
    void receiveFrames(long long stamp){
        for(;;){int ret=avcodec_receive_frame(decoder,frame);if(ret==AVERROR(EAGAIN) || ret==AVERROR_EOF)return;
            if(ret<0){++decodeErrors;error("decoder receive failed after IDR recovery");return;}
            if(frame->width!=o.width || frame->height!=o.height){error("decoded dimensions differ from explicit local configuration");return;}
            QImage img(o.width,o.height,o.pixels=="gray8"?QImage::Format_Grayscale8:QImage::Format_RGB888);
            sws=sws_getCachedContext(sws,o.width,o.height,static_cast<AVPixelFormat>(frame->format),o.width,o.height,
                o.pixels=="gray8"?AV_PIX_FMT_GRAY8:AV_PIX_FMT_RGB24,SWS_BILINEAR,nullptr,nullptr,nullptr);
            if(!sws)throw std::runtime_error("sws allocation failed");
            const int* coeff=sws_getCoefficients(frame->colorspace==AVCOL_SPC_BT709?SWS_CS_ITU709:SWS_CS_ITU601);
            sws_setColorspaceDetails(sws,coeff,frame->color_range==AVCOL_RANGE_JPEG,coeff,1,0,1<<16,1<<16);
            uint8_t* dest[]={img.bits(),nullptr,nullptr,nullptr};int strides[]={img.bytesPerLine(),0,0,0};
            sws_scale(sws,frame->data,frame->linesize,0,o.height,dest,strides);image(img,0,stamp);av_frame_unref(frame);
        }
    }
    void packet(uint8_t* p,int n,long long stamp){
        std::vector<uint8_t> recovered;
        if(n && waitingIdr){
            bool idr=false;
            auto prefix=[&](int i){if(i+3<=n && !p[i]&&!p[i+1]&&p[i+2]==1)return 3;
                if(i+4<=n && !p[i]&&!p[i+1]&&!p[i+2]&&p[i+3]==1)return 4;return 0;};
            for(int i=0;i<n;){int pre=prefix(i);if(!pre){++i;continue;}int end=i+pre;
                while(end<n && !prefix(end))++end;
                if(i+pre<n){int type=p[i+pre]&31;idr=idr||type==5;
                    if((type==7||type==8) && end-i<=65536){auto& parameter=type==7?sps:pps;parameter.assign(p+i,p+end);}}
                i=end;
            }
            if(!idr || sps.empty() || pps.empty()){++recoverySkippedPackets;return;}
            recovered.reserve(sps.size()+pps.size()+n);recovered.insert(recovered.end(),sps.begin(),sps.end());recovered.insert(recovered.end(),pps.begin(),pps.end());recovered.insert(recovered.end(),p,p+n);
            p=recovered.data();n=static_cast<int>(recovered.size());waitingIdr=false;
        }
        AVPacket pkt={};pkt.data=p;pkt.size=n;
        int ret=avcodec_send_packet(decoder,n?&pkt:nullptr);
        if(ret==AVERROR(EAGAIN)){receiveFrames(stamp);ret=avcodec_send_packet(decoder,n?&pkt:nullptr);}
        if(ret<0 && ret!=AVERROR_EOF){++decodeErrors;error("decoder packet rejected after IDR recovery");return;}receiveFrames(stamp);
    }
    void flush(){if(!parser)return;uint8_t* p=nullptr;int n=0;
        av_parser_parse2(parser,decoder,&p,&n,nullptr,0,AV_NOPTS_VALUE,AV_NOPTS_VALUE,0);if(n)packet(p,n,lastReceive);packet(nullptr,0,lastReceive);}
    void run(){
        try {
            wire.open(o.out+(o.codec=="h264"?"/received.h264":o.pixels=="gray8"?"/received.gray8":"/received.rgb24"),std::ios::binary);
            audit.open(o.out+"/receive.jsonl");if(!wire || !audit)throw std::runtime_error("output open failed");
            if(o.codec=="h264")initDecoder();
            for(;;){Sample sample;
                {std::unique_lock<std::mutex> lock(mutex);cv.wait(lock,[&]{return stopping || !queue.empty();});
                    if(queue.empty() && stopping)break;sample=std::move(queue.front());queue.pop_front();queued-=sample.data.size();}
                if(o.slowMs>0)std::this_thread::sleep_for(std::chrono::milliseconds(o.slowMs));
                const auto begin=ns();hash.update(sample.data.data(),sample.data.size());wire.write(reinterpret_cast<const char*>(sample.data.data()),sample.data.size());++processed;
                CustomerAvSha256 itemHash;itemHash.update(sample.data.data(),sample.data.size());
                audit<<"{\"kind\":\"sample\",\"index\":"<<processed<<",\"frameId\":"<<sample.id<<",\"bytes\":"<<sample.data.size()
                     <<",\"receivedNs\":"<<sample.received<<",\"workerNs\":"<<begin<<",\"sha256\":\""<<itemHash.finishHex()<<"\"}\n";
                if(o.codec=="raw"){
                    QImage img(sample.data.data(),o.width,o.height,o.width*(o.pixels=="gray8"?1:3),o.pixels=="gray8"?QImage::Format_Grayscale8:QImage::Format_RGB888);
                    image(img,sample.id,sample.received);
                } else {
                    // A reliable stream may pause inside a NAL without losing bytes.
                    // Preserve parser/reference state across timing gaps. Reopening
                    // this receiver creates a fresh SPS/PPS+IDR recovery context;
                    // actual parser/decoder errors fail explicitly below.
                    const auto size=sample.data.size();sample.data.resize(size+AV_INPUT_BUFFER_PADDING_SIZE,0);
                    auto* p=sample.data.data();int remaining=static_cast<int>(size);
                    while(remaining>0){uint8_t* output=nullptr;int length=0;
                        int used=av_parser_parse2(parser,decoder,&output,&length,p,remaining,AV_NOPTS_VALUE,AV_NOPTS_VALUE,0);
                        if(used<0 || (!used && !length))throw std::runtime_error("H264 parser made no progress");
                        if(length)packet(output,length,sample.received);p+=used;remaining-=used;
                    }
                }
                if(!wire || !audit)throw std::runtime_error("output storage failed");
            }
            flush();streamHash=hash.finishHex();wire.close();audit.close();
            if(!lastImage.isNull() && !lastImage.save(QString::fromStdString(o.out+"/last.png")))error("last PNG write failed");
        } catch(const std::exception& e){error(e.what());}
        closeDecoder();
    }
    void start(){worker=std::thread(&Receiver::run,this);}
    void stop(){ {std::lock_guard<std::mutex> lock(mutex);accepting=false;stopping=true;cv.notify_all();}if(worker.joinable())worker.join();}
    bool saveMedia(){
        QStringList args;args<<"-y"<<"-v"<<"error";
        if(o.codec=="raw")args<<"-f"<<"rawvideo"<<"-pixel_format"<<QString::fromStdString(o.pixels=="gray8"?"gray":"rgb24")<<"-video_size"<<QString("%1x%2").arg(o.width).arg(o.height);
        else args<<"-f"<<"h264";
        args<<"-r"<<QString::number(o.fps)<<"-i"<<QString::fromStdString(o.out+(o.codec=="h264"?"/received.h264":o.pixels=="gray8"?"/received.gray8":"/received.rgb24"));
        if(o.codec=="h264")args<<"-c:v"<<"copy";else args<<"-c:v"<<"libx264"<<"-crf"<<"0"<<"-preset"<<"fast";
        args<<"-movflags"<<"+faststart"<<QString::fromStdString(o.out+"/output.mp4");
        QProcess ff;ff.start(QString::fromStdString(o.ffmpeg),args);
        if(!ff.waitForStarted(5000) || !ff.waitForFinished(180000) || ff.exitCode()!=0){std::cerr<<ff.readAllStandardError().constData();error("MP4 conversion/remux failed");return false;}
        // Saved frame count is independently verified with ffprobe by the harness.
        return true;
    }
    void summary(bool media){QJsonObject j;
        j["topic"]=QString::fromStdString(o.topic);j["codec"]=QString::fromStdString(o.codec);j["pixelFormat"]=QString::fromStdString(o.pixels);
        j["width"]=o.width;j["height"]=o.height;j["fpsConfigured"]=o.fps;j["receivedSamples"]=double(received);j["receivedBytes"]=double(receivedBytes);
        j["processedSamples"]=double(processed);j["decodedFrames"]=double(decoded);j["displayUpdates"]=double(shown);j["invalidSamples"]=double(invalid);
        j["rejectedSamples"]=double(rejected);j["decodeErrors"]=double(decodeErrors);j["decoderResets"]=double(resets);j["maxQueueBytes"]=double(maxQueued);
        j["recoverySkippedPackets"]=double(recoverySkippedPackets);j["callbackFaultInjectionMs"]=o.callbackBlockMs;
        j["firstReceiveNs"]=double(firstReceive);j["lastReceiveNs"]=double(lastReceive);j["lastDecodedNs"]=double(lastDecoded);
        j["streamSha256"]=QString::fromStdString(streamHash);j["failed"]=bool(failed);j["mp4Created"]=media;
        j["savedFrameCount"]=QString("NOT_VERIFIED_until_ffprobe");j["screenPresentationLatency"]=QString("NOT_VERIFIED");
        QFile f(QString::fromStdString(o.out+"/summary.json"));if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument(j).toJson());
        std::cout<<QJsonDocument(j).toJson(QJsonDocument::Compact).constData()<<std::endl;
    }
};
template<class Sample,class Seq,class Reader>struct Listener:DDS::SimpleDataReaderListener<Sample,Seq,Reader>{
    Receiver& receiver;explicit Listener(Receiver& r):receiver(r){}
    void on_process_sample(DDS::DataReader*,const Sample& s,const DDS::SampleInfo&)override{
        // Explicit fault-injection only. Normal callbacks never wait/decode/write files.
        if(receiver.o.callbackBlockMs)std::this_thread::sleep_for(std::chrono::milliseconds(receiver.o.callbackBlockMs));
        copy(s);
    }
    void copy(const HD264Stream& s){receiver.push(s.VedioStream,s.DataLength,0);}
    void copy(const LD264Stream& s){receiver.push(s.VedioStream,s.DataLength,0);}
    void copy(const HDDecoder& s){receiver.push(s.stream.get_contiguous_buffer(),s.stream.length(),s.frameId);}
    void copy(const LDDecoder& s){receiver.push(s.stream.get_contiguous_buffer(),s.stream.length(),s.frameId);}
};
int main(int argc,char** argv){
    QApplication app(argc,argv);
    try {
        Options o=parse(argc,argv);if(!QDir().mkpath(QString::fromStdString(o.out)))throw std::runtime_error("cannot create output directory");
        if(QFile::exists(QString::fromStdString(o.out+"/summary.json")))throw std::runtime_error("output already contains evidence; choose a new directory");
        Receiver receiver(o);receiver.start();
        Listener<HD264Stream,HD264StreamSeq,HD264StreamDataReader> hd(receiver);
        Listener<LD264Stream,LD264StreamSeq,LD264StreamDataReader> ld(receiver);
        Listener<HDDecoder,HDDecoderSeq,HDDecoderDataReader> hr(receiver);
        Listener<LDDecoder,LDDecoderSeq,LDDecoderDataReader> lr(receiver);
        auto* factory=DDS::DDSIF::Init(o.qos.c_str(),"hwasimir_factory");
        auto* participant=factory?DDS::DDSIF::CreateDP(o.domain,"hwasimir_tcp"):nullptr;
        DDS::DataReader* reader=nullptr;
        if(participant){
            if(o.topic=="TX/HD264")reader=DDS::DDSIF::SubTopic(participant,o.topic.c_str(),HD264StreamTypeSupport::get_instance(),"customer_tx_h264_reader",&hd);
            if(o.topic=="TX/LD264")reader=DDS::DDSIF::SubTopic(participant,o.topic.c_str(),LD264StreamTypeSupport::get_instance(),"customer_tx_h264_reader",&ld);
            if(o.topic=="TX/HDecode")reader=DDS::DDSIF::SubTopic(participant,o.topic.c_str(),HDDecoderTypeSupport::get_instance(),"customer_tx_raw_reader",&hr);
            if(o.topic=="TX/LDecode")reader=DDS::DDSIF::SubTopic(participant,o.topic.c_str(),LDDecoderTypeSupport::get_instance(),"customer_tx_raw_reader",&lr);
        }
        if(!reader){receiver.stop();if(factory)DDS::DDSIF::Finalize();throw std::runtime_error("typed customer subscription failed");}
        std::cout<<"[CustomerRxReady] topic="<<o.topic<<" domain="<<o.domain<<" width="<<o.width<<" height="<<o.height<<" pixels="<<o.pixels<<" direction=top_down oldTopics=0"<<std::endl;
        QLabel label;label.setWindowTitle(QString::fromStdString("Customer TX | "+o.topic));label.resize(o.width,o.height);
        label.move(o.topic=="TX/LDecode" || o.topic=="TX/LD264"?20:390,40);if(o.display)label.show();
        const auto started=ns();QTimer timer;std::uint64_t shownId=0;
        QObject::connect(&timer,&QTimer::timeout,[&]{
            if(o.display){std::lock_guard<std::mutex> lock(receiver.imageMutex);if(receiver.latestId!=shownId){label.setPixmap(QPixmap::fromImage(receiver.latest));shownId=receiver.latestId;++receiver.shown;}}
            const auto now=ns(),last=receiver.lastReceive.load();
            if(receiver.failed || now-started>o.timeoutMs*1000000LL || (last && now-last>o.idleMs*1000000LL))app.quit();
        });timer.setTimerType(Qt::PreciseTimer);timer.start(1);app.exec();timer.stop();
        DDS::DDSIF::UnSubTopic(reader);receiver.stop();DDS::DDSIF::Finalize();
        const bool media=receiver.decoded>0 && receiver.saveMedia();receiver.summary(media);
        return receiver.failed || !media?2:0;
    } catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
}
