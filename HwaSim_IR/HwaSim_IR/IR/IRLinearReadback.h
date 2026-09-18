#pragma once
#include "textureContext.h"
#ifndef _WIN32
#include <dlfcn.h>
#endif

struct IRLinearReadbackTiming {
    double textureStoreMs=0.0;
    double glSetupMs=0.0;
    double gpuWaitReadbackMs=0.0;
    double cpuCopyMs=0.0;
    double totalMs=0.0;
};

static double IRLinearElapsedMs(const std::chrono::steady_clock::time_point& begin){
    return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
}

static bool ReadSceneLinearRamImage(Texture* texture,int width,int height,PfmFile& result,
                                    IRLinearReadbackTiming* timing=nullptr){
    const auto totalBegin=std::chrono::steady_clock::now();
    if(!texture||!texture->has_ram_image())return false;
    PfmFile allocated;
    const auto storeBegin=std::chrono::steady_clock::now();
    if(!texture->store(allocated))return false;
    if(timing)timing->textureStoreMs+=IRLinearElapsedMs(storeBegin);
    if(allocated.get_x_size()<width||allocated.get_y_size()<height)return false;
    const auto copyBegin=std::chrono::steady_clock::now();
    result.clear(width,height,3);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x)
        result.set_point3(x,y,allocated.get_point3(x,y+allocated.get_y_size()-height));
    if(timing){timing->cpuCopyMs+=IRLinearElapsedMs(copyBegin);timing->totalMs=IRLinearElapsedMs(totalBegin);}
    return true;
}

// Diagnostic / AGC readback only. The scene's native color/depth attachments
// are never replaced. Returned rows match the delivered, top-down RGB8 image.
static bool ReadSceneLinear(GraphicsEngine* engine,Texture* texture,GraphicsOutput* output,
                            int width,int height,PfmFile& result,bool consumeExistingRam=true,
                            IRLinearReadbackTiming* timing=nullptr){
    const auto totalBegin=std::chrono::steady_clock::now();
    if(!texture||!output||width<=0||height<=0)return false;
    if(consumeExistingRam&&ReadSceneLinearRamImage(texture,width,height,result,timing)){
        static bool loggedRam=false;if(!loggedRam){
            std::cout<<"[LinearReadback] route=render_target_ram_copy sceneAttachmentsChanged=0 depthReadback=0"<<std::endl;
            loggedRam=true;
        }
        if(timing)timing->totalMs=IRLinearElapsedMs(totalBegin);
        return true;
    }
#ifdef _WIN32
    const auto extractBegin=std::chrono::steady_clock::now();
    if(!engine->extract_texture_data(texture,output->get_gsg()))return false;
    if(timing)timing->gpuWaitReadbackMs+=IRLinearElapsedMs(extractBegin);
    const bool ok=ReadSceneLinearRamImage(texture,width,height,result,timing);
    if(timing)timing->totalMs=IRLinearElapsedMs(totalBegin);
    return ok;
#else
	// Read the bound formal floating-point color texture through a temporary read-only
    // FBO. This path is invoked only for selected diagnostic source sequences;
    // it never adds full-resolution per-frame CPU or disk traffic.
    static void* library=dlopen("libGLESv2.so.2",RTLD_LAZY|RTLD_LOCAL);
    if(!library)return false;
    using U=unsigned int;using I=int;
    auto get=reinterpret_cast<void(*)(U,I*)>(dlsym(library,"glGetIntegerv"));
    auto gen=reinterpret_cast<void(*)(I,U*)>(dlsym(library,"glGenFramebuffers"));
    auto bind=reinterpret_cast<void(*)(U,U)>(dlsym(library,"glBindFramebuffer"));
    auto attach=reinterpret_cast<void(*)(U,U,U,U,I)>(dlsym(library,"glFramebufferTexture2D"));
    auto check=reinterpret_cast<U(*)(U)>(dlsym(library,"glCheckFramebufferStatus"));
    auto read=reinterpret_cast<void(*)(I,I,I,I,U,U,void*)>(dlsym(library,"glReadPixels"));
    auto erase=reinterpret_cast<void(*)(I,const U*)>(dlsym(library,"glDeleteFramebuffers"));
    auto pack=reinterpret_cast<void(*)(U,I)>(dlsym(library,"glPixelStorei"));
    auto bindBuffer=reinterpret_cast<void(*)(U,U)>(dlsym(library,"glBindBuffer"));
    auto error=reinterpret_cast<U(*)()>(dlsym(library,"glGetError"));
    if(!get||!gen||!bind||!attach||!check||!read||!erase||!pack||!bindBuffer||!error)return false;
    const auto setupBegin=std::chrono::steady_clock::now();
    auto* gsg=output->get_gsg();
    TextureContext* tc=texture->prepare_now(0,gsg->get_prepared_objects(),gsg);
    if(!tc||!tc->get_native_id())return false;
    const U READ_FBO=0x8CA8,COLOR0=0x8CE0,PACK_BUFFER=0x88EB;
    I previous=0,packBuffer=0,alignment=0,rowLength=0,skipRows=0,skipPixels=0;
    get(0x8CAA,&previous);get(0x88ED,&packBuffer);get(0x0D05,&alignment);
    get(0x0D02,&rowLength);get(0x0D03,&skipRows);get(0x0D04,&skipPixels);
    U fbo=0;gen(1,&fbo);bind(READ_FBO,fbo);
    attach(READ_FBO,COLOR0,0x0DE1,U(tc->get_native_id()),0);
    const U status=check(READ_FBO);
    std::vector<float> rgba;
    if(timing)timing->glSetupMs=IRLinearElapsedMs(setupBegin);
    if(status==0x8CD5){
        rgba.resize(size_t(width)*height*4);
        bindBuffer(PACK_BUFFER,0);pack(0x0D05,1);pack(0x0D02,0);pack(0x0D03,0);pack(0x0D04,0);
        const auto readBegin=std::chrono::steady_clock::now();
        read(0,0,width,height,0x1908,0x1406,rgba.data()); // RGBA / FLOAT
        if(timing)timing->gpuWaitReadbackMs=IRLinearElapsedMs(readBegin);
    }
    const U err=error();
    pack(0x0D05,alignment);pack(0x0D02,rowLength);pack(0x0D03,skipRows);pack(0x0D04,skipPixels);
    bindBuffer(PACK_BUFFER,U(packBuffer));bind(READ_FBO,U(previous));erase(1,&fbo);
    if(status!=0x8CD5||err){
        std::cerr<<"[LinearReadback][ERROR] framebufferStatus="<<status<<" glError="<<err<<std::endl;return false;
    }
    const auto copyBegin=std::chrono::steady_clock::now();
    result.clear(width,height,3);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){
        const float* p=&rgba[(size_t(y)*width+x)*4];
        result.set_point3(x,height-1-y,LVecBase3f(p[0],p[1],p[2]));
    }
    if(timing){timing->cpuCopyMs=IRLinearElapsedMs(copyBegin);timing->totalMs=IRLinearElapsedMs(totalBegin);}
    static bool logged=false;if(!logged){
        std::cout<<"[LinearReadback] route=gles_rgba_float temporaryReadFbo=1 sceneAttachmentsChanged=0 depthReadback=0"<<std::endl;logged=true;
    }
    return true;
#endif
}
