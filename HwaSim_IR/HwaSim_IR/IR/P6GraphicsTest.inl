// P6 fixtures are enabled only by P6Scene. ExistingTargets reads the selected
// protocol key solely to position the diagnostic camera; business data is unchanged.
std::vector<IRWorldCloudDescriptor> HwaSimIR::P6CloudDescriptors() const {
    std::vector<IRWorldCloudDescriptor> out;
    const double ground=m_stage7GroundZOffset-(m_isInitReferencePoint?m_stage7GeoReferenceAltitudeM:0.0);
    const auto cam=m_cameraNode.get_pos(m_renderRoot);
    for(int i=0;i<m_p6.count;++i){
        if(m_p6.disableMask&(1<<i))continue;
        auto d=m_stage7VolumeStreaming.descriptorForCell(10+i,20,ground,"P6OrdinaryWorld",1.0,1.0,4);
        d.worldX=m_p6.count==1?0:(i%2==0?-230:230);
        d.worldY=(i%2)*100;
        d.worldZ=ground+3100+(m_p6.count>2?(i<2?180:-180):100);
        if(m_p6.view=="overlap"){d.worldX=(i%2)*65;d.worldY=i*260;d.worldZ=ground+3150;}
        const float scale=m_p6.large?2.0f:1.0f;
        d.radiusX=d.radiusY=m_p6.radius*scale;d.radiusZ=m_p6.verticalRadius*scale;
        if(m_p6.existingTargets&&m_p6WorldCloudOriginReady){
            const auto center=m_p6WorldCloudOrigin+m_p6WorldCloudRight*float(d.worldX)+m_p6WorldCloudForward*float(d.worldY)+m_p6WorldCloudUp*float(d.worldZ-ground-3100);
            d.worldX=center[0];d.worldY=center[1];d.worldZ=center[2];
        }
        d.densityTemplate=i;d.density=m_p6.density;d.temperatureOffsetK=0;
        d.centerDistanceM=std::hypot(d.worldX-cam[0],d.worldY-cam[1]);
        out.push_back(d);
    }return out;
}
void HwaSimIR::UpdateP6GraphicsTestScene(){
    if(!m_p6.enabled||m_currentFrameTelemetry.sourceSeq==0||m_p5TestRoot.is_empty())return;
    const float time=m_gameSpriteTimeOrigin<0?0.f:float(m_realTimeSceneData.time*.001-m_gameSpriteTimeOrigin);
    const bool mixed=m_p6.scene=="mixed", display=m_p6.scene=="display";
    const double ground=m_stage7GroundZOffset-(m_isInitReferencePoint?m_stage7GeoReferenceAltitudeM:0.0);
    LPoint3f focus=mixed?LPoint3f(0,0,float(ground+3100)):LPoint3f(0,0,100);
    if(m_p6.existingTargets){
        for(auto& target:m_targetPlatformList)if(target.isExist&&!target.nodePath.is_empty()&&m_realTimeSceneData.targetNumValid>0&&
            target.targetState.targetType==m_realTimeSceneData.targetState[0].targetType&&
            target.targetState.targetPlatID==m_realTimeSceneData.targetState[0].targetPlatID&&
            target.targetState.targetID==m_realTimeSceneData.targetState[0].targetID){
            focus=target.nodePath.get_pos(m_renderRoot);
            auto targetOrientation=target.nodePath.get_quat(m_renderRoot);
            const bool smallAsset=target.type==AIM120||target.type==AIM9;
            const float cameraScale=smallAsset?.15f:1.f;
            LVector3f localCamera(45,-65,15);
            if(m_p6.view=="end")localCamera=LVector3f(0,-65,8);
            if(m_p6.view=="side")localCamera=LVector3f(65,0,8);
            if(m_p6.view=="orbit")localCamera=LVector3f(65*std::sin(time*.5f),-65*std::cos(time*.5f),15);
            m_cameraNode.set_pos(m_renderRoot,focus+(targetOrientation.get_right()*localCamera[0]+targetOrientation.get_forward()*localCamera[1]+LVector3f(0,0,localCamera[2]))*cameraScale);
            m_cameraNode.look_at(m_renderRoot,focus+LVector3f(0,0,12*cameraScale));m_cameraLens->set_fov(35,35);
            if(!m_p6WorldCloudOriginReady){
                auto q=m_cameraNode.get_quat(m_renderRoot);
                m_p6WorldCloudRight=q.get_right();m_p6WorldCloudForward=q.get_forward();m_p6WorldCloudUp=q.get_up();
                m_p6WorldCloudOrigin=focus+m_p6WorldCloudForward*1600.f+m_p6WorldCloudUp*160.f;
                m_p6WorldCloudOriginReady=true;
                std::cout<<"[P6ExistingTarget] type="<<target.targetState.targetType<<" platID="<<target.targetState.targetPlatID<<" targetID="<<target.targetState.targetID<<" position="<<focus<<" selection=full_protocol_key"<<std::endl;
            }break;
        }
    }else if(mixed){
        float move=0;
        if(m_p6.view=="translate")move=std::sin(time*.5f)*350.f;
        if(m_p6.view=="entry")move=std::sin(time*.6f)*1100.f;
        const float distance=m_p6.nativeFov?26000.f:1600.f;
        m_cameraNode.set_pos(m_renderRoot,focus+LVector3f(move,-distance,0));
        // Translation with fixed orientation tests world parallax and frustum entry.
        m_cameraNode.look_at(m_renderRoot,focus+LVector3f(move,0,0));
        if(m_p6.nativeFov)m_cameraLens->set_fov(m_sensorDisplayConfig.horizontalFovDeg,m_sensorDisplayConfig.verticalFovDeg);
        else m_cameraLens->set_fov(35,35);
    }else if(m_p6.scene=="plume"){
        float dist=m_p6.view=="near"?12.f:m_p6.view=="far"?90.f:30.f;
        LVector3f offset(0,-dist,0);
        if(m_p6.view=="side")offset=LVector3f(dist,0,0);
        if(m_p6.view=="oblique")offset=LVector3f(dist*.75f,-dist*.7f,dist*.2f);
        if(m_p6.view=="orbit")offset=LVector3f(std::sin(time*.8f)*dist,-std::cos(time*.8f)*dist,dist*.12f);
        m_cameraNode.set_pos(m_renderRoot,focus+offset);m_cameraNode.look_at(m_renderRoot,focus+LVector3f(0,-3,0));
    }
    UpdateStage7SkyHorizonPositionOnly();
    const auto quat=m_cameraNode.get_quat(m_renderRoot);
    const auto forward=quat.get_forward(),right=quat.get_right(),up=quat.get_up();
    auto fullscreen=[&](NodePath& node,const std::string& fragment,const char* name,int sort){
        if(node.is_empty()){
            CardMaker cm(name);cm.set_frame(-1,1,-1,1);node=m_p5TestRoot.attach_new_node(cm.generate());
#ifdef _WIN32
            std::string version="#version 130\n";
#else
            std::string version="#version 300 es\nprecision highp float;\nprecision highp int;\n";
#endif
            PT(Shader) shader=Shader::make(Shader::SL_GLSL,
                version+IRGameSpriteBatch::read((m_p6.root+"/Tests/P6/fullscreen.vert").c_str()),
                version+IRGameSpriteBatch::read((m_p6.root+"/"+fragment).c_str()));
            node.set_shader(shader,200);node.set_light_off();node.set_depth_test(false);node.set_depth_write(false);
            node.set_bin("background",sort);node.set_two_sided(true);
        }
        node.set_pos(m_renderRoot,m_cameraNode.get_pos(m_renderRoot)+forward*10.f);
    };
    // Plume comparisons keep background and display mapping identical.
    if(!m_p6.legacy||m_p6.scene=="plume")fullscreen(m_p6Background,"Weather/game_background.frag","P6_GameEnvironment",5);
    if(display){
        fullscreen(m_p6Display,"Tests/P6/display.frag","P6_LinearSteps",10);
        const char* caseValue=std::getenv("P6CDisplayCase");
        m_p6Display.set_shader_input("u_display_case",LVecBase2i(caseValue?std::atoi(caseValue):0,0));
        m_p6Display.set_shader_input("u_frame_idx",LVecBase2i(int(m_currentFrameTelemetry.sourceSeq),0));
        m_p5TestModel.hide();m_p5TestCore.hide();m_p5TestHalo.hide();
        for(auto& n:m_cloudNodes)n.hide();
        return;
    }
    // The generated nozzle is near the camera; world clouds remain far away.
    LPoint3f nozzle=focus;
    if(mixed)nozzle=m_cameraNode.get_pos(m_renderRoot)+forward*45.f+right*8.f-up*7.f;
    m_p5TestModel.set_pos(m_renderRoot,nozzle);m_p5TestModel.set_h(mixed?-90.f:0.f);
    m_p5TestCore.set_h(mixed?-90.f:0.f);m_p5TestHalo.set_h(mixed?-90.f:0.f);
    if(!m_p6.legacy){
        if(m_p6Core.is_empty()){
            m_p6Core=m_p5TestRoot.attach_new_node(IRGameSpriteBatch::makeGeometry());
            IRGameSpriteBatch::apply(m_p6Core);
            m_p6Core.set_shader_input("u_plume_enabled",LVecBase2i(1,0));m_p6Core.set_shader_input("u_plume_layer",LVecBase2i(1,0));
        }
        NodePath layers[]={m_p6Core,m_p5TestCore,m_p5TestHalo};
        const float widths[]={m_p6.coreRadius,m_p6.bodyRadius,m_p6.smokeRadius};
        const float lengths[]={m_p6.coreLength,m_p6.bodyLength,m_p6.smokeLength};
        const float grays[]={m_p6.coreGray,m_p6.bodyGray,m_p6.smokeGray};
        for(int i=0;i<3;++i){
            auto& n=layers[i];n.set_h(mixed?-90.f:0.f);n.set_pos(m_renderRoot,nozzle);n.set_scale(widths[i],lengths[i],widths[i]);
            n.set_shader_input("u_sprite_time",LVecBase2f(time,0));n.set_shader_input("u_plume_gray",LVecBase2f(grays[i],0));
            n.set_shader_input("u_plume_opacity",LVecBase2f(1,0));
            n.set_shader_input("u_game_sprite",LVecBase4f(1,i==0?5.f:i==1?2.2f:.8f,i==0?2.f:1.f,float(i)));
            n.set_shader_input("u_sprite_lod",LVecBase2f(std::max(8.f,std::min(32.f,1600.f/(nozzle-m_cameraNode.get_pos(m_renderRoot)).length())),0));
            n.set_shader_input("u_stage7_fog_gray",LVecBase2f(0,0));n.set_shader_input("u_stage7_fog_density",LVecBase2f(0,0));
            n.set_shader_input("u_stage7_target_contrast_scale",LVecBase2f(1,0));
            n.set_bin("transparent",i==2?5:6);
            bool show=m_p6.vfx!="off"&&(m_p6.vfx=="both"||(m_p6.vfx=="glow"&&i<2)||(m_p6.vfx=="smoke"&&i==2));
            if(show)n.show();else n.hide();
        }
    }else{
        m_p5TestCore.set_pos(m_renderRoot,nozzle);m_p5TestHalo.set_pos(m_renderRoot,nozzle);
        if(m_p6.vfx=="off"){m_p5TestCore.hide();m_p5TestHalo.hide();}
    }
    if(m_p6.view=="occluded"||m_p6.view=="behind"||m_p6.view=="blocked"){
        if(m_p6Blocker.is_empty()){
            CardMaker card("P6_OrdinaryOpaquePlate");card.set_frame(-1,1,-1,1);
            m_p6Blocker=m_p5TestRoot.attach_new_node(card.generate());
            m_p6Blocker.set_color(.16,.16,.16,1);m_p6Blocker.set_light_off();m_p6Blocker.set_two_sided(true);
        }
        const bool behind=m_p6.view=="behind";
        m_p6Blocker.set_scale(mixed?(behind?600.f:12.f):2.5f);
        m_p6Blocker.set_pos(m_renderRoot,mixed?(behind?focus+forward*550.f:m_cameraNode.get_pos(m_renderRoot)+forward*40.f-right*8.f):nozzle-forward*3.f);
        m_p6Blocker.look_at(m_renderRoot,m_cameraNode.get_pos(m_renderRoot));
    }
    if(mixed&&!m_p6.legacy){
        static PT(Texture) sheet;
        if(!sheet){sheet=TexturePool::load_texture(Filename::from_os_specific(m_p6.root+"/"+m_p6.sheetTexture));
            if(sheet){sheet->set_minfilter(SamplerState::FT_linear_mipmap_linear);sheet->set_magfilter(SamplerState::FT_linear);sheet->set_wrap_u(SamplerState::WM_repeat);sheet->set_wrap_v(SamplerState::WM_repeat);sheet->generate_ram_mipmap_images();}}
        for(auto& n:m_cloudNodes)if(sheet){n.set_texture(sheet,200);n.set_shader_input("u_stage7_cloud_mask_channel",LVecBase2i(m_p6.sheetChannel,0));n.set_shader_input("u_game_sheet",LVecBase4f(1,m_sensorParam.trackerSensorBand==1?m_p6.cloudNir:m_p6.cloudMwir,m_p6.sheetDepthScale,0));}
    }
    if(m_p6.existingTargets){
        m_p5TestModel.hide();m_p5TestCore.hide();m_p5TestHalo.hide();if(!m_p6Core.is_empty())m_p6Core.hide();
    }
    if(m_currentFrameTelemetry.sourceSeq<=3||m_currentFrameTelemetry.sourceSeq%120==0)
        std::cout<<"[P6Scene] sourceSeq="<<m_currentFrameTelemetry.sourceSeq<<" elapsedSec="<<time<<" camera="<<m_cameraNode.get_pos(m_renderRoot)
            <<" fov="<<m_cameraLens->get_fov()<<" requestedClouds="<<m_p6.count<<" vfx="<<m_p6.vfx<<" existingTargets="<<m_p6.existingTargets<<" businessParametersChanged=0"<<std::endl;
}
bool HwaSimIR::IsP6LinearCaptureRequested(std::uint64_t seq) const {
    const char* output=std::getenv("LinearDiagnosticPath"),*sample=std::getenv("LinearDiagnosticSeq");
	const char* sampleList=std::getenv("LinearDiagnosticSeqs");
    if(!output&&m_p6.enabled){output=std::getenv("P6DumpPath");sample=std::getenv("P6DumpSeq");}
	if(!output)return false;
	if(sample&&seq==std::strtoull(sample,nullptr,10))return true;
	if(sampleList&&*sampleList){
		const char* cursor=sampleList;
		while(*cursor){
			char* end=nullptr;const unsigned long long requested=std::strtoull(cursor,&end,10);
			if(end==cursor)break;
			if(seq==requested)return true;
			cursor=end;while(*cursor==','||*cursor==';'||*cursor==' '||*cursor=='\t')++cursor;
		}
	}
	return false;
}

