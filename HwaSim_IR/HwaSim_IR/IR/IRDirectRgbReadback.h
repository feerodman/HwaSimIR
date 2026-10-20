#pragma once
#include "callbackObject.h"
#include "callbackData.h"
#include "texture.h"
#include "IRNativeRgbCopy.h"
#include <vector>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <cstdlib>
#ifndef _WIN32
#include <dlfcn.h>

// 在最终标注之后同步读取同一RGB8附件。只替换Panda读回中的分量重排，
// 仍通过modify_ram_image推进图像身份，保留Panda BGR、底行优先的RAM契约。
// 没有PBO、跨帧缓存、缩放、丢帧，也没有修改厂商Panda/GL库。
class IRDirectRgbReadback : public CallbackObject {
    PT(Texture) texture;
    int width,height;
    bool audit;
    std::vector<unsigned char> rgb;
    void* library;
    using Get=void(*)(unsigned,int*);using Bind=void(*)(unsigned,unsigned);
    using Pack=void(*)(unsigned,int);using Read=void(*)(int,int,int,int,unsigned,unsigned,void*);using Error=unsigned(*)();
    Get get;Bind bindFramebuffer,bindBuffer;Pack pack;Read read;Error error;
    unsigned auditFrames=0;
    bool timing=false;
    void(*finish)()=nullptr;
public:
    ALLOC_DELETED_CHAIN(IRDirectRgbReadback);
    IRDirectRgbReadback(Texture* t,int w,int h,bool compare):texture(t),width(w),height(h),audit(compare),rgb(size_t(w)*h*3){
        library=dlopen("libGLESv2.so.2",RTLD_NOW|RTLD_LOCAL);
        if(!library)throw std::runtime_error("P20 RGB readback GLES library unavailable");
        get=(Get)dlsym(library,"glGetIntegerv");bindFramebuffer=(Bind)dlsym(library,"glBindFramebuffer");bindBuffer=(Bind)dlsym(library,"glBindBuffer");pack=(Pack)dlsym(library,"glPixelStorei");read=(Read)dlsym(library,"glReadPixels");error=(Error)dlsym(library,"glGetError");
        if(!get||!bindFramebuffer||!bindBuffer||!pack||!read||!error)throw std::runtime_error("P20 RGB readback GLES function unavailable");
        const char* probe=std::getenv("HwaSimIRDirectRgbTiming");timing=probe && std::string(probe)=="1";
        if(timing){finish=(void(*)())dlsym(library,"glFinish");if(!finish)throw std::runtime_error("P20 RGB timing glFinish unavailable");}
    }
    ~IRDirectRgbReadback(){if(library)dlclose(library);}
    bool isAudit()const{return audit;}
    void compareAfterPanda(){
        if(!audit)return;
        const auto reference=texture->get_ram_image_as("RGB");
        const bool exact=reference.size()==rgb.size() && std::memcmp(reference.p(),rgb.data(),rgb.size())==0;
        ++auditFrames;
        if(!exact || auditFrames<=3 || auditFrames%120==0)std::cout<<"[P20DirectRgbAudit] frame="<<auditFrames<<" bytes="<<rgb.size()<<" exact="<<exact<<" diagnosticDoubleRead=1"<<std::endl;
        if(!exact)throw std::runtime_error("P20 RGB readback pixel comparison failed");
    }
    void do_callback(CallbackData* data)override{
        data->upcall();
        // 审计模式由Panda在本帧末尾协商RAM格式/尺寸；绑定时可能暂时是POT。
        // 快速模式没有RTM协商，必须严格保持已设置的原生RGB8。
        if(!audit && (texture->get_x_size()!=width || texture->get_y_size()!=height || texture->get_num_components()!=3 || texture->get_component_type()!=Texture::T_unsigned_byte))
            throw std::runtime_error("P20 RGB readback dimensions or format changed without rebuilding");
        int draw=0,previous=0,pbo=0,viewport[4]={0};
        get(0x8CA6,&draw);get(0x8CAA,&previous);get(0x88ED,&pbo);get(0x0BA2,viewport);
        if(viewport[0]!=0 || viewport[1]!=0 || viewport[2]!=width || viewport[3]!=height)throw std::runtime_error("P20 RGB readback viewport mismatch");
        const unsigned keys[]={0x0D05,0x0D02,0x0D03,0x0D04};int values[4];
        for(int i=0;i<4;++i){get(keys[i],&values[i]);pack(keys[i],i==0?1:0);}
        bindFramebuffer(0x8CA8,unsigned(draw));bindBuffer(0x88EB,0);
        using Clock=std::chrono::steady_clock;
        const auto start=timing?Clock::now():Clock::time_point();
        if(timing)finish();
        const auto ready=timing?Clock::now():Clock::time_point();
        read(0,0,width,height,0x1907,0x1401,rgb.data());
        const auto copied=timing?Clock::now():Clock::time_point();
        bindBuffer(0x88EB,unsigned(pbo));bindFramebuffer(0x8CA8,unsigned(previous));
        for(int i=0;i<4;++i)pack(keys[i],values[i]);
        const unsigned glError=error();
        if(glError){std::cerr<<"[P20DirectRgbReadback] GL error="<<glError<<std::endl;throw std::runtime_error("P20 RGB readback failed");}
        if(!audit){
            auto ram=texture->modify_ram_image();
            if(ram.size()!=rgb.size())throw std::runtime_error("P20 RGB readback RAM capacity mismatch");
            IRNativeRgbCopy(rgb.data(),ram.p(),size_t(width)*height,3);
        }
        if(timing)std::cout<<"[P20DirectRgbTiming] w="<<width<<" h="<<height<<" gpuWaitMs="<<std::chrono::duration<double,std::milli>(ready-start).count()<<" readMs="<<std::chrono::duration<double,std::milli>(copied-ready).count()<<" convertAndRestoreMs="<<std::chrono::duration<double,std::milli>(Clock::now()-copied).count()<<" diagnostic=1"<<std::endl;
    }
};
#endif
