"""原P18尾焰shader与云前缀扩展的真实GPU比较，保持原alpha与积分步数。
从实际C++扩展函数导出shader；独立CPU云数值积分参考，非客户标定。
"""
from pathlib import Path
import json,math,struct,array,os
from panda3d.core import *
from direct.showbase.ShowBase import ShowBase
root=Path(__file__).resolve().parents[1];out=root/os.environ.get('P20_GAS_OUTPUT_DIR','logs/p20/transparent_gas_fixture')
load_prc_file_data('','window-type offscreen\nwin-size 64 64\naudio-library-name null\nsync-video false\n')
b=ShowBase(windowType='offscreen');b.camNode.set_active(False)
fb=FrameBufferProperties();fb.set_rgb_color(True);fb.set_float_color(True);fb.set_rgba_bits(32,32,32,32)
tex=Texture();tex.setup_2d_texture(64,64,Texture.T_float,Texture.F_rgba32);buffer=b.win.make_texture_buffer('gas',64,64,tex,False,fb)
scene=NodePath('gas');lens=PerspectiveLens();lens.set_fov(20);lens.set_near_far(.1,100)
c=Camera('gas');c.set_scene(scene);c.set_lens(lens);cam=scene.attach_new_node(c)
dr=buffer.make_display_region();dr.set_camera(cam);dr.set_clear_color_active(True);dr.set_clear_color(Vec4(0));dr.set_clear_depth_active(True)
vd=GeomVertexData('gas',GeomVertexFormat.get_v3n3t2(),Geom.UH_static);v=GeomVertexWriter(vd,'vertex');normal=GeomVertexWriter(vd,'normal');uv=GeomVertexWriter(vd,'texcoord')
for x,y in [(-1,-1),(1,-1),(1,1),(-1,1)]:v.add_data3(x,y,0);normal.add_data3(0,0,0);uv.add_data2(0,0)
tri=GeomTriangles(Geom.UH_static);tri.add_vertices(0,1,2);tri.add_vertices(0,2,3);tri.close_primitive();geom=Geom(vd);geom.add_primitive(tri);gn=GeomNode('gas');gn.add_geom(geom);node=scene.attach_new_node(gn)
node.set_two_sided(True);node.set_depth_test(False);node.set_depth_write(False)
original=Shader.make(Shader.SL_GLSL,(out/'original.vert').read_text(encoding='utf-8'),(out/'original.frag').read_text(encoding='utf-8'))
prefix=Shader.make(Shader.SL_GLSL,(out/'prefix.vert').read_text(encoding='utf-8'),(out/'prefix.frag').read_text(encoding='utf-8'))
for k,val in dict(u_sprite_art=Vec3(1),u_sprite_time=0.,u_sprite_aspect=Vec2(1,0),u_sprite_emitters=1.,u_sprite_nozzle_offset=Vec3(0),u_plume_enabled=1,u_plume_layer=0,u_plume_gray=50.,u_plume_opacity=.8,u_plume_radius_root=.4,u_plume_radius_tail=.4,u_plume_axial_decay=4.,u_plume_radial_decay=4.,u_plume_noise_scale=1.,u_plume_noise_strength=0.,u_stage6_raw_si_domain=1,u_ir_log_depth=Vec2(math.log2(1.1),1/(math.log2(101)-math.log2(1.1)))).items():node.set_shader_input(k,val)
white=Texture();im=PNMImage(1,1,4);im.fill(1);im.alpha_fill(1);white.load(im)
records=Texture();records.setup_2d_texture(64,1,Texture.T_float,Texture.F_r32);records.set_minfilter(SamplerState.FT_nearest);records.set_magfilter(SamplerState.FT_nearest)
rows=[[0.]*4 for _ in range(12)];rows[0]=[0,-3,0,2.5];rows[1]=[.1,0,0,1];rows[2]=[0,.1,0,1];rows[3]=[0,0,.1,1];rows[4]=[4,.7,.62,.98]
records.set_ram_image(struct.pack('64f',*([x for r in rows for x in r]+[0.]*16)))
t=math.tan(math.radians(10))
for k,val in dict(u_prefix_records=records,u_prefix_sheet=white,u_scene_depth_rgb=white,u_prefix_count=1,u_prefix_output=0,u_prefix_viewport=Vec2(64),u_prefix_ray_base=Vec3(-t,1,-t),u_prefix_ray_x=Vec3(2*t,0,0),u_prefix_ray_y=Vec3(0,0,2*t)).items():node.set_shader_input(k,val)
def capture(shader):
 node.set_shader(shader)
 for _ in range(3):b.graphicsEngine.render_frame()
 b.graphicsEngine.extract_texture_data(tex,b.win.get_gsg());data=array.array('f',bytes(tex.get_ram_image_as('RGBA')))
 return data,list(data[(32*64+32)*4:(32*64+32)*4+4])