void HwaSimIR::ArmP6LinearCapture(std::uint64_t seq){
	if(!IsP6LinearCaptureRequested(seq))return;
	if(m_stage6LinearCaptureCompletedSourceSeq==seq)return;
	m_stage6LinearReadbackSourceSeq=0;
	if(!m_stage6RawSiDomain||!m_stage6RawSceneBuffer||!m_stage6RawSceneTex){
		std::cerr<<"[P6LinearCapture][ERROR] triggered_float_readback_unavailable sourceSeq="<<seq<<std::endl;
		return;
	}
	m_stage6LinearReadbackSourceSeq=seq;
#ifdef _WIN32
	if(!m_stage6LinearReadbackTex){
		std::cerr<<"[P6LinearCapture][ERROR] triggered_float_readback_unavailable sourceSeq="<<seq<<std::endl;
		m_stage6LinearReadbackSourceSeq=0;
		return;
	}
	m_stage6LinearReadbackTex->clear_ram_image();
	m_stage6RawSceneBuffer->trigger_copy();
	std::cout<<"[P6LinearCapture] phase=armed sourceSeq="<<seq
		<<" route=RTM_triggered_copy_ram perFrameReadback=0"<<std::endl;
#else
	std::cout<<"[P6LinearCapture] phase=armed sourceSeq="<<seq
		<<" route=gles_rgba_float perFrameReadback=0"<<std::endl;
#endif
}

