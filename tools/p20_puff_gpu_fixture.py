"""独立 Windows GPU 光学夹具：使用生产着色器和真实几何深度，不使用标注框。"""
from pathlib import Path
import json, math, re, sys
from panda3d.core import *
from direct.showbase.ShowBase import ShowBase

ROOT=Path(__file__).resolve().parents[1]
HALF='--half-float' in sys.argv
TEMPLATE='--template12' in sys.argv
puffs=json.loads((ROOT/'logs/p20/puff_templates_12/templates.json').read_text())[0] if TEMPLATE else [[0,0,0,1,1,1,1,.7,.1]]
OUT=ROOT/('logs/p20/gpu_fixture_fullpath_half' if TEMPLATE and HALF else 'logs/p20/gpu_fixture_fullpath' if TEMPLATE else 'logs/p20/gpu_fixture_template12_half' if TEMPLATE and HALF else 'logs/p20/gpu_fixture_template12' if TEMPLATE else 'logs/p20/gpu_fixture_half' if HALF else 'logs/p20/gpu_fixture');OUT.mkdir(parents=True,exist_ok=True)
contract=dict(width=800,height=800,alphaAbsTolerance=.002 if HALF else .0002,sourceAbsTolerance=.002 if HALF else .0002,
              targetPixels=[1,3,7],relations=['front','inside','behind','camera_inside'],
              backend='Windows_Panda_OpenGL',boardResult='NOT_VERIFIED')
(OUT/'contract.json').write_text(json.dumps(contract,indent=2),encoding='utf-8')
load_prc_file_data('', 'window-type offscreen\nwin-size 800 800\naudio-library-name null\nsync-video false\ntextures-power-2 none\n')
base=ShowBase(windowType='offscreen');base.disableMouse()
base.camNode.set_active(False)
source=(ROOT/'HwaSim_IR/HwaSim_IR/IR/IRCloudPuffGeometry.h').read_text(encoding='utf-8-sig')
fragment=re.search(r'IRCloudPuffGeometryFragment\(\).*?R"\((.*?)\)";',source,re.S).group(1)
vertex=re.search(r'IRCloudPuffGeometryVertex\(\).*?R"\((.*?)\)";',source,re.S).group(1)
# 使用生产的球面代理和同一对数深度包装。
vertex='uniform highp vec2 u_ir_log_depth;\n'+vertex.replace('void main()','void irOriginalMain()')+'\nvoid main(){irOriginalMain();float w=gl_Position.w;if(w>0.0)gl_Position.z=(2.0*(log2(1.0+w)-u_ir_log_depth.x)*u_ir_log_depth.y-1.0)*w;}\n'
shader=Shader.make(Shader.SL_GLSL,'#version 140\n'+vertex,'#version 140\n'+fragment)
lens=PerspectiveLens();lens.set_fov(45);lens.set_near_far(.1,1000)
logNear=math.log2(1.1);logReciprocal=1/(math.log2(1001)-logNear)

def buffer(name,sort,floating):
    tex=Texture(name);tex.setup_2d_texture(800,800,(Texture.T_half_float if HALF else Texture.T_float) if floating else Texture.T_unsigned_byte,Texture.F_rgba if floating else Texture.F_rgb)
    tex.set_minfilter(SamplerState.FT_nearest);tex.set_magfilter(SamplerState.FT_nearest)
    props=FrameBufferProperties();props.set_rgb_color(True);props.set_depth_bits(24)
    props.set_rgba_bits(*([16 if HALF else 32]*4)) if floating else props.set_rgba_bits(8,8,8,0)
    props.set_float_color(floating)
    buf=base.win.make_texture_buffer(name,800,800,tex,False,props);buf.set_sort(sort)
    root=NodePath(name+'Root');cam=Camera(name+'Camera');cam.set_scene(root);cam.set_lens(lens)
    cp=root.attach_new_node(cam);cp.set_pos(0,-30,0)
    region=buf.make_display_region();region.set_camera(cp);region.set_clear_color_active(True)
    region.set_clear_color((0,0,0,0) if floating else (1,1,1,1));region.set_clear_depth_active(True)
    return tex,buf,root,cp

depth,depthBuffer,depthRoot,depthCam=buffer('P20FixtureDepth',-20,False)
optical,opticalBuffer,opticalRoot,opticalCam=buffer('P20FixtureOptics',-10,True)
cm=CardMaker('ActualOpaqueSmallTarget');cm.set_frame(-1,1,-1,1)
target=depthRoot.attach_new_node(cm.generate());target.set_two_sided(True)
target.set_shader(Shader.make(Shader.SL_GLSL,'''#version 130
uniform mat4 p3d_ModelViewProjectionMatrix,p3d_ModelViewMatrix;
in vec4 p3d_Vertex;out vec3 eye;
void main(){gl_Position=p3d_ModelViewProjectionMatrix*p3d_Vertex;eye=(p3d_ModelViewMatrix*p3d_Vertex).xyz;}
''','''#version 130
uniform vec2 u_ir_log_depth;in vec3 eye;out vec4 fragColor;
void main(){float d=clamp((log2(1.0+max(0.0,-eye.z))-u_ir_log_depth.x)*u_ir_log_depth.y,0.0,1.0);
float n=floor(d*16777215.0+.5);float r=floor(n/65536.0);n-=r*65536.0;float g=floor(n/256.0);
fragColor=vec4(r,g,n-g*256.0,255)/255.0;}
'''))
target.set_shader_input('u_ir_log_depth',Vec2(logNear,logReciprocal))
vdata=GeomVertexData('ConservativePuff',GeomVertexFormat.get_v3(),Geom.UH_static)
writer=GeomVertexWriter(vdata,'vertex')
for stack in range(11):
    latitude=-math.pi/2+math.pi*stack/10
    for segment in range(19):
        longitude=2*math.pi*segment/18
        writer.add_data3f(1.07*math.cos(latitude)*math.cos(longitude),1.07*math.cos(latitude)*math.sin(longitude),1.07*math.sin(latitude))
