#pragma once
#include "InputAuditV1.h"
namespace HwaStageAuditV1 {
class Ledger {
    char buffer[65536]; std::ofstream stream;
public:
    Ledger(const char* name,const char* columns) {
        const std::string path=HwaInputAuditV1::environment("HwaInputAuditDirectory");if(path.empty())return;
        stream.rdbuf()->pubsetbuf(buffer,sizeof(buffer));
        stream.open(std::string(path)+"/stage_"+name+"_"+HwaFrameV2::processSession()+".csv",std::ios::app);
        if(stream){stream<<"sourceSeq,steadyNs,"<<columns<<'\n';HwaInputAuditV1::registerStream(stream);}
        else std::cerr<<"[StageAuditV1][ERROR] cannot open "<<name<<'\n';
    }
    ~Ledger(){if(stream.is_open()){HwaInputAuditV1::unregisterStream(stream);stream.flush();}}
    template<class... T> void record(std::uint64_t seq,T... values) {
        if(!stream.is_open())return;
        std::lock_guard<std::mutex> lock(HwaInputAuditV1::registry().mutex);
        stream<<seq<<','<<HwaInputAuditV1::now();
        using Expansion=int[];(void)Expansion{0,((void)(stream<<','<<values),0)...};
        stream<<'\n';
        if(seq%120==0)stream.flush();
    }
};
}