void HwaSimIR::CaptureP6LinearFrame(const unsigned char* pixels,int width,int height,std::uint64_t seq){
    const char* output=std::getenv("LinearDiagnosticPath"),*sample=std::getenv("LinearDiagnosticSeq");
	const char* sampleList=std::getenv("LinearDiagnosticSeqs");
    if(!output&&m_p6.enabled){output=std::getenv("P6DumpPath");sample=std::getenv("P6DumpSeq");}
    if(m_p6.enabled&&m_p6.scene=="display"&&std::getenv("P6CMappingLog")){
        std::ostringstream row;row<<std::setprecision(12)<<"[P6CMapping] sourceSeq="<<seq
            <<" fixedGain="<<m_stage6DisplayConfig.displayGain<<" offsetGray="<<m_stage6DisplayConfig.displayOffset
            <<" gamma="<<m_stage5SensorInputDisplayGamma<<" reinhard="<<m_stage6Reinhard
            <<" whiteHot="<<m_stage6DisplayConfig.whiteHot<<" automatic="<<m_stage6AgcEnabled
            <<" agcGain="<<m_stage6AgcGain<<" agcOffset="<<m_stage6AgcOffset
            <<" statisticsSourceSeq="<<m_stage6AgcLastUpdateSourceSeq<<" low="<<m_stage6AgcLowInput<<" high="<<m_stage6AgcHighInput
            <<" fallback="<<m_stage6AgcFallbackReason;
        std::cout<<row.str()<<std::endl;
    }
	bool multiSample=false;
	if(output&&sampleList&&*sampleList){
		const char* cursor=sampleList;
		while(*cursor){
			char* end=nullptr;const unsigned long long requested=std::strtoull(cursor,&end,10);
			if(end==cursor)break;
			if(seq==requested){multiSample=true;break;}
			cursor=end;while(*cursor==','||*cursor==';'||*cursor==' '||*cursor=='\t')++cursor;
		}
    }
    if(!IsP6LinearCaptureRequested(seq))return;
	if(m_stage6LinearCaptureCompletedSourceSeq==seq)return;
	const auto diagnosticBegin=std::chrono::steady_clock::now();
	auto elapsedMs=[](const std::chrono::steady_clock::time_point& begin){
		return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
	};
	IRLinearReadbackTiming statsReadbackTiming,linearReadbackTiming;
	bool statsRequested=false,statsReadbackOk=true;
	std::ostringstream captureBase;captureBase<<output;
	if(multiSample)captureBase<<"_seq"<<seq;
    std::ostringstream mapping;mapping<<std::setprecision(17)<<"[DisplayFrameMapping] sourceSeq="<<seq
        <<" agcGain="<<m_stage6AgcGain<<" agcOffset="<<m_stage6AgcOffset
        <<" fixedGain="<<m_stage6DisplayConfig.displayGain<<" offsetGray="<<m_stage6DisplayConfig.displayOffset
        <<" gamma="<<m_stage5SensorInputDisplayGamma<<" reinhard="<<m_stage6Reinhard<<" whiteHot="<<m_stage6DisplayConfig.whiteHot
        <<" statisticsSourceSeq="<<m_stage6AgcLastUpdateSourceSeq<<" captureBeforeStatisticsUpdate=1 hdrUntilAgc=1";
    std::cout<<mapping.str()<<std::endl;
	std::shared_ptr<PfmFile> stats;
    if(m_agcSampleBuffer&&m_stage6AgcEnabled){
		statsRequested=true;
		stats.reset(new PfmFile);
		if(ReadSceneLinear(m_pFramework->get_graphics_engine(),m_agcSampleTexture,m_agcSampleBuffer,
			m_agcSampleSize,m_agcSampleSize,*stats,true,&statsReadbackTiming)){}
		else{
			statsReadbackOk=false;stats.reset();
			std::cerr<<"[P6LinearCapture][ERROR] sourceSeq="<<seq
				<<" reason=agc_stats_readback_failed action=stats_pfm_not_queued"<<std::endl;
		}
    }
	std::shared_ptr<PfmFile> linear(new PfmFile);
	const bool markerMatches=m_stage6LinearReadbackSourceSeq==seq;
	bool linearReadbackOk=false;
	const char* linearReadbackRoute="none";
#ifdef _WIN32
	// Desktop GL keeps the sparse triggered-copy route. Never fall back from an
	// unattached or unfinished diagnostic texture to undefined pixels.
	const bool ramCopyReady=markerMatches&&m_stage6LinearReadbackTex&&m_stage6LinearReadbackTex->has_ram_image();
	if(!ramCopyReady){
		std::cerr<<"[P6LinearCapture][ERROR] sourceSeq="<<seq
			<<" reason=triggered_ram_copy_not_ready markerMatches="<<(markerMatches?1:0)
			<<" hasRamImage="<<((m_stage6LinearReadbackTex&&m_stage6LinearReadbackTex->has_ram_image())?1:0)
			<<" action=no_pfm_written"<<std::endl;
	}else{
		linearReadbackOk=ReadSceneLinearRamImage(m_stage6LinearReadbackTex,width,height,*linear,&linearReadbackTiming);
		linearReadbackRoute="RTM_triggered_copy_ram";
		if(!linearReadbackOk)std::cerr<<"[P6LinearCapture][ERROR] sourceSeq="<<seq
			<<" reason=triggered_ram_image_decode_failed action=no_pfm_written"<<std::endl;
	}
#else
	// The production raw texture is the actual formal SI attachment on Mali after
	// do_frame. Read it directly through the diagnostic-only temporary FBO,
	// bypassing stale RAM and failing closed if FLOAT readback is unsupported.
	if(!markerMatches){
		std::cerr<<"[P6LinearCapture][ERROR] sourceSeq="<<seq
			<<" reason=direct_readback_marker_mismatch action=no_pfm_written"<<std::endl;
	}else{
		linearReadbackOk=ReadSceneLinear(m_pFramework->get_graphics_engine(),
			m_stage6RawSceneTex,m_stage6RawSceneBuffer,width,height,*linear,false,&linearReadbackTiming);
		linearReadbackRoute="gles_rgba_float";
		if(!linearReadbackOk)std::cerr<<"[P6LinearCapture][ERROR] sourceSeq="<<seq
			<<" reason=gles_rgba_float_readback_failed action=no_pfm_written"<<std::endl;
	}
#endif
	if(!linearReadbackOk)linear.reset();
	m_stage6LinearReadbackSourceSeq=0;
	P12DiagnosticWriteJob job;
	job.sourceSeq=seq;job.captureBase=captureBase.str();job.readbackRoute=linearReadbackRoute;
	job.domain=m_stage6RawSiDomain?"spectral_radiance":"common_scaled_linear";
	job.unit=m_stage6RawSiDomain?"W/(m^2_sr_um)":"dimensionless";
	job.commonScaledLinear=m_stage6RawSiDomain?0:1;job.physicalRadiance=m_stage6RawSiDomain?1:0;
	job.width=width;job.height=height;job.linear=linear;job.stats=stats;job.statsRequired=statsRequested;
	const auto rgbCopyBegin=std::chrono::steady_clock::now();
	job.rgb.assign(pixels,pixels+static_cast<std::size_t>(width)*height*3u);
	const double rgbOwnershipCopyMs=elapsedMs(rgbCopyBegin);
	std::size_t queueDepth=0;
	const auto enqueueBegin=std::chrono::steady_clock::now();
	P12DiagnosticWriterQueue& writer=P12DiagnosticWriterQueue::instance();
	const bool queueAccepted=writer.enqueue(std::move(job),queueDepth);
	const double queueEnqueueMs=elapsedMs(enqueueBegin);
	if(queueAccepted)m_stage6LinearCaptureCompletedSourceSeq=seq;
	else std::cerr<<"[P6LinearCapture][ERROR] sourceSeq="<<seq
		<<" reason=diagnostic_writer_queue_full queueDepth="<<queueDepth
		<<" queueCapacity="<<writer.capacity()
		<<" action=required_artifacts_not_written"<<std::endl;
	std::cout<<std::fixed<<std::setprecision(3)
		<<"[P6LinearCapturePerf] sourceSeq="<<seq
		<<" mode=render_thread_gpu_readback_plus_bounded_async_cpu_writer"
		<<" renderThreadMs="<<elapsedMs(diagnosticBegin)
		<<" statsRequested="<<(statsRequested?1:0)
		<<" statsReadbackOk="<<(statsReadbackOk?1:0)
		<<" statsGpuWaitReadbackMs="<<statsReadbackTiming.gpuWaitReadbackMs
		<<" statsCpuCopyMs="<<statsReadbackTiming.cpuCopyMs
		<<" linearGlSetupMs="<<linearReadbackTiming.glSetupMs
		<<" gpuWaitReadbackMs="<<linearReadbackTiming.gpuWaitReadbackMs
		<<" readbackCpuCopyMs="<<linearReadbackTiming.cpuCopyMs
		<<" linearReadbackOk="<<(linearReadbackOk?1:0)
		<<" rgbOwnershipCopyMs="<<rgbOwnershipCopyMs
		<<" queueEnqueueMs="<<queueEnqueueMs
		<<" queueDepth="<<queueDepth
		<<" queueCapacity="<<writer.capacity()
		<<" queueAccepted="<<(queueAccepted?1:0)
		<<std::endl;
}
