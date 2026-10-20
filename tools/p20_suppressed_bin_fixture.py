"""相机专用禁用bin：避免空draw，同时保留原节点和另一相机输出。"""
from panda3d.core import *
from direct.showbase.ShowBase import ShowBase
from pathlib import Path
import json
load_prc_file_data('','window-type offscreen\nwin-size 64 64\naudio-library-name null\nsync-video false\n')
b=ShowBase(windowType='offscreen');b.camNode.set_active(False)
t=Texture();buffer=b.win.make_texture_buffer('suppression',64,64,t)
r=NodePath('source');parent=r.attach_new_node('parent');cm=CardMaker('actual_geometry');cm.set_frame(-1,1,-1,1);n=parent.attach_new_node(cm.generate());n.set_y(2);n.set_color(1,0,0,1);n.set_bin('fixed',71);n.set_tag('kind','plume')
c=Camera('camera');lens=PerspectiveLens();lens.set_fov(45);c.set_lens(lens);c.set_scene(r);c.set_tag_state_key('kind');cam=r.attach_new_node(c);dr=buffer.make_display_region();dr.set_camera(cam);dr.set_clear_color_active(True);dr.set_clear_color(Vec4(0));dr.set_clear_depth_active(True)
manager=CullBinManager.get_global_ptr();idx=manager.add_bin('p20_fixture_suppressed',CullBinManager.BT_unsorted,0);manager.set_bin_active(idx,False)
state=NodePath('state');state.set_bin('p20_fixture_suppressed',0,10001)
def image():
 for _ in range(3):b.graphicsEngine.render_frame()
 b.graphicsEngine.extract_texture_data(t,b.win.get_gsg());return bytes(t.get_ram_image_as('RGBA'))
original=image();c.set_tag_state('plume',state.get_state());suppressed=image();c.clear_tag_state('plume');restored=image();parent.hide();hidden=image()
checks=dict(originalLit=any(original[::4]),suppressedBlack=not any(suppressed[::4]),restoredIdentical=restored==original,parentHidden=not any(hidden[::4]),originalBinUnchanged=n.get_bin_name()=='fixed' and n.get_bin_draw_order()==71)
result=dict(result='PASS' if all(checks.values()) else 'FAIL',checks=checks,backend=b.win.get_gsg().get_driver_renderer(),scope='真实GPU相机tag排除、恢复、业务隐藏；生产减少draw数量需板端另核验')
Path('logs/p20/suppressed_bin32_fixture.json').write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8');print(result);b.destroy();raise SystemExit(0 if result['result']=='PASS' else 2)