def smooth(a,b,x):q=min(1.,max(0.,(x-a)/(b-a)));return q*q*(3-2*q)
cases=[]
for distance in [float(v) for v in os.environ.get('P20_GAS_POSITIONS','10,30,50').split(',')]:
 node.clear_transparency();node.set_shader_input('u_prefix_output',0);dr.set_clear_color(Vec4(0))
 node.set_y(distance);base,basePixel=capture(original);node.set_shader_input('u_prefix_count',0);noCloud,noCloudPixel=capture(prefix)
 unchanged=max(abs(x-y) for x,y in zip(base,noCloud))
 node.set_shader_input('u_prefix_count',1);actual,px=capture(prefix)
 rx=rz=t/64;norm=math.sqrt(1+rx*rx+rz*rz);ray=[rx/norm,1/norm,rz/norm]
 # 同一P18包围盒/24步密度；云参考使用独立4096步数值积分。
 enter=(distance-1.5)/ray[1];leave=distance/ray[1];step=(leave-enter)/24;gasT=1.;weighted=0.;weightSum=0.
 for i in range(24):
  s=enter+(i+.5)*step;x=s*ray[0];y=-distance+s*ray[1];z=s*ray[2];ax=max(0.,-y)
  taper=1-smooth(.25,1.5,ax);radius=.35+.65*taper;r2=(x*x+z*z)/(radius*radius)
  density=math.exp(-ax*4*.28-r2*4)*(1-smooth(.65,1,r2))*(1-smooth(.55,1.5,ax))
  stepAlpha=1-math.exp(-density*step*(-math.log(.2))/2);w=gasT*stepAlpha;gasT*=1-stepAlpha
  forward=s*ray[1];dt=forward/4096
  tau=sum(max(0.,1-((rx*(j+.5)*dt/10)**2+(((j+.5)*dt-30)/10)**2+(rz*(j+.5)*dt/10)**2))**2*dt for j in range(4096))*2.5*norm/10
  cloudT=math.exp(-tau);weighted+=w*(4*(1-cloudT)+50*cloudT);weightSum+=w
 expected=weighted/weightSum;error=abs(px[0]-expected)
 node.set_shader_input('u_prefix_output',1);_,partial=capture(prefix)
 dt=100./20000
 fullTau=sum(max(0.,1-((rx*(j+.5)*dt/10)**2+(((j+.5)*dt-30)/10)**2+(rz*(j+.5)*dt/10)**2))**2*dt for j in range(20000))*2.5*norm/10
 fullT=math.exp(-fullTau);background=2*fullT+4*(1-fullT);cloudSource=4*(1-fullT);whiteSource=.872*(1-fullT)
 node.set_transparency(TransparencyAttrib.M_alpha);node.set_shader_input('u_prefix_output',0);dr.set_clear_color(Vec4(background,background,background,1));_,blended=capture(prefix)
 expectedBlend=px[3]*expected+(1-px[3])*background;blendError=abs(blended[0]-expectedBlend)
 node.set_shader_input('u_prefix_output',1);dr.set_clear_color(Vec4(cloudSource,whiteSource,0,1));_,optical=capture(prefix)
 expectedOptical=[partial[0]*px[3]+cloudSource*(1-px[3]),partial[1]*px[3]+whiteSource*(1-px[3])];opticalError=max(abs(x-y) for x,y in zip(optical,expectedOptical))
 node.set_shader_input('u_prefix_output',2);dr.set_clear_color(Vec4(background,cloudSource,whiteSource,1));_,packed=capture(prefix)
 packedError=max(abs(x-y) for x,y in zip(packed,[expectedBlend,*expectedOptical]))
 node.set_attrib(ColorBlendAttrib.make(ColorBlendAttrib.M_add,ColorBlendAttrib.O_incoming_alpha,ColorBlendAttrib.O_one_minus_incoming_alpha,ColorBlendAttrib.M_add,ColorBlendAttrib.O_one,ColorBlendAttrib.O_one_minus_incoming_alpha))
 dr.set_clear_color(Vec4(0));_,sparse=capture(prefix)
 recomposed=[x+y*(1-sparse[3]) for x,y in zip(sparse,[background,cloudSource,whiteSource])]
 sparseError=max(abs(x-y) for x,y in zip(recomposed,[expectedBlend,*expectedOptical]))
 node.set_shader_input('u_prefix_output',3);dr.set_clear_color(Vec4(cloudSource,whiteSource,cloudSource,1-fullT));_,merged=capture(prefix)
 mergedValues=[2*(1-merged[3])+merged[0],merged[2],merged[1]]
 mergedError=max(abs(x-y) for x,y in zip(mergedValues,[expectedBlend,*expectedOptical]));node.clear_attrib(ColorBlendAttrib)
 cases.append(dict(distance=distance,originalPixel=basePixel,noCloudPixel=noCloudPixel,cloudPixel=px,independentReference=expected,sourceError=error,cloudOffMaxPixelError=unchanged,alphaError=abs(px[3]-basePixel[3]),blendedSi=blended[0],blendedReference=expectedBlend,blendError=blendError,blendedOptical=optical[:2],opticalError=opticalError,packedPixel=packed,packedError=packedError,sparsePixel=sparse,sparseRecomposed=recomposed,sparseError=sparseError,mergedPixel=merged,mergedError=mergedError,result='PASS' if unchanged<.0001 and error<.002 and abs(px[3]-basePixel[3])<.0001 and blendError<.002 and opticalError<.002 and packedError<.002 and sparseError<.002 and mergedError<.002 else 'FAIL'))
