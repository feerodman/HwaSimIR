// Display-only cloud appearance. The SI scene attachment and its readback are
// never rebound or written here. Geometry, density and opaque ray clipping are
// shared with the physical cloud path.
#include "colorBlendAttrib.h"
void HwaSimIR::SetupCloudPresentation() {
    if(!m_stage6RawSceneBuffer || !m_stage7SceneDepthTex || m_stage6FinalCard.is_empty())return;
    if(!m_cloudPresentationBuffer) {
        m_cloudPresentationTexture=new Texture("CloudPresentationRGBA");
        m_cloudPresentationTexture->setup_2d_texture(m_stage6FinalWidth,m_stage6FinalHeight,Texture::T_unsigned_byte,Texture::F_rgba);
        m_cloudPresentationTexture->set_minfilter(SamplerState::FT_linear);
        m_cloudPresentationTexture->set_magfilter(SamplerState::FT_linear);
        m_cloudPresentationBuffer=MakeStage6OffscreenOutput(m_pFramework,"CloudPresentation",
            m_stage6RawSceneBuffer->get_sort()+1,m_stage6FinalWidth,m_stage6FinalHeight,
            m_stage6RawSceneBuffer->get_gsg(),m_stage6RawSceneBuffer,true,0,false,16,true);
        if(!m_cloudPresentationBuffer)throw std::runtime_error("cloud presentation RGBA buffer unavailable");
        m_cloudPresentationBuffer->add_render_texture(m_cloudPresentationTexture,GraphicsOutput::RTM_bind_or_copy,GraphicsOutput::RTP_color);
        m_cloudPresentationRoot=NodePath("CloudPresentationScene");
        if(!m_cloudSheetWorldRoot.is_empty())m_cloudSheetWorldRoot.instance_to(m_cloudPresentationRoot);
        if(!m_cloudVolumeWorldRoot.is_empty())m_cloudVolumeWorldRoot.instance_to(m_cloudPresentationRoot);
        PT(Camera) camera=new Camera("CloudPresentationCamera");
        camera->set_scene(m_cloudPresentationRoot);camera->set_lens(m_cameraLens);
        camera->set_tag_state_key("cloudPresentationKind");
        m_cloudPresentationCamera=m_cloudPresentationRoot.attach_new_node(camera);
#ifdef _WIN32
        const std::string version="#version 130\n";
#else
        const std::string version="#version 300 es\nprecision highp float;\nprecision highp int;\n";
#endif
        std::string vertex=version+R"(
uniform mat4 p3d_ModelViewProjectionMatrix,p3d_ModelMatrix,p3d_ModelViewMatrix,u_cloud_render_to_world;
uniform vec2 u_cloud_world_uv_reciprocal;
in vec4 p3d_Vertex;
out vec2 cloudUv;
out vec3 eyePosition;
void main(){
    gl_Position=p3d_ModelViewProjectionMatrix*p3d_Vertex;
    cloudUv=(u_cloud_render_to_world*p3d_ModelMatrix*p3d_Vertex).xy*u_cloud_world_uv_reciprocal.x;
    eyePosition=(p3d_ModelViewMatrix*p3d_Vertex).xyz;
})";
        std::string fragment=version+R"(
uniform sampler2D p3d_Texture0,u_scene_depth_rgb;
uniform vec2 u_ir_log_depth;
uniform vec2 u_cloud_uv_offset,u_cloud_base_scale,u_cloud_detail_scale,u_cloud_detail_strength,u_cloud_edge_softness;
uniform vec2 u_cloud_wind_direction,u_cloud_wind_speed_uv;
uniform float u_time;
uniform float u_stage7_cloud_coverage,u_stage7_cloud_opacity,u_stage7_cloud_optical_depth,u_cloud_layer_weight,u_cloud_hybrid_scale;
uniform int u_stage7_cloud_mask_channel;
uniform vec4 u_game_sheet,u_cloud_presentation;
uniform mat4 p3d_ModelViewMatrix;
in vec2 cloudUv;
in vec3 eyePosition;
out vec4 fragColor;
void main(){
    vec3 packed=texture(u_scene_depth_rgb,gl_FragCoord.xy/vec2(textureSize(u_scene_depth_rgb,0))).rgb;
    float opaque=dot(floor(packed*255.0+.5),vec3(65536,256,1))/16777215.0;
    float here=(log2(1.0+max(0.0,-eyePosition.z))-u_ir_log_depth.x)*u_ir_log_depth.y;
    if(here>opaque+0.0000002)discard;
    vec2 windDir=u_cloud_wind_direction/max(length(u_cloud_wind_direction),.0001);
    vec2 windBase=windDir*u_cloud_wind_speed_uv.x*u_time;
    vec2 detailDir=normalize(vec2(windDir.x-windDir.y*.27,windDir.y+windDir.x*.27));
    vec2 windDetail=detailDir*u_cloud_wind_speed_uv.x*1.31*u_time;
    vec2 uv=cloudUv*max(.05,u_cloud_base_scale.x)+u_cloud_uv_offset+windBase;
    vec4 base=texture(p3d_Texture0,uv);
    vec4 detail=texture(p3d_Texture0,cloudUv*max(.05,u_cloud_base_scale.x)*max(1.0,u_cloud_detail_scale.x)+u_cloud_uv_offset*2.17+windDetail);
    float b=dot(base.rgb,vec3(.299,.587,.114)),d=dot(detail.rgb,vec3(.299,.587,.114));
    float bm=u_stage7_cloud_mask_channel==1?base.a:smoothstep(.34,.72,b);
    float dm=u_stage7_cloud_mask_channel==1?detail.a:smoothstep(.34,.72,d);
    float density=mix(bm,dm,clamp(u_cloud_detail_strength.x,0.0,1.0));
    float coverage=clamp(u_stage7_cloud_coverage,0.0,1.0);
    float threshold=mix(.72,.32,pow(coverage,.85));
    float mask=u_stage7_cloud_mask_channel==1?density*coverage:
        smoothstep(threshold,min(.999,threshold+clamp(u_cloud_edge_softness.x,.02,.5)),density);
    float optical=max(0.0,u_stage7_cloud_optical_depth)*clamp(u_stage7_cloud_opacity,0.0,1.0)*
        clamp(u_cloud_layer_weight,0.0,1.0)*clamp(u_cloud_hybrid_scale,0.0,1.0)*mask;
    float alpha=1.0-exp(-optical*u_cloud_presentation.y);
    if(u_game_sheet.w>0.0){
        vec3 normal=normalize(mat3(p3d_ModelViewMatrix)*vec3(0,0,1));
        alpha*=smoothstep(0.0,u_game_sheet.w,abs(dot(normalize(eyePosition),normal)));
    }
    float structure=u_stage7_cloud_mask_channel==1?density:clamp((mix(b,d,.25)-.25)/.60,0.0,1.0);
    float shade=mix(.70,.98,smoothstep(.05,.95,structure));
    fragColor=vec4(vec3(shade),alpha);
})";
        IRDepthPrecision::apply(vertex,fragment);
        NodePath sheetState("CloudPresentationSheetState");
        sheetState.set_shader(Shader::make(Shader::SL_GLSL,vertex,fragment),10000);
        camera->set_tag_state("sheet",sheetState.get_state());
        auto* region=m_cloudPresentationBuffer->make_display_region();
        region->set_camera(m_cloudPresentationCamera);
        region->set_clear_color_active(true);region->set_clear_color(LColor(0,0,0,0));
        region->set_clear_depth_active(true);
        std::cout<<"[CloudPresentation] resource=RGBA8 mode="<<(m_cloudAppearance.whitePresentation?"WhiteCloud":"Radiometric")
            <<" stage=after_sensor_mapping rawSiUnchanged=1 sharedWorldGeometry=1 opaqueDepthClip=1"<<std::endl;
    }
    m_stage6FinalCard.set_shader_input("u_cloud_presentation_texture",m_cloudPresentationTexture);
    UpdateCloudPresentation();
}

