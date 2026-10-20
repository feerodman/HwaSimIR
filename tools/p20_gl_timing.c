/* 仅用于测量：GPU 完成等待与 glReadPixels 分开计时。
 * 不改变像素格式，不使用 PBO，不丢帧；不能把插入同步后的时序当生产性能。 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
typedef void (*Read)(int,int,int,int,unsigned,unsigned,void*);
typedef void (*Finish)(void);
static Read read_pixels;
static Finish finish_gpu;
static pthread_once_t once=PTHREAD_ONCE_INIT;
static void resolve(void){
    void* lib=dlopen("libGLESv2.so.2",RTLD_NOW|RTLD_LOCAL);
    read_pixels=(Read)dlsym(RTLD_NEXT,"glReadPixels");
    finish_gpu=(Finish)dlsym(RTLD_NEXT,"glFinish");
    if(!read_pixels && lib)read_pixels=(Read)dlsym(lib,"glReadPixels");
    if(!finish_gpu && lib)finish_gpu=(Finish)dlsym(lib,"glFinish");
}
static double ms(struct timespec a,struct timespec b){return (b.tv_sec-a.tv_sec)*1000.+(b.tv_nsec-a.tv_nsec)/1e6;}
void glReadPixels(int x,int y,int w,int h,unsigned format,unsigned type,void* data){
    pthread_once(&once,resolve);
    if(!read_pixels){fprintf(stderr,"[P20GlTiming] error=missing_real_read\n");return;}
    struct timespec a,b,c;clock_gettime(CLOCK_MONOTONIC,&a);
    if(finish_gpu && !getenv("P20_STATE_ONLY"))finish_gpu();clock_gettime(CLOCK_MONOTONIC,&b);
    read_pixels(x,y,w,h,format,type,data);clock_gettime(CLOCK_MONOTONIC,&c);
    fprintf(stderr,"[P20GlTiming] w=%d h=%d format=%u type=%u gpuWaitMs=%.6f readMs=%.6f finishAvailable=%d diagnostic=1\n",w,h,format,type,ms(a,b),ms(b,c),finish_gpu!=0);
    static struct timespec last_wall,last_cpu;struct timespec cpu;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&cpu);
    typedef int(*Count)(void);static Count states,transforms;
    if(!states)states=(Count)dlsym(RTLD_DEFAULT,"_ZN11RenderState14get_num_statesEv");
    if(!transforms)transforms=(Count)dlsym(RTLD_DEFAULT,"_ZN14TransformState14get_num_statesEv");
    if(last_wall.tv_sec)fprintf(stderr,"[P20StateTiming] wallMs=%.6f threadCpuMs=%.6f renderStates=%d transforms=%d diagnostic=1\n",ms(last_wall,c),ms(last_cpu,cpu),states?states():-1,transforms?transforms():-1);
    last_wall=c;last_cpu=cpu;
    /* 独立诊断开关：清除仅缓存持有的组合状态，不改场景节点或像素。
     * 默认关闭；用于验证状态缓存增长是否导致后段 CPU 耗时增长。 */
    static unsigned reads;
    ++reads;
    if(getenv("P20_CLEAR_STATE_CACHE") && reads%60==0){
        Count clear=(Count)dlsym(RTLD_DEFAULT,"_ZN11RenderState11clear_cacheEv");
        Count unused=(Count)dlsym(RTLD_DEFAULT,"_ZN11RenderState21get_num_unused_statesEv");
        int before=states?states():-1,un=unused?unused():-1;
        struct timespec begin,end;clock_gettime(CLOCK_MONOTONIC,&begin);
        int freed=clear?clear():-1;clock_gettime(CLOCK_MONOTONIC,&end);
        fprintf(stderr,"[P20StateClear] read=%u before=%d unused=%d freed=%d after=%d ms=%.6f diagnostic=1\n",reads,before,un,freed,states?states():-1,ms(begin,end));
    }
}

