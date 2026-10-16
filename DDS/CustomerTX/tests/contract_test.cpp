#include "CustomerTxWriter.h"
#include "Generated/CustomerTxVideoDataReader.h"
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <fstream>
static void require(bool v,const char* why){if(!v)throw std::runtime_error(why);}
struct Sink {
    std::mutex mutex;std::condition_variable ready;std::vector<std::vector<unsigned char>> data;std::vector<int> ids;
    bool valid=true;
    void put(const void* p,size_t n,int id){std::lock_guard<std::mutex> lock(mutex);const auto* b=static_cast<const unsigned char*>(p);data.emplace_back(b,b+n);ids.push_back(id);ready.notify_all();}
    void wait(size_t n){std::unique_lock<std::mutex> lock(mutex);require(ready.wait_for(lock,std::chrono::seconds(8),[&]{return data.size()>=n;}),"typed receive timeout");}
};
template<class T,class S,class R>struct Listen:DDS::SimpleDataReaderListener<T,S,R>{
    Sink& sink;Listen(Sink& s):sink(s){}
    void on_process_sample(DDS::DataReader*,const T& t,const DDS::SampleInfo&)override{copy(t);}
    template<class C>void compressed(const C& s){if(!CustomerTX::validLength(s.DataLength)){sink.valid=false;return;}
        for(unsigned i=s.DataLength;i<1400;++i)if(s.VedioStream[i])sink.valid=false;sink.put(s.VedioStream,s.DataLength,0);}
    void copy(const HD264Stream& s){compressed(s);}void copy(const LD264Stream& s){compressed(s);}
    void copy(const HDDecoder& s){sink.put(s.stream.get_contiguous_buffer(),s.stream.length(),s.frameId);}
    void copy(const LDDecoder& s){sink.put(s.stream.get_contiguous_buffer(),s.stream.length(),s.frameId);}
};
int main(int argc,char** argv){
    try {
        require(argc>=2,"usage: contract_test qos.xml [fixture.h264 topic [pause-ms]]");
        for(size_t size:{1u,1399u,1400u,1401u,2800u,2801u}){
            std::vector<unsigned char> source(size);for(size_t i=0;i<size;++i)source[i]=static_cast<unsigned char>(i);
            std::vector<unsigned char> assembled;unsigned count=0;
            require(CustomerTX::fragments<HD264Stream>(source.data(),source.size(),[&](const HD264Stream& s){
                require(CustomerTX::validLength(s.DataLength),"length");++count;
                assembled.insert(assembled.end(),s.VedioStream,s.VedioStream+s.DataLength);
                for(unsigned i=s.DataLength;i<1400;++i)require(s.VedioStream[i]==0,"nonzero tail");return true;
            }),"fragments");require(assembled==source && count==(size+1399)/1400,"boundary/order");
        }
        require(!CustomerTX::validLength(0)&&!CustomerTX::validLength(1401)&&!CustomerTX::validLength(0xffffffff),"invalid length acceptance");
        const unsigned char bgr[]={0,0,255,255,0,0,0,255,0,255,255,255};std::vector<unsigned char> rgb,gray;
        require(CustomerTX::convertBgr(bgr,2,2,6,true,CustomerTX::Pixels::RGB24,rgb),"RGB conversion");
        require(rgb==std::vector<unsigned char>({0,255,0,255,255,255,255,0,0,0,0,255}),"RGB order/topdown");
        require(CustomerTX::convertBgr(bgr,2,2,6,false,CustomerTX::Pixels::Gray8,gray),"gray conversion");
        require(gray==std::vector<unsigned char>({77,29,149,255}),"gray coefficients");
        std::size_t bytes=0;require(CustomerTX::imageBytes(800,800,CustomerTX::Pixels::Gray8,bytes)&&bytes==640000,"800 gray");
        require(CustomerTX::imageBytes(800,800,CustomerTX::Pixels::RGB24,bytes)&&bytes==1920000,"800 RGB");
        require(!CustomerTX::imageBytes(0,1,CustomerTX::Pixels::Gray8,bytes),"invalid geometry");
        require(DDS::DDSIF::Init(argv[1],"hwasimir_factory")!=nullptr,"DDS Init");
        auto* participant=DDS::DDSIF::CreateDP(150,"hwasimir_tcp");require(participant!=nullptr,"participant");
        std::atomic<bool> cancel{false};
        if(argc==4 && std::string(argv[2])=="--hold-raw-reader"){
            const int seconds=std::stoi(argv[3]);require(seconds>=15 && seconds<=60,"hold duration range");
            // Deliberately do not take any Samples. Unlike a blocked worker,
            // this fills the actual DDS reader history and tests writer backpressure.
            auto* reader=DDS::DDSIF::SubTopic(participant,"TX/HDecode",HDDecoderTypeSupport::get_instance(),"customer_tx_raw_reader",nullptr);
            require(reader!=nullptr,"holding reader");DDS::DataReaderQos actual;require(reader->get_qos(actual)==DDS::RETCODE_OK,"holding reader qos");
            require(actual.resource_limits.max_samples==16,"holding reader history bound");
            std::cout<<"{\"holdingReader\":\"TX/HDecode\",\"takeSamples\":false,\"maxSamples\":16,\"seconds\":"<<seconds<<"}"<<std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(seconds));
            DDS::DDSIF::UnSubTopic(reader);DDS::DDSIF::Finalize();return 0;
        }
        if(argc==4 || argc==5){
            const std::string topic=argv[3];require(topic=="TX/HD264"||topic=="TX/LD264","fixture topic");
            std::ifstream f(argv[2],std::ios::binary);std::vector<unsigned char> stream((std::istreambuf_iterator<char>(f)),{});require(!stream.empty(),"fixture missing");
            CustomerTX::Writer writer;std::string error;require(writer.open(participant,topic=="TX/HD264",true,error),error.c_str());
            std::this_thread::sleep_for(std::chrono::seconds(5));std::uint64_t samples=0,sent=0;
            // Deliberately split at 1/1399/... bytes so real start codes/NALs cross samples.
            const size_t sizes[]={1,1399,1400,1401,2800,2801};size_t offset=0,index=0;
            const int pauseMs=argc==5?std::stoi(argv[4]):0;require(pauseMs>=0 && pauseMs<=5000,"fixture pause range");bool paused=false;
            while(offset<stream.size()){const auto n=(std::min)(sizes[index++%6],stream.size()-offset);
                if(pauseMs && !paused && offset>=stream.size()/2){std::this_thread::sleep_for(std::chrono::milliseconds(pauseMs));paused=true;}
                require(writer.send(stream.data()+offset,n,1,samples,sent,cancel),"fixture write");offset+=n;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));}
            std::this_thread::sleep_for(std::chrono::seconds(3));std::cout<<"{\"fixtureSamples\":"<<samples<<",\"bytes\":"<<sent<<"}"<<std::endl;
            writer.close();DDS::DDSIF::Finalize();return 0;
        }
        Sink sinks[4];Listen<HD264Stream,HD264StreamSeq,HD264StreamDataReader> hd(sinks[0]);
        Listen<LD264Stream,LD264StreamSeq,LD264StreamDataReader> ld(sinks[1]);
        Listen<HDDecoder,HDDecoderSeq,HDDecoderDataReader> hr(sinks[2]);
        Listen<LDDecoder,LDDecoderSeq,LDDecoderDataReader> lr(sinks[3]);
        DDS::DataReader* readers[]={
            DDS::DDSIF::SubTopic(participant,"TX/HD264",HD264StreamTypeSupport::get_instance(),"customer_tx_h264_reader",&hd),
            DDS::DDSIF::SubTopic(participant,"TX/LD264",LD264StreamTypeSupport::get_instance(),"customer_tx_h264_reader",&ld),
            DDS::DDSIF::SubTopic(participant,"TX/HDecode",HDDecoderTypeSupport::get_instance(),"customer_tx_raw_reader",&hr),
            DDS::DDSIF::SubTopic(participant,"TX/LDecode",LDDecoderTypeSupport::get_instance(),"customer_tx_raw_reader",&lr)};
        CustomerTX::Writer writers[4];std::string error;
        for(int i=0;i<4;++i){
            require(readers[i]!=nullptr,"typed reader");
            DDS::DataReaderQos readerQos;require(readers[i]->get_qos(readerQos)==DDS::RETCODE_OK,"reader get_qos");
            DDS::TypeSupport* support=i==0?static_cast<DDS::TypeSupport*>(HD264StreamTypeSupport::get_instance()):
                i==1?static_cast<DDS::TypeSupport*>(LD264StreamTypeSupport::get_instance()):
                i==2?static_cast<DDS::TypeSupport*>(HDDecoderTypeSupport::get_instance()):LDDecoderTypeSupport::get_instance();
            auto* probe=DDS::DDSIF::PubTopic(participant,CustomerTX::topic(i%2==0,i<2),support,i<2?"customer_tx_h264_writer":"customer_tx_raw_writer",nullptr);
            require(probe!=nullptr,"QoS probe writer");DDS::DataWriterQos writerQos;
            require(probe->get_qos(writerQos)==DDS::RETCODE_OK,"writer get_qos");
            const int capacity=i<2?4096:16;
            require(readerQos.resource_limits.max_samples==capacity && readerQos.resource_limits.max_samples_per_instance==capacity,"actual reader resource limit");
            require(writerQos.resource_limits.max_samples==capacity && writerQos.resource_limits.max_samples_per_instance==capacity,"actual writer resource limit");
            std::cout<<"{\"topic\":\""<<CustomerTX::topic(i%2==0,i<2)<<"\",\"actualReaderMaxSamples\":"<<readerQos.resource_limits.max_samples
                     <<",\"actualWriterMaxSamples\":"<<writerQos.resource_limits.max_samples<<",\"actualWriteTimeoutNs\":"<<writerQos.reliability.max_blocking_time.nanosec<<"}"<<std::endl;
            DDS::DDSIF::UnPubTopic(probe);
            require(writers[i].open(participant,i%2==0,i<2,error),error.c_str());
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
        for(int i=0;i<4;++i){std::uint64_t samples=0,sent=0;std::vector<unsigned char> expected;
            for(size_t n: (i<2?std::vector<size_t>{1,1399,1400,1401,2800,2801}:std::vector<size_t>{102400,307200,640000})){
                std::vector<unsigned char> source(n);for(size_t j=0;j<n;++j)source[j]=static_cast<unsigned char>(j);
                const int id=static_cast<int>(sinks[i].ids.size()+1);require(writers[i].send(source.data(),n,id,samples,sent,cancel),"typed send");
                expected.insert(expected.end(),source.begin(),source.end());sinks[i].wait(samples);
            }
            std::vector<unsigned char> received;for(const auto& v:sinks[i].data)received.insert(received.end(),v.begin(),v.end());
            require(received==expected && sinks[i].valid,"typed roundtrip mismatch or tail");
            std::cout<<"{\"topic\":\""<<CustomerTX::topic(i%2==0,i<2)<<"\",\"type\":\""<<CustomerTX::type(i%2==0,i<2)
                     <<"\",\"samples\":"<<samples<<",\"bytes\":"<<sent<<",\"result\":\"PASS\"}"<<std::endl;
        }
        for(int i=0;i<4;++i){writers[i].close();DDS::DDSIF::UnSubTopic(readers[i]);}DDS::DDSIF::Finalize();
        std::cout<<"{\"unitAndTypedRoundtrip\":\"PASS\",\"globalInitCount\":1}"<<std::endl;return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<std::endl;return 1;}
}
