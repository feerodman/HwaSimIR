"""精确比较逐项覆盖与一次compose得到的Panda不可变RenderState。"""
from panda3d.core import *
from pathlib import Path
import json
texture=Texture('actual_depth');checks=[]
for count in [1,20,200]:
 for revision in range(4):
  parent=NodePath('root')
  for i in range(count):parent.set_shader_input('input_'+str(i),Vec4(revision,i,.4,1))
  parent.set_shader_input('u_ir_log_depth',Vec2(revision,1))
  old=NodePath('old');old.set_state(parent.get_state());small=NodePath('overrides')
  for node in [old,small]:
   node.set_shader_input('u_cloud_presentation',Vec4(2,1,.62,.98),10000)
   node.set_shader_input('u_ray_steps',Vec2(24,0),10000)
   node.set_shader_input('u_scene_depth_rgb',texture,10000)
   node.set_shader_input('u_ir_log_depth',Vec2(1,.01+revision*.001),10000)
   node.set_attrib(ColorBlendAttrib.make(ColorBlendAttrib.M_add,ColorBlendAttrib.O_incoming_alpha,ColorBlendAttrib.O_one_minus_incoming_alpha,ColorBlendAttrib.M_add,ColorBlendAttrib.O_one,ColorBlendAttrib.O_one_minus_incoming_alpha),10000)
  composed=parent.get_state().compose(small.get_state())
  checks.append(dict(inputCount=count,revision=revision,identical=old.get_state()==composed))
result=dict(result='PASS' if all(c['identical'] for c in checks) else 'FAIL',checks=checks,scope='相同参数/优先级/颜色混合对应同一不可变RenderState；真实主程序逐帧时序另测')
Path('logs/p20/cloud_state34_fixture.json').write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8');print(result);raise SystemExit(0 if result['result']=='PASS' else 2)
