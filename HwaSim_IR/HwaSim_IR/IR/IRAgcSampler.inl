void HwaSimIR::SetupStage6AgcSampler() {
    const bool active=IsStage6AgcEffective()&&m_stage6AgcMode!="Manual"&&m_stage6RawSceneTex&&m_stage6RawSceneBuffer;
    if(!active){if(m_agcSampleBuffer)m_agcSampleBuffer->set_active(false);return;}
    if(m_agcSampleBuffer&&m_agcAllocatedSize!=m_agcSampleSize){
        m_pFramework->get_graphics_engine()->remove_window(m_agcSampleBuffer);m_agcSampleBuffer=nullptr;
        m_agcSampleCopyRamPending=false;
        if(!m_agcSampleRoot.is_empty())m_agcSampleRoot.remove_node();m_agcSampleRoot=NodePath();
    }
    if(!m_agcSampleBuffer){
        m_agcSampleTexture=new Texture("DisplayStratifiedSamples");
#ifdef _WIN32
        m_agcSampleTexture->setup_2d_texture(m_agcSampleSize,m_agcSampleSize,Texture::T_half_float,Texture::F_rgb);
        const bool agcSampleFloatColor=true;
        const char* agcSampleFormat="RGB16F";
#else
        // The RK3588 GLES driver cannot convert an RGB16F render target to a
        // Panda RAM image reliably.  This surface already contains normalized
        // display-window samples, so RGB8 is sufficient for AGC statistics
        // and does not alter the production SI floating-point raw texture.
        m_agcSampleTexture->setup_2d_texture(m_agcSampleSize,m_agcSampleSize,Texture::T_unsigned_byte,Texture::F_rgb);
        const bool agcSampleFloatColor=false;
        const char* agcSampleFormat="RGB8_UNORM";
#endif
        m_agcSampleBuffer=MakeStage6OffscreenOutput(m_pFramework,"DisplayStratifiedSamples",90,
            m_agcSampleSize,m_agcSampleSize,m_stage6RawSceneBuffer->get_gsg(),m_stage6RawSceneBuffer,
            true,0,agcSampleFloatColor);
        if(!m_agcSampleBuffer)throw std::runtime_error("Auto sample color buffer unavailable");
        // The 64x64 sampler is active only when statistics are due.  On Linux,
        // defer the copy-RAM attachment itself until a protocol frame owns the
        // source image; making the output inactive after attachment is too late
        // because Panda prepares a new RAM target on its first graphics tick.
#ifdef _WIN32
        m_agcSampleBuffer->add_render_texture(m_agcSampleTexture,GraphicsOutput::RTM_copy_ram);
        m_agcSampleCopyRamPending=false;
#else
        m_agcSampleBuffer->set_active(false);
        m_agcSampleCopyRamPending=true;
        std::cout<<"[Stage6 AgcReadbackGate] phase=setup"
            <<" attached=0 pending=1 active=0 platform=linux"<<std::endl;
#endif
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
uniform int u_raw_si_domain;
uniform float u_raw_to_display_scale;
uniform float u_raw_to_display_offset;
out vec4 fragColor;
void main(){
    // One existing pixel per equal-area stratum, no averaging or mip filtering.
    ivec2 p=ivec2(floor(gl_FragCoord.xy*u_source_size/u_sample_size));
    p=clamp(p,ivec2(0),ivec2(u_source_size)-ivec2(1));
	vec3 sampleValue=texelFetch(u_linear_scene,p,0).rgb;
	if(u_raw_si_domain==1){
		sampleValue=sampleValue*u_raw_to_display_scale+vec3(u_raw_to_display_offset);
	}
	fragColor=vec4(sampleValue,1.0);
}
)");
        if(!shader)throw std::runtime_error("Auto sample shader unavailable");
        m_agcSampleCard.set_shader(shader,1);m_agcSampleCard.set_depth_test(false);m_agcSampleCard.set_depth_write(false);
        m_agcAllocatedSize=m_agcSampleSize;
        std::cout<<"[AutoSampler] method=stratified_pixel_centers size="<<m_agcSampleSize
            <<" format="<<agcSampleFormat
            <<" rawSceneFormatUnchanged=1 fullFrameCPURead=0 sceneDepthAttachmentChanged=0 nearestTexelFetch=1"<<std::endl;
    }
    m_agcSampleCard.set_shader_input("u_linear_scene",m_stage6RawSceneTex);
	m_agcSampleCard.set_shader_input("u_source_size",LVecBase2f(float(m_stage6FinalWidth),float(m_stage6FinalHeight)));
	m_agcSampleCard.set_shader_input("u_sample_size",LVecBase2f(float(m_agcSampleSize),float(m_agcSampleSize)));
	double rawMinimum=0.0,rawMaximum=1.0;Stage6PhysicalDisplayWindow(rawMinimum,rawMaximum);
	const double rawSpan=std::max(1.0e-12,rawMaximum-rawMinimum);
	m_agcSampleCard.set_shader_input("u_raw_si_domain",LVecBase2i(m_stage6RawSiDomain?1:0,0));
	m_agcSampleCard.set_shader_input("u_raw_to_display_scale",LVecBase2f(float(m_stage6RawSiDomain?1.0/rawSpan:1.0),0));
	m_agcSampleCard.set_shader_input("u_raw_to_display_offset",LVecBase2f(float(m_stage6RawSiDomain?-rawMinimum/rawSpan:0.0),0));
	// Setup can run before START while the graphics engine is already servicing
	// frames.  Keep the RAM-copy sampler dormant until PrepareStage6AgcSampleFrame
	// observes an owned protocol frame; otherwise Panda attempts one conversion
	// from an uninitialized render target during startup on Mali.
	m_agcSampleBuffer->set_active(false);
}

void HwaSimIR::PrepareStage6AgcSampleFrame() {
    if(!m_agcSampleBuffer)return;
    const double elapsed=double(IRPerfStats::steadyTimeNs()-m_stage6AgcLastUpdateNs)/1.e9;
    bool due=!m_stage6AgcInitialized||m_stage6AgcLastUpdateNs==0||elapsed>=1.0/std::max(.1,m_stage6AgcUpdateHz);
    if(m_stage6AgcInitialized&&m_realTimeSceneData.time==m_stage6AgcLastSimulationMs)due=false;
    // Every explicitly requested PFM (single value or list) requires samples
    // from this exact source frame, not the previous periodic AGC update.
    if(IsP6LinearCaptureRequested(m_currentFrameTelemetry.sourceSeq))due=true;
    // Do not ask Panda to copy the sampler before a protocol frame owns valid
    // raw-scene pixels.  The pre-START do_frame loop exists only to service
    // commands and previously caused one uninitialized conversion attempt.
    const bool sourceFrameActive=m_syncFrameActive.load()&&m_currentFrameTelemetry.sourceSeq>0;
    const bool sampleFrameActive=IsStage6AgcEffective()&&m_stage6AgcMode!="Manual"&&sourceFrameActive&&due;
#ifndef _WIN32
    if(sampleFrameActive&&m_agcSampleCopyRamPending){
        m_agcSampleBuffer->add_render_texture(m_agcSampleTexture,GraphicsOutput::RTM_copy_ram);
        m_agcSampleCopyRamPending=false;
        std::cout<<"[Stage6 AgcReadbackGate] phase=first_sample_frame"
            <<" attached=1 pending=0 sourceSeq="<<m_currentFrameTelemetry.sourceSeq
            <<" platform=linux"<<std::endl;
    }
#endif
    m_agcSampleBuffer->set_active(sampleFrameActive);
}
