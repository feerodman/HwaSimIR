#pragma once
#include <cstdint>
#include <vector>

namespace CustomerTX {
// H.264 7.3.2.5/6: end_of_seq / end_of_stream RBSP is empty (no 0x80).
// A complete start code and valid type 10/11 header therefore identify the end.
// Retain only four bytes across arbitrary DDS boundaries, never an entire AU.
// No timestamps, short-sample rule, private marker or idle heuristic is used.
class AnnexBEndScanner {
public:
    template<class Bytes,class End>
    void push(const std::uint8_t* bytes,std::size_t size,Bytes feed,End finish) {
        std::vector<std::uint8_t> input;
        input.reserve(tail.size()+size);
        input.insert(input.end(),tail.begin(),tail.end());
        input.insert(input.end(),bytes,bytes+size);
        tail.clear();
        std::size_t start=0;
        for(std::size_t i=0;i+3<input.size();++i) {
            if(input[i] || input[i+1] || input[i+2]!=1 || (input[i+3]!=10 && input[i+3]!=11))continue;
            const std::size_t prefix=(i>start && input[i-1]==0)?i-1:i;
            if(prefix>start)feed(input.data()+start,prefix-start);
            finish(input[i+3]);
            start=i+4;i+=3;
        }
        const std::size_t left=input.size()-start;
        const std::size_t keep=left<4?left:4;
        if(left>keep)feed(input.data()+start,left-keep);
        tail.assign(input.end()-keep,input.end());
    }
    template<class Bytes>void flushTail(Bytes feed) {
        if(!tail.empty())feed(tail.data(),tail.size());
        tail.clear();
    }
    bool pending()const{return !tail.empty();}
private:
    std::vector<std::uint8_t> tail;
};
}
