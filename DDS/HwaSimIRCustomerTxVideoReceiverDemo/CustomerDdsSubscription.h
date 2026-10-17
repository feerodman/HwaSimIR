#pragma once
template<class Sample,class Seq,class Reader>struct Listener:DDS::SimpleDataReaderListener<Sample,Seq,Reader>{
    Receiver& receiver;explicit Listener(Receiver& r):receiver(r){}
    void on_process_sample(DDS::DataReader*,const Sample& s,const DDS::SampleInfo&)override{
        // SDK 内部 take/valid_data/return_loan 成对处理；这里只复制序列，不再次 take。
        // callbackBlockMs 仅用于明确开启的故障测试，生产默认 0。
        if(receiver.o.callbackBlockMs)std::this_thread::sleep_for(std::chrono::milliseconds(receiver.o.callbackBlockMs));
        copy(s);
    }
    void on_sample_rejected(DDS::DataReader*,const DDS::SampleRejectedStatus& s)override {
        std::cerr<<"[CustomerRxSampleRejected] total="<<s.total_count<<" reason="<<int(s.last_reason)<<std::endl;
    }
    void on_sample_lost(DDS::DataReader*,const DDS::SampleLostStatus& s)override {
        std::cerr<<"[CustomerRxSampleLost] total="<<s.total_count<<std::endl;
    }
    void on_requested_incompatible_qos(DDS::DataReader*,const DDS::RequestedIncompatibleQosStatus& s)override {
        std::cerr<<"[CustomerRxIncompatibleQos] total="<<s.total_count<<" policy="<<s.last_policy_id<<std::endl;
    }
    void copy(const HD264Stream& s){receiver.push(s.VedioStream,s.DataLength,0);}
    void copy(const LD264Stream& s){receiver.push(s.VedioStream,s.DataLength,0);}
    void copy(const HDDecoder& s){receiver.push(s.stream.get_contiguous_buffer(),s.stream.length(),s.frameId);}
    void copy(const LDDecoder& s){receiver.push(s.stream.get_contiguous_buffer(),s.stream.length(),s.frameId);}
};
