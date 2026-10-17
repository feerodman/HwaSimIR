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
#include "ReceiverOptions.h"
#include "CustomerReceiver.h"
#include "CustomerDdsSubscription.h"
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
        // RAII 保证正常退出及异常路径都先停止 SDK 回调，再 join 消费线程。
        // 此对象在 listeners 之后构造，析构顺序不会让 SDK 使用已销毁的 listener。
        struct DdsSession {
            Receiver& receiver;DDS::DataReader* reader=nullptr;bool initialized=false;
            explicit DdsSession(Receiver& r):receiver(r){}
            void close(){if(reader){DDS::DDSIF::UnSubTopic(reader);reader=nullptr;}receiver.stop();if(initialized){DDS::DDSIF::Finalize();initialized=false;}}
            ~DdsSession(){close();}
        } session(receiver);
        auto* factory=DDS::DDSIF::Init(o.qos.c_str(),"hwasimir_factory");
        session.initialized=factory!=nullptr;
        auto* participant=factory?DDS::DDSIF::CreateDP(o.domain,"hwasimir_tcp"):nullptr;
        DDS::DataReader* reader=nullptr;
        if(participant){
            if(o.topic=="TX/HD264")reader=DDS::DDSIF::SubTopic(participant,o.topic.c_str(),HD264StreamTypeSupport::get_instance(),"customer_tx_h264_reader",&hd);
            if(o.topic=="TX/LD264")reader=DDS::DDSIF::SubTopic(participant,o.topic.c_str(),LD264StreamTypeSupport::get_instance(),"customer_tx_h264_reader",&ld);
            if(o.topic=="TX/HDecode")reader=DDS::DDSIF::SubTopic(participant,o.topic.c_str(),HDDecoderTypeSupport::get_instance(),"customer_tx_raw_reader",&hr);
            if(o.topic=="TX/LDecode")reader=DDS::DDSIF::SubTopic(participant,o.topic.c_str(),LDDecoderTypeSupport::get_instance(),"customer_tx_raw_reader",&lr);
        }
        session.reader=reader;
        if(!reader)throw std::runtime_error("typed customer subscription failed");
        DDS::DataReaderQos actual;
        const auto qosResult=reader->get_qos(actual);
        std::cout<<"[CustomerRxQos] api=get_qos code="<<int(qosResult);
        if(qosResult==DDS::RETCODE_OK)std::cout<<" reliability="<<int(actual.reliability.kind)<<" history="<<int(actual.history.kind)
            <<" maxSamples="<<actual.resource_limits.max_samples<<" maxPerInstance="<<actual.resource_limits.max_samples_per_instance
            <<" durability="<<int(actual.durability.kind);
        std::cout<<" path="<<o.qos<<std::endl;
        std::cout<<"[CustomerRxReady] topic="<<o.topic<<" domain="<<o.domain<<" width="<<o.width<<" height="<<o.height<<" pixels="<<o.pixels<<" direction=top_down oldTopics=0"<<std::endl;
        QLabel label;label.setWindowTitle(QString::fromStdString("Customer TX | "+o.topic));label.resize(o.width,o.height);
        label.move(o.topic=="TX/LDecode" || o.topic=="TX/LD264"?20:390,40);if(o.display)label.show();
        const auto started=ns();QTimer timer;std::uint64_t shownId=0;
        QObject::connect(&timer,&QTimer::timeout,[&]{
            if(o.display){std::lock_guard<std::mutex> lock(receiver.imageMutex);if(receiver.latestId!=shownId){label.setPixmap(QPixmap::fromImage(receiver.latest));shownId=receiver.latestId;++receiver.shown;}}
            const auto now=ns(),last=receiver.lastReceive.load();
            if(receiver.failed || (o.timeoutMs && now-started>o.timeoutMs*1000000LL) ||
               (o.idleMs && last && now-last>o.idleMs*1000000LL) ||
               (o.exitAfterEnds && receiver.completedEnds>=unsigned(o.exitAfterEnds)))app.quit();
        });timer.setTimerType(Qt::PreciseTimer);timer.start(1);app.exec();timer.stop();
        session.close();
        const bool media=(receiver.mediaCreated && receiver.completedEnds<=1) || (!o.countOnly && !receiver.uncertainEnd && receiver.decoded>0 && receiver.saveMedia());
        receiver.summary(media);
        return receiver.failed || receiver.uncertainEnd || (!o.countOnly && !media)?2:0;
    } catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
}
