from pathlib import Path
import json
base=Path('logs/p20/mask_visibility_fixture.py').read_text(encoding='utf-8').split('mask=BitMask32.bit(27)')[0];exec(base)
parent=r.attach_new_node('business_parent');n.reparent_to(parent);n.set_scale(.2);parent.set_pos(.22,0,-.1)
isolated=NodePath('isolated');holder=isolated.attach_new_node('holder');n.instance_to(holder);n.clear_shader_input('u_values');parent.set_shader_input('u_values',a)
d.set_clear_color_active(True);d.set_clear_color(Vec4(0));checks=[]
def pixels(scene):
 c.set_scene(scene)
 for _ in range(3):b.graphicsEngine.render_frame()
 b.graphicsEngine.extract_texture_data(t,b.win.get_gsg());return bytes(t.get_ram_image_as('RGBA'))
for name,action in [('visible',lambda:None),('parent_hidden',lambda:parent.hide()),('restored_rotated',lambda:(parent.show(),parent.set_hpr(0,0,25))),('node_hidden',lambda:n.hide())]:
 action();visible=not n.is_hidden(PandaNode.get_all_camera_mask());holder.set_mat(parent.get_mat(r));holder.set_state(parent.get_net_state());holder.show() if visible else holder.hide()
 source=pixels(r);copy=pixels(isolated);difference=max(abs(x-y) for x,y in zip(source,copy));lit=sum(x>0 for x in copy[::4])
 checks.append(dict(case=name,visible=visible,maxPixelError=difference,litPixels=lit,result='PASS' if difference==0 and (lit>0)==visible else 'FAIL'))
result=dict(result='PASS' if all(x['result']=='PASS' for x in checks) else 'FAIL',backend=b.win.get_gsg().get_driver_renderer(),checks=checks)
Path('logs/p20/transparent_instance_fixture.json').write_text(json.dumps(result,indent=2),encoding='utf-8');print(result);b.destroy();raise SystemExit(0 if result['result']=='PASS' else 2)
