// P20 独立 RK3588 光学夹具：真实不透明几何、原生800视口、生产puff着色器。
#include "pandaFramework.h"
#include "graphicsEngine.h"
#include "graphicsOutput.h"
#include "graphicsPipe.h"
#include "graphicsStateGuardian.h"
#include "frameBufferProperties.h"
#include "windowProperties.h"
#include "displayRegion.h"
#include "texture.h"
#include "textureContext.h"
#include "camera.h"
#include "perspectiveLens.h"
#include "cardMaker.h"
#include "geomVertexWriter.h"
#include "geomVertexFormat.h"
#include "geomTriangles.h"
#include "geomNode.h"
#include "colorBlendAttrib.h"
#include "cullFaceAttrib.h"
#include "load_prc_file.h"
#include "pta_LVecBase4.h"
#include <algorithm>
#include <vector>
#include <cmath>
#include <fstream>
#include <iostream>
#include <dlfcn.h>
#include "IRCloudPuffTemplates.h"
#include "IRCloudPuffGeometry.h"
#include "IRDepthPrecision.h"
struct Surface{PT(Texture) texture;PT(GraphicsOutput) output;NodePath root,camera;};
Surface makeSurface(PandaFramework& app,const char* name,int sort,GraphicsOutput* host,Lens* lens,bool floating){
    Surface s;s.texture=new Texture(name);s.texture->setup_2d_texture(800,800,floating?Texture::T_half_float:Texture::T_unsigned_byte,Texture::F_rgba);
    s.texture->set_minfilter(SamplerState::FT_nearest);s.texture->set_magfilter(SamplerState::FT_nearest);
    FrameBufferProperties f;f.set_rgb_color(true);f.set_rgba_bits(floating?16:8,floating?16:8,floating?16:8,floating?16:8);f.set_float_color(floating);f.set_depth_bits(24);f.set_back_buffers(0);
    WindowProperties w;w.set_size(800,800);
    s.output=app.get_graphics_engine()->make_output(app.get_default_pipe(),name,sort,f,w,GraphicsPipe::BF_refuse_window|(host?GraphicsPipe::BF_can_bind_every:(GraphicsPipe::BF_fb_props_optional|GraphicsPipe::BF_can_bind_color)),host?host->get_gsg():nullptr,host);
    if(!s.output)throw std::runtime_error("fixture output unavailable");
    if(floating)const_cast<FrameBufferProperties&>(s.output->get_fb_properties()).set_float_color(false);
    s.output->add_render_texture(s.texture,GraphicsOutput::RTM_bind_or_copy,GraphicsOutput::RTP_color);
    s.root=NodePath(name);PT(Camera)c=new Camera(name);c->set_scene(s.root);c->set_lens(lens);s.camera=s.root.attach_new_node(c);s.camera.set_y(-30);
    DisplayRegion* r=s.output->make_display_region();r->set_camera(s.camera);r->set_clear_color_active(true);r->set_clear_color(floating?LColor(0,0,0,0):LColor(1,1,1,1));r->set_clear_depth_active(true);return s;
}
// 与正式SI诊断相同的只读临时FBO；读取后恢复绑定，不改场景附件。
void saveTexture(Surface& s,const std::string& file,bool floating){
    void* lib=dlopen("libGLESv2.so.2",RTLD_NOW|RTLD_LOCAL);if(!lib)throw std::runtime_error("GLES missing");
    using U=unsigned;using I=int;
#define GLFN(name,type) auto name=reinterpret_cast<type>(dlsym(lib,#name));if(!name)throw std::runtime_error(#name)
    GLFN(glGetIntegerv,void(*)(U,I*));GLFN(glGenFramebuffers,void(*)(I,U*));GLFN(glBindFramebuffer,void(*)(U,U));
    GLFN(glFramebufferTexture2D,void(*)(U,U,U,U,I));GLFN(glCheckFramebufferStatus,U(*)(U));GLFN(glReadPixels,void(*)(I,I,I,I,U,U,void*));
    GLFN(glDeleteFramebuffers,void(*)(I,const U*));GLFN(glGetError,U(*)());GLFN(glPixelStorei,void(*)(U,I));
    U prior=glGetError();if(prior)std::cout<<"[P20Fixture] priorGlError="<<prior<<" before_read=1"<<std::endl;
    auto* gsg=s.output->get_gsg();auto* context=s.texture->prepare_now(0,gsg->get_prepared_objects(),gsg);if(!context)throw std::runtime_error("texture not prepared");
    std::cout<<"[P20FixtureBackend] renderer="<<gsg->get_driver_renderer()<<" version="<<gsg->get_driver_version()<<std::endl;
    I old=0;glGetIntegerv(0x8CAA,&old);U fbo=0;glGenFramebuffers(1,&fbo);glBindFramebuffer(0x8CA8,fbo);glFramebufferTexture2D(0x8CA8,0x8CE0,0x0DE1,context->get_native_id(),0);
    if(glCheckFramebufferStatus(0x8CA8)!=0x8CD5)throw std::runtime_error("fixture read FBO incomplete");
    U setupError=glGetError();if(setupError)throw std::runtime_error("fixture setup GL error="+std::to_string(setupError));
    glPixelStorei(0x0D05,1);std::ofstream out(file,std::ios::binary);
    if(floating){std::vector<float> v(800*800*4);glReadPixels(0,0,800,800,0x1908,0x1406,v.data());out.write(reinterpret_cast<const char*>(v.data()),v.size()*sizeof(float));}
    else{std::vector<unsigned char> v(800*800*4);glReadPixels(0,0,800,800,0x1908,0x1401,v.data());for(int i=0;i<800*800;++i)out.write(reinterpret_cast<const char*>(v.data()+i*4),3);}
    U error=glGetError();glBindFramebuffer(0x8CA8,U(old));glDeleteFramebuffers(1,&fbo);dlclose(lib);if(error)throw std::runtime_error("fixture read GL error="+std::to_string(error));
}
int main(int argc,char** argv){try{
    load_prc_file_data("p20-fixture","load-display pandagles2\nwindow-type offscreen\nwin-size 800 800\nsync-video false\naudio-library-name null\n");
    PandaFramework app;app.open_framework(argc,argv);PT(PerspectiveLens)lens=new PerspectiveLens();lens->set_fov(45);lens->set_near_far(.1,1000);
    const LVecBase2f logDepth(float(std::log2(1.1)),float(1/(std::log2(1001)-std::log2(1.1))));
    FrameBufferProperties hostProps;hostProps.set_rgb_color(true);hostProps.set_rgba_bits(8,8,8,0);hostProps.set_depth_bits(24);hostProps.set_back_buffers(0);
    WindowProperties hostWindow;hostWindow.set_size(800,800);
    PT(GraphicsOutput)host=app.get_graphics_engine()->make_output(app.get_default_pipe(),"fixture_host",100,hostProps,hostWindow,GraphicsPipe::BF_refuse_window|GraphicsPipe::BF_fb_props_optional|GraphicsPipe::BF_can_bind_color);
    if(!host)throw std::runtime_error("fixture host unavailable");
    Surface depth=makeSurface(app,"actual_opaque_depth",-20,host,lens,false);Surface optical=makeSurface(app,"shared_optical",-10,depth.output,lens,true);
    const std::string version="#version 300 es\nprecision highp float;\nprecision highp int;\n";
    CardMaker cm("small_actual_target");cm.set_frame(-1,1,-1,1);NodePath target=depth.root.attach_new_node(cm.generate());target.set_two_sided(true);
    std::string dv=version+R"(uniform mat4 p3d_ModelViewProjectionMatrix,p3d_ModelViewMatrix;in vec4 p3d_Vertex;out vec3 eye;void main(){gl_Position=p3d_ModelViewProjectionMatrix*p3d_Vertex;eye=(p3d_ModelViewMatrix*p3d_Vertex).xyz;})";
    std::string df=version+R"(uniform vec2 u_ir_log_depth;in vec3 eye;out vec4 fragColor;void main(){float d=clamp((log2(1.0+max(0.0,-eye.z))-u_ir_log_depth.x)*u_ir_log_depth.y,0.0,1.0);float n=floor(d*16777215.0+.5);float r=floor(n/65536.0);n-=r*65536.0;float g=floor(n/256.0);fragColor=vec4(r,g,n-g*256.0,255)/255.0;})";
    IRDepthPrecision::apply(dv,df);target.set_shader(Shader::make(Shader::SL_GLSL,dv,df));target.set_shader_input("u_ir_log_depth",logDepth);
    PT(GeomVertexData)data=new GeomVertexData("conservative_puff",GeomVertexFormat::get_v3(),Geom::UH_static);GeomVertexWriter writer(data,"vertex");
    const double pi=std::acos(-1.);
    for(int z=0;z<=10;++z)for(int s=0;s<=18;++s){double a=-pi/2+pi*z/10,b=2*pi*s/18;writer.add_data3f(float(1.07*std::cos(a)*std::cos(b)),float(1.07*std::cos(a)*std::sin(b)),float(1.07*std::sin(a)));}
    PT(GeomTriangles)t=new GeomTriangles(Geom::UH_static);for(int z=0;z<10;++z)for(int s=0;s<18;++s){int a=z*19+s,b=a+1,c=a+19,d=c+1;t->add_vertices(a,b,c);t->add_vertices(b,d,c);}t->close_primitive();PT(Geom)g=new Geom(data);g->add_primitive(t);PT(GeomNode)n=new GeomNode("puff");n->add_geom(g);
    NodePath cloud=optical.root.attach_new_node(n);cloud.set_scale(10);cloud.set_instance_count(24);cloud.set_depth_test(false);cloud.set_depth_write(false);cloud.set_attrib(CullFaceAttrib::make(CullFaceAttrib::M_cull_counter_clockwise),2);cloud.set_transparency(TransparencyAttrib::M_alpha);
    cloud.set_attrib(ColorBlendAttrib::make(ColorBlendAttrib::M_add,ColorBlendAttrib::O_incoming_alpha,ColorBlendAttrib::O_one_minus_incoming_alpha,ColorBlendAttrib::M_add,ColorBlendAttrib::O_one,ColorBlendAttrib::O_one_minus_incoming_alpha));
    std::string vertex=version+IRCloudPuffGeometryVertex(),fragment=version+IRCloudPuffGeometryFragment(),dummy=fragment;IRDepthPrecision::apply(vertex,dummy);cloud.set_shader(Shader::make(Shader::SL_GLSL,vertex,fragment));
    PTA_LVecBase4f centers,radii,lights;for(int i=0;i<24;++i){auto*p=kP20PuffTemplates24[0][i];centers.push_back(LVecBase4f(p[0],p[1],p[2],p[6]));radii.push_back(LVecBase4f(p[3],p[4],p[5],p[7]));lights.push_back(LVecBase4f(p[8],.4,1000,0));}
    cloud.set_shader_input("u_puff_center",centers);cloud.set_shader_input("u_puff_radius",radii);cloud.set_shader_input("u_puff_light",lights);
    cloud.set_shader_input("u_density_scale",LVecBase2f(1,0));cloud.set_shader_input("u_optical_depth",LVecBase2f(2.5,0));cloud.set_shader_input("u_spawn_fade",LVecBase2f(1,0));cloud.set_shader_input("u_cloud_presentation",LVecBase4f(2,1,.62,.98));cloud.set_shader_input("u_scene_depth_rgb",depth.texture);cloud.set_shader_input("u_ir_log_depth",logDepth);
    NodePath second=cloud.copy_to(optical.root);second.set_pos(3,15,0);second.hide();
    const char* relations[]={"front","inside","behind","camera_inside","edge_right6","edge_right10","edge_left6","fov20","fov80","two_clouds"};const float distances[]={10,30,50,5,30,30,30,30,30,50};
    for(int relation=0;relation<10;++relation)for(int pixels:{1,3,7}){
        const float x=relation==4?6.f:relation==5?10.f:relation==6?-6.f:0.f;
        const float fov=relation==7?20.f:relation==8?80.f:45.f;lens->set_fov(fov);
        float origin=relation==3?-2.5f:-30.f;depth.camera.set_pos(x,origin,0);optical.camera.set_pos(x,origin,0);cloud.set_shader_input("u_camera_local",LVecBase3f(x/10,origin/10,0));
        if(relation==9){second.show();second.set_shader_input("u_camera_local",LVecBase3f((x-3)/10,(origin-15)/10,0));}else second.hide();
        target.set_pos(x,origin+distances[relation],0);float half=distances[relation]*float(std::tan(fov*pi/360))*pixels/800;target.set_scale(half,1,half);
        for(int i=0;i<3;++i)app.get_graphics_engine()->render_frame();std::string name=std::string(relations[relation])+"_"+std::to_string(pixels)+"px";
        saveTexture(optical,name+".rgba32f",true);saveTexture(depth,name+".depth.rgb8",false);std::cout<<"[P20Fixture] case="<<name<<" rendered=800x800 opticalStorage=RGBA16F readback=RGBA_FLOAT opaqueSource=actual_geometry"<<std::endl;
    }
    app.close_framework();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 2;}}
