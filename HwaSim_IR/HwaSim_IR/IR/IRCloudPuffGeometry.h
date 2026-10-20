#pragma once
#include <string>
// 每个世界空间云团单独绘制。片元仅计算一个平滑密度体的解析光程，
// 无动态数组、逐像素排序或三维纹理采样；Panda 对云团几何排序。
inline std::string IRCloudPuffGeometryFragment() {
    return R"(
flat in vec3 v_puff_origin;
flat in vec4 v_puff_shape;
flat in vec4 v_puff_cache;
uniform float u_density_scale,u_optical_depth,u_cloud_gray,u_spawn_fade;
uniform vec4 u_cloud_incident,u_cloud_presentation;
uniform sampler2D u_scene_depth_rgb;
uniform vec2 u_ir_log_depth;
uniform mat4 p3d_ModelViewMatrix;
uniform int u_stage6_raw_si_domain;
in vec3 v_local_pos;
out vec4 fragColor;
float primitive(float s,float q){return s*(q*q-(2.0/3.0)*q*s*s+.2*s*s*s*s);}
void main(){
    // 出口代理面覆盖完整局部光程，硬件已剔除入口面。
    vec3 origin=v_puff_origin,direction=normalize(v_local_pos-origin);
    float center=-dot(origin,direction);
    vec3 radial=origin+center*direction;
    float q=1.0-dot(radial,radial);
    if(q<=0.0)discard;
    float h=sqrt(q),enter=max(0.0,center-h),leave=center+h;
    vec3 depthBytes=texture(u_scene_depth_rgb,gl_FragCoord.xy/vec2(textureSize(u_scene_depth_rgb,0))).rgb;
    float depth=dot(depthBytes,vec3(16711680.0,65280.0,255.0))/16777215.0;
    // 整个椭球位于真实far以内且没有不透明物时，无需计算裁切。
    // 含目标或跨far的射线仍保留完整深度解码及局部光程。
    if(depth<1.0 || v_puff_cache.w<.5){
        float opaqueForward=depth>=1.0?v_puff_cache.y:exp2(depth/u_ir_log_depth.y+u_ir_log_depth.x)-1.0;
        vec3 eyeDirection=mat3(p3d_ModelViewMatrix)*(direction*v_puff_shape.xyz);
        leave=min(leave,opaqueForward/max(.000001,-eyeDirection.z));
    }
    if(leave<=enter)discard;
    // 完整区间的解析化简；云内相机及目标裁切仍用原局部原函数。
    float path=(enter<=center-h && leave>=center+h)?(16.0/15.0)*q*q*h:
        max(0.0,primitive(leave-center,q)-primitive(enter-center,q));
    // v_puff_shape.xyz 是相对原云局部坐标的尺度，保持光学厚度单位。
    path*=length(direction*v_puff_shape.xyz);
    float tau=path*v_puff_shape.w*u_density_scale*u_spawn_fade*max(0.0,u_optical_depth);
    float source=v_puff_cache.z;
    float physicalSource=source;
    if(u_cloud_presentation.x>.5){
        if(u_cloud_presentation.x<1.5)tau*=u_cloud_presentation.y;
        float light=clamp(v_puff_cache.x*1.4+.13*(-radial.x+.6*radial.z)-.08*sqrt(max(0.0,1.0-q)),.35,1.25);
        source=mix(u_cloud_presentation.z,u_cloud_presentation.w,(light-.35)/.9);
    }
    float alpha=1.0-exp(-tau);
    if(alpha<.000001)discard;
    fragColor=u_cloud_presentation.x>1.5?vec4(physicalSource,source,physicalSource,alpha):vec4(vec3(max(0.0,source)),alpha);
}
)";
}
// 顶点阶段选择实例；片元不读取动态云团数组。
inline std::string IRCloudPuffGeometryVertex(){
    return R"(
uniform mat4 p3d_ModelViewProjectionMatrix;
uniform mat4 p3d_ModelViewMatrix;
uniform vec3 u_camera_local;
uniform vec4 u_puff_center[24],u_puff_radius[24],u_puff_light[24];
in vec4 p3d_Vertex;
out vec3 v_local_pos;
flat out vec3 v_puff_origin;
flat out vec4 v_puff_shape;
flat out vec4 v_puff_cache;
void main(){
    int i=gl_InstanceID;
    vec4 center=u_puff_center[i],radius=u_puff_radius[i];
    v_local_pos=p3d_Vertex.xyz;
    v_puff_origin=(u_camera_local-center.xyz)/radius.xyz;
    v_puff_shape=vec4(radius.xyz,center.w);
    vec3 eyeCenter=(p3d_ModelViewMatrix*vec4(center.xyz,1)).xyz;
    vec3 eyeZ=vec3(p3d_ModelViewMatrix[0].z,p3d_ModelViewMatrix[1].z,p3d_ModelViewMatrix[2].z);
    float farthest=-eyeCenter.z+length(eyeZ*radius.xyz);
    v_puff_cache=vec4(radius.w,u_puff_light[i].z,u_puff_light[i].y,u_puff_light[i].z>=farthest?1.0:0.0);
    gl_Position=p3d_ModelViewProjectionMatrix*vec4(center.xyz+radius.xyz*p3d_Vertex.xyz,1);
}
)";
}
inline void IRConfigurePuffNodes(NodePath node,int index,int count,const NodePath& camera=NodePath(),bool rawSi=true,float farPlane=50000.f,std::string* cacheKey=nullptr){
    const float (*rows)[9]=(count==12?kP20PuffTemplates12:kP20PuffTemplates24)[index%4];
    std::vector<int> order;
    for(int i=0;i<count;++i)order.push_back(i);
    if(!camera.is_empty()){
        const LMatrix4f view=node.get_mat(camera);
        std::sort(order.begin(),order.end(),[&](int a,int b){
            const float* p=rows[a];const float* q=rows[b];
            return view.xform_point(LPoint3f(p[0],p[1],p[2]))[1]>view.xform_point(LPoint3f(q[0],q[1],q[2]))[1];
        });
    }
    const float gray=node.get_shader_input("u_cloud_gray").get_vector()[0];
    const float optical=node.get_shader_input("u_optical_depth").get_vector()[0];
    const float density=node.get_shader_input("u_density_scale").get_vector()[0];
    const LVecBase4f incident=node.get_shader_input("u_cloud_incident").get_vector();
    // PTA数组按对象身份进入Panda ShaderAttrib缓存，内容未变时复用对象。
    // 本项减少分配；独立长测没有证明它能消除整体状态增长，不能作为根因结论。
    // 键逐字节比较真实参数，既无浮点舍入也无哈希碰撞；空间受云池大小约束。
    // 相机矩阵、u_camera_local及fade仍逐帧更新；这里只复用不变的uniform数组。
    std::string key;
    if(cacheKey){
        key.append(reinterpret_cast<const char*>(&index),sizeof(index));
        key.append(reinterpret_cast<const char*>(&count),sizeof(count));
        key.append(reinterpret_cast<const char*>(&rawSi),sizeof(rawSi));
        const float values[]={gray,optical,density,farPlane,incident[0],incident[1],incident[2]};
        key.append(reinterpret_cast<const char*>(values),sizeof(values));
        key.append(reinterpret_cast<const char*>(order.data()),order.size()*sizeof(int));
        if(*cacheKey==key)return;
    }
    PTA_LVecBase4f centers,radii,lights;
    for(int i:order){
        const float* p=rows[i];
        centers.push_back(LVecBase4f(p[0],p[1],p[2],p[6]));
        radii.push_back(LVecBase4f(p[3],p[4],p[5],p[7]));
        const float column=2.f*p[8]*optical*density;
        const float source=rawSi?std::max(0.f,gray-incident[0]-incident[1])+incident[0]*std::exp(-column/std::max(.1f,incident[2]))+incident[1]*std::exp(-column):gray*p[7]*1.4f;
        lights.push_back(LVecBase4f(p[8],source,farPlane,0));
    }
    node.set_shader_input("u_puff_center",centers);node.set_shader_input("u_puff_radius",radii);
    node.set_shader_input("u_puff_light",lights);
    if(cacheKey)*cacheKey=key;
}
