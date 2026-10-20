"""从保留的真实回执汇总交付入口；不把PASS计数或有画面提升为整阶段通过。"""
from pathlib import Path
import argparse,csv,json,html,re,datetime
root=Path(__file__).resolve().parents[1];out=root/'logs/p20'
ap=argparse.ArgumentParser();ap.add_argument('--build',required=True);ap.add_argument('--prefix',required=True)
ap.add_argument('--optics',required=True);ap.add_argument('--transparent',required=True);ap.add_argument('--camera',required=True);ap.add_argument('--h264',required=True)
ap.add_argument('--extra-evidence',action='append',default=[]);args=ap.parse_args()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
weather={0:'Clear',1:'Cloudy',2:'Rain',3:'Snow',4:'Fog',5:'Overcast'}
graphics={r['case']:r for r in read(out/'graphics_log_audit.json')} if (out/'graphics_log_audit.json').exists() else {}
runs=[];performances=[]
for p in sorted((out/'runs').glob('*/result.json')):
 r=read(p);o=r.get('options',{});item=dict(name=p.parent.name,status=r.get('status','FAIL'),options=o,weather=weather.get(o.get('sky'),'INI'),systemPolicyRestored=r.get('systemPolicyRestored',False),error=r.get('error'))
 item['streams']={role:{k:r[role].get(k) for k in ['senderFrames','decodedFrames','savedFramesVerified','countPass','pixelsIdentical','cleanStreamIdentical','endReceiveToLastDecodedMs','endReceiveToMp4Ms']} for role in ['coarse','precise'] if role in r}
 if 'faultRecovery' in r:item['faultRecovery']=r['faultRecovery']
 if 'wrongDomainIsolation' in r:item['wrongDomainIsolation']=r['wrongDomainIsolation']
 if p.parent.name in graphics:
  item['graphicsLogStatus']=graphics[p.parent.name]['graphicsLogStatus']
  if item['graphicsLogStatus']=='FAIL':item['status']='FAIL_GRAPHICS';item['originalFunctionalStatus']=r.get('status')
 runs.append(item)
 if (p.parent/'performance.json').exists():
  perf=read(p.parent/'performance.json');perf['weather']=item['weather'];perf['options']=o;performances.append(perf)
(out/'performance.json').write_text(json.dumps(performances,indent=2,ensure_ascii=False),encoding='utf-8')
rows=[]
for r in performances:
 for role,v in r['streams'].items():
  latency=v.get('windowsStimulusToDecodedImageMs');latency=latency if isinstance(latency,dict) else {}
  rows.append(dict(case=r['case'],weather=r['weather'],role=role,frames=v['renderFrames'],fps=v['renderFps'],latencyP50Ms=latency.get('p50'),latencyP95Ms=latency.get('p95'),latencyP99Ms=latency.get('p99'),latencyMaxMs=latency.get('maximum'),over80msRatio=latency.get('over80msRatio'),queuePeak=v['maximumPendingInputFrames'],stopMs=max(v.get('stopTotalMs',[]),default=0),conserved=v.get('queueConservation',{}).get('pass','NOT_RECORDED'),performance=r.get('performanceStatus','NOT_RECORDED')))
with (out/'performance.csv').open('w',newline='',encoding='utf-8-sig') as f:
 w=csv.DictWriter(f,fieldnames=list(rows[0]) if rows else ['case']);w.writeheader();w.writerows(rows)
