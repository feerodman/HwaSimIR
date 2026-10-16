#include "Sha256File.h"
#include <QCryptographicHash>
#include <chrono>
#include <iostream>
#include <vector>
extern "C" {
#include <libavutil/sha.h>
#include <libavutil/mem.h>
}
int main(){
    std::vector<unsigned char> bytes(800*800*3);for(size_t i=0;i<bytes.size();++i)bytes[i]=i;
    std::string reference;const auto start=std::chrono::steady_clock::now();
    for(int i=0;i<32;++i){HwaHash::Sha256 hash;hash.update(bytes.data(),bytes.size());reference=hash.finishHex();}
    const auto middle=std::chrono::steady_clock::now();std::string actual;
    for(int i=0;i<32;++i){QCryptographicHash hash(QCryptographicHash::Sha256);hash.addData(reinterpret_cast<const char*>(bytes.data()),bytes.size());actual=hash.result().toHex().toStdString();}
    const auto qtEnd=std::chrono::steady_clock::now();std::string avDigest;
    for(int i=0;i<32;++i){auto* hash=av_sha_alloc();av_sha_init(hash,256);av_sha_update(hash,bytes.data(),bytes.size());unsigned char digest[32];av_sha_final(hash,digest);av_free(hash);avDigest=QByteArray(reinterpret_cast<const char*>(digest),32).toHex().toStdString();}
    const auto end=std::chrono::steady_clock::now();
    std::cout<<"{\"bytesPerImage\":"<<bytes.size()<<",\"portableMsPerImage\":"<<std::chrono::duration<double,std::milli>(middle-start).count()/32
        <<",\"qtMsPerImage\":"<<std::chrono::duration<double,std::milli>(qtEnd-middle).count()/32<<",\"avMsPerImage\":"<<std::chrono::duration<double,std::milli>(end-qtEnd).count()/32<<",\"sameSha256\":"<<(reference==actual && reference==avDigest?"true":"false")<<"}"<<std::endl;
    return reference==actual && reference==avDigest?0:1;
}
