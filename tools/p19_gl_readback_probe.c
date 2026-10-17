/* Diagnostic interposition only. Observe the real readback; do not alter pixels,
 * format, synchronization, GL state, or application frame ownership. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdlib.h>
typedef void (*ReadPixels)(int,int,int,int,unsigned,unsigned,void*);
static ReadPixels real_read;
static pthread_once_t once=PTHREAD_ONCE_INIT;
static void resolve(void) {
    real_read=(ReadPixels)dlsym(RTLD_NEXT,"glReadPixels");
    if(!real_read) {
        void* library=dlopen("libGLESv2.so.2",RTLD_NOW|RTLD_LOCAL);
        if(library)real_read=(ReadPixels)dlsym(library,"glReadPixels");
    }
}
static int read_pbo(int x,int y,int w,int h,unsigned format,unsigned type,void* data) {
    typedef void (*Get)(unsigned,int*);typedef void (*Gen)(int,unsigned*);
    typedef void (*Bind)(unsigned,unsigned);typedef void (*Allocate)(unsigned,long,const void*,unsigned);
    typedef void* (*Map)(unsigned,long,long,unsigned);typedef unsigned char (*Unmap)(unsigned);
    static Get get;static Gen gen;static Bind bind;static Allocate alloc;static Map map;static Unmap unmap;
    static unsigned buffer;static long capacity;
    if(!get){get=dlsym(RTLD_NEXT,"glGetIntegerv");gen=dlsym(RTLD_NEXT,"glGenBuffers");bind=dlsym(RTLD_NEXT,"glBindBuffer");alloc=dlsym(RTLD_NEXT,"glBufferData");map=dlsym(RTLD_NEXT,"glMapBufferRange");unmap=dlsym(RTLD_NEXT,"glUnmapBuffer");}
    if(!get||!gen||!bind||!alloc||!map||!unmap)return 0;
    int previous,row,skipRow,skipPixel;get(0x88ed,&previous);get(0x0d02,&row);get(0x0d03,&skipRow);get(0x0d04,&skipPixel);
    if(previous||row||skipRow||skipPixel||w%4||format!=0x1907||type!=0x1401)return 0;
    const int rgba=getenv("P19PboRgba")!=0;
    long pixels=(long)w*h,bytes=pixels*(rgba?4:3);if(!buffer)gen(1,&buffer);
    bind(0x88eb,buffer);if(capacity<bytes){alloc(0x88eb,bytes,0,0x88e1);capacity=bytes;}
    struct timespec a,b,c;clock_gettime(CLOCK_MONOTONIC,&a);
    real_read(x,y,w,h,rgba?0x1908:format,type,0);clock_gettime(CLOCK_MONOTONIC,&b);
    void* mapped=map(0x88eb,0,bytes,1);clock_gettime(CLOCK_MONOTONIC,&c);
    int ok=mapped!=0;if(mapped){if(rgba){unsigned char* src=mapped;unsigned char* dst=data;for(long i=0;i<pixels;++i){dst[i*3]=src[i*4];dst[i*3+1]=src[i*4+1];dst[i*3+2]=src[i*4+2];}}else memcpy(data,mapped,bytes);unmap(0x88eb);}
    bind(0x88eb,0);
    static unsigned calls;static double submit,wait;
    submit+=(b.tv_sec-a.tv_sec)*1000.+(b.tv_nsec-a.tv_nsec)/1.e6;
    wait+=(c.tv_sec-b.tv_sec)*1000.+(c.tv_nsec-b.tv_nsec)/1.e6;
    if(++calls%120==0){fprintf(stderr,"[P19PboProbe] submitMs=%.4f mapWaitMs=%.4f mapped=%d\n",submit/120,wait/120,ok);submit=wait=0;}
    return ok;
}
void glReadPixels(int x,int y,int w,int h,unsigned format,unsigned type,void* data) {
    pthread_once(&once,resolve);
    if(!real_read){fprintf(stderr,"[P19ReadbackProbe] real glReadPixels unavailable\n");return;}
    const char* finishEnv=getenv("P19FinishBeforeRead");
    if(finishEnv && strcmp(finishEnv,"1")==0) {
        typedef void (*Finish)(void);static Finish finish;if(!finish)finish=dlsym(RTLD_NEXT,"glFinish");
        struct timespec s,e;clock_gettime(CLOCK_MONOTONIC,&s);if(finish)finish();clock_gettime(CLOCK_MONOTONIC,&e);
        static unsigned count;static double wait;wait+=(e.tv_sec-s.tv_sec)*1000.+(e.tv_nsec-s.tv_nsec)/1.e6;
        if(++count%120==0){fprintf(stderr,"[P19FinishProbe] gpuWaitBeforeReadMs=%.4f\n",wait/120);wait=0;}
    }
    typedef void (*Get)(unsigned,int*);static Get get;static int logged;
    if(!logged){get=dlsym(RTLD_NEXT,"glGetIntegerv");int preferredFormat=0,preferredType=0;if(get){get(0x8b9b,&preferredFormat);get(0x8b9a,&preferredType);}fprintf(stderr,"[P19ReadFormat] preferred=0x%x type=0x%x\n",preferredFormat,preferredType);logged=1;}
    struct timespec a,b;clock_gettime(CLOCK_MONOTONIC,&a);
    const char* rgba=getenv("P19ProbeReadRgba");
    const char* pbo=getenv("P19ProbePbo");
    if(pbo && strcmp(pbo,"1")==0 && read_pbo(x,y,w,h,format,type,data)) {}
    else if(rgba && strcmp(rgba,"1")==0 && format==0x1907 && type==0x1401 && w>0 && h>0 && w%4==0) {
        typedef void (*Get)(unsigned,int*);
        static Get get;if(!get)get=(Get)dlsym(RTLD_NEXT,"glGetIntegerv");
        int buffer=-1,row=-1,skipRow=-1,skipPixel=-1;
        if(get){get(0x88ed,&buffer);get(0x0d02,&row);get(0x0d03,&skipRow);get(0x0d04,&skipPixel);}
        if(buffer==0 && row==0 && skipRow==0 && skipPixel==0) {
            static unsigned char* scratch;static size_t capacity;
            const size_t pixels=(size_t)w*h;
            if(capacity<pixels*4){unsigned char* next=realloc(scratch,pixels*4);if(next){scratch=next;capacity=pixels*4;}}
            if(capacity>=pixels*4) {
                real_read(x,y,w,h,0x1908,type,scratch);
                unsigned char* out=data;
                for(size_t i=0;i<pixels;++i){out[i*3]=scratch[i*4];out[i*3+1]=scratch[i*4+1];out[i*3+2]=scratch[i*4+2];}
            } else real_read(x,y,w,h,format,type,data);
        } else real_read(x,y,w,h,format,type,data);
    } else real_read(x,y,w,h,format,type,data);
    clock_gettime(CLOCK_MONOTONIC,&b);
    static unsigned count;static double total,maximum;
    double ms=(b.tv_sec-a.tv_sec)*1000.+(b.tv_nsec-a.tv_nsec)/1.e6;
    total+=ms;if(ms>maximum)maximum=ms;
    if(++count%120==0){fprintf(stderr,"[P19ReadbackProbe] calls=%u width=%d height=%d format=0x%x type=0x%x meanMs=%.4f maxMs=%.4f\n",count,w,h,format,type,total/120.,maximum);total=maximum=0;}
}
/* Optional scheduling experiment: submit completed FBO work earlier without
 * waiting for the GPU. No glFinish, image changes or frame dropping. */
