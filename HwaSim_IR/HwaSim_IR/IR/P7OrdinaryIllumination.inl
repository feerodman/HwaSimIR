// Explicit ordinary-geometry regression entry. Uses the existing CPU evaluator
// and the unmodified production M1/L2 shader, with documented artificial inputs.
void HwaSimIR::UpdateP7OrdinaryIllumination()
{
    const char* test=std::getenv("P7OrdinaryIllumination");
    if(!test || std::string(test)!="1" || m_cameraNode.is_empty())return;
    if(m_p7IlluminationNode.is_empty()) {
        PT(GeomVertexData) data=new GeomVertexData("OrdinaryPlateSphere",GeomVertexFormat::get_v3n3t2(),Geom::UH_static);
        GeomVertexWriter vertex(data,"vertex"),normal(data,"normal"),uv(data,"texcoord");
        PT(GeomTriangles) triangles=new GeomTriangles(Geom::UH_static);
        const LPoint3f plate[]={LPoint3f(-65,0,-35),LPoint3f(-5,0,-35),LPoint3f(-5,0,35),LPoint3f(-65,0,35)};
        for(int n=0;n<4;++n){vertex.add_data3f(plate[n]);normal.add_data3f(0,-1,0);uv.add_data2f(.5,.5);}
        triangles->add_vertices(0,1,2);triangles->add_vertices(0,2,3);
        const int rows=24,cols=48;
        for(int r=0;r<=rows;++r)for(int c=0;c<=cols;++c) {
            const double lat=3.141592653589793*r/rows,lon=6.283185307179586*c/cols;
            const LVecBase3f n(float(std::sin(lat)*std::cos(lon)),float(std::sin(lat)*std::sin(lon)),float(std::cos(lat)));
            vertex.add_data3f(LPoint3f(35,0,0)+n*30);normal.add_data3f(n);uv.add_data2f(.5,.5);
        }
        for(int r=0;r<rows;++r)for(int c=0;c<cols;++c){const int i=4+r*(cols+1)+c;
            triangles->add_vertices(i,i+1,i+cols+2);triangles->add_vertices(i,i+cols+2,i+cols+1);}
        triangles->close_primitive();PT(Geom) geom=new Geom(data);geom->add_primitive(triangles);
        PT(GeomNode) geometry=new GeomNode("P7OrdinaryPlateSphere");geometry->add_geom(geom);
        m_p7IlluminationNode=m_renderRoot.attach_new_node(geometry);
        m_p7IlluminationNode.set_two_sided(true);
        PNMImage white(1,1,4);white.fill(1,1,1);white.alpha_fill(1);
        PT(Texture) texture=new Texture("P7OrdinaryWhite");texture->load(white);m_p7IlluminationNode.set_texture(texture,1);
        ApplyInfraredShader(m_p7IlluminationNode,false);
        SetShaderInputCached(m_p7IlluminationNode,"u_stage5_radiance_debug_en",LVecBase2i(1,0));
        SetShaderInputCached(m_p7IlluminationNode,"u_m1_physics_runtime_en",LVecBase2i(1,0));
        SetShaderInputCached(m_p7IlluminationNode,"u_m1_display_scale",LVecBase2f(.1f,0));
        SetShaderInputCached(m_p7IlluminationNode,"u_m1_path_radiance",LVecBase2f(2.f,0));
        // Artificial zero-emission plate isolates the existing active term in both bands.
        SetShaderInputCached(m_p7IlluminationNode,"u_material_emissivity",LVecBase2f(0,0));
        SetShaderInputCached(m_p7IlluminationNode,"u_emissivity",LVecBase2f(0,0));
        std::cout<<"[P7OrdinaryIllumination] scope=test_only geometry=plate_sphere rangeM=1000 rho=.5 tau=1 baseCommonLinear=.2 displayScale=.1 productionShader=M1_L2\n";
    }
    IRActiveIlluminatorInput input;
    input.protocolEnabled=m_realTimeSceneData.weaponState.illuminatorEn;
    input.sensorBand=m_sensorParam.trackerSensorBand==1?IRBand::NearInfrared:IRBand::MidWaveInfrared;
    const auto& profile=m_irSensorProfiles.profileForBand(input.sensorBand);
    input.sensorLowUm=profile.spectralLowUm;input.sensorHighUm=profile.spectralHighUm;
    input.protocolAngleMrad=m_sensorParam.illuminatorAngle;input.protocolSpotRadiance=m_sensorParam.illuminatorSpotRad;
    input.rangeM=1000;input.beamAngleRad=0;input.surfaceNdotL=1;
    input.tauInbound=1;input.tauInboundValid=true;input.tauFallbackReason="synthetic_uniform_tau";
    input.activeVisibility=1;input.bandReflectance=.5;input.reflectanceSource="synthetic_ordinary_plate";
    const auto out=m_l2ActiveIlluminator.evaluate(m_l2ActiveIlluminatorConfig,input);
    NodePath& node=m_p7IlluminationNode;
    node.set_mat(m_cameraNode.get_mat(m_renderRoot));
    node.set_pos(m_renderRoot,m_cameraNode.get_pos(m_renderRoot)+m_cameraNode.get_quat(m_renderRoot).get_forward()*1000.f);
    SetShaderInputCached(node,"u_ir_band_index",LVecBase2i(input.sensorBand==IRBand::NearInfrared?1:3,0));
    SetShaderInputCached(node,"u_l2_active_en",LVecBase2i(out.activeContributionEnabled?1:0,0));
    SetShaderInputCached(node,"u_l2_active_beam_profile",LVecBase2i(m_l2ActiveIlluminatorConfig.beamProfile==IRActiveBeamProfile::TopHat?1:0,0));
    SetShaderInputCached(node,"u_l2_active_source_pos_local",LVecBase3f(0,-1000,0));
    SetShaderInputCached(node,"u_l2_active_source_dir_local",LVecBase3f(0,1,0));
    SetShaderInputCached(node,"u_l2_active_half_angle_rad",LVecBase2f(float(out.halfAngleRad),0));
    SetShaderInputCached(node,"u_l2_active_center_sensor_radiance",LVecBase2f(float(out.activeSensorRadianceWm2SrUm),0));
    const auto seq=m_currentFrameTelemetry.sourceSeq;
    if(seq<=3 || seq%30==0) {
        std::ostringstream row;row<<"[P7IlluminationSample] sourceSeq="<<seq<<" protocolEn="<<input.protocolEnabled
            <<" effectiveEn="<<out.activeContributionEnabled<<" sensorBand="<<out.sensorBand<<" sourceBand="<<out.sourceBand
            <<" angleMrad="<<input.protocolAngleMrad<<" spotRaw="<<input.protocolSpotRadiance
            <<" centerReference="<<out.activeSensorRadianceWm2SrUm<<" fallback="<<out.fallbackReason;
        std::cout<<row.str()<<std::endl;
    }
}
