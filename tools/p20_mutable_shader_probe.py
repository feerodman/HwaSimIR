from pathlib import Path
import json
from panda3d.core import *
from direct.showbase.ShowBase import ShowBase
load_prc_file_data('', 'window-type offscreen\nwin-size 64 64\naudio-library-name null\nsync-video false\n')
b=ShowBase(windowType='offscreen');b.camNode.set_active(False)
t=Texture();t.setup_2d_texture(64,64,Texture.T_unsigned_byte,Texture.F_rgba)
f=FrameBufferProperties();f.set_rgb_color(True);f.set_rgba_bits(8,8,8,8)
o=b.win.make_texture_buffer('test',64,64,t,False,f)
r=NodePath('r');l=OrthographicLens();l.set_film_size(2,2);l.set_near_far(-10,10)
c=Camera('c');c.set_scene(r);c.set_lens(l);cn=r.attach_new_node(c);cn.set_y(-1)
d=o.make_display_region();d.set_camera(cn)
cm=CardMaker('quad');cm.set_frame(-1,1,-1,1);n=r.attach_new_node(cm.generate())
n.set_shader(Shader.make(Shader.SL_GLSL,'#version 130\nuniform mat4 p3d_ModelViewProjectionMatrix;in vec4 p3d_Vertex;void main(){gl_Position=p3d_ModelViewProjectionMatrix*p3d_Vertex;}','#version 130\nuniform vec4 u_values[1];out vec4 color;void main(){color=u_values[0];}'))
a=PTA_LVecBase4f();a.push_back(Vec4(.2,.3,.4,1));n.set_shader_input('u_values',a);state=n.get_state()
def capture():
 for i in range(3):b.graphicsEngine.render_frame()
 b.graphicsEngine.extract_texture_data(t,b.win.get_gsg());v=bytes(t.get_ram_image_as('RGBA'));return list(v[(32*64+32)*4:(32*64+32)*4+4])
first=capture();a[0]=Vec4(.8,.3,.2,1);second=capture()
RenderState.clear_cache();RenderState.garbage_collect();third=capture()
result=dict(first=first,second=second,afterCacheMaintenance=third,cacheMaintenancePixelsIdentical=second==third,stateUnchanged=state==n.get_state(),result='PASS' if abs(first[0]-51)<=1 and abs(second[0]-204)<=1 and abs(second[2]-51)<=1 and state==n.get_state() and second==third else 'FAIL',backend=b.win.get_gsg().get_driver_renderer())
Path('logs/p20/mutable_shader_windows.json').write_text(json.dumps(result,indent=2));print(result);b.destroy()