selected=[r for r in runs if r['name'].startswith(args.prefix+'_')]
planned=['cloudy_original_r1','cloudy_original_r2','cloudy_original_r3','cloudy_performance_r1','overcast_sky5','overcast_swir','full_mwir_coexist','full_swir_no_vd','stress_310s','stress_310s_repeat','native_raw_600','rgb_inverse_topics','gray_inverse_topics_all150','three_domains_150_1_2','application_recovery','identity_customer_only']+[f'lowalt_band{band}_sky{sky}' for band in [0,2] for sky in [0,2,3]]
missingTests=[args.prefix+'_'+name for name in planned if not any(r['name']==args.prefix+'_'+name for r in selected)]
localTests={p.parent.name:read(p) for p in (out/'local_tests').glob('*/result.json')}
capacityLog=out/'local_tests/contract_four_topics_capacity_retry.log'
capacityPass=capacityLog.exists() and '"unitAndTypedRoundtrip":"PASS","globalInitCount":1' in capacityLog.read_text(encoding='utf-8',errors='replace')
receipt=read(out/'deployment'/args.build/'receipt.json')
optical=read(root/args.optics);transparent=read(root/args.transparent);camera=read(root/args.camera)
currentPerformance=[r for r in performances if r['case'].startswith(args.prefix+'_')]
engineeringPass=(not missingTests and all(r['status']=='PASS' for r in selected) and all(not r.get('sustainedBacklog',True) for r in currentPerformance) and all(r['result']=='PASS' for r in [optical,transparent,camera]))
primaryNames=['cloudy_original_r1','cloudy_original_r2','cloudy_original_r3','overcast_sky5','overcast_swir','full_mwir_coexist','full_swir_no_vd','stress_310s','stress_310s_repeat']
latencyChecks=[]
for name in primaryNames:
 case=args.prefix+'_'+name
 found=next((r for r in currentPerformance if r['case']==case),None)
 for role in ['coarse','precise']:
  measured=(found or {}).get('streams',{}).get(role,{}).get('windowsStimulusToDecodedImageMs')
  p95=measured.get('p95') if isinstance(measured,dict) else None
  # 目标永远是80ms。额外分类只表达工程偏差，不能将它写成80ms达标。
  fraction=measured.get('over80msRatio') if isinstance(measured,dict) else None
  status='NOT_VERIFIED' if p95 is None else 'WITHIN_TARGET' if p95<=80 else 'MINOR_DEVIATION_RECORDED' if p95<=80+1000/60 and fraction<=.1 else 'TARGET_OVERRUN'
  latencyChecks.append(dict(case=case,role=role,p95Ms=p95,p99Ms=measured.get('p99') if isinstance(measured,dict) else None,maxMs=measured.get('maximum') if isinstance(measured,dict) else None,over80msRatio=fraction,status=status))
