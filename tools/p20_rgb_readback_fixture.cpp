// 独立Mali读回对照：保持RGB8、同步读回及Panda BGR RAM契约。
#define main p20OriginalOpaqueFixtureMain
#include "p20_board_optics_fixture.cpp"
#undef main
#include "IRNativeRgbCopy.h"
#include "callbackObject.h"
#include "callbackData.h"
#include "orthographicLens.h"
#include <chrono>
#include <cstring>
class Readback : public CallbackObject {
public:
    PT(Texture) texture;int mode=0;std::vector<unsigned char> rgb;PTA_uchar bgr;
    double readMs=0,convertMs=0;unsigned frames=0;
    Readback(Texture* t):texture(t),rgb(800*800*3){bgr.resize(rgb.size());}
    ALLOC_DELETED_CHAIN(Readback);
    void do_callback(CallbackData* data)override{
        data->upcall();if(!mode)return;
        static void* gl=dlopen("libGLESv2.so.2",RTLD_NOW|RTLD_LOCAL);
        static auto get=(void(*)(unsigned,int*))dlsym(gl,"glGetIntegerv");
        static auto bind=(void(*)(unsigned,unsigned))dlsym(gl,"glBindFramebuffer");
        static auto pack=(void(*)(unsigned,int))dlsym(gl,"glPixelStorei");
        static auto read=(void(*)(int,int,int,int,unsigned,unsigned,void*))dlsym(gl,"glReadPixels");
        static auto error=(unsigned(*)())dlsym(gl,"glGetError");
        int draw=0,previous=0,alignment=0,row=0;get(0x8CA6,&draw);get(0x8CAA,&previous);get(0x0D05,&alignment);get(0x0D02,&row);
        bind(0x8CA8,draw);pack(0x0D05,1);pack(0x0D02,0);
        const auto start=std::chrono::steady_clock::now();read(0,0,800,800,0x1907,0x1401,rgb.data());const auto ready=std::chrono::steady_clock::now();
        bind(0x8CA8,previous);pack(0x0D05,alignment);pack(0x0D02,row);
        if(error()!=0)throw std::runtime_error("direct read GL error");
        if(mode==2){auto ram=texture->modify_ram_image();IRNativeRgbCopy(rgb.data(),ram.p(),800*800,3);}
        const auto end=std::chrono::steady_clock::now();readMs+=std::chrono::duration<double,std::milli>(ready-start).count();convertMs+=std::chrono::duration<double,std::milli>(end-ready).count();++frames;
    }
};
int main(int argc,char**argv){try{
    load_prc_file_data("rgb-fixture","load-display pandagles2\nwindow-type offscreen\nwin-size 800 800\nsync-video false\naudio-library-name null\n");
    PandaFramework app;app.open_framework(argc,argv);
    FrameBufferProperties fp;fp.set_rgb_color(true);fp.set_rgba_bits(8,8,8,0);fp.set_depth_bits(0);fp.set_back_buffers(0);WindowProperties wp;wp.set_size(800,800);
    PT(GraphicsOutput) output=app.get_graphics_engine()->make_output(app.get_default_pipe(),"rgb",100,fp,wp,GraphicsPipe::BF_refuse_window|GraphicsPipe::BF_fb_props_optional);
    if(!output)throw std::runtime_error("output unavailable");
    PT(Texture) texture=new Texture("capture");texture->setup_2d_texture(800,800,Texture::T_unsigned_byte,Texture::F_rgb);
    NodePath root("rgb-scene");PT(OrthographicLens) lens=new OrthographicLens();lens->set_film_size(2,2);lens->set_near_far(-10,10);
    PT(Camera) camera=new Camera("rgb-camera");camera->set_scene(root);camera->set_lens(lens);NodePath cam=root.attach_new_node(camera);cam.set_y(-1);
    auto* dr=output->make_display_region();dr->set_camera(cam);dr->set_sort(0);
    CardMaker cm("pattern");cm.set_frame(-1,1,-1,1);auto card=root.attach_new_node(cm.generate());card.set_depth_test(false);card.set_depth_write(false);
    card.set_shader(Shader::make(Shader::SL_GLSL,"#version 300 es\nprecision highp float;uniform mat4 p3d_ModelViewProjectionMatrix;in vec4 p3d_Vertex;void main(){gl_Position=p3d_ModelViewProjectionMatrix*p3d_Vertex;}","#version 300 es\nprecision highp float;uniform vec2 frame;out vec4 color;void main(){color=vec4(mod(floor(gl_FragCoord.x)+frame.x,256.0)/255.0,mod(floor(gl_FragCoord.y)+frame.x,256.0)/255.0,mod(floor(gl_FragCoord.x+gl_FragCoord.y),256.0)/255.0,1);}"));
    NodePath empty("read-root");PT(Camera) readCam=new Camera("read-camera");readCam->set_scene(empty);readCam->set_lens(lens);auto readNode=empty.attach_new_node(readCam);
    auto* last=output->make_display_region();last->set_camera(readNode);last->set_sort(10000);PT(Readback) callback=new Readback(texture);last->set_draw_callback(callback);
    int differences=0;
    for(int mode:{1,0,2,0,2}){
        callback->mode=mode;output->clear_render_textures();if(mode!=2)output->add_render_texture(texture,GraphicsOutput::RTM_copy_ram);
        for(int i=0;i<3;++i)app.get_graphics_engine()->render_frame();
        callback->readMs=callback->convertMs=0;callback->frames=0;double total=0,maximum=0;
        for(int i=0;i<120;++i){card.set_shader_input("frame",LVecBase2f(float(i),0));auto before=texture->get_image_modified().get_seq();const auto start=std::chrono::steady_clock::now();app.get_graphics_engine()->render_frame();double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();total+=ms;maximum=std::max(maximum,ms);
            if(texture->get_image_modified().get_seq()==before)++differences;
            if(mode){auto reference=texture->get_ram_image_as("RGB");if(reference.size()!=callback->rgb.size() || std::memcmp(reference.p(),callback->rgb.data(),callback->rgb.size()))++differences;}
        }
        std::cout<<"[P20RgbFixture] mode="<<mode<<" frames=120 meanMs="<<total/120<<" maxMs="<<maximum<<" readMs="<<callback->readMs/120<<" convertMs="<<callback->convertMs/120<<" differences="<<differences<<std::endl;
    }
    std::cout<<"[P20RgbFixtureResult] result="<<(differences?"FAIL":"PASS")<<" renderer="<<output->get_gsg()->get_driver_renderer()<<std::endl;app.close_framework();return differences?2:0;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 2;}}
