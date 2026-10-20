// 复用已验证的Mali原生800 RGBA16F附件和只读FBO提取，仅替换测试场景。
#define main p20OriginalOpaqueFixtureMain
#include "p20_board_optics_fixture.cpp"
#undef main
#include "IRCloudTransparentTransport.h"
#include "IRGameSpriteBatch.h"
#include <cstring>
#include <chrono>
int main(int argc,char** argv){try{
    if(argc==1)return p20OriginalOpaqueFixtureMain(argc,argv);
    const int layer=argc>2?std::atoi(argv[2]):1;
    const bool gather=argc>3 && std::string(argv[3])=="gather";
    const int copies=argc>4?std::atoi(argv[4]):1;
    if(copies<1 || copies>193)throw std::runtime_error("invalid prefix copies");
    int storageRows=1;while(storageRows<copies)storageRows*=2;
    const std::string config=argv[1];
    load_prc_file_data("p20-transparent-fixture","load-display pandagles2\nwindow-type offscreen\nwin-size 800 800\nsync-video false\naudio-library-name null\n");
    PandaFramework app;app.open_framework(argc,argv);PT(PerspectiveLens)lens=new PerspectiveLens();lens->set_fov(20);lens->set_near_far(.1,100);
    FrameBufferProperties hp;hp.set_rgb_color(true);hp.set_rgba_bits(8,8,8,0);hp.set_depth_bits(24);hp.set_back_buffers(0);WindowProperties hw;hw.set_size(800,800);
    PT(GraphicsOutput)host=app.get_graphics_engine()->make_output(app.get_default_pipe(),"prefix_host",100,hp,hw,GraphicsPipe::BF_refuse_window|GraphicsPipe::BF_fb_props_optional|GraphicsPipe::BF_can_bind_color);
    if(!host)throw std::runtime_error("Mali host unavailable");
    Surface s=makeSurface(app,"transparent",-10,host,lens,true);s.camera.set_y(0);
    NodePath node=s.root.attach_new_node(IRGameSpriteBatch::makeGeometry());node.set_two_sided(true);node.set_depth_test(false);node.set_depth_write(false);
    const std::string version=std::string(gather?"#version 310 es\n":"#version 300 es\n")+"precision highp float;\nprecision highp int;\n";
    std::string v=version+IRGameSpriteBatch::read((config+"/GameVFX/sprite.vert").c_str()),f=version+IRGameSpriteBatch::read((config+"/GameVFX/sprite.frag").c_str());
    std::string ov=v,of=f;IRDepthPrecision::apply(ov,of);PT(Shader)original=Shader::make(Shader::SL_GLSL,ov,of);
    IRApplyPlumeCloudPrefix(v,f,gather);
    IRDepthPrecision::apply(v,f);PT(Shader)prefix=Shader::make(Shader::SL_GLSL,v,f);
    node.set_shader_input("u_sprite_art",LVecBase3f(1));node.set_shader_input("u_sprite_time",LVecBase2f(0));node.set_shader_input("u_sprite_aspect",LVecBase2f(1,0));
    node.set_shader_input("u_sprite_emitters",LVecBase2f(1,0));node.set_shader_input("u_sprite_nozzle_offset",LVecBase3f(0));
    node.set_shader_input("u_plume_enabled",LVecBase2i(1,0));node.set_shader_input("u_plume_layer",LVecBase2i(layer,0));
    node.set_shader_input("u_plume_gray",LVecBase2f(50,0));node.set_shader_input("u_plume_opacity",LVecBase2f(.8f,0));
    node.set_shader_input("u_plume_radius_root",LVecBase2f(.4f,0));node.set_shader_input("u_plume_radius_tail",LVecBase2f(.4f,0));
    node.set_shader_input("u_plume_axial_decay",LVecBase2f(4,0));node.set_shader_input("u_plume_radial_decay",LVecBase2f(4,0));
    node.set_shader_input("u_plume_noise_scale",LVecBase2f(1,0));node.set_shader_input("u_plume_noise_strength",LVecBase2f(0));node.set_shader_input("u_stage6_raw_si_domain",LVecBase2i(1,0));
    node.set_shader_input("u_ir_log_depth",LVecBase2f(float(std::log2(1.1)),float(1/(std::log2(101)-std::log2(1.1)))));
    PT(Texture)white=new Texture();white->setup_2d_texture(1,1,Texture::T_unsigned_byte,Texture::F_rgba);PTA_uchar whiteBytes;whiteBytes.resize(4);std::fill(whiteBytes.begin(),whiteBytes.end(),255);white->set_ram_image(whiteBytes);
    PT(Texture)records=new Texture();records->setup_2d_texture(gather?32:64,storageRows*(gather?2:1),Texture::T_float,Texture::F_r32);records->set_minfilter(SamplerState::FT_nearest);records->set_magfilter(SamplerState::FT_nearest);
    LVecBase4f rows[16];for(auto& r:rows)r=LVecBase4f(0);rows[0]=LVecBase4f(0,-3,0,2.5f);rows[1]=LVecBase4f(.1f,0,0,1);rows[2]=LVecBase4f(0,.1f,0,1);rows[3]=LVecBase4f(0,0,.1f,1);rows[4]=LVecBase4f(4,.7f,.62f,.98f);
    rows[0][3]/=copies;
    std::vector<float> packed(storageRows*64,0.f);
    for(int row=0;row<copies;++row)for(int i=0;i<16;++i){
        if(gather){packed[row*64+i*2]=rows[i][3];packed[row*64+i*2+1]=rows[i][2];packed[row*64+32+i*2]=rows[i][0];packed[row*64+33+i*2]=rows[i][1];}
        else for(int c=0;c<4;++c)packed[row*64+i*4+c]=rows[i][c];
    }
    PTA_uchar bytes;bytes.resize(packed.size()*sizeof(float));std::memcpy(bytes.p(),packed.data(),bytes.size());
    records->set_ram_image(bytes);
    node.set_shader_input("u_prefix_records",records);node.set_shader_input("u_prefix_sheet",white);node.set_shader_input("u_scene_depth_rgb",white);node.set_shader_input("u_prefix_viewport",LVecBase2f(800,800));
    float t=float(std::tan(10*std::acos(-1.)/180));node.set_shader_input("u_prefix_ray_base",LVecBase3f(-t,1,-t));node.set_shader_input("u_prefix_ray_x",LVecBase3f(2*t,0,0));node.set_shader_input("u_prefix_ray_y",LVecBase3f(0,0,2*t));
    PT(DisplayRegion) region=s.output->get_display_region(s.output->get_num_display_regions()-1);
    for(int distance:{10,30,50})for(int mode=0;mode<6;++mode){
        node.clear_transparency();node.clear_attrib(ColorBlendAttrib::get_class_type());region->set_clear_color(LColor(0));
        if(mode==5){
            float alpha=1.f-std::exp(-2.5f*16.f/15.f);region->set_clear_color(LColor(4*alpha,.872f*alpha,4*alpha,alpha));
            node.set_transparency(TransparencyAttrib::M_alpha);
            node.set_attrib(ColorBlendAttrib::make(ColorBlendAttrib::M_add,ColorBlendAttrib::O_incoming_alpha,ColorBlendAttrib::O_one_minus_incoming_alpha,ColorBlendAttrib::M_add,ColorBlendAttrib::O_one,ColorBlendAttrib::O_one_minus_incoming_alpha));
        }
        node.set_y(float(distance));node.set_shader(mode==0?original:prefix);node.set_shader_input("u_prefix_count",LVecBase2i(mode==1?0:copies,0));node.set_shader_input("u_prefix_output",LVecBase2i(mode==5?3:mode==4?2:mode==3?1:0,0));
        for(int i=0;i<3;++i)app.get_graphics_engine()->render_frame();
        if(mode==5){
            void* gl=dlopen("libGLESv2.so.2",RTLD_NOW|RTLD_LOCAL);auto finish=reinterpret_cast<void(*)()>(dlsym(gl,"glFinish"));
            if(!finish)throw std::runtime_error("benchmark finish unavailable");double total=0,maximum=0;
            for(int i=0;i<30;++i){finish();auto begin=std::chrono::steady_clock::now();app.get_graphics_engine()->render_frame();finish();double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();total+=ms;maximum=std::max(maximum,ms);}
            std::cout<<"[P20PrefixBenchmark] distance="<<distance<<" copies="<<copies<<" gather="<<gather<<" measuredFrames=30 warmupFrames=3 gpuSynchronized=1 meanMs="<<total/30<<" maxMs="<<maximum<<std::endl;dlclose(gl);
        }
        saveTexture(s,"gas_"+std::to_string(distance)+"_"+std::to_string(mode)+".rgba32f",true);
    }
    app.close_framework();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 2;}}
