#pragma once
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
extern "C" {
#include <libavutil/sha.h>
#include <libavutil/mem.h>
}

// Keep complete SHA256 auditing without the byte-at-a-time update loop in
// the renderer's portable hash helper. FFmpeg is already a receiver dependency.
class CustomerAvSha256 {
public:
    CustomerAvSha256():m_context(av_sha_alloc()) {
        if(!m_context)throw std::runtime_error("SHA256 allocation failed");
        if(av_sha_init(m_context,256)!=0){av_free(m_context);m_context=nullptr;throw std::runtime_error("SHA256 initialization failed");}
    }
    ~CustomerAvSha256(){av_free(m_context);}
    CustomerAvSha256(const CustomerAvSha256&)=delete;
    CustomerAvSha256& operator=(const CustomerAvSha256&)=delete;
    void update(const std::uint8_t* data,std::size_t bytes){av_sha_update(m_context,data,bytes);}
    std::string finishHex(){
        std::uint8_t digest[32];av_sha_final(m_context,digest);
        static const char hex[]="0123456789abcdef";std::string result(64,'0');
        for(std::size_t i=0;i<32;++i){result[i*2]=hex[digest[i]>>4];result[i*2+1]=hex[digest[i]&15];}
        return result;
    }
private:
    AVSHA* m_context;
};