rain=Shader.make(Shader.SL_GLSL,(out/'rain.vert').read_text(encoding='utf-8'),(out/'rain.frag').read_text(encoding='utf-8'))
node.clear_transparency();node.set_texture(white);node.set_shader_input('u_prefix_output',0);node.set_shader_input('u_precip_state',Vec4(1,0,0,20));dr.set_clear_color(Vec4(0))
for distance in [10.,30.,50.]:
 node.set_y(distance);_,px=capture(rain);dt=distance/10000
 tau=sum(max(0.,1-((rx*(j+.5)*dt/10)**2+(((j+.5)*dt-30)/10)**2+(rz*(j+.5)*dt/10)**2))**2*dt for j in range(10000))*2.5*norm/10
 T=math.exp(-tau);expected=4*(1-T)+20*T;error=abs(px[0]-expected)
 cases.append(dict(kind='rain',distance=distance,actual=px,reference=expected,sourceError=error,result='PASS' if error<.002 and abs(px[3]-.5)<.0001 else 'FAIL'))
encoded=(math.log2(21)-math.log2(1.1))/(math.log2(101)-math.log2(1.1));code=round(encoded*16777215);depth=Texture();im=PNMImage(1,1,3);im.set_xel(0,0,((code>>16)&255)/255,((code>>8)&255)/255,(code&255)/255);depth.load(im);node.set_shader_input('u_scene_depth_rgb',depth);node.set_y(30);_,px=capture(rain)
cases.append(dict(kind='rain_behind_opaque20',actual=px,result='PASS' if all(v==0 for v in px) else 'FAIL'))
result=dict(result='PASS' if all(x['result']=='PASS' for x in cases) else 'FAIL',backend=b.win.get_gsg().get_driver_renderer(),scope='实际C++生成的P18尾焰/降水扩展shader，云前中后，原始alpha逐像素，透明SI及云源blend，不透明深度裁切；未代表主程序集成/板端性能',sourceAbsTolerance=.002,alphaTolerance=.0001,cases=cases)
(out/'result.json').write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8');print(result);b.destroy();raise SystemExit(0 if result['result']=='PASS' else 2)