/* 逐绘制诊断强制同步，用来定位绘制成本，不能作为生产帧率。 */
static unsigned current_program;
static char shader_kind[4096][40],program_kind[4096][40];
static void* symbol(const char* name){
    void* p=dlsym(RTLD_NEXT,name);if(p)return p;
    void* lib=dlopen("libGLESv2.so.2",RTLD_NOW|RTLD_LOCAL);return lib?dlsym(lib,name):0;
}
void glShaderSource(unsigned shader,int count,const char* const* strings,const int* lengths){
    typedef void (*Fn)(unsigned,int,const char* const*,const int*);static Fn real;
    if(!real)real=(Fn)symbol("glShaderSource");
    if(shader<4096)for(int i=0;i<count;++i){
        const char* kind=strstr(strings[i],"prefixCloud")?"transparent_prefix":strstr(strings[i],"u_puff_center")||strstr(strings[i],"v_puff_origin")?"puff":
          strstr(strings[i],"u_cloud_shared_optics")?"final_mapping":strstr(strings[i],"cloudUv")?"cloud_sheet":
          strstr(strings[i],"u_plume_")?"plume":strstr(strings[i],"u_draw_enabled")?"scene_or_depth":"other";
        snprintf(shader_kind[shader],40,"%s",kind);
    }
    real(shader,count,strings,lengths);
}
void glAttachShader(unsigned program,unsigned shader){
    typedef void (*Fn)(unsigned,unsigned);static Fn real;if(!real)real=(Fn)symbol("glAttachShader");
    if(program<4096&&shader<4096)snprintf(program_kind[program],40,"%s",shader_kind[shader]);
    real(program,shader);
}
void glUseProgram(unsigned program){
    typedef void (*Fn)(unsigned);static Fn real;if(!real)real=(Fn)symbol("glUseProgram");
    current_program=program;real(program);
}
static struct timespec draw_begin(void){
    struct timespec t;pthread_once(&once,resolve);if(finish_gpu)finish_gpu();clock_gettime(CLOCK_MONOTONIC,&t);return t;
}
static void draw_end(struct timespec t,int count,int instances){
    struct timespec end;if(finish_gpu)finish_gpu();clock_gettime(CLOCK_MONOTONIC,&end);
    typedef void(*GetI)(unsigned,int*);typedef int(*GetL)(unsigned,const char*);typedef void(*GetF)(unsigned,int,float*);
    static GetI gi;static GetL gl;static GetF gf;if(!gi)gi=(GetI)symbol("glGetIntegerv");if(!gl)gl=(GetL)symbol("glGetUniformLocation");if(!gf)gf=(GetF)symbol("glGetUniformfv");
    int fbo=-1;float presentation[4]={-1,0,0,0};if(gi)gi(0x8CA6,&fbo);int location=gl?gl(current_program,"u_cloud_presentation"):-1;if(location>=0&&gf)gf(current_program,location,presentation);
    float prefix_count=-1,prefix_output=-1;int loc=gl?gl(current_program,"u_prefix_count"):-1;if(loc>=0&&gf)gf(current_program,loc,&prefix_count);loc=gl?gl(current_program,"u_prefix_output"):-1;if(loc>=0&&gf)gf(current_program,loc,&prefix_output);
    fprintf(stderr,"[P20DrawTiming] program=%u kind=%s count=%d instances=%d gpuCallMs=%.6f fbo=%d presentation=%.1f prefixCount=%.0f prefixOutput=%.0f diagnostic=1\n",current_program,current_program<4096?program_kind[current_program]:"unknown",count,instances,ms(t,end),fbo,presentation[0],prefix_count,prefix_output);
}
void glDrawElements(unsigned mode,int count,unsigned type,const void* indices){
    typedef void (*Fn)(unsigned,int,unsigned,const void*);static Fn real;if(!real)real=(Fn)symbol("glDrawElements");
    if(!getenv("P20_DRAW_TIMING")){real(mode,count,type,indices);return;}
    struct timespec t=draw_begin();real(mode,count,type,indices);draw_end(t,count,1);
}
void glDrawArrays(unsigned mode,int first,int count){
    typedef void (*Fn)(unsigned,int,int);static Fn real;if(!real)real=(Fn)symbol("glDrawArrays");
    if(!getenv("P20_DRAW_TIMING")){real(mode,first,count);return;}
    struct timespec t=draw_begin();real(mode,first,count);draw_end(t,count,1);
}
void glDrawElementsInstanced(unsigned mode,int count,unsigned type,const void* indices,int instances){
    typedef void (*Fn)(unsigned,int,unsigned,const void*,int);static Fn real;if(!real)real=(Fn)symbol("glDrawElementsInstanced");
    if(!getenv("P20_DRAW_TIMING")){real(mode,count,type,indices,instances);return;}
    struct timespec t=draw_begin();real(mode,count,type,indices,instances);draw_end(t,count,instances);
}
void glDrawArraysInstanced(unsigned mode,int first,int count,int instances){
    typedef void (*Fn)(unsigned,int,int,int);static Fn real;if(!real)real=(Fn)symbol("glDrawArraysInstanced");
    if(!getenv("P20_DRAW_TIMING")){real(mode,first,count,instances);return;}
    struct timespec t=draw_begin();real(mode,first,count,instances);draw_end(t,count,instances);
}
void glDrawRangeElements(unsigned mode,unsigned start,unsigned end,int count,unsigned type,const void* indices){
    typedef void (*Fn)(unsigned,unsigned,unsigned,int,unsigned,const void*);static Fn real;if(!real)real=(Fn)symbol("glDrawRangeElements");
    if(!getenv("P20_DRAW_TIMING")){real(mode,start,end,count,type,indices);return;}
    struct timespec t=draw_begin();real(mode,start,end,count,type,indices);draw_end(t,count,1);
}
/* 参数纹理更新只计API耗时，不插入额外GPU同步。 */
void glTexImage2D(unsigned target,int level,int internal,int w,int h,int border,unsigned format,unsigned type,const void* data){
    typedef void(*Fn)(unsigned,int,int,int,int,int,unsigned,unsigned,const void*);static Fn real;if(!real)real=(Fn)symbol("glTexImage2D");
    struct timespec a,b;clock_gettime(CLOCK_MONOTONIC,&a);real(target,level,internal,w,h,border,format,type,data);clock_gettime(CLOCK_MONOTONIC,&b);
    if(getenv("P20_UPLOAD_TIMING"))fprintf(stderr,"[P20UploadTiming] api=TexImage w=%d h=%d format=%u type=%u ms=%.6f diagnostic=1\n",w,h,format,type,ms(a,b));
}
void glTexSubImage2D(unsigned target,int level,int x,int y,int w,int h,unsigned format,unsigned type,const void* data){
    typedef void(*Fn)(unsigned,int,int,int,int,int,unsigned,unsigned,const void*);static Fn real;if(!real)real=(Fn)symbol("glTexSubImage2D");
    struct timespec a,b;clock_gettime(CLOCK_MONOTONIC,&a);real(target,level,x,y,w,h,format,type,data);clock_gettime(CLOCK_MONOTONIC,&b);
    if(getenv("P20_UPLOAD_TIMING"))fprintf(stderr,"[P20UploadTiming] api=TexSubImage w=%d h=%d format=%u type=%u ms=%.6f diagnostic=1\n",w,h,format,type,ms(a,b));
}
void* eglGetProcAddress(const char* name){
    typedef void* (*Get)(const char*);static Get real_get;
    if(!real_get)real_get=(Get)dlsym(RTLD_NEXT,"eglGetProcAddress");
#define ROUTE(fn) if(strcmp(name,#fn)==0)return (void*)fn
    ROUTE(glDrawRangeElements);
    ROUTE(glTexImage2D);ROUTE(glTexSubImage2D);
    ROUTE(glReadPixels);ROUTE(glShaderSource);ROUTE(glAttachShader);ROUTE(glUseProgram);
    ROUTE(glDrawElements);ROUTE(glDrawArrays);ROUTE(glDrawElementsInstanced);ROUTE(glDrawArraysInstanced);
    return real_get?real_get(name):0;
}
