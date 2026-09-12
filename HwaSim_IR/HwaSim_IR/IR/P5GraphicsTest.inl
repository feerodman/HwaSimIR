// Opt-in graphics acceptance fixtures. Never changes protocol layout or production FOV.
void HwaSimIR::UpdateP5GraphicsTestScene()
{
    if(m_currentFrameTelemetry.sourceSeq==0) return;
    const char* requested = std::getenv("P5Scene");
    if (!requested || !*requested) return;
    const std::string scene(requested);
    const char* viewEnv = std::getenv("P5View");
    const std::string view = viewEnv ? viewEnv : "oblique";
    const char* materialEnv = std::getenv("P5MaterialView");
    const int materialView = materialEnv ? std::atoi(materialEnv) : 0;
    const bool asset = scene=="f22" || scene=="aim120" || scene=="aim9x";
    const bool clouds = scene=="cloud" || scene=="mixed";
    if (m_p5TestRoot.is_empty()) {
        m_p5TestRoot = m_renderRoot.attach_new_node("P5_GraphicsAcceptance_ONLY");
        m_p5TestCenter = LPoint3f(0,0,100);
        if (clouds) {
            const double testGroundZ=m_stage7GroundZOffset-(m_isInitReferencePoint?m_stage7GeoReferenceAltitudeM:0.0);
            const auto candidates = m_stage7VolumeStreaming.queryCandidates(0,0,testGroundZ,
                m_stage7WeatherState.weatherName,m_stage7WeatherState.volumeCloudProbability,
                m_stage7WeatherState.volumeCloudDensityScale,4);
            if (!candidates.empty()) {
                const auto& c = candidates.front();
                m_p5TestCenter = LPoint3f(float(c.worldX),float(c.worldY),float(c.worldZ));
            }
            m_stage7VolumeStreamingCenter = "Camera";
        }
        if (asset) {
            PLATFORM_TYPE type = scene=="f22" ? F22 : (scene=="aim120" ? AIM120 : AIM9);
            m_p5TestModel = LoadPlatformAssetNode(type,m_platformResMap[type]);
            m_p5TestModel.reparent_to(m_p5TestRoot);
            const char* bandCase=std::getenv("P5AssetBandCase");
            if(bandCase && *bandCase) {
                IRMaterialBandOptics testOptics;
                testOptics.load(std::string("Config/Tests/P5/AssetBandOptics_")+bandCase+".csv");
                m_irSceneMaterialMapper.bindPlatformNode(m_p5TestModel,m_platformResMap[type],
                    m_irMaterialDatabase,testOptics,m_l1DefaultEffectiveThicknessM);
            }
            // No scale change: camera fit only, with explicit diagnostic FOV.
            LPoint3f lo,hi;
            if (m_p5TestModel.calc_tight_bounds(lo,hi)) m_p5TestCenter=(lo+hi)*.5f;
        } else if (scene=="materials") {
            CardMaker card("P5_TwoMaterialPlate"); card.set_frame(-2,2,-2,2);
            m_p5TestModel = m_p5TestRoot.attach_new_node(card.generate());
            m_p5TestModel.set_pos(m_p5TestCenter);
            ApplyInfraredShader(m_p5TestModel,false);
            IRMaterialDatabase testDb;
            IRMaterialBandOptics testOptics;
            const char* b = std::getenv("P5MaterialCase");
            testDb.load("Config/Tests/P5/MaterialDatabase.csv");
            testOptics.load(b && std::string(b)=="B" ? "Config/Tests/P5/MaterialBandOptics_B.csv" : "Config/Tests/P5/MaterialBandOptics_A.csv");
            PlatformResPath res;
            res.displayName="P5_ARTIFICIAL_TEST_VALUES"; res.defaultMaterialName="TEST_A";
            res.materialIdTexturePath="Config/Tests/P5/two_ids.png";
            res.materialMapPath="Config/Tests/P5/two_ids.xml";
            m_irSceneMaterialMapper.bindPlatformNode(m_p5TestModel,res,testDb,testOptics,.02);
            if(std::getenv("P5Blend")) {
                m_p5TestModel.set_shader_off(200);m_p5TestModel.set_light_off();m_p5TestModel.set_texture_off(200);
                m_p5TestModel.set_color(.2f,.2f,.2f,1.f);
                NodePath layer=m_p5TestModel.attach_new_node(card.generate());
                layer.set_y(-.01f);layer.set_color(.8f,.8f,.8f,.5f);
                layer.set_transparency(TransparencyAttrib::M_alpha);layer.set_depth_write(false);
            }
        } else {
            // Ordinary open cylinder nozzle, generated geometry and fixed emission axis.
            PT(GeomVertexData) data = new GeomVertexData("P5_NeutralNozzle",GeomVertexFormat::get_v3n3t2(),Geom::UH_static);
            GeomVertexWriter v(data,"vertex"), n(data,"normal"), uv(data,"texcoord");
            PT(GeomTriangles) tri = new GeomTriangles(Geom::UH_static);
            for (int i=0;i<=24;++i) {
                const float a=float(i*6.283185307/24.0), x=std::cos(a),z=std::sin(a);
                for (int j=0;j<2;++j) { v.add_data3f(x,float(j)*2.f,z); n.add_data3f(x,0,z); uv.add_data2f(float(i)/24,float(j)); }
                if (i<24) { tri->add_vertices(i*2,i*2+2,i*2+1); tri->add_vertices(i*2+1,i*2+2,i*2+3); }
            }
            tri->close_primitive(); PT(Geom) g=new Geom(data);g->add_primitive(tri);
            PT(GeomNode) gn=new GeomNode("P5_Nozzle");gn->add_geom(g);
            m_p5TestModel=m_p5TestRoot.attach_new_node(gn);
            m_p5TestModel.set_color(.32,.32,.32,1); m_p5TestModel.set_light_off(); m_p5TestModel.set_two_sided(true);
            if(clouds && view=="behind") {
                CardMaker screen("P5_OpaquePropBehindCloud");screen.set_frame(-700,700,-700,700);
                NodePath blocker=m_p5TestRoot.attach_new_node(screen.generate());
                LVector3f offset(1040,-1600,480), direction=offset;direction.normalize();
                blocker.set_pos(m_p5TestCenter-direction*500.f);
                blocker.look_at(m_renderRoot,m_p5TestCenter+offset);
                blocker.set_color(.30,.30,.30,1);blocker.set_light_off();blocker.set_two_sided(true);
                blocker.set_depth_test(true);blocker.set_depth_write(true);
            }
            if(clouds && view=="occluded") {
                CardMaker screen("P5_CloudOpaqueForeground");screen.set_frame(-12,0,-12,12);
                NodePath blocker=m_p5TestRoot.attach_new_node(screen.generate());
                // Half-frame opaque prop, 40 m in front of the camera; cloud stays 1600 m away.
                blocker.set_pos(m_p5TestCenter+LVecBase3f(0,1560,0));
                blocker.set_color(.15,.15,.15,1);blocker.set_light_off();blocker.set_two_sided(true);
                blocker.set_depth_test(true);blocker.set_depth_write(true);
            }
            if(view=="occluded") {
                CardMaker cap("P5_OpaqueOccluder");cap.set_frame(-3,3,-3,3);
                NodePath blocker=m_p5TestModel.attach_new_node(cap.generate());
                blocker.set_pos(0,2,0);blocker.set_two_sided(true);
                blocker.set_depth_test(true);blocker.set_depth_write(true);
            }
            const bool legacy=std::getenv("P5LegacyVisuals") && std::string(std::getenv("P5LegacyVisuals"))=="1";
            NodePath* layers[]={&m_p5TestCore,&m_p5TestHalo};
            for(int i=0;i<2;++i) {
                NodePath& node=*layers[i]; node=m_p5TestRoot.attach_new_node(CreateStage5EnginePlumeBillboardNode());
                ApplyInfraredShader(node,false);
                if(!legacy) IRGameSpriteBatch::apply(node);
                node.set_transparency(TransparencyAttrib::M_alpha);node.set_depth_write(false);node.set_depth_test(true);node.set_two_sided(true);
                node.set_shader_input("u_object_kind",LVecBase2i(4,0));node.set_shader_input("u_plume_layer",LVecBase2i(i+1,0));
                node.set_shader_input("u_plume_enabled",LVecBase2i(1,0));
                node.set_shader_input("u_plume_gray",LVecBase2f(i==0?.8f:.35f,0));
                node.set_shader_input("u_plume_opacity",LVecBase2f(.8f,0));
                node.set_shader_input("u_plume_radius_root",LVecBase2f(1,0));node.set_shader_input("u_plume_radius_tail",LVecBase2f(1.5f,0));
                node.set_shader_input("u_plume_axial_decay",LVecBase2f(2,0));node.set_shader_input("u_plume_radial_decay",LVecBase2f(3,0));
                node.set_scale(i==0?1.f:1.7f,i==0?8.f:12.f,i==0?1.f:1.7f);
            }
        }
        std::cout<<"[P5GraphicsTest] scene="<<scene<<" view="<<view<<" diagnosticFovDeg=35 diagnosticNearM=0.1 modelScaleUnchanged=1 values=artificial_game_only center="<<m_p5TestCenter<<std::endl;
    }
    m_cameraLens->set_fov(35.f,35.f);
    m_cameraLens->set_near(.1f);
    LPoint3f focus=m_p5TestCenter;
    double distance=20.0;
    if(asset) {
        LPoint3f lo,hi;
        if(m_p5TestModel.calc_tight_bounds(lo,hi)) distance=std::max(1.0,double((hi-lo).length())*2.0);
    }
    if(clouds) distance=1600;
    if(view=="near") distance*=.6;
    if(view=="far") distance*=4;
    LVecBase3f offset(float(distance*.65),float(-distance),float(distance*.3));
    if(view=="end" || view=="near" || view=="far") offset=LVecBase3f(0,float(-distance),0);
    if(view=="side") offset=LVecBase3f(float(distance),0,0);
    if(view=="occluded") offset=LVecBase3f(0,float(distance),0);
    if(view=="below") offset=LVecBase3f(0,float(-distance),float(-distance*.55));
    if(view=="above") offset=LVecBase3f(0,float(-distance),float(distance*.65));
    if(view=="parallax") offset[0]+=float(std::sin(m_realTimeSceneData.time*.001)*distance*.25);
    m_cameraNode.set_pos(m_renderRoot,focus+offset);
    m_cameraNode.look_at(m_renderRoot,focus);
    UpdateStage7SkyHorizonPositionOnly();
    if(asset || scene=="materials") {
        m_p5TestModel.set_shader_input("u_p5_material_view",LVecBase2i(materialView,0));
        m_p5TestModel.set_shader_input("u_debug_material_id",LVecBase2i(materialView==6?1:0,0));
        const IRBand band=IRBandFromProtocol(m_sensorParam.trackerSensorBand);
        m_p5TestModel.set_shader_input("u_ir_band_index",LVecBase2i(int(band),0));
        if(materialView==0 && asset) {
            const PLATFORM_TYPE type=scene=="f22"?F22:(scene=="aim120"?AIM120:AIM9);
            auto radiance=EvaluateNodeRadiance(MaterialNameForPlatform(type),m_p5TestModel,false,false,false,false,0.0,1000.0);
            ApplyRadianceInputs(m_p5TestModel,radiance,0);
            TargetPlatformData testTarget={}; testTarget.type=type;testTarget.nodePath=m_p5TestModel;
            testTarget.targetState.targetID=1;testTarget.targetState.targetLoc.alt=1000;testTarget.targetState.viewValid=true;
            ApplyStage5RadianceDebug(testTarget,radiance,IRHotspotState(),IRBrightSpotState(),false,0.f,"P5_Asset",0.f);
        }
    } else {
        LPoint3f nozzle=focus;
        if(clouds) {
            // A nearby ordinary prop, separated from the world-fixed cloud.
            const LVector3f forward=m_cameraNode.get_quat(m_renderRoot).get_forward();
            const LVector3f right=m_cameraNode.get_quat(m_renderRoot).get_right();
            nozzle=m_cameraNode.get_pos(m_renderRoot)+forward*45.f+right*8.f;
        }
        m_p5TestModel.set_pos(m_renderRoot,nozzle);
        NodePath layers[]={m_p5TestCore,m_p5TestHalo};
        for(auto& node:layers) {
            node.set_pos(m_renderRoot,nozzle);
            const float elapsed=m_gameSpriteTimeOrigin<0?0.f:float(m_realTimeSceneData.time*.001-m_gameSpriteTimeOrigin);
            node.set_shader_input("u_sprite_time",LVecBase2f(elapsed,0));
            node.set_shader_input("u_time",LVecBase2f(elapsed,0));
            ApplyStage7WeatherInputs(node,m_stage7WeatherState);
            if(scene=="cloud" || view=="off") node.hide(); else node.show();
        }
    }
}