void HwaSimIR::UpdateCloudPresentation() {
    const std::string requested=m_runtimeConfig.getString("Stage7Weather","CloudPresentation","CloudPresentation",
        m_cloudAppearance.whitePresentation?"WhiteCloud":"Radiometric");
    if(requested!="WhiteCloud" && requested!="Radiometric")throw std::runtime_error("invalid cloud presentation mode");
    const bool enabled=requested=="WhiteCloud" && m_stage6RawSiDomain && m_stage7WeatherEnabled && m_stage7WeatherState.cloudEnable && !m_p6.enabled;
    if(!m_stage6FinalCard.is_empty()) {
        m_stage6FinalCard.set_shader_input("u_cloud_presentation_enabled",LVecBase2i(enabled && m_cloudPresentationBuffer?1:0,0));
        m_stage6FinalCard.set_shader_input("u_cloud_presentation_texel",LVecBase2f(
            1.f/float(std::max(1,m_cloudPresentationTexture?m_cloudPresentationTexture->get_x_size():m_stage6FinalWidth)),
            1.f/float(std::max(1,m_cloudPresentationTexture?m_cloudPresentationTexture->get_y_size():m_stage6FinalHeight))));
        if(!m_cloudPresentationTexture && m_stage6RawSceneTex)
            m_stage6FinalCard.set_shader_input("u_cloud_presentation_texture",m_stage6RawSceneTex);
    }
    if(!m_cloudPresentationBuffer)return;
    // The first final-pipeline setup precedes weather node creation. Attach
    // shared subgraphs once they exist, including after INIT scene rebuilding.
    if(!m_cloudSheetWorldRoot.is_empty() && m_cloudPresentationRoot.find("PublicCloudWorldSheets").is_empty())
        m_cloudSheetWorldRoot.instance_to(m_cloudPresentationRoot);
    if(!m_cloudVolumeWorldRoot.is_empty() && m_cloudPresentationRoot.find("PublicCloudWorldVolumes").is_empty())
        m_cloudVolumeWorldRoot.instance_to(m_cloudPresentationRoot);
    m_cloudPresentationBuffer->set_active(enabled);
    if(m_stage7DepthBuffer)m_stage7DepthBuffer->set_active(enabled || m_stage7VolumeVisibleCount>0);
    if(!enabled)return;
    m_cloudPresentationRoot.set_state(m_renderRoot.get_state());
    m_cloudPresentationRoot.set_shader_input("u_cloud_presentation",LVecBase4f(1,2.5f,.62f,.98f),10000);
    m_cloudPresentationRoot.set_shader_input("u_ray_steps",LVecBase2i(24,0),10000);
    m_cloudPresentationRoot.set_shader_input("u_scene_depth_rgb",m_stage7SceneDepthTex,10000);
    const double n=m_cameraLens->get_near(),f=m_cameraLens->get_far();
    m_cloudPresentationRoot.set_shader_input("u_ir_log_depth",LVecBase2f(float(std::log2(1+n)),float(1/(std::log2(1+f)-std::log2(1+n)))),10000);
    // RGB is premultiplied by the ordinary straight-alpha blend; alpha must
    // accumulate coverage, not alpha squared. This attachment is display only.
    m_cloudPresentationRoot.set_attrib(ColorBlendAttrib::make(ColorBlendAttrib::M_add,
        ColorBlendAttrib::O_incoming_alpha,ColorBlendAttrib::O_one_minus_incoming_alpha,
        ColorBlendAttrib::M_add,ColorBlendAttrib::O_one,ColorBlendAttrib::O_one_minus_incoming_alpha),10000);
    DCAST(Camera,m_cloudPresentationCamera.node())->set_lens(m_cameraLens);
    m_cloudPresentationCamera.set_transform(m_cameraNode.get_transform(m_renderRoot));
}
