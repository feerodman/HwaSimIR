"""GPU验证：继承父场景状态与复制完整状态后设置前缀参数逐像素相同。"""
from pathlib import Path
import json
exec(Path('tools/p20_shared_camera_fixture.py').read_text(encoding='utf-8-sig').split('checks=[]')[0])
dr2.set_active(False);isolated.reparent_to(cloudRoot)
node.clear_shader_input('u_plume_gray');node.clear_shader_input('u_prefix_count')
holder.set_bin('fixed',171,10002)
checks=[]
for source in [50.,80.,20.,50.]:
 scene.set_shader_input('u_plume_gray',source)
 cloudRoot.set_state(scene.get_state());cloudRoot.set_attrib(blend,10000)
 holder.set_mat(parent.get_mat(scene));holder.set_state(parent.get_net_state());holder.set_bin('fixed',171,10002)
 isolated.set_state(scene.get_state());isolated.set_shader_input('u_prefix_count',1);reference=image()
 isolated.set_state(RenderState.make_empty());isolated.set_shader_input('u_prefix_count',1);actual=image()
 error=max(abs(x-y) for x,y in zip(reference,actual));checks.append(dict(source=source,maxError=error,result='PASS' if error<1e-6 else 'FAIL'))
result=dict(result='PASS' if all(x['result']=='PASS' for x in checks) else 'FAIL',checks=checks,backend=b.win.get_gsg().get_driver_renderer(),scope='实际生产前缀shader、动态父状态、相同tag/继承优先级；非性能结果')
Path('logs/p20/inherited_prefix33_fixture.json').write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8');print(result);b.destroy();raise SystemExit(0 if result['result']=='PASS' else 2)
