#pragma once
#include "callbackObject.h"
#include "callbackData.h"
#include <iostream>
#ifndef _WIN32
#include <dlfcn.h>
#endif
// Explicit one-shot diagnostics from the actual raw-scene draw context.
// No attachments are replaced, and all touched bindings are restored.
class IRFramebufferAudit : public CallbackObject {
    bool done=false;
    void audit(const char* phase) {
#ifndef _WIN32
        using U=unsigned int;using I=int;
        void* lib=dlopen("libGLESv2.so.2",RTLD_LAZY|RTLD_LOCAL);
        if(!lib){std::cerr<<"[P10Fbo] unavailable=gles_library\n";return;}
        auto get=(void(*)(U,I*))dlsym(lib,"glGetIntegerv");
        auto check=(U(*)(U))dlsym(lib,"glCheckFramebufferStatus");
        auto attachment=(void(*)(U,U,U,I*))dlsym(lib,"glGetFramebufferAttachmentParameteriv");
        auto bind=(void(*)(U,U))dlsym(lib,"glBindRenderbuffer");
        auto rb=(void(*)(U,U,I*))dlsym(lib,"glGetRenderbufferParameteriv");
        auto bindTex=(void(*)(U,U))dlsym(lib,"glBindTexture");
        auto tex=(void(*)(U,I,U,I*))dlsym(lib,"glGetTexLevelParameteriv");
        auto error=(U(*)())dlsym(lib,"glGetError");
        if(!get||!check||!attachment||!bind||!rb||!error){dlclose(lib);return;}
        I fbo=0,samples=0,previous=0;get(0x8CA6,&fbo);get(0x80A9,&samples);get(0x8CA7,&previous);
        std::cout<<"[P10Fbo] phase="<<phase<<" rawDrawFbo="<<fbo<<" status="<<check(0x8CA9)<<" GL_SAMPLES="<<samples;
        for(U a:{U(0x8CE0),U(0x8D00)}){
            I type=0,name=0,format=0,n=0,w=0,h=0;attachment(0x8CA9,a,0x8CD0,&type);
            if(type){attachment(0x8CA9,a,0x8CD1,&name);}
            if(type==0x8D41){bind(0x8D41,U(name));rb(0x8D41,0x8D44,&format);rb(0x8D41,0x8CAB,&n);rb(0x8D41,0x8D42,&w);rb(0x8D41,0x8D43,&h);}
            if(type==0x1702 && bindTex && tex){I oldTex=0;get(0x8069,&oldTex);bindTex(0x0DE1,U(name));tex(0x0DE1,0,0x1003,&format);tex(0x0DE1,0,0x1000,&w);tex(0x0DE1,0,0x1001,&h);bindTex(0x0DE1,U(oldTex));}
            std::cout<<(a==0x8CE0?" color":" depth")<<"Type="<<type<<" name="<<name<<" format="<<format<<" samples="<<n<<" size="<<w<<'x'<<h;
        }
        bind(0x8D41,U(previous));std::cout<<" glError="<<error()<<" attachmentMutation=0 resolve=Panda_GLGraphicsBuffer\n";dlclose(lib);
#else
        std::cout<<"[P10Fbo] native_GL_query=not_measured_Windows fbProperties_logged_separately=1\n";
#endif
    }
public:
    ALLOC_DELETED_CHAIN(IRFramebufferAudit);
    void do_callback(CallbackData* data) override {
        if(done){data->upcall();return;}
        audit("before_scene");
        data->upcall();
        audit("after_scene");done=true;
    }
};
