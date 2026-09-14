#pragma once
// Optional compact diagnostic sidecar. No wire fields, retry or queue policy changes.
#include "FrameProductV2.h"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <algorithm>

namespace HwaInputAuditV1 {
struct Registry { std::mutex mutex; std::vector<std::ofstream*> streams; };
inline Registry& registry(){static Registry value;return value;}
inline void registerStream(std::ofstream& stream){std::lock_guard<std::mutex> lock(registry().mutex);registry().streams.push_back(&stream);}
inline void unregisterStream(std::ofstream& stream){std::lock_guard<std::mutex> lock(registry().mutex);auto& s=registry().streams;s.erase(std::remove(s.begin(),s.end(),&stream),s.end());}
inline void flushAll(){std::lock_guard<std::mutex> lock(registry().mutex);for(auto* s:registry().streams)s->flush();}
inline std::string environment(const char* name) {
#if defined(_MSC_VER)
    char* value=nullptr;std::size_t size=0;
    if(_dupenv_s(&value,&size,name)!=0)return std::string();
    const std::string result=value?value:"";std::free(value);return result;
#else
    const char* value=std::getenv(name);return value?value:"";
#endif
}
inline std::int64_t now() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
inline std::uint64_t digest(const BYHWICD::DisplayC2cObjTrackingData& input) {
    // Explicit little-endian field serialization, never native padding.
    // Constant product prefix; FNV-1a is a diagnostic checksum, not authentication.
    HwaFrameV2::Product product; product.realtime=input;
    const auto bytes=HwaFrameV2::serialize(product);
    std::uint64_t hash=14695981039346656037ULL;
    for(auto byte:bytes){hash^=byte;hash*=1099511628211ULL;}
    return hash;
}
class Ledger {
    char buffer[65536]; std::ofstream stream; std::uint64_t ordinal=0;
public:
    explicit Ledger(const char* role) {
        const std::string directory=environment("HwaInputAuditDirectory");
        if(directory.empty())return;
        stream.rdbuf()->pubsetbuf(buffer,sizeof(buffer));
        const std::string path=std::string(directory)+"/input_"+role+"_"+HwaFrameV2::processSession()+".csv";
        stream.open(path,std::ios::out|std::ios::app);
        if(!stream){std::cerr<<"[InputAuditV1][ERROR] cannot open "<<path<<'\n';return;}
        stream<<"ordinal,sourceSeq,beginNs,endNs,success,queueDepth,digestFNV1a64\n";
        registerStream(stream);
        std::cout<<"[InputAuditV1] role="<<role<<" path="<<path<<" canonical=FrameProductV2_constant_prefix orderedMultiplicity=1\n";
    }
    bool enabled() const {return stream.is_open();}
    ~Ledger(){if(enabled()){unregisterStream(stream);stream.flush();}}
    // One stream per existing owner thread. Buffered rows are flushed at normal exit.
    void record(const BYHWICD::DisplayC2cObjTrackingData& input,std::uint64_t seq,
                std::int64_t begin,std::int64_t end,bool success,int depth=0) {
        if(!enabled())return;
        std::lock_guard<std::mutex> lock(registry().mutex);
        stream<<++ordinal<<','<<seq<<','<<begin<<','<<end<<','<<success<<','<<depth<<','
              <<std::hex<<std::setfill('0')<<std::setw(16)<<digest(input)<<std::dec<<'\n';
        if(ordinal%120==0)stream.flush();
        if(!stream)std::cerr<<"[InputAuditV1][ERROR] sidecar write failed\n";
    }
};
}
