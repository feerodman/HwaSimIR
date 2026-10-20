// 独立Panda状态检查：参数不变必须复用，真实参数变化必须刷新，不需要图形窗口。
#include "pandaFramework.h"
#include "camera.h"
#include "renderState.h"
#include <algorithm>
#include <vector>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include "IRCloudPuffTemplates.h"
#include "IRCloudPuffGeometry.h"
void require(bool pass,const char* why){if(!pass)throw std::runtime_error(why);}
int main(){try{
    NodePath root("fixture"),node=root.attach_new_node("cloud");
    node.set_shader_input("u_cloud_gray",LVecBase2f(.4f,0));
    node.set_shader_input("u_optical_depth",LVecBase2f(2.5f,0));
    node.set_shader_input("u_density_scale",LVecBase2f(1,0));
    node.set_shader_input("u_cloud_incident",LVecBase4f(.1f,.1f,1,96));
    std::string key;IRConfigurePuffNodes(node,0,24,NodePath(),true,1000,&key);
    CPT(RenderState) state=node.get_state();const int before=RenderState::get_num_states();
    for(int i=0;i<10000;++i)IRConfigurePuffNodes(node,0,24,NodePath(),true,1000,&key);
    require(state==node.get_state(),"unchanged parameters replaced ShaderAttrib");
    require(RenderState::get_num_states()==before,"unchanged parameters grew state cache");
    require(key.size()<256,"cache key bound");
    IRConfigurePuffNodes(node,1,24,NodePath(),true,1000,&key);require(state!=node.get_state(),"template invalidation");state=node.get_state();
    IRConfigurePuffNodes(node,1,12,NodePath(),true,1000,&key);require(state!=node.get_state(),"count invalidation");state=node.get_state();
    IRConfigurePuffNodes(node,1,12,NodePath(),true,2000,&key);require(state!=node.get_state(),"far invalidation");state=node.get_state();
    IRConfigurePuffNodes(node,1,12,NodePath(),false,2000,&key);require(state!=node.get_state(),"domain invalidation");
    node.set_shader_input("u_cloud_incident",LVecBase4f(.2f,.1f,1,96));state=node.get_state();
    IRConfigurePuffNodes(node,1,12,NodePath(),true,2000,&key);require(state!=node.get_state(),"lighting invalidation");
    PT(Camera) camera=new Camera("camera");NodePath cam=root.attach_new_node(camera);cam.set_pos(0,-30,0);
    IRConfigurePuffNodes(node,1,12,cam,true,2000,&key);const auto orderKey=key;cam.set_h(180);
    IRConfigurePuffNodes(node,1,12,cam,true,2000,&key);require(key!=orderKey,"view order invalidation");
    camera->set_camera_mask(PandaNode::get_all_camera_mask() & ~(BitMask32::bit(28)|BitMask32::bit(29)));
    require((camera->get_camera_mask()&PandaNode::get_overall_bit()).is_zero(),"reserved camera bit");
    key.clear();state=node.get_state();IRConfigurePuffNodes(node,1,12,cam,true,2000,&key);require(state!=node.get_state(),"pool reset invalidation");
    std::cout<<"{\"result\":\"PASS\",\"unchangedCalls\":10000,\"stateGrowth\":0,\"cacheKeyBytes\":"<<key.size()<<",\"scope\":\"uniform_state_reuse_and_invalidation_not_image_history\"}"<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 2;}}
