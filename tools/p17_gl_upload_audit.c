/* Diagnostic only: log GLES 3D uploads and pixel-store state without changing it. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
typedef unsigned U;
typedef int I;
static int auditArmed=0;
static void rawPixel(const char *phase){
    void (*get)(U,I*)=dlsym(RTLD_NEXT,"glGetIntegerv");
    void (*read)(I,I,I,I,U,U,void*)=dlsym(RTLD_NEXT,"glReadPixels");
    unsigned char (*enabled)(U)=dlsym(RTLD_NEXT,"glIsEnabled");
    I type=0,format=0,func=0;get(0x8B9A,&type);get(0x8B9B,&format);get(0x0B74,&func);
    union {unsigned char bytes[32];float f[8];unsigned short h[16];} value={0};
    read(400,395,1,1,format,type,&value);
    fprintf(stderr,"[P17CloudBlendPixel] phase=%s depthTest=%d func=%x blend=%d readType=%x format=%x valueFloat=%f,%f,%f valueHalf=%x,%x,%x valueByte=%u,%u,%u\n",phase,enabled(0x0B71),func,enabled(0x0BE2),type,format,value.f[0],value.f[1],value.f[2],value.h[0],value.h[1],value.h[2],value.bytes[0],value.bytes[1],value.bytes[2]);
}
static void audit(const char *name,I level,I w,I h,I d,U format,U type,const void *p){
    void (*get)(U,I*)=dlsym(RTLD_NEXT,"glGetIntegerv");
    I row=0,image=0,align=0;
    if(get){get(0x0CF2,&row);get(0x806E,&image);get(0x0CF5,&align);}
    fprintf(stderr,"[P17GLUpload] %s level=%d size=%d,%d,%d format=%x type=%x data=%p unpackRow=%d imageHeight=%d align=%d\n",name,level,w,h,d,format,type,p,row,image,align);
    fflush(stderr);
}
void glTexImage3D(U target,I level,I internal,I w,I h,I d,I border,U format,U type,const void *p){
    void (*real)(U,I,I,I,I,I,I,U,U,const void*)=dlsym(RTLD_NEXT,"glTexImage3D");
    audit("image",level,w,h,d,format,type,p);real(target,level,internal,w,h,d,border,format,type,p);
}
void glTexSubImage3D(U target,I level,I x,I y,I z,I w,I h,I d,U format,U type,const void *p){
    void (*real)(U,I,I,I,I,I,I,I,U,U,const void*)=dlsym(RTLD_NEXT,"glTexSubImage3D");
    audit("subimage",level,w,h,d,format,type,p);real(target,level,x,y,z,w,h,d,format,type,p);
}
void glGenerateMipmap(U target){
    void (*real)(U)=dlsym(RTLD_NEXT,"glGenerateMipmap");
    real(target);
    if(target==0x806F){
        void (*get)(U,I,U,I*)=dlsym(RTLD_NEXT,"glGetTexLevelParameteriv");
        if(get)for(I n=0;n<7;++n){
            I w=0,h=0,d=0;get(target,n,0x1000,&w);get(target,n,0x1001,&h);get(target,n,0x8071,&d);
            fprintf(stderr,"[P17GLGeneratedMip] level=%d size=%d,%d,%d\n",n,w,h,d);
        }
        fflush(stderr);
    }
}
static void audit_cloud_depth(void){
    auditArmed=0;
    if(!getenv("P17CloudDepthReadAudit"))return;
    void (*get)(U,I*)=dlsym(RTLD_NEXT,"glGetIntegerv");
    I (*location)(U,const char*)=dlsym(RTLD_NEXT,"glGetUniformLocation");
    void (*uniformI)(U,I,I*)=dlsym(RTLD_NEXT,"glGetUniformiv");
    void (*uniformF)(U,I,float*)=dlsym(RTLD_NEXT,"glGetUniformfv");
    void (*active)(U)=dlsym(RTLD_NEXT,"glActiveTexture");
    void (*gen)(I,U*)=dlsym(RTLD_NEXT,"glGenFramebuffers");
    void (*bind)(U,U)=dlsym(RTLD_NEXT,"glBindFramebuffer");
    void (*attach)(U,U,U,U,I)=dlsym(RTLD_NEXT,"glFramebufferTexture2D");
    void (*read)(I,I,I,I,U,U,void*)=dlsym(RTLD_NEXT,"glReadPixels");
    void (*erase)(I,U*)=dlsym(RTLD_NEXT,"glDeleteFramebuffers");
    I program=0;get(0x8B8D,&program);I loc=location(program,"u_scene_depth_rgb");if(loc<0)return;
    static I calls=0;if((calls++%120)>1)return;
    I unit=0,oldActive=0,tex=0,oldRead=0;uniformI(program,loc,&unit);get(0x84E0,&oldActive);
    active(0x84C0+unit);get(0x8069,&tex);active(oldActive);get(0x8CAA,&oldRead);
    float logDepth[2]={0};uniformF(program,location(program,"u_ir_log_depth"),logDepth);
    float camera[3]={0},mv[16]={0},density=0,optical=0,fade=0;
    uniformF(program,location(program,"u_camera_local"),camera);
    uniformF(program,location(program,"p3d_ModelViewMatrix"),mv);
    uniformF(program,location(program,"u_density_scale"),&density);
    uniformF(program,location(program,"u_optical_depth"),&optical);
    uniformF(program,location(program,"u_spawn_fade"),&fade);
    double length=sqrt(camera[0]*camera[0]+camera[1]*camera[1]+camera[2]*camera[2]);
    fprintf(stderr,"[P17CloudRayUniform] camera=%.3f,%.3f,%.3f matrixZ=%.3f,%.3f,%.3f eyeDz=%.3f density=%.3f optical=%.3f fade=%.3f\n",camera[0],camera[1],camera[2],mv[2],mv[6],mv[10],-(camera[0]*mv[2]+camera[1]*mv[6]+camera[2]*mv[10])/length,density,optical,fade);
    U fbo=0;gen(1,&fbo);bind(0x8CA8,fbo);attach(0x8CA8,0x8CE0,0x0DE1,tex,0);
    unsigned char rgba[4];
    for(I y=390;y<=400;y+=5){
        read(400,y,1,1,0x1908,0x1401,rgba);
        double depth=(rgba[0]*65536.+rgba[1]*256.+rgba[2])/16777215.;
        fprintf(stderr,"[P17CloudDepthPixel] draw=%d xy=400,%d rgba=%d,%d,%d,%d distance=%.3f log=%.6f,%.6f\n",calls,y,rgba[0],rgba[1],rgba[2],rgba[3],exp2(depth/logDepth[1]+logDepth[0])-1,logDepth[0],logDepth[1]);
    }
    bind(0x8CA8,oldRead);erase(1,&fbo);auditArmed=1;rawPixel("before");fflush(stderr);
}
void glDrawElements(U mode,I count,U type,const void *indices){
    void (*real)(U,I,U,const void*)=dlsym(RTLD_NEXT,"glDrawElements");
    audit_cloud_depth();real(mode,count,type,indices);if(auditArmed)rawPixel("after");
}
void glDrawRangeElements(U mode,U start,U end,I count,U type,const void *indices){
    void (*real)(U,U,U,I,U,const void*)=dlsym(RTLD_NEXT,"glDrawRangeElements");
    audit_cloud_depth();real(mode,start,end,count,type,indices);if(auditArmed)rawPixel("after");
}
void *eglGetProcAddress(const char *name){
    void *(*real)(const char*)=dlsym(RTLD_NEXT,"eglGetProcAddress");
    if(!strcmp(name,"glTexImage3D"))return glTexImage3D;
    if(!strcmp(name,"glTexSubImage3D"))return glTexSubImage3D;
    if(!strcmp(name,"glGenerateMipmap"))return glGenerateMipmap;
    if(!strcmp(name,"glDrawElements"))return glDrawElements;
    if(!strcmp(name,"glDrawRangeElements"))return glDrawRangeElements;
    return real(name);
}