void glBindFramebuffer(unsigned target,unsigned framebuffer) {
    typedef void (*Bind)(unsigned,unsigned);
    typedef void (*Flush)(void);
    static Bind real_bind;static Flush real_flush;static int enabled=-1;
    static unsigned previous;
    if(!real_bind)real_bind=(Bind)dlsym(RTLD_NEXT,"glBindFramebuffer");
    if(!real_flush)real_flush=(Flush)dlsym(RTLD_NEXT,"glFlush");
    if(enabled<0){const char* value=getenv("P19FlushOnFbo");enabled=value&&strcmp(value,"1")==0;
        if(enabled)fprintf(stderr,"[P19GpuSubmit] flushOnFboChange=1 diagnostic=1\n");}
    if(enabled && real_flush && (target==0x8d40 || target==0x8ca9) && previous && previous!=framebuffer)real_flush();
    if(target==0x8d40 || target==0x8ca9)previous=framebuffer;
    if(real_bind)real_bind(target,framebuffer);
}
void* eglGetProcAddress(const char* name) {
    typedef void* (*GetProc)(const char*);
    static GetProc real_get;
    if(!real_get)real_get=(GetProc)dlsym(RTLD_NEXT,"eglGetProcAddress");
    if(strcmp(name,"glReadPixels")==0)return (void*)glReadPixels;
    if(strcmp(name,"glBindFramebuffer")==0)return (void*)glBindFramebuffer;
    return real_get?real_get(name):0;
}
