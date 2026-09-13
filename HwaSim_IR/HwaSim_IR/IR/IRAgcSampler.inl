void HwaSimIR::SetupStage6AgcSampler() {
    const bool active=IsStage6AgcEffective()&&m_stage6AgcMode!="Manual"&&m_stage6RawSceneTex&&m_stage6RawSceneBuffer;
    if(!active){if(m_agcSampleBuffer)m_agcSampleBuffer->set_active(false);return;}
    if(m_agcSampleBuffer&&m_agcAllocatedSize!=m_agcSampleSize){
        m_pFramework->get_graphics_engine()->remove_window(m_agcSampleBuffer);m_agcSampleBuffer=nullptr;
        if(!m_agcSampleRoot.is_empty())m_agcSampleRoot.remove_node();m_agcSampleRoot=NodePath();
    }
    if(!m_agcSampleBuffer){
        m_agcSampleTexture=new Texture("DisplayStratifiedSamples");
        m_agcSampleTexture->setup_2d_texture(m_agcSampleSize,m_agcSampleSize,Texture::T_half_float,Texture::F_rgb);
        m_agcSampleBuffer=MakeStage6OffscreenOutput(m_pFramework,"DisplayStratifiedSamples",90,
            m_agcSampleSize,m_agcSampleSize,m_stage6RawSceneBuffer->get_gsg(),m_stage6RawSceneBuffer,true,0,true);
        if(!m_agcSampleBuffer)throw std::runtime_error("Auto sample color buffer unavailable");
        m_agcSampleBuffer->add_render_texture(m_agcSampleTexture,GraphicsOutput::RTM_copy_texture);
        m_agcSampleRoot=NodePath("AutoSampleRoot");
        PT(Camera) camera=new Camera("AutoSampleCamera");PT(OrthographicLens) lens=new OrthographicLens();
        lens->set_film_size(2,2);lens->set_near_far(-10,10);camera->set_lens(lens);
        NodePath cameraNode=m_agcSampleRoot.attach_new_node(camera);cameraNode.set_pos(0,-1,0);
        auto* region=m_agcSampleBuffer->make_display_region();region->set_camera(cameraNode);
        region->set_clear_color_active(true);region->set_clear_depth_active(true);
        CardMaker cm("AutoSampleCard");cm.set_frame(-1,1,-1,1);m_agcSampleCard=m_agcSampleRoot.attach_new_node(cm.generate());
#ifdef _WIN32
        std::string version="#version 130\n";
#else
        std::string version="#version 300 es\nprecision highp float;\nprecision highp int;\n";
#endif
        auto shader=Shader::make(Shader::SL_GLSL,version+R"(
in vec4 p3d_Vertex;
void main(){gl_Position=vec4(p3d_Vertex.x,p3d_Vertex.z,0.0,1.0);}
)",version+R"(
uniform sampler2D u_linear_scene;
uniform vec2 u_source_size;
uniform vec2 u_sample_size;
out vec4 fragColor;
void main(){
    // One existing pixel per equal-area stratum, no averaging or mip filtering.
    ivec2 p=ivec2(floor(gl_FragCoord.xy*u_source_size/u_sample_size));
    p=clamp(p,ivec2(0),ivec2(u_source_size)-ivec2(1));
    fragColor=vec4(texelFetch(u_linear_scene,p,0).rgb,1.0);
}
)");
        if(!shader)throw std::runtime_error("Auto sample shader unavailable");
        m_agcSampleCard.set_shader(shader,1);m_agcSampleCard.set_depth_test(false);m_agcSampleCard.set_depth_write(false);
        m_agcAllocatedSize=m_agcSampleSize;
        std::cout<<"[AutoSampler] method=stratified_pixel_centers size="<<m_agcSampleSize
            <<" format=RGB16F fullFrameCPURead=0 sceneDepthAttachmentChanged=0 nearestTexelFetch=1"<<std::endl;
    }
    m_agcSampleCard.set_shader_input("u_linear_scene",m_stage6RawSceneTex);
    m_agcSampleCard.set_shader_input("u_source_size",LVecBase2f(float(m_stage6FinalWidth),float(m_stage6FinalHeight)));
    m_agcSampleCard.set_shader_input("u_sample_size",LVecBase2f(float(m_agcSampleSize),float(m_agcSampleSize)));
    m_agcSampleBuffer->set_active(true);
}

void HwaSimIR::PrepareStage6AgcSampleFrame() {
    if(!m_agcSampleBuffer)return;
    const double elapsed=double(IRPerfStats::steadyTimeNs()-m_stage6AgcLastUpdateNs)/1.e9;
    bool due=!m_stage6AgcInitialized||m_stage6AgcLastUpdateNs==0||elapsed>=1.0/std::max(.1,m_stage6AgcUpdateHz);
    // Explicit paired PFM diagnostics require the current frame's samples too.
    const char* sequence=std::getenv("LinearDiagnosticSeq");
    if(!sequence&&m_p6.enabled)sequence=std::getenv("P6DumpSeq");
    if(sequence&&m_currentFrameTelemetry.sourceSeq==std::strtoull(sequence,nullptr,10))due=true;
    m_agcSampleBuffer->set_active(IsStage6AgcEffective()&&m_stage6AgcMode!="Manual"&&due);
}
