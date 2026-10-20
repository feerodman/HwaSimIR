// 同一云相机/附件追加透明贡献；物理源和白云源分通道。
#include "IRCloudTransparentTransport.h"
#include "cullBinManager.h"
void HwaSimIR::SetupPuffTransparent(){
    if(!m_puffOpticalRoot.is_empty() || !m_puffSiBuffer)return;
#ifndef _WIN32
    const char* gatherSetting=std::getenv("HwaSimIRPuffPrefixGather");
    m_puffPrefixGather=!(gatherSetting && std::string(gatherSetting)=="0");
#endif
    m_renderRoot.hide(BitMask32::bit(27));
    m_puffPrefixRecords=new Texture("P20CloudPrefixRecords");
    // 部署版Panda不支持RGBA浮点上传转换。R32F标量布局不经过BGRA重排。
    m_puffPrefixRecords->setup_2d_texture(m_puffPrefixGather?32:64,m_puffPrefixGather?2:1,Texture::T_float,Texture::F_r32);
    m_puffPrefixRecords->set_minfilter(SamplerState::FT_nearest);m_puffPrefixRecords->set_magfilter(SamplerState::FT_nearest);
    std::cout<<"[P20PrefixPacking] gather="<<m_puffPrefixGather<<" scalarFormat=R32F valuesQuantized=0 texelFetchesPerVec4="<<(m_puffPrefixGather?1:4)<<std::endl;
#ifdef _WIN32
    const std::string version="#version 130\n";
#else
    const std::string version=std::string(m_puffPrefixGather?"#version 310 es\n":"#version 300 es\n")+"precision highp float;\nprecision highp int;\n";
#endif
    const std::string plainVertex=version+"uniform mat4 p3d_ModelViewProjectionMatrix;in vec4 p3d_Vertex;void main(){gl_Position=p3d_ModelViewProjectionMatrix*p3d_Vertex;}";
    PT(Shader) discardShader=Shader::make(Shader::SL_GLSL,plainVertex,version+"out vec4 fragColor;void main(){discard;}");
    NodePath discardState("P20TransparentDiscard");discardState.set_shader(discardShader,10001);
    discardState.set_depth_write(false,10001);discardState.set_depth_test(false,10001);
    // 已在独立云路径绘制的透明几何，不再向原SI路径提交空discard draw。
    // 仅相机tag状态进入专用禁用bin，不修改节点原bin或业务hide/show。
    auto* bins=CullBinManager::get_global_ptr();
    int suppressed=bins->find_bin("p20_transparent_suppressed");
    if(suppressed<0)suppressed=bins->add_bin("p20_transparent_suppressed",CullBinManager::BT_unsorted,0);
    bins->set_bin_active(suppressed,false);
    discardState.set_bin("p20_transparent_suppressed",0,10001);
    m_puffDiscardState=discardState.get_state();
    std::string plumeVertex=version+IRGameSpriteBatch::read("Config/GameVFX/sprite.vert"),plumeFragment=version+IRGameSpriteBatch::read("Config/GameVFX/sprite.frag");
    IRApplyPlumeCloudPrefix(plumeVertex,plumeFragment,m_puffPrefixGather);IRDepthPrecision::apply(plumeVertex,plumeFragment);
    PT(Shader) plumeShader=Shader::make(Shader::SL_GLSL,plumeVertex,plumeFragment);
    std::string rainVertex=version+IRGameSpriteBatch::read("Config/Weather/precipitation.vert"),rainFragment=version+IRGameSpriteBatch::read("Config/Weather/precipitation.frag");
    IRApplyRainCloudPrefix(rainFragment,m_puffPrefixGather);IRDepthPrecision::apply(rainVertex,rainFragment);
    PT(Shader) rainShader=Shader::make(Shader::SL_GLSL,rainVertex,rainFragment);
    // 只排队至正常图形帧准备，不跨线程调用GL。
    plumeShader->prepare(m_stage6RawSceneBuffer->get_gsg()->get_prepared_objects());
    rainShader->prepare(m_stage6RawSceneBuffer->get_gsg()->get_prepared_objects());
    m_puffOpticalRoot=m_cloudPresentationRoot.attach_new_node("P20TransparentCloudSources");
    {
        auto* camera=DCAST(Camera,m_cloudPresentationCamera.node());
        // 场景仅含云和原透明几何共享实例，保留节点mask与业务隐藏。
        camera->set_camera_mask(PandaNode::get_all_camera_mask());
        for(int kind=0;kind<2;++kind){
            NodePath state("P20TransparentSourceState");state.set_shader(kind?rainShader:plumeShader,10002);
            state.set_depth_test(false,10002);state.set_depth_write(false,10002);
            state.set_shader_input("u_prefix_output",LVecBase2i(3,0),10002);
            state.set_transparency(TransparencyAttrib::M_alpha,10002);
            state.set_attrib(ColorBlendAttrib::make(ColorBlendAttrib::M_add,ColorBlendAttrib::O_incoming_alpha,ColorBlendAttrib::O_one_minus_incoming_alpha,
                ColorBlendAttrib::M_add,ColorBlendAttrib::O_one,ColorBlendAttrib::O_one_minus_incoming_alpha),10002);
            camera->set_tag_state(kind?"rain":"plume",state.get_state());
        }
    }
    std::cout<<"[P20TransparentCloud] actualDepthPrefix=1 gasSteps=unchanged originalAssets=unchanged noInverseTransmittance=1 separateSiAndWhiteSource=1 sharedOpticalAttachment=1 sharedCloudCamera=1 channels=transportedSI,cloudWhite,cloudSI,combinedOpacity transparentDraws=one"<<std::endl;
}

