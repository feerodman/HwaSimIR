"""汇总最终候选新增路径证据；缺失/失败不能提升为PASS。"""
from pathlib import Path
import json,re,hashlib
root=Path(__file__).resolve().parents[1];out=root/'logs/p20'
def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
receipt=read(out/'deployment/aarch64_40_prefix_prewarm/receipt.json')
audit=read(out/'runs/diagnostic40_rgb_audit/result.json');checks={}
for role in ['coarse','precise']:
 log=(out/f'runs/diagnostic40_rgb_audit/{role}/board/board.log').read_text(encoding='utf-8',errors='replace')
 matches=re.findall(r'\[P20DirectRgbAudit\] frame=(\d+) bytes=(\d+) exact=(\d+)',log)
 checks[role]=dict(loggedComparisons=len(matches),allExact=bool(matches) and all(x[2]=='1' for x in matches),lastAuditCounter=max((int(x[0]) for x in matches),default=0),scope='审计计数含INIT/空闲，不能当业务帧数；业务接收数量由主回执核验')
resize=read(out/'runs/extra40_samepid_resize/result.json')
reuse=read(out/'final_source_reuse40.json');timing=read(out/'runs/diagnostic40_readback_timing/readback_summary.json')
sources={}
for name in ['board_rgb_readback38_native/result.json','board_transparent35_gather25/result.json','board_transparent35_gather25/scalar_comparison.json','board_optics30/result.json','shared_camera31_fixture.json','inherited_prefix33_fixture.json']:
 sources[name]=read(out/name)
recordings={str(p.relative_to(out)).replace('\\','/'):read(p) for p in (out/'runs/puff40_full_mwir_coexist').glob('old_*/*/p20_validation.json')}
ok=audit['status']=='PASS' and all(v['allExact'] for v in checks.values()) and resize['status']=='PASS' and reuse['opticalShaderReuse']=='PASS' and len(recordings)==2 and all(r['result']=='PASS' for r in recordings.values())
result=dict(result='PASS' if ok else 'FAIL',build=receipt['build'],productionAudit=dict(evidence='runs/diagnostic40_rgb_audit/result.json',status=audit['status'],checks=checks),samePidResize=resize,opticalSourceReuse=reuse,readbackDiagnostic=timing,oldRecordingFullIdentityValidation=recordings,retainedIndependentFixtures=sources,readback38To40='readback38_to40.diff: 仅新增默认关闭计时；40主链另做双读/尺寸/全矩阵',limitations=['独立夹具不代表生产性能','用户视觉PENDING','真实客户现场码7/最终互通/客户硬解/辐射标定/屏幕呈现NOT_VERIFIED'])
(out/'supplemental40.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(result['result'])