latencyPass=all(x['status'] in ['WITHIN_TARGET','MINOR_DEVIATION_RECORDED'] and x['maxMs'] is not None and x['maxMs']<1000 for x in latencyChecks)
requiredLocal=['end_exact1400','end_pause13s_two_rounds_retry','receiver_late_reconnect']
engineeringPass=engineeringPass and latencyPass and capacityPass and all(localTests.get(n,{}).get('status')=='PASS' for n in requiredLocal)
extraEvidence={name:read(root/name) for name in args.extra_evidence}
engineeringPass=engineeringPass and all(r.get('result',r.get('status'))=='PASS' for r in extraEvidence.values())
engineeringStatus='PASS_PENDING_USER_VISUAL' if engineeringPass else 'NOT_ACCEPTED'
if engineeringPass and any(x['status']=='MINOR_DEVIATION_RECORDED' for x in latencyChecks):engineeringStatus='ENGINEERING_CHECKED_WITH_LATENCY_DEVIATIONS_PENDING_VISUAL'
handoff=read(out/'deployment/final_state.json') if (out/'deployment/final_state.json').exists() else None
if handoff and args.build not in handoff.get('candidate',''):handoff=None
notice='当前候选的功能、光学工程检查和持续积压门槛已通过；用户视觉确认仍PENDING。' if engineeringPass else '当前候选尚未通过完整工程矩阵；缺测、性能失败和光学结果分别列出，不能视为正式替换版。'
if engineeringStatus=='ENGINEERING_CHECKED_WITH_LATENCY_DEVIATIONS_PENDING_VISUAL':notice+=' 部分短测P95超过80ms，已单列偏差，不能称全部时延达标。'
acceptance=dict(schema='hwasimir.p20.acceptance.v1',generatedAt=datetime.datetime.now().astimezone().isoformat(),
 phase='ENGINEERING_CANDIDATE_NOT_RELEASED',build=receipt['build'],actualDomains=receipt['instances']['precise']['domains'],
 userVisualAcceptance='PENDING',overall=engineeringStatus,currentRelease='P19',previousRelease='P18',
 missingPlannedTests=missingTests,localProtocolTests=localTests,
 supplementalEvidence=extraEvidence,
 latencyAssessment=dict(targetMs=80,minorDeviationClassification='工程分类：P95超目标不超过一个60Hz帧周期，且超80ms比例<=10%；仍标偏差而非80ms达标。未采用的performance调度另外披露，不作为默认配置门槛。',scope='主生产/重云/长测两路全部冷帧，P99/max完整报告',noLargeLatencyOverrun=latencyPass,checks=latencyChecks),
 rawCapacityAndFourTopics=dict(result='PASS' if capacityPass else 'NOT_VERIFIED',evidence='local_tests/contract_four_topics_capacity_retry.log',actualSdkFailureRecovery='NOT_REPRODUCED'),
 h264ActualStream=read(root/args.h264),
 finalHandoff=handoff if handoff else 'PENDING',
 opticalFixture=dict(evidence=args.optics,report=optical),
 transparentCloudContract=dict(evidence=args.transparent,report=transparent,cameraEvidence=args.camera,camera=camera,historicalIncorrectTotalAlpha=read(out/'optics/transparent_contract.json')),
 limitations=dict(transparentPlumeAndRainInsideCloud='TESTED_LOCAL_PREFIX_SUBSET',differentMediumSourceOrdering='APPROXIMATE_TESTED_BOUNDED_SUBSET',customerOriginalCode7RootCause='NOT_VERIFIED',actualCustomerInteroperability='NOT_VERIFIED',customerHardwareDecode='NOT_VERIFIED',radiometricCalibration='NOT_VERIFIED',screenPresentationLatency='NOT_VERIFIED'),
 tests=selected,historicalTests=runs,notes=['功能、视觉、性能分开判定；源哈希仅追溯，不做输入准入','历史用例名称含overcast但sky=2时实际为Rain，表格按实际INIT值分类','压力失败、诊断同步和早期重复云通道证据不能作为最终性能PASS'])
(out/'acceptance.json').write_text(json.dumps(acceptance,indent=2,ensure_ascii=False),encoding='utf-8')
parts=['<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>HwaSimIR P20 工程交付</title><style>body{font:16px system-ui;max-width:1200px;margin:36px auto;padding:0 20px;color:#182432;background:#f5f6f8}table{border-collapse:collapse;width:100%;font-size:13px}td,th{padding:7px;border:1px solid #bdc8d2;text-align:left}a{color:#155da8}video{width:48%;vertical-align:top}img{max-width:48%}.notice{background:#fff3cc;padding:16px}code{overflow-wrap:anywhere}details{margin:16px 0}</style><h1>HwaSimIR P20 工程交付</h1>',
 '<p class="notice">'+notice+' 正式 current=P19、previous=P18。历史失败证据保留在机器验收中。</p>',f'<p>构建：<code>{html.escape(args.build)}</code><br>ELF SHA256：<code>{receipt["build"]["elf_sha256"]}</code><br>实际三域：控制150 / 原视频1 / 客户1。</p><ul>']
for name in ['Closeout','Cloud_Rendering_Design','Performance_Report','RK3588_Runtime_Tuning','DDS_Domain_Guide','Source_Index']:
 p=root/'docs'/f'HwaSimIR_P20_{name}.md'
 if p.exists():parts.append(f'<li><a href="../../docs/{p.name}">{name}</a></li>')
