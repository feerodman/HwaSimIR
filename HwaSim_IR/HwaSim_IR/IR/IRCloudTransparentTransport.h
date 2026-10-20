#pragma once
#include <string>
#include <stdexcept>

// 透明源在云合成坐标中混合：C(z)+T(z)*L。背景已经含整条云光程。
// 光学源项单独保存，WhiteCloud只替换云源，绝不再次衰减尾焰。
// descriptor每行48个有效R32F标量，摄像机坐标采用Panda的+Y前向。
inline std::string IRCloudPrefixGlsl(bool gather=false){return std::string(gather?"#define P20_PREFIX_GATHER\n":"")+R"(
#ifdef GL_ES
precision highp sampler2D;
#endif
uniform sampler2D u_prefix_records,u_prefix_sheet,u_scene_depth_rgb;
uniform int u_prefix_count,u_prefix_output;
uniform vec2 u_prefix_viewport;
uniform vec3 u_prefix_ray_base,u_prefix_ray_x,u_prefix_ray_y;
#ifdef P20_PREFIX_GATHER
// 2x2中的(w,z)/(x,y)，gather按(x,y,z,w)返回原始R32F数值。
vec4 prefixRecord(int i,int j){return textureGather(u_prefix_records,vec2(float(j*2+1)/32.0,float(i*2+1)/float(textureSize(u_prefix_records,0).y)),0);}
#else
vec4 prefixRecord(int i,int j){int x=j*4;return vec4(texelFetch(u_prefix_records,ivec2(x,i),0).r,texelFetch(u_prefix_records,ivec2(x+1,i),0).r,texelFetch(u_prefix_records,ivec2(x+2,i),0).r,texelFetch(u_prefix_records,ivec2(x+3,i),0).r);}
#endif
vec3 prefixRay(){vec2 p=gl_FragCoord.xy/u_prefix_viewport;return u_prefix_ray_base+p.x*u_prefix_ray_x+p.y*u_prefix_ray_y;}
float prefixPrimitive(float s,float q){return s*(q*q-(2.0/3.0)*q*s*s+.2*s*s*s*s);}
// 返回(前方云SI源，前方云显示源，0，透射率)，源项为预乘值。
// 直接保留T，避免1-alpha的浮点消减提前消掉极热源。
vec4 prefixCloud(float forwardDepth){
 vec3 ray=prefixRay();vec4 sum=vec4(0,0,0,1);
 for(int i=0;i<u_prefix_count;++i){
  vec4 a=prefixRecord(i,0),bx=prefixRecord(i,1),by=prefixRecord(i,2),bz=prefixRecord(i,3);
  vec3 rawDir=ray.x*bx.xyz+ray.y*by.xyz+ray.z*bz.xyz;
  vec4 optical=vec4(0);
  if(prefixRecord(i,11).w<.5){
   float rayScale=length(rawDir);vec3 dir=rawDir/max(rayScale,1e-20);
   float center=-dot(a.xyz,dir);vec3 radial=a.xyz+center*dir;
   float q=1.0-dot(radial,radial);
   if(q<=0.0)continue;
   float h=sqrt(q),enter=max(0.0,center-h),leave=min(center+h,forwardDepth*rayScale);
   if(leave<=enter)continue;
   float path=(enter<=center-h && leave>=center+h)?(16.0/15.0)*q*q*h:
    max(0.0,prefixPrimitive(leave-center,q)-prefixPrimitive(enter-center,q));
   path*=length(dir*vec3(bx.w,by.w,bz.w));
   vec4 source=prefixRecord(i,4);
   float light=clamp(source.y*1.4+.13*(-radial.x+.6*radial.z)-.08*sqrt(max(0.0,1.0-q)),.35,1.25);
   optical=vec4(source.x,mix(source.z,source.w,(light-.35)/.9),0,exp(-max(0.0,path*a.w)));
  }else{
   if(abs(rawDir.z)<1e-12)continue;
   float distance=-a.z/rawDir.z;
   if(distance<=0.0 || distance>forwardDepth)continue;
   vec3 local=a.xyz+distance*rawDir;vec4 bounds=prefixRecord(i,9);
   if(local.x<bounds.x || local.x>bounds.y || local.y<bounds.z || local.y>bounds.w)continue;
   vec4 uv0=prefixRecord(i,4),uv1=prefixRecord(i,5),uv2=prefixRecord(i,6),wind=prefixRecord(i,7),cfg=prefixRecord(i,8),game=prefixRecord(i,10);
   vec2 worldUv=uv0.xy+distance*(ray.x*uv1.xy+ray.y*uv1.zw+ray.z*uv2.xy);
   vec2 windDir=wind.xy/max(length(wind.xy),.0001);
   vec2 detailDir=normalize(vec2(windDir.x-windDir.y*.27,windDir.y+windDir.x*.27));
   vec4 base=texture(u_prefix_sheet,worldUv*max(.05,uv0.z)+uv2.zw+windDir*wind.z*wind.w);
   vec4 detail=texture(u_prefix_sheet,worldUv*max(.05,uv0.z)*max(1.0,uv0.w)+uv2.zw*2.17+detailDir*wind.z*1.31*wind.w);
   float b=dot(base.rgb,vec3(.299,.587,.114)),d=dot(detail.rgb,vec3(.299,.587,.114));
   float bm=cfg.w>.5?base.a:smoothstep(.34,.72,b),dm=cfg.w>.5?detail.a:smoothstep(.34,.72,d);
   if(game.x>.5 && cfg.w<.5){bm=b;dm=d;}
   float density=mix(bm,dm,clamp(cfg.y,0.0,1.0));
   float coverage=clamp(cfg.x,0.0,1.0),threshold=mix(.72,.32,pow(coverage,.85));
   float mask=cfg.w>.5?density*coverage:smoothstep(threshold,min(.999,threshold+clamp(cfg.z,.02,.5)),density);
   if(game.x>.5)mask=smoothstep(.04,.92,bm);
   mask=clamp(mask*mix(1.0,1.18,clamp(bx.w,0.0,1.0)),0.0,1.0);
   float alpha=min(.94,1.0-exp(-max(0.0,a.w)*mask*(game.x>.5?game.z:1.0)));
   if(by.w>0.0)alpha*=smoothstep(0.0,by.w,abs(rawDir.z)/max(length(rawDir),1e-20));
   float structure=cfg.w>.5?density:clamp((mix(b,d,.25)-.25)/.60,0.0,1.0);
   optical=vec4(max(0.0,game.x>.5?game.y:bz.w),mix(.70,.98,smoothstep(.05,.95,structure)),0,1.0-alpha);
  }
  // 与独立云通道相同的后到前次序；局部光程在实际透明深度截断。
  sum=vec4(optical.rgb*(1.0-optical.a)+sum.rgb*optical.a,sum.a*optical.a);
 }
 return sum;
}
// 只在实际气体积分区间与任一云密度支撑/层表面相交时才逐步重算。
// 完全云前/云后的前缀严格恒定；没有按总alpha或中心深度近似。
bool prefixVaries(float front,float back){
 vec3 ray=prefixRay();
 for(int i=0;i<u_prefix_count;++i){
  vec4 a=prefixRecord(i,0);if(a.w<=0.0)continue;
  vec3 d=ray.x*prefixRecord(i,1).xyz+ray.y*prefixRecord(i,2).xyz+ray.z*prefixRecord(i,3).xyz;
  if(prefixRecord(i,11).w<.5){
   float scale=length(d);vec3 direction=d/max(scale,1e-20);float center=-dot(a.xyz,direction);
   vec3 radial=a.xyz+center*direction;float q=1.0-dot(radial,radial);
   if(q>0.0){float h=sqrt(q);if(max(front*scale,center-h)<min(back*scale,center+h))return true;}
  }else if(abs(d.z)>1e-12){
   float t=-a.z/d.z;if(t>=front && t<=back){vec3 local=a.xyz+t*d;vec4 b=prefixRecord(i,9);if(local.x>=b.x && local.x<=b.y && local.y>=b.z && local.y<=b.w)return true;}
  }
 }
 return false;
}
float prefixOpaqueForward(){
 vec3 packed=texture(u_scene_depth_rgb,gl_FragCoord.xy/vec2(textureSize(u_scene_depth_rgb,0))).rgb;
 float depth=dot(floor(packed*255.0+.5),vec3(65536,256,1))/16777215.0;
 return exp2(depth/u_ir_log_depth.y+u_ir_log_depth.x)-1.0;
}
)";}