tri=GeomTriangles(Geom.UH_static)
for stack in range(10):
    for segment in range(18):
        a=stack*19+segment;b=a+1;c=(stack+1)*19+segment;d=c+1
        tri.add_vertices(a,b,c);tri.add_vertices(b,d,c)
tri.close_primitive();geom=Geom(vdata);geom.add_primitive(tri);gn=GeomNode('Puff');gn.add_geom(geom)
cloud=opticalRoot.attach_new_node(gn);cloud.set_scale(10);cloud.set_shader(shader);cloud.set_instance_count(len(puffs))
cloud.set_depth_test(False);cloud.set_depth_write(False);cloud.set_two_sided(True)
cloud.set_attrib(CullFaceAttrib.make(CullFaceAttrib.M_cull_counter_clockwise),2)
cloud.set_transparency(TransparencyAttrib.M_alpha)
cloud.set_attrib(ColorBlendAttrib.make(ColorBlendAttrib.M_add,ColorBlendAttrib.O_incoming_alpha,ColorBlendAttrib.O_one_minus_incoming_alpha,ColorBlendAttrib.M_add,ColorBlendAttrib.O_one,ColorBlendAttrib.O_one_minus_incoming_alpha))
centers=PTA_LVecBase4f();radii=PTA_LVecBase4f();lights=PTA_LVecBase4f()
for i in range(24):
    p=puffs[i] if i<len(puffs) else [0,0,0,1,1,1,0,.7,.1]
    centers.push_back(Vec4(p[0],p[1],p[2],p[6]));radii.push_back(Vec4(p[3],p[4],p[5],p[7]));lights.push_back(Vec4(p[8],.4,1000,0))
for name,value in [('u_puff_center',centers),('u_puff_radius',radii),('u_puff_light',lights),
                   ('u_camera_local',Vec3(0,-3,0)),('u_density_scale',1.),('u_optical_depth',2.5),
                   ('u_cloud_gray',.4),('u_spawn_fade',1.),('u_cloud_incident',Vec4(0,0,1,96)),
                   ('u_cloud_presentation',Vec4(2,1,.62,.98)),('u_scene_depth_rgb',depth),
                   ('u_ir_log_depth',Vec2(logNear,logReciprocal)),('u_stage6_raw_si_domain',1)]:cloud.set_shader_input(name,value)
cases=[]
for relation,distance,origin in [('front',10,-30),('inside',30,-30),('behind',50,-30),('camera_inside',5,-2.5)]:
    for pixels in contract['targetPixels']:
        name=relation+'_'+str(pixels)+'px'
        depthCam.set_y(origin);opticalCam.set_y(origin);cloud.set_shader_input('u_camera_local',Vec3(0,origin/10,0))
        target.set_pos(0,origin+distance,0)
        half=distance*math.tan(math.radians(22.5))*pixels/800
        target.set_scale(half,1,half)
        for _ in range(3):base.graphicsEngine.render_frame()
        if not base.graphicsEngine.extract_texture_data(optical,base.win.get_gsg()):raise RuntimeError('光学附件读回失败')
        if not base.graphicsEngine.extract_texture_data(depth,base.win.get_gsg()):raise RuntimeError('真实几何深度读回失败')
        (OUT/(name+('.rgba16f' if optical.get_component_width()==2 else '.rgba32f'))).write_bytes(bytes(optical.get_ram_image_as('RGBA')))
        (OUT/(name+'.depth.rgb8')).write_bytes(bytes(depth.get_ram_image_as('RGB')))
        cases.append(dict(name=name,distance=distance,cameraY=origin,targetPixels=pixels,width=optical.get_x_size(),height=optical.get_y_size(),puffs=puffs,componentBytes=optical.get_component_width(),framebufferRedBits=opticalBuffer.get_fb_properties().get_red_bits(),framebufferFloat=opticalBuffer.get_fb_properties().get_float_color()))
(OUT/'cases.json').write_text(json.dumps(cases,indent=2),encoding='utf-8')
print(json.dumps(dict(result='CAPTURED_PENDING_NUMERICAL_CHECK',cases=len(cases),driver=base.win.get_gsg().get_driver_renderer())))
base.destroy()