void HwaSimIR::UpdatePuffTransparent(){
    UpdateCloudPresentation();
    if(m_puffOpticalRoot.is_empty())return;
    const BitMask32 mask=BitMask32::bit(27),rawMask=PandaNode::get_all_camera_mask() & ~(mask|BitMask32::bit(28)|BitMask32::bit(29));
    bool transparentVisible=false;
    // 只实例化原透明几何，保留原父状态/变换/业务隐藏，不遍历完整场景。
    std::vector<PandaNode*> live;
    const auto instance=[&](const NodePath& node,bool visible){
        auto key=node.node();live.push_back(key);auto found=m_puffTransparentInstances.find(key);
        if(found==m_puffTransparentInstances.end()){
            NodePath holder=m_puffOpticalRoot.attach_new_node("P20TransparentInstance");node.instance_to(holder);
            holder.set_tag("cloudPresentationKind",node.get_tag("p20TransparentKind"));
            found=m_puffTransparentInstances.insert(std::make_pair(key,holder)).first;
        }
        NodePath& holder=found->second;
        if(visible){
            holder.set_mat(node.get_parent().get_mat(m_renderRoot));holder.set_state(node.get_parent().get_net_state());
            // 云先绘制；保留尾流70/核心71/雨雪100的原相对顺序。
            holder.set_bin("fixed",100+node.get_bin_draw_order(),10002);
            holder.show();transparentVisible=true;
        }else holder.hide();
    };
    for(auto& target:m_targetPlatformList){NodePath layers[]={target.enginePlumeCoreNodePath,target.enginePlumeHaloNodePath};
        for(auto& node:layers)if(!node.is_empty()){
            const bool visible=!node.is_hidden(rawMask) && node.get_shader_input("u_plume_enabled").get_vector()[0]>.5f;
            instance(node,visible);
        }
    }
    for(auto& node:m_stage7PrecipitationNodes)if(!node.is_empty()){
        const bool visible=!node.is_hidden(BitMask32::bit(29));
        instance(node,visible);
    }
    for(auto i=m_puffTransparentInstances.begin();i!=m_puffTransparentInstances.end();){
        if(std::find(live.begin(),live.end(),i->first)==live.end()){i->second.remove_node();i=m_puffTransparentInstances.erase(i);}else ++i;
    }
    const bool active=transparentVisible && m_stage6RawSiDomain && m_stage7WeatherEnabled && m_stage7WeatherState.cloudEnable && !m_p6.enabled;
    if(active)m_puffOpticalRoot.show();else m_puffOpticalRoot.hide();
    const auto setSuppression=[&](const NodePath& cam){
        if(cam.is_empty() || !cam.node()->is_of_type(Camera::get_class_type()))return;
        auto* c=DCAST(Camera,cam.node());c->set_tag_state_key("p20TransparentKind");
        for(const char* key:{"plume","rain"}){if(active)c->set_tag_state(key,m_puffDiscardState);else c->clear_tag_state(key);}
    };
    if(m_stage6RawSceneRegion)setSuppression(m_stage6RawSceneRegion->get_camera());
    setSuppression(m_stage7VolumeCameraNode);
    if(!active)return;
    if(m_stage7VolumeRegion)m_stage7VolumeRegion->set_active(false);
    // 该节点已位于cloudPresentationRoot下，原场景状态从父节点继承。
    // 不每帧复制完整ShaderInput表再逐项重建；这里只保留专属前缀参数。
    struct Group{NodePath node;int volume;float depth;};std::vector<Group> groups;
    // 原volume路径祖先仅向29位开放，查询实际28位共享实例路径。
    NodePath cloudInstance=m_cloudPresentationRoot.find("PublicCloudWorldVolumes");
    for(size_t i=0;i<m_stage7CloudVolumePool.size();++i){auto& v=m_stage7CloudVolumePool[i];
        if(!cloudInstance.is_empty() && v.active && v.visibleWanted && v.fade>.001 && !NodePath(cloudInstance,v.node.node()).is_hidden(BitMask32::bit(28)))groups.push_back({v.node,int(i),v.node.get_pos(m_cameraNode)[1]});}
    for(auto& n:m_cloudNodes)if(!n.is_empty() && !n.is_hidden(BitMask32::bit(28)))groups.push_back({n,-1,n.get_pos(m_cameraNode)[1]});
    std::stable_sort(groups.begin(),groups.end(),[](const Group& a,const Group& b){return a.depth>b.depth;});
    std::vector<LVecBase4f> records;
    const auto append=[&](const std::vector<LVecBase4f>& row){records.insert(records.end(),row.begin(),row.end());records.resize(records.size()+4,LVecBase4f(0));};
    for(const auto& group:groups){
        NodePath n=group.node;const auto get=[&](const char* key){return n.get_shader_input(InternalName::make(key)).get_vector();};
        const LMatrix4f local=m_cameraNode.get_mat(n);
        const LPoint3f origin=local.xform_point(LPoint3f(0,0,0));
        const LVector3f x=local.xform_vec(LVector3f(1,0,0)),y=local.xform_vec(LVector3f(0,1,0)),z=local.xform_vec(LVector3f(0,0,1));
        if(group.volume>=0){
            auto& volume=m_stage7CloudVolumePool[group.volume];
            const float (*rows)[9]=(m_cloudPuffCount==12?kP20PuffTemplates12:kP20PuffTemplates24)[volume.descriptor.densityTemplate%4];
            std::vector<int> order;for(int i=0;i<m_cloudPuffCount;++i)order.push_back(i);
            const LMatrix4f view=n.get_mat(m_cameraNode);
            std::stable_sort(order.begin(),order.end(),[&](int a,int b){return view.xform_point(LPoint3f(rows[a][0],rows[a][1],rows[a][2]))[1]>view.xform_point(LPoint3f(rows[b][0],rows[b][1],rows[b][2]))[1];});
            const auto incident=get("u_cloud_incident");const float density=get("u_density_scale")[0],optical=get("u_optical_depth")[0],gray=get("u_cloud_gray")[0];
            for(int i:order){const float* p=rows[i];std::vector<LVecBase4f> row(12,LVecBase4f(0));
                row[0]=LVecBase4f((origin[0]-p[0])/p[3],(origin[1]-p[1])/p[4],(origin[2]-p[2])/p[5],p[6]*density*optical*float(volume.fade));
                row[1]=LVecBase4f(x[0]/p[3],x[1]/p[4],x[2]/p[5],p[3]);row[2]=LVecBase4f(y[0]/p[3],y[1]/p[4],y[2]/p[5],p[4]);row[3]=LVecBase4f(z[0]/p[3],z[1]/p[4],z[2]/p[5],p[5]);
                const float column=2*p[8]*optical*density;
                const float source=std::max(0.f,gray-incident[0]-incident[1])+incident[0]*std::exp(-column/std::max(.1f,incident[2]))+incident[1]*std::exp(-column);
                row[4]=LVecBase4f(source,p[7],.62f,.98f);append(row);
            }
        }else{
            std::vector<LVecBase4f> row(12,LVecBase4f(0));
            const float optical=std::max(0.f,get("u_stage7_cloud_optical_depth")[0])*std::max(0.f,std::min(1.f,get("u_stage7_cloud_opacity")[0]))*std::max(0.f,std::min(1.f,get("u_cloud_layer_weight")[0]))*std::max(0.f,std::min(1.f,get("u_cloud_hybrid_scale")[0]));
            row[0]=LVecBase4f(origin[0],origin[1],origin[2],optical);
            row[1]=LVecBase4f(x[0],x[1],x[2],get("u_cloud_inside_factor")[0]);row[2]=LVecBase4f(y[0],y[1],y[2],get("u_game_sheet")[3]);row[3]=LVecBase4f(z[0],z[1],z[2],get("u_stage7_cloud_gray")[0]);
            // 相机到世界云坐标的同一刚性矩阵，避免大世界坐标相减损失UV相位。
            const LMatrix4f world=m_cameraNode.get_mat(m_cloudSheetWorldRoot);const auto w0=world.xform_point(LPoint3f(0,0,0));
            const auto wx=world.xform_vec(LVector3f(1,0,0)),wy=world.xform_vec(LVector3f(0,1,0)),wz=world.xform_vec(LVector3f(0,0,1));
            const float scale=get("u_cloud_world_uv_reciprocal")[0];const auto offset=get("u_cloud_uv_offset");
            row[4]=LVecBase4f(w0[0]*scale,w0[1]*scale,get("u_cloud_base_scale")[0],get("u_cloud_detail_scale")[0]);
            row[5]=LVecBase4f(wx[0]*scale,wx[1]*scale,wy[0]*scale,wy[1]*scale);row[6]=LVecBase4f(wz[0]*scale,wz[1]*scale,offset[0],offset[1]);
            auto wind=get("u_cloud_wind_direction");row[7]=LVecBase4f(wind[0],wind[1],get("u_cloud_wind_speed_uv")[0],get("u_time")[0]);
            row[8]=LVecBase4f(get("u_stage7_cloud_coverage")[0],get("u_cloud_detail_strength")[0],get("u_cloud_edge_softness")[0],m_stage7WeatherState.cloudMaskUsesAlpha?1.f:0.f);
            float half=float(m_stage7CloudWorldSizeM*.5);row[9]=LVecBase4f(-half,half,-half,half);row[10]=get("u_game_sheet");row[11][3]=1;append(row);
        }
    }
    const int count=int(records.size()/16),capacity=std::max(1,int(m_stage7CloudVolumePool.size())*m_cloudPuffCount+int(m_cloudNodes.size()));
    static unsigned auditFrame=0;++auditFrame;if(auditFrame<=2 || auditFrame%60==0)std::cout<<"[P20TransparentPrefix] active=1 records="<<count<<" groups="<<groups.size()<<" visibleVolumeCount="<<m_stage7VolumeVisibleCount<<" sharedCloudCamera=1"<<std::endl;
    if(count>capacity)throw std::runtime_error("P20 prefix descriptor capacity exceeded");
    int storageRows=1;while(storageRows<capacity)storageRows*=2;
    const int storageWidth=m_puffPrefixGather?32:64,storageHeight=storageRows*(m_puffPrefixGather?2:1);
    if(m_puffPrefixRecords->get_x_size()!=storageWidth || m_puffPrefixRecords->get_y_size()!=storageHeight)m_puffPrefixRecords->setup_2d_texture(storageWidth,storageHeight,Texture::T_float,Texture::F_r32);
    records.resize(size_t(storageRows)*16,LVecBase4f(0));
    PTA_uchar bytes;bytes.resize(records.size()*sizeof(LVecBase4f));
    if(m_puffPrefixGather){
        // 只重排位模式，不量化；归一化坐标分母为2的幂，gather精确取2x2。
        for(int row=0;row<storageRows;++row)for(int j=0;j<16;++j){
            const auto& v=records[row*16+j];const float lower[]={v[3],v[2]},upper[]={v[0],v[1]};
            std::memcpy(bytes.p()+(row*64+j*2)*sizeof(float),lower,sizeof(lower));
            std::memcpy(bytes.p()+(row*64+32+j*2)*sizeof(float),upper,sizeof(upper));
        }
    }else std::memcpy(bytes.p(),records.data(),bytes.size());
    // 连续标量直接上传，无颜色通道转换。每记录64标量中前48个有效。
    m_puffPrefixRecords->set_ram_image(bytes);
    m_puffOpticalRoot.set_shader_input("u_prefix_records",m_puffPrefixRecords);
    m_puffOpticalRoot.set_shader_input("u_prefix_sheet",m_stage7CloudTexture?m_stage7CloudTexture:m_stage7SceneDepthTex);
    m_puffOpticalRoot.set_shader_input("u_prefix_count",LVecBase2i(count,0));m_puffOpticalRoot.set_shader_input("u_prefix_viewport",LVecBase2f(float(m_stage6FinalWidth),float(m_stage6FinalHeight)));
    m_puffOpticalRoot.set_shader_input("u_scene_depth_rgb",m_stage7SceneDepthTex);
    LPoint3f nearPoint,base,rx,ry;m_cameraLens->extrude(LPoint2f(-1,-1),nearPoint,base);m_cameraLens->extrude(LPoint2f(1,-1),nearPoint,rx);m_cameraLens->extrude(LPoint2f(-1,1),nearPoint,ry);
    base/=base[1];rx/=rx[1];ry/=ry[1];
    m_puffOpticalRoot.set_shader_input("u_prefix_ray_base",LVecBase3f(base));m_puffOpticalRoot.set_shader_input("u_prefix_ray_x",LVecBase3f(rx-base));m_puffOpticalRoot.set_shader_input("u_prefix_ray_y",LVecBase3f(ry-base));
}
