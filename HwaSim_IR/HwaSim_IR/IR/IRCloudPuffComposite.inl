// PuffHybrid：一次云光学绘制同时输出 SI 源项、白云外观和覆盖率。
// 合成附件保持原生尺寸；最终显示只替换云源项，不重复衰减背景。
Texture* HwaSimIR::EffectiveStage6SiTexture() const {
    return m_puffSiTexture ? m_puffSiTexture.p() : m_stage6RawSceneTex.p();
}
GraphicsOutput* HwaSimIR::EffectiveStage6SiBuffer() const {
    return m_puffSiBuffer ? m_puffSiBuffer : m_stage6RawSceneBuffer.p();
}
void HwaSimIR::SetupPuffSiComposite(){
    if(m_puffSiBuffer || !m_cloudPresentationBuffer || !m_stage6RawSceneTex)return;
    m_puffSiTexture=new Texture("P20PhysicalCloudComposite");
    m_puffSiTexture->setup_2d_texture(m_stage6FinalWidth,m_stage6FinalHeight,Texture::T_half_float,Texture::F_rgba);
    m_puffSiTexture->set_minfilter(SamplerState::FT_nearest);m_puffSiTexture->set_magfilter(SamplerState::FT_nearest);
    m_puffSiBuffer=MakeStage6OffscreenOutput(m_pFramework,"P20PhysicalCloudComposite",
        m_cloudPresentationBuffer->get_sort()+1,m_stage6FinalWidth,m_stage6FinalHeight,
        m_stage6RawSceneBuffer->get_gsg(),m_stage6RawSceneBuffer,true,0,true,16,true);
    if(!m_puffSiBuffer)throw std::runtime_error("P20 physical cloud composite unavailable");
#ifndef _WIN32
    ApplyStage6GlesHalfTextureNegotiationWorkaround(m_puffSiBuffer,true);
#endif
    m_puffSiBuffer->add_render_texture(m_puffSiTexture,GraphicsOutput::RTM_bind_or_copy,GraphicsOutput::RTP_color);
    m_puffSiRoot=NodePath("P20PhysicalCloudCompositeScene");
    PT(OrthographicLens) lens=new OrthographicLens();lens->set_film_size(2,2);lens->set_near_far(-10,10);
    PT(Camera) camera=new Camera("P20PhysicalCloudCompositeCamera");camera->set_scene(m_puffSiRoot);camera->set_lens(lens);
    NodePath cam=m_puffSiRoot.attach_new_node(camera);cam.set_pos(0,-1,0);
    auto* region=m_puffSiBuffer->make_display_region();region->set_camera(cam);
    region->set_clear_color_active(true);region->set_clear_color(LColor(0,0,0,1));
    CardMaker cm("P20PhysicalCloudCompositeCard");cm.set_frame(-1,1,-1,1);
    m_puffSiCard=m_puffSiRoot.attach_new_node(cm.generate());
#ifdef _WIN32
    const std::string version="#version 130\n";
#else
    const std::string version="#version 300 es\nprecision highp float;\n";
#endif
    const std::string vertex=version+R"(
uniform mat4 p3d_ModelViewProjectionMatrix;
in vec4 p3d_Vertex;
void main(){gl_Position=p3d_ModelViewProjectionMatrix*p3d_Vertex;}
)";
    const std::string fragment=version+R"(
uniform sampler2D u_base_si,u_cloud_optical;
uniform int u_cloud_enabled;
out vec4 fragColor;
void main(){
    vec2 uv=gl_FragCoord.xy/vec2(textureSize(u_base_si,0));
    vec3 base=texture(u_base_si,uv).rgb;
    vec4 cloud=u_cloud_enabled==1?texture(u_cloud_optical,uv):vec4(0);
    fragColor=vec4(base*(1.0-cloud.a)+vec3(cloud.r),1);
}
)";
    m_puffSiCard.set_shader(Shader::make(Shader::SL_GLSL,vertex,fragment),1);
    m_puffSiCard.set_depth_test(false);m_puffSiCard.set_depth_write(false);
    m_puffSiCard.set_shader_input("u_base_si",m_stage6RawSceneTex);
    m_puffSiCard.set_shader_input("u_cloud_optical",m_cloudPresentationTexture);
    m_puffSiCard.set_shader_input("u_cloud_enabled",LVecBase2i(0,0));
    SetupPuffTransparent();
    std::cout<<"[P20CloudSharedOptics] size="<<m_stage6FinalWidth<<"x"<<m_stage6FinalHeight
        <<" attachment=RGBA16F channels=transported_SI,white_source,cloud_SI,combined_opacity opticalEvaluations=one_per_puff"
        <<" backgroundExtinction=once sceneDepth=actual_opaque logDepth=unchanged historyCache=none"<<std::endl;
}