parts+=['<li><a href="acceptance.json">逐项机器验收</a> · <a href="performance.json">全部性能JSON</a> · <a href="performance.csv">性能CSV</a></li>',f'<li><a href="deployment/{args.build}/receipt.json">隔离部署回执</a> · <a href="build/{args.build}/build_receipt.json">构建回执</a></li></ul>','<h2>当前候选实测</h2><table><tr><th>场景</th><th>路</th><th>FPS</th><th>P95 ms</th><th>队列峰值</th><th>STOP ms</th><th>守恒/性能</th></tr>']
parts.insert(len(parts)-1,f'<p>完整运行配置：<a href="configs/{args.build}/coarse/HwaSimIRRuntime.ini">粗跟INI</a> · <a href="configs/{args.build}/precise/HwaSimIRRuntime.ini">精跟INI</a> · <a href="configs/{args.build}/precise/ZRDDS_PROTOCOL_QOS.xml">共享QoS</a> · <a href="deployment/final_state.json">启动/退出回执</a> · <a href="deployment/final_machine_audit.json">最终进程与策略复核</a></p>')
for r in rows:
 if not r['case'].startswith(args.prefix+'_'):continue
 parts.append('<tr>'+''.join('<td>'+html.escape(str(x))+'</td>' for x in [r['case']+' / '+r['weather'],r['role'],round(r['fps'],3) if r['fps'] else None,round(r['latencyP95Ms'],2) if r['latencyP95Ms'] else None,r['queuePeak'],r['stopMs'],str(r['conserved'])+' / '+r['performance']])+'</tr>')
parts.append('</table><h2>未经增强的媒体</h2>')
for name in ['full_mwir_coexist','full_swir_no_vd','stress_310s','stress_310s_repeat']:
 p=out/'runs'/(args.prefix+'_'+name)/'performance_plot.png'
 if p.exists():parts.append(f'<p><a href="runs/{args.prefix}_{name}/performance_plot.png">{name} 完整帧队列/帧龄/FPS曲线</a></p><img style="max-width:100%" src="runs/{args.prefix}_{name}/performance_plot.png">')
for r in selected:
 parts.append(f'<details><summary>{html.escape(r["name"])} — {r["weather"]} — 功能 {r["status"]}</summary><p><a href="runs/{r["name"]}/result.json">原始回执</a></p>')
 for role in r['streams']:
  p=out/'runs'/r['name']/role/'output.mp4'
  if p.exists():parts.append(f'<video controls preload="none" src="runs/{r["name"]}/{role}/output.mp4"></video>')
 parts.append('</details>')
parts.append(f'<h2>同输入帧前后对照</h2><p>左P19基线，右P20 {html.escape(args.prefix)}；固定映射、未经增强，形态差异不等于光学误差。</p>')
for frame in [1,300,480,600]:
 for name in ['baseline24_cloudy_original_r1',args.prefix+'_cloudy_original_r1']:
  p=out/'visual'/f'{name}_frame{frame}.png'
  if p.exists():parts.append(f'<img loading="lazy" src="visual/{p.name}" alt="{name} frame{frame}">')
parts.append('<h2>板端光学夹具</h2><p><a href="'+str((root/args.optics).relative_to(out)).replace('\\','/')+'">不透明局部光程结果</a> · <a href="'+str((root/args.transparent).relative_to(out)).replace('\\','/')+'">尾焰局部光程结果</a>；以下显示alpha线性0..255，不是生产图像。</p>')
for p in sorted((out/'visual/optics').glob('*_alpha.png')):parts.append(f'<img loading="lazy" src="visual/optics/{p.name}" alt="{p.stem}">')
parts.append('</html>');(out/'index.html').write_text('\n'.join(parts),encoding='utf-8')
missing=[]
for link in re.findall(r'(?:href|src)="([^"]+)"',(out/'index.html').read_text(encoding='utf-8')):
 if not (out/link).resolve().exists():missing.append(link)
(out/'link_check.json').write_text(json.dumps(dict(result='PASS' if not missing else 'FAIL',missing=missing,scope='workspace_relative_offline_links_no_external_dependencies'),indent=2),encoding='utf-8')
print(json.dumps(dict(tests=len(selected),performanceRows=len(rows),missingLinks=missing)))