inline void IRPrefixReplace(std::string& source,const std::string& before,const std::string& after){
 const auto at=source.find(before);if(at==std::string::npos)throw std::runtime_error("P20 transparent shader contract changed");
 source.replace(at,before.size(),after);
}

// 只扩展透明源的云传输，不改变P18密度、形状、噪声、步数或alpha公式。
// 每个原积分步按gas透射权重累积Ccloud(z)+Tcloud(z)*Lgas。
inline void IRApplyPlumeCloudPrefix(std::string& vertex,std::string& fragment,bool gather=false){
 (void)vertex;
 const auto main=fragment.find("void main()");
 fragment.insert(main,"uniform mat4 p3d_ModelViewMatrix;\n"+IRCloudPrefixGlsl(gather));
 IRPrefixReplace(fragment,"vec3 ray=normalize(gas_ray_point-gas_ray_origin);","vec3 ray=normalize(gas_ray_point-gas_ray_origin);\nfloat p20_forward_per_local=-(p3d_ModelViewMatrix*vec4(ray,0)).z;\n");
 IRPrefixReplace(fragment,"float stepSize=(leave-enter)/float(steps),integral=0.0;",R"(
    leave=min(leave,prefixOpaqueForward()/max(p20_forward_per_local,1e-20));
    if(leave<=enter)discard;
    float stepSize=(leave-enter)/float(steps),integral=0.0;
    float tauFactor=-log(max(1.0-clamp(u_plume_opacity,0.0,1.0),.000001))*length(ray*gas_metric)/max(2.0*root*gas_metric.x,.0001);
    vec4 cloudWeighted=vec4(0);float gasT=1.0,gasWeight=0.0;
    bool cloudConstant=!prefixVaries(enter*p20_forward_per_local,leave*p20_forward_per_local);
    vec4 constantCloud=cloudConstant?prefixCloud(enter*p20_forward_per_local):vec4(0);
)");
 IRPrefixReplace(fragment,"integral+=density*stepSize;",R"(
        integral+=density*stepSize;
        float stepAlpha=1.0-exp(-density*stepSize*tauFactor);
        float weight=gasT*stepAlpha;
        gasWeight+=weight;
        if(!cloudConstant && weight>0.0)cloudWeighted+=weight*prefixCloud((enter+(float(i)+.5)*stepSize)*p20_forward_per_local);
        gasT*=1.0-stepAlpha;
)");
 IRPrefixReplace(fragment,"fragColor=vec4(vec3(source),alpha);",R"(
    vec4 prefix=cloudConstant?constantCloud:cloudWeighted/max(gasWeight,1e-20);
    float transported=prefix.r+prefix.a*source;
    fragColor=u_prefix_output==3?vec4(transported,prefix.g,prefix.r,alpha):(u_prefix_output==2?vec4(transported,prefix.rg,alpha):(u_prefix_output==1?vec4(prefix.rg,0,alpha):vec4(vec3(transported),alpha)));
)");
}

inline void IRApplyRainCloudPrefix(std::string& fragment,bool gather=false){
 fragment.insert(fragment.find("void main()"),IRCloudPrefixGlsl(gather));
 IRPrefixReplace(fragment,"fragColor=vec4(vec3(source),alpha);",R"(
    float here=1.0/gl_FragCoord.w;
    if(here>prefixOpaqueForward())discard;
    vec4 prefix=prefixCloud(here);
    float transported=prefix.r+prefix.a*source;
    fragColor=u_prefix_output==3?vec4(transported,prefix.g,prefix.r,alpha):(u_prefix_output==2?vec4(transported,prefix.rg,alpha):(u_prefix_output==1?vec4(prefix.rg,0,alpha):vec4(vec3(transported),alpha)));
)");
}
