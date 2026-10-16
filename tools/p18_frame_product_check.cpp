#include "FrameProductV2.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
int main(int argc,char** argv){
    if(argc!=2)return 1;
    std::ifstream file(argv[1],std::ios::binary);if(!file)return 2;
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),{});
    std::uint64_t count=0,annotations=0,realtime=0,stamped=0,first=0,last=0;bool continuous=true;std::set<std::string> identities;
    for(std::size_t i=0;i<bytes.size();){const auto prefix=HwaFrameV2::startCode(bytes.data(),bytes.size(),i);if(!prefix){++i;continue;}
        auto end=i+prefix+1;while(end<bytes.size()&&!HwaFrameV2::startCode(bytes.data(),bytes.size(),end))++end;
        HwaFrameV2::Product p;
        if(HwaFrameV2::extract(bytes.data()+i,end-i,p)){
            if(!count)first=p.frameSeq;else continuous=continuous && p.frameSeq==last+1;
            ++count;last=p.frameSeq;annotations+=p.annotationEnabled && !p.annotation.empty();realtime+=p.realtime.flag==0x38;
            stamped+=p.writerSubmitNs>0 && p.writerSubmitNs>=p.encodeNs;
            identities.insert(std::to_string(p.platID)+"/"+std::to_string(p.sensorID)+"/"+p.channel);
        }i=end;
    }
    std::cout<<"{\"privateProducts\":"<<count<<",\"annotations\":"<<annotations<<",\"realtime\":"<<realtime<<",\"writerStamped\":"<<stamped
             <<",\"firstFrame\":"<<first<<",\"lastFrame\":"<<last<<",\"continuous\":"<<(continuous?"true":"false")<<",\"identities\":[";
    bool comma=false;for(const auto& id:identities){if(comma)std::cout<<",";comma=true;std::cout<<"\""<<id<<"\"";}std::cout<<"]}"<<std::endl;return 0;
}
