from pathlib import Path
import json
# 复用小型真实GPU夹具初始化，只更换被测共享实例路径。
base=Path('logs/p20/mask_visibility_fixture.py').read_text(encoding='utf-8').split('mask=BitMask32.bit(27)')[0]
exec(base)
legacy=NodePath('legacy_volume_scene');legacy.hide(BitMask32.bit(28));legacy.show(BitMask32.bit(29))
world=legacy.attach_new_node('PublicCloudWorldVolumes');n.reparent_to(world)
world.hide(BitMask32.all_on());world.show(BitMask32.bit(28))
instance=world.instance_to(r);visiblePath=NodePath(instance,n.node());c.set_camera_mask(BitMask32.bit(28));d.set_clear_color_active(True);d.set_clear_color(Vec4(0))
checks=[]
for label,action,expected in [('visible',lambda:n.show(),True),('business_hidden',lambda:n.hide(),False),('restored',lambda:n.show(),True)]:
 action();oldVisible=not n.is_hidden(BitMask32.bit(28));actualVisible=not visiblePath.is_hidden(BitMask32.bit(28));pixel=capture()
 checks.append(dict(case=label,legacyPathVisible=oldVisible,sharedPathVisible=actualVisible,pixel=pixel,result='PASS' if actualVisible==expected and (pixel[0]>0)==expected else 'FAIL'))
result=dict(result='PASS' if all(x['result']=='PASS' for x in checks) else 'FAIL',backend=b.win.get_gsg().get_driver_renderer(),scope='共享场景实际相机路径与CPU选择一致；业务hide/show保留',checks=checks)
Path('logs/p20/shared_cloud_visibility.json').write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8');print(result);b.destroy();raise SystemExit(0 if result['result']=='PASS' else 2)
