"""实际GLSL云前缀函数的独立GPU检查；不替代renderer集成及性能验收。"""
from pathlib import Path
import json,math,struct,re,os
from panda3d.core import *
from direct.showbase.ShowBase import ShowBase
root=Path(__file__).resolve().parents[1];out=root/os.environ.get('P20_PREFIX_OUTPUT_DIR','logs/p20/transparent_prefix_fixture');out.mkdir(exist_ok=True)
source=(root/'HwaSim_IR/HwaSim_IR/IR/IRCloudTransparentTransport.h').read_text(encoding='utf-8-sig')
glsl=source.split('return R"(',1)[1].split(')";}',1)[0]
load_prc_file_data('','window-type offscreen\nwin-size 32 32\naudio-library-name null\nsync-video false\n')
b=ShowBase(windowType='offscreen');b.camNode.set_active(False)
fb=FrameBufferProperties();fb.set_rgb_color(True);fb.set_float_color(True);fb.set_rgba_bits(32,32,32,32)
target=Texture();target.setup_2d_texture(32,32,Texture.T_float,Texture.F_rgba32)
buffer=b.win.make_texture_buffer('prefix',32,32,target,False,fb)
scene=NodePath('prefix');lens=OrthographicLens();lens.set_film_size(2,2);lens.set_near_far(-10,10)
camera=Camera('prefix');camera.set_scene(scene);camera.set_lens(lens);cn=scene.attach_new_node(camera);cn.set_y(-1)
region=buffer.make_display_region();region.set_camera(cn)
card=CardMaker('prefix');card.set_frame(-1,1,-1,1);node=scene.attach_new_node(card.generate());node.set_depth_test(False);node.set_depth_write(False)
vertex='#version 130\nuniform mat4 p3d_ModelViewProjectionMatrix;in vec4 p3d_Vertex;void main(){gl_Position=p3d_ModelViewProjectionMatrix*p3d_Vertex;}'
fragment='#version 130\nuniform vec2 u_ir_log_depth;\n'+glsl+'\nuniform float u_test_depth;out vec4 fragColor;void main(){fragColor=prefixCloud(u_test_depth);}'
(out/'prefix.frag').write_text(fragment,encoding='utf-8')
node.set_shader(Shader.make(Shader.SL_GLSL,vertex,fragment))
records=Texture();records.setup_2d_texture(64,1,Texture.T_float,Texture.F_r32);records.set_minfilter(SamplerState.FT_nearest);records.set_magfilter(SamplerState.FT_nearest)
sheet=Texture();pnm=PNMImage(1,1,4);pnm.fill(.8,.8,.8);pnm.alpha_fill(1);sheet.load(pnm)
node.set_shader_input('u_prefix_records',records);node.set_shader_input('u_prefix_sheet',sheet);node.set_shader_input('u_scene_depth_rgb',sheet)
node.set_shader_input('u_prefix_count',1);node.set_shader_input('u_prefix_output',0);node.set_shader_input('u_prefix_viewport',Vec2(32,32))
node.set_shader_input('u_prefix_ray_base',Vec3(0,1,0));node.set_shader_input('u_prefix_ray_x',Vec3(0));node.set_shader_input('u_prefix_ray_y',Vec3(0));node.set_shader_input('u_ir_log_depth',Vec2(0,1))
cases=[]
for kind in ['puff','sheet','hot_puff']:
 for depth in [10.,25.,30.,35.,50.]:
  row=[[0.]*4 for _ in range(12)]
  if kind!='sheet':
   strength=20. if kind=='hot_puff' else 2.5
   row[0]=[0,-3,0,strength];row[1]=[.1,0,0,1];row[2]=[0,.1,0,1];row[3]=[0,0,.1,1];row[4]=[4,.7,.62,.98]
   # 独立高分辨率中点积分，单位是云局部坐标，不复用GLSL原函数。
   steps=20000;step=(depth/10)/steps
   tau=sum(max(0.,1-(-3+(i+.5)*step)**2)**2*strength*step for i in range(steps))
   T=math.exp(-tau);light=min(1.25,max(.35,.7*1.4));white=.62+(.98-.62)*(light-.35)/.9
  else:
   row[0]=[0,0,-30,2];row[1]=[1,0,0,0];row[2]=[0,0,1,0];row[3]=[0,1,0,4]
   row[4]=[0,0,1,1];row[7]=[1,0,0,0];row[8]=[1,0,.1,0];row[9]=[-100,100,-100,100];row[10]=[0,0,1,0];row[11][3]=1
   T=math.exp(-2) if depth>=30 else 1.
   structure=(.8-.25)/.60;v=min(1.,max(0.,(structure-.05)/.90));white=.70+.28*v*v*(3-2*v)
  records.set_ram_image(struct.pack('64f',*([x for r in row for x in r]+[0.]*16)));node.set_shader_input('u_test_depth',depth)
  for _ in range(3):b.graphicsEngine.render_frame()
  b.graphicsEngine.extract_texture_data(target,b.win.get_gsg());data=target.get_ram_image_as('RGBA');actual=struct.unpack_from('4f',data,(16*32+16)*16)
  reference=[4*(1-T),white*(1-T),0,T];err=max(abs(x-y) for x,y in zip(actual,reference))
  hotError=abs((actual[0]+actual[3]*1e8)-(reference[0]+reference[3]*1e8)) if kind=='hot_puff' and depth==50 else None
  cases.append(dict(kind=kind,depth=depth,actual=actual,reference=reference,maxError=err,hotSourceError=hotError,result='PASS' if err<.0001 and (hotError is None or hotError<.0001) else 'FAIL'))
result=dict(result='PASS' if all(c['result']=='PASS' for c in cases) else 'FAIL',backend=b.win.get_gsg().get_driver_renderer(),scope='实际生产GLSL前缀函数；独立密度数值积分；非完整renderer/非视觉验收',absTolerance=.0001,cases=cases)
(out/'result.json').write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8');print(result);b.destroy()
raise SystemExit(0 if result['result']=='PASS' else 2)
