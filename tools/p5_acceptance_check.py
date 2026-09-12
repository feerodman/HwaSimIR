"""Fail the acceptance gate for missing counts, graphics errors or wrong GPU values."""
from pathlib import Path
import json

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'logs/p5'
evidence=json.loads((OUT/'evidence.json').read_text(encoding='utf8'))
names=['rk_release_sync20','rk_release_sync30','rk_release_sync60','rk_release_60s','rk_release_600s',
    'rk_release_sheet','rk_release_gamma2_blend','rk_volumeoff','rk_legacy','rk_cloud_large','rk_cloud_small',
    'rk_cloud_occluded','rk_cloud_behind','rk_plume_end','rk_pause','rk_plume_off','rk_cloud_volume_only']
names += [f'rk_{asset}_lookup{case}' for asset in ['f22','aim120','aim9x'] for case in ['A','B']]
checks=[]
for name in names:
    data=evidence['board_cases'].get(name,{})
    ok=data.get('single_round_count_pass',False) and data.get('receiver_decode_errors_max',1)==0
    checks.append(dict(case=name,passed=ok,counts=data.get('counts',{}),visible_volumes=data.get('visible_volumes_max',0)))
    if not ok:raise RuntimeError(f'Case not accepted: {name}')
    if name.startswith('rk_release_') and name not in ['rk_release_sheet','rk_release_gamma2_blend']:
        assert data['visible_volumes_max']>0, f'No volume selected: {name}'
data=evidence['board_cases']['rk_weather_reinit_fixed']
assert data['multiple_init_count_pass'], 'Repeated INIT association failed'
checks.append(dict(case='rk_weather_reinit_fixed',passed=True,total_received=data['accepted_across_inits'],
                   rounds=data['conservation_all']))
for asset in ['rk_f22','rk_aim120','rk_aim9x']:
    data=evidence['pixels'][asset]
    assert data['selected_pixels']>0 and data['before_median']==51 and data['after_median']==102
    assert data['changed_selected_pixels']==data['selected_pixels'] and data['changed_outside_selected_in_roi']==0
assert evidence['pixels']['rk_release_gamma2_blend']['center_rgb_median']==[180,180,180]
versions={}
for name in ['rk_release_sync60','rk_release_600s']:
    text=(OUT/name/'board.log').read_text(encoding='utf8',errors='replace')
    line=next(x for x in text.splitlines() if x.startswith('[DeploymentVersion]'))
    versions[name]=line
result=dict(code_and_count_gate='PASS',cases=checks,versions=versions,
    image_quality='Reviewed separately; cloud art detail/contrast limits remain in the Chinese implementation report')
(OUT/'acceptance_suite.json').write_text(json.dumps(result,indent=2),encoding='utf8')
print(f'PASS: {len(checks)} measured RK cases, GPU material A/B and gamma constant check')
