"""实际GPU对照：两个display region与同一云相机的原几何实例，保持顺序和隐藏。"""
from pathlib import Path
import os,json,array
os.environ['P20_GAS_OUTPUT_DIR']='logs/p20/transparent_gas30_fixture'
exec(Path('tools/p20_transparent_gas_gpu_fixture.py').read_text(encoding='utf-8-sig').split('def smooth')[0])
node.set_y(30);node.set_shader(prefix);node.set_shader_input('u_prefix_output',3)
node.set_bin('fixed',71);node.set_tag('p20TransparentKind','plume')
parent=scene.attach_new_node('original_parent');node.reparent_to(parent)
cloudRoot=NodePath('cloudRoot');cm=CardMaker('cloud');cm.set_frame(-1,1,-1,1)
cloud=cloudRoot.attach_new_node(cm.generate());cloud.set_bin('fixed',80);cloud.set_depth_test(False);cloud.set_depth_write(False)
cloud.set_shader(Shader.make(Shader.SL_GLSL,'#version 130\nin vec4 p3d_Vertex;void main(){gl_Position=vec4(p3d_Vertex.x,p3d_Vertex.z,0,1);}', '#version 130\nout vec4 fragColor;void main(){fragColor=vec4(4,.872,4,.93);}'))
blend=ColorBlendAttrib.make(ColorBlendAttrib.M_add,ColorBlendAttrib.O_incoming_alpha,ColorBlendAttrib.O_one_minus_incoming_alpha,ColorBlendAttrib.M_add,ColorBlendAttrib.O_one,ColorBlendAttrib.O_one_minus_incoming_alpha)
cloudRoot.set_attrib(blend,10000)
isolated=NodePath('transparentRoot');holder=isolated.attach_new_node('holder');node.instance_to(holder)
holder.set_tag('cloudPresentationKind','plume')
state=NodePath('tagState');state.set_shader(prefix,10002);state.set_attrib(blend,10002);state.set_depth_test(False,10002);state.set_depth_write(False,10002)
def camera(name,root):
 x=Camera(name);x.set_scene(root);x.set_lens(lens);x.set_camera_mask(PandaNode.get_all_camera_mask());x.set_tag_state_key('cloudPresentationKind');x.set_tag_state('plume',state.get_state());return root.attach_new_node(x)
cc=camera('cloud',cloudRoot);tc=camera('transparent',isolated)
dr.set_camera(cc);dr.set_clear_color(Vec4(0));dr2=buffer.make_display_region();dr2.set_sort(10);dr2.set_camera(tc);dr2.set_clear_color_active(False);dr2.set_clear_depth_active(False)
def image():
 for _ in range(3):b.graphicsEngine.render_frame()
 b.graphicsEngine.extract_texture_data(tex,b.win.get_gsg());return array.array('f',bytes(tex.get_ram_image_as('RGBA')))
checks=[]
for name,action in [('visible',lambda:None),('parent_hidden',lambda:parent.hide()),('rotated',lambda:(parent.show(),parent.set_hpr(0,0,25))),('node_hidden',lambda:node.hide()),('restored',lambda:node.show())]:
 action();visible=not node.is_hidden(PandaNode.get_all_camera_mask());holder.set_mat(parent.get_mat(scene));holder.set_state(parent.get_net_state());holder.set_bin('fixed',100+node.get_bin_draw_order(),10002);holder.show() if visible else holder.hide()
 isolated.detach_node();dr2.set_active(True);separate=image()
 dr2.set_active(False);isolated.reparent_to(cloudRoot);shared=image()
 error=max(abs(x-y) for x,y in zip(separate,shared));checks.append(dict(case=name,maxError=error,visible=visible,result='PASS' if error<1e-6 else 'FAIL'))
result=dict(result='PASS' if all(c['result']=='PASS' for c in checks) else 'FAIL',checks=checks,backend=b.win.get_gsg().get_driver_renderer(),scope='同云相机与原双region像素对照；原父变换、隐藏、tag和混合顺序；非生产性能')
Path('logs/p20/shared_camera31_fixture.json').write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8');print(result);b.destroy();raise SystemExit(0 if result['result']=='PASS' else 2)
