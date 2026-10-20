void HwaSimIR::SetupDirectRgbReadback(){
#ifndef _WIN32
    if(m_directRgbReadback || !IsHeadlessOffscreenMode() || !m_stage6FinalSensorBuffer || !m_renderTex)return;
    if(!m_runtimeConfig.getBool("RenderBackend","DirectRgbReadback","HwaSimIRDirectRgbReadback",false))return;
    if(m_stage6FinalSensorBuffer->get_fb_properties().get_multisamples()!=0)throw std::runtime_error("DirectRgbReadback requires a resolved non-MSAA final buffer");
    const bool audit=m_runtimeConfig.getBool("RenderBackend","DirectRgbReadbackAudit","HwaSimIRDirectRgbReadbackAudit",false);
    NodePath empty("P20ReadbackScene");PT(Camera) camera=new Camera("P20ReadbackCamera");
    PT(OrthographicLens) lens=new OrthographicLens();lens->set_film_size(2,2);lens->set_near_far(-10,10);camera->set_lens(lens);camera->set_scene(empty);
    NodePath node=empty.attach_new_node(camera);auto* region=m_stage6FinalSensorBuffer->make_display_region();
    region->set_camera(node);region->set_sort(10000);region->set_clear_color_active(false);region->set_clear_depth_active(false);
    m_directRgbReadback=new IRDirectRgbReadback(m_renderTex,m_stage6FinalWidth,m_stage6FinalHeight,audit);region->set_draw_callback(m_directRgbReadback);
    m_stage6FinalSensorBuffer->clear_render_textures();
    if(audit)m_stage6FinalSensorBuffer->add_render_texture(m_renderTex,GraphicsOutput::RTM_copy_ram);
    else m_renderTex->setup_2d_texture(m_stage6FinalWidth,m_stage6FinalHeight,Texture::T_unsigned_byte,Texture::F_rgb);
    m_headlessCopyRamAttached=audit;m_stage6FinalCopyRamPending=false;m_stage6FinalSensorBuffer->set_active(true);
    std::cout<<"[P20DirectRgbReadback] enabled=1 size="<<m_stage6FinalWidth<<"x"<<m_stage6FinalHeight<<" format=RGB8 ramOrder=BGR bottomUp=1 synchronous=1 PBO=0 imageIdentity=modify_ram_image audit="<<audit<<" afterAnnotation=1"<<std::endl;
#endif
}
