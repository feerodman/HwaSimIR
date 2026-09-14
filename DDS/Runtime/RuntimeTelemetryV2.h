#pragma once
#include "../Protocol/FrameProductV2.h"
#include <chrono>
#include <deque>
#include <mutex>
#include <algorithm>
#include <limits>

namespace HwaTelemetryV2 {
inline std::int64_t nowNs(){return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct Message {
    int version=2,kind=0; // 1 request, 2 clock reply, 3 metrics
    std::string server,client;
    std::uint64_t sequence=0,generation=0,accepted=0,executed=0;
    std::uint64_t run=0,outputSeq=0;
    bool finished=false;
    std::int64_t t1=0,t2=0,t3=0,sampleNs=0;
    double acceptedHz=0,executedHz=0,queueWaitMs=0;
};
template<class IO>inline void fields(IO& io,Message& m){
    io.value(m.version);io.value(m.kind);io.value(m.server);io.value(m.client);
    io.value(m.sequence);io.value(m.generation);io.value(m.accepted);io.value(m.executed);
    io.value(m.run);io.value(m.outputSeq);io.value(m.finished);
    io.value(m.t1);io.value(m.t2);io.value(m.t3);io.value(m.sampleNs);
    io.value(m.acceptedHz);io.value(m.executedHz);io.value(m.queueWaitMs);
}
inline std::vector<std::uint8_t> encode(Message m){HwaFrameV2::Writer w;fields(w,m);return w.bytes;}
inline bool decode(const char* p,std::size_t n,Message& m){try{HwaFrameV2::Reader r(reinterpret_cast<const std::uint8_t*>(p),n);fields(r,m);return r.pos==n&&m.version==2&&m.kind>=1&&m.kind<=3;}catch(const std::exception&){return false;}}
class Counters {
    std::mutex mutex;std::deque<std::int64_t> arrivals,starts;
    std::uint64_t accepted=0,executed=0,generation=0;double wait=0;
    std::uint64_t run=0,outputSeq=0;bool finished=false;
    static void trim(std::deque<std::int64_t>& q,std::int64_t now){while(!q.empty()&&q.front()<=now-1000000000LL)q.pop_front();}
public:
    void reset(){std::lock_guard<std::mutex> lock(mutex);arrivals.clear();starts.clear();accepted=executed=0;wait=0;outputSeq=0;finished=false;++generation;}
    void output(std::uint64_t g,std::uint64_t r,std::uint64_t seq){std::lock_guard<std::mutex> lock(mutex);generation=g;run=r;outputSeq=seq;finished=false;}
    void finish(){std::lock_guard<std::mutex> lock(mutex);finished=true;}
    void accept(std::int64_t now){std::lock_guard<std::mutex> lock(mutex);trim(arrivals,now);arrivals.push_back(now);++accepted;}
    void execute(std::int64_t now,std::int64_t acceptedNs){std::lock_guard<std::mutex> lock(mutex);trim(starts,now);starts.push_back(now);++executed;wait=(now-acceptedNs)/1.e6;}
    Message snapshot(){std::lock_guard<std::mutex> lock(mutex);auto now=nowNs();trim(arrivals,now);trim(starts,now);Message m;m.kind=3;m.sampleNs=now;m.generation=generation;m.accepted=accepted;m.executed=executed;m.acceptedHz=double(arrivals.size());m.executedHz=double(starts.size());m.queueWaitMs=starts.empty()?0:wait;m.run=run;m.outputSeq=outputSeq;m.finished=finished;return m;}
};
// Four monotonic timestamps estimate server minus client offset. The chosen
// minimum RTT sample is retained at most 15 s; a 200 ppm drift allowance grows
// its uncertainty. This is an interval estimate, not an OS clock correction.
class ClockEstimate {
    struct Sample{std::int64_t at;double offset,rtt;};
    std::deque<Sample> samples;std::string server;
public:
    double offsetNs=0,uncertaintyMs=0,rttMs=0;std::int64_t lastReplyNs=0;
    void reset(){samples.clear();server.clear();lastReplyNs=0;}
    bool observe(const Message& reply,std::int64_t t4){
        if(reply.kind!=2||reply.t3<reply.t2||t4<reply.t1)return false;
        const double rtt=double(t4-reply.t1)-double(reply.t3-reply.t2);
        if(rtt<0||rtt>100000000.0)return false;
        if(server!=reply.server){reset();server=reply.server;}
        const double offset=(double(reply.t2-reply.t1)+double(reply.t3-t4))*.5;
        samples.push_back({t4,offset,rtt});lastReplyNs=t4;
        return valid(t4);
    }
    bool valid(std::int64_t now){
        while(!samples.empty()&&now-samples.front().at>15000000000LL)samples.pop_front();
        if(samples.size()<3||now-lastReplyNs>3500000000LL)return false;
        auto best=std::min_element(samples.begin(),samples.end(),[](const Sample& a,const Sample& b){return a.rtt<b.rtt;});
        offsetNs=best->offset;rttMs=best->rtt/1.e6;
        uncertaintyMs=(best->rtt*.5+double(now-best->at)*.0002)/1.e6+.1;
        // Reject mutually inconsistent intervals (clock reset / excessive drift).
        for(const auto& s:samples)if(std::abs(s.offset-offsetNs)>(s.rtt+best->rtt)*.5+std::abs(double(s.at-best->at))*.0002+200000.0)return false;
        return uncertaintyMs<=10.0;
    }
};
}
