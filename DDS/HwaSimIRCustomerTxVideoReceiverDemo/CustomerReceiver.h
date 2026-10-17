#pragma once
#include "AnnexBEndScanner.h"
struct Receiver {
    struct Sample {std::vector<std::uint8_t> data;int id=0;long long received=0;};
    Options o;std::mutex mutex,imageMutex;std::condition_variable cv;std::deque<Sample> queue;
    std::size_t queued=0,maxQueued=0;bool accepting=true,stopping=false;std::thread worker;
    std::atomic<bool> failed{false};std::atomic<long long> lastReceive{0};
    std::atomic<unsigned long long> received{0},receivedBytes{0},invalid{0},rejected{0},decoded{0},shown{0};
    unsigned long long processed=0,saved=0,decodeErrors=0,resets=0,recoverySkippedPackets=0;long long firstReceive=0,lastDecoded=0;
    bool waitingIdr=true;std::vector<uint8_t> sps,pps;
    CustomerTX::AnnexBEndScanner endScanner;
    std::atomic<unsigned> completedEnds{0};
    bool streamActive=false,uncertainEnd=false,mediaCreated=false;
    long long lastParserOutput=0,endReceivedNs=0,drainCompleteNs=0,mp4CompleteNs=0;
    QImage latest,lastImage;std::uint64_t latestId=0;std::ofstream wire,audit,roundWire;
    std::string roundPath;
    CustomerAvSha256 hash;std::string streamHash;
    AVCodecParserContext* parser=nullptr;AVCodecContext* decoder=nullptr;AVFrame* frame=nullptr;SwsContext* sws=nullptr;
    explicit Receiver(const Options& options):o(options){}
    ~Receiver(){stop();}
    void error(const std::string& reason){failed=true;std::cerr<<"[CustomerRxError] "<<reason<<std::endl;}
    void push(const void* data,std::size_t bytes,int id) {
        const auto stamp=ns();std::size_t expected=static_cast<std::size_t>(o.width)*o.height*(o.pixels=="gray8"?1:3);
        if(!data || (o.codec=="h264"?!CustomerTX::validLength(static_cast<std::uint32_t>(bytes)):(bytes!=expected || bytes>640000 || id<=0))) {++invalid;failed=true;return;}
        std::lock_guard<std::mutex> lock(mutex);
        if(!accepting || failed || queued+bytes>o.queueBytes || queue.size()>=8192){++rejected;failed=true;accepting=false;cv.notify_all();return;}
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
        if(decoded==1 && !o.countOnly && !img.save(QString::fromStdString(o.out+"/first.png")))error("first PNG write failed");
        if(o.audit) {
        CustomerAvSha256 pixelHash;for(int y=0;y<img.height();++y)pixelHash.update(img.constScanLine(y),static_cast<std::size_t>(img.width())*(img.format()==QImage::Format_Grayscale8?1:3));
        audit<<"{\"kind\":\"frame\",\"index\":"<<decoded<<",\"frameId\":"<<rawId<<",\"receivedNs\":"<<receivedNs<<",\"decodedNs\":"<<lastDecoded
             <<",\"pixelSha256\":\""<<pixelHash.finishHex()<<"\"}\n";
        }
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
        if(n) {
            lastParserOutput=ns();
            if(o.audit)audit<<"{\"kind\":\"parser_output\",\"bytes\":"<<n<<",\"receivedNs\":"<<stamp
                <<",\"parserOutputNs\":"<<lastParserOutput<<"}\n";
        }
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
    void flush(long long stamp){if(!parser)return;uint8_t* p=nullptr;int n=0;
        av_parser_parse2(parser,decoder,&p,&n,nullptr,0,AV_NOPTS_VALUE,AV_NOPTS_VALUE,0);if(n)packet(p,n,stamp);packet(nullptr,0,stamp);}
    void parseBytes(const std::uint8_t* data,std::size_t size,long long stamp) {
        if(!size)return;
        if(!streamActive && !o.countOnly) {
            roundPath=o.out+"/round_"+std::to_string(completedEnds.load()+1)+".h264";
            roundWire.open(roundPath,std::ios::binary|std::ios::trunc);
            if(!roundWire)throw std::runtime_error("round stream open failed");
        }
        streamActive=true;
        if(!o.countOnly) {
            roundWire.write(reinterpret_cast<const char*>(data),size);
            if(!roundWire)throw std::runtime_error("round stream write failed");
        }
        // FFmpeg requires zero padding beyond input. Parser owns any pending AU.
        std::vector<std::uint8_t> padded(data,data+size);
        padded.resize(size+AV_INPUT_BUFFER_PADDING_SIZE,0);
        auto* p=padded.data();int remaining=static_cast<int>(size);
        while(remaining>0) {
            uint8_t* output=nullptr;int length=0;
            const int used=av_parser_parse2(parser,decoder,&output,&length,p,remaining,AV_NOPTS_VALUE,AV_NOPTS_VALUE,0);
            if(used<0 || (!used && !length))throw std::runtime_error("H264 parser made no progress");
            if(length)packet(output,length,stamp);
            p+=used;remaining-=used;
        }
    }
    void finishStream(int nalType,long long stamp) {
        if(!streamActive)return; // EOS after end_of_sequence is not a second round/frame.
        endReceivedNs=stamp;
        flush(stamp);drainCompleteNs=ns();streamActive=false;
        // An explicit standard end establishes a new decoding boundary. A pause does not.
        closeDecoder();initDecoder();++resets;
        wire.flush();
        if(!o.countOnly) {
            const char end[]={0,0,0,1,char(nalType)};
            roundWire.write(end,sizeof(end));roundWire.close();
            if(decoded) {
                const std::string destination=o.out+"/round_"+std::to_string(completedEnds.load()+1)+".mp4";
                mediaCreated=saveMedia(roundPath,destination);
                if(completedEnds==0 && mediaCreated) {
                    QFile::remove(QString::fromStdString(o.out+"/output.mp4"));
                    if(!QFile::copy(QString::fromStdString(destination),QString::fromStdString(o.out+"/output.mp4")))error("MP4 result copy failed");
                }
            }
        }
        audit<<"{\"kind\":\"standard_end\",\"nalType\":"<<nalType<<",\"receivedNs\":"<<endReceivedNs
             <<",\"parserOutputNs\":"<<lastParserOutput<<",\"lastDecodedNs\":"<<lastDecoded
             <<",\"drainCompleteNs\":"<<drainCompleteNs<<",\"mp4CompleteNs\":"<<mp4CompleteNs
             <<",\"decodedFrames\":"<<decoded<<"}\n";
        audit.flush();
        ++completedEnds;
        std::cout<<"[CustomerRxEnd] nalType="<<nalType<<" endReceivedNs="<<endReceivedNs
                 <<" lastParserOutputNs="<<lastParserOutput<<" lastDecodedNs="<<lastDecoded
                 <<" drainCompleteNs="<<drainCompleteNs<<" mp4CompleteNs="<<mp4CompleteNs
                 <<" decodedFrames="<<decoded<<" completedEnds="<<completedEnds<<std::endl;
    }
    void run(){
        try {
            wire.open(o.out+(o.codec=="h264"?"/received.h264":o.pixels=="gray8"?"/received.gray8":"/received.rgb24"),std::ios::binary);
            audit.open(o.out+"/receive.jsonl");if(!wire || !audit)throw std::runtime_error("output open failed");
            if(o.codec=="h264")initDecoder();
            for(;;){Sample sample;
                {std::unique_lock<std::mutex> lock(mutex);cv.wait(lock,[&]{return stopping || !queue.empty();});
                    if(queue.empty() && stopping)break;sample=std::move(queue.front());queue.pop_front();queued-=sample.data.size();}
                if(o.slowMs>0)std::this_thread::sleep_for(std::chrono::milliseconds(o.slowMs));
                const auto begin=ns();
                if(o.audit)hash.update(sample.data.data(),sample.data.size());
                if(!o.countOnly)wire.write(reinterpret_cast<const char*>(sample.data.data()),sample.data.size());
                ++processed;
                if(o.audit) {
                CustomerAvSha256 itemHash;itemHash.update(sample.data.data(),sample.data.size());
                audit<<"{\"kind\":\"sample\",\"index\":"<<processed<<",\"frameId\":"<<sample.id<<",\"bytes\":"<<sample.data.size()
                     <<",\"receivedNs\":"<<sample.received<<",\"workerNs\":"<<begin<<",\"sha256\":\""<<itemHash.finishHex()<<"\"}\n";
                }
                if(o.codec=="raw"){
                    if(o.countOnly){++decoded;lastDecoded=ns();continue;}
                    QImage img(sample.data.data(),o.width,o.height,o.width*(o.pixels=="gray8"?1:3),o.pixels=="gray8"?QImage::Format_Grayscale8:QImage::Format_RGB888);
                    image(img,sample.id,sample.received);
                } else {
                    // A reliable stream may pause inside a NAL without losing bytes.
                    // Preserve parser/reference state across timing gaps. Reopening
                    // this receiver creates a fresh SPS/PPS+IDR recovery context;
                    // actual parser/decoder errors fail explicitly below.
                    endScanner.push(sample.data.data(),sample.data.size(),
                        [&](const std::uint8_t* p,std::size_t n){parseBytes(p,n,sample.received);},
                        [&](int type){finishStream(type,sample.received);});
                }
                if(!wire || !audit)throw std::runtime_error("output storage failed");
            }
            // Silence/forced shutdown cannot prove the final NAL complete. Do not
            // feed a truncated picture to flush and count a concealed image as success.
            if(o.codec=="h264" && (streamActive || endScanner.pending())) {
                uncertainEnd=true;
                audit<<"{\"kind\":\"incomplete_or_unterminated_stream\",\"forcedFlush\":false}\n";
            }
            streamHash=o.audit?hash.finishHex():"DISABLED";wire.close();audit.close();
            if(!o.countOnly && !lastImage.isNull() && !lastImage.save(QString::fromStdString(o.out+"/last.png")))error("last PNG write failed");
        } catch(const std::exception& e){error(e.what());}
        closeDecoder();
    }
    void start(){worker=std::thread(&Receiver::run,this);}
    void stop(){ {std::lock_guard<std::mutex> lock(mutex);accepting=false;stopping=true;cv.notify_all();}if(worker.joinable())worker.join();}
    bool saveMedia(const std::string& input=std::string(), const std::string& destination=std::string()){
        QStringList args;args<<"-y"<<"-v"<<"error";
        if(o.codec=="raw")args<<"-f"<<"rawvideo"<<"-pixel_format"<<QString::fromStdString(o.pixels=="gray8"?"gray":"rgb24")<<"-video_size"<<QString("%1x%2").arg(o.width).arg(o.height);
        else args<<"-f"<<"h264";
        args<<"-r"<<QString::number(o.fps)<<"-i"<<QString::fromStdString(input.empty()?o.out+(o.codec=="h264"?"/received.h264":o.pixels=="gray8"?"/received.gray8":"/received.rgb24"):input);
        if(o.codec=="h264")args<<"-c:v"<<"copy";else args<<"-c:v"<<"libx264"<<"-crf"<<"0"<<"-preset"<<"fast";
        args<<"-movflags"<<"+faststart"<<QString::fromStdString(destination.empty()?o.out+"/output.mp4":destination);
        QProcess ff;ff.start(QString::fromStdString(o.ffmpeg),args);
        if(!ff.waitForStarted(5000) || !ff.waitForFinished(30000) || ff.exitCode()!=0){std::cerr<<ff.readAllStandardError().constData();error("MP4 conversion/remux failed");return false;}
        // Saved frame count is independently verified with ffprobe by the harness.
        mp4CompleteNs=ns();
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
        j["completedStandardEnds"]=int(completedEnds);j["unterminatedOrIncomplete"]=uncertainEnd;
        j["lastParserOutputNs"]=double(lastParserOutput);j["endReceivedNs"]=double(endReceivedNs);
        j["drainCompleteNs"]=double(drainCompleteNs);j["mp4CompleteNs"]=double(mp4CompleteNs);
        j["countOnly"]=o.countOnly;j["packetAuditEnabled"]=o.audit;
        j["savedFrameCount"]=QString("NOT_VERIFIED_until_ffprobe");j["screenPresentationLatency"]=QString("NOT_VERIFIED");
        QFile f(QString::fromStdString(o.out+"/summary.json"));if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument(j).toJson());
        std::cout<<QJsonDocument(j).toJson(QJsonDocument::Compact).constData()<<std::endl;
    }
};
