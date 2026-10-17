"""Build an honest, relative-link evidence index; never upgrade historical FAILs."""
import argparse,html,json
from pathlib import Path
root=Path(__file__).resolve().parents[1];base=root/'logs/p19'
ap=argparse.ArgumentParser();ap.add_argument('--cloud-case',default='cloud_resumed_reference_raw');a=ap.parse_args()
deployment=json.loads((base/'deployment/current.json').read_text())
releaseBuild=deployment['instances']['precise']['metadata']['SourceIdentity'].removeprefix('P19-')
entries=[]
for kind in ('runs','tests'):
 for folder in sorted((base/kind).iterdir()):
  if not folder.is_dir():continue
  result=next((folder/n for n in ('result.json','matrix.json') if (folder/n).exists()),None)
  if result is None:continue
  data=json.loads(result.read_text(encoding='utf-8-sig'))
  status=data.get('status','RECORDED') if isinstance(data,dict) else ('PASS' if data and all(x.get('status')=='PASS' for x in data) else 'FAIL')
  entry={'case':folder.name,'kind':kind,'status':status,'evidence':result.relative_to(base).as_posix()}
  perf=folder/'performance.json'
  if perf.exists():
   p=json.loads(perf.read_text());entry['performanceStatus']=p.get('performanceStatus','NOT_VERIFIED');entry['performanceEvidence']=perf.relative_to(base).as_posix()
  entries.append(entry)
report={'schema':'hwasimir.p19.evidence-index.v1','phaseStatus':'DDS_DOMAINS_VERIFIED_CLOUD_PERFORMANCE_UNRESOLVED',
 'overallStage':'NOT_ALL_PASSED',
 'cloudPerformance':{'status':'UNRESOLVED','lastMeasuredResult':'FAIL','issue':'Sustained cloudy precise-render slowdown and seconds of backlog','experimentIncludedInRelease':False,'evidence':'runs/'+a.cloud_case+'/performance.json'},
 'actualCustomerRaw':'NOT_VERIFIED','originalCustomerTransportCode7RootCause':'NOT_CONFIRMED',
 'actualCustomerHardware':'NOT_VERIFIED','screenLatency':'NOT_VERIFIED','nativeSaveDialogAutomation':'NOT_VERIFIED',
 'customerH264Software':'USER_REPORTED','previousP18VisualAcceptance':'USER_REPORTED',
 'release':{'build':releaseBuild,'elfSha256':deployment['build']['elf_sha256'],'receipt':'deployment/current.json'},
 'independentDomains':json.loads((base/'resumed_domains/acceptance.json').read_text(encoding='utf-8')),
 'frozenFiles':json.loads((base/'baseline/frozen_check_current.json').read_text()),
 'functionalChecks':[
  {'item':'real_sdk_failure_same_pid_recovery','status':'PASS','evidence':'runs/domains19_raw_sdk_recover/result.json'},
  {'item':'normal_stop_two_rounds','status':'PASS','evidence':'runs/dual120_two_rounds_final/result.json'},
  {'item':'arbitrary_ids_models','status':'PASS','evidence':'runs/ids_models_dual30/result.json'},
  {'item':'independent_sources','status':'PASS','evidence':'runs/independent_sources_final/result.json'},
  {'item':'late_join_reconnect','status':'PASS','evidence':'tests/receiver_late_reconnect_v2/result.json'},
  {'item':'dual_ui_validation_migration','status':'PASS','evidence':'ui/validation_domains12/result.json'},
  {'item':'private_sei_isolation','status':'PASS','evidence':'h264/private_sei_audit.json'},
  {'item':'board_release_cleanup','status':'PASS','evidence':'board/cleanup/result.json'},
  {'item':'old_display_exit_reopen','status':'PASS','note':'Split domains, clear sky; cloud performance remains separate','evidence':'runs/domains19_old_reopen/result.json'}],
 'cases':entries}
(base/'acceptance.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
rows=[]
for e in entries:
 links=f'<a href="{e["evidence"]}">{html.escape(e["case"])}</a>'
 if 'performanceEvidence' in e:links+=f' · <a href="{e["performanceEvidence"]}">性能数据</a>'
 rows.append(f'<tr><td>{links}</td><td>{e["status"]}</td><td>{e.get("performanceStatus","未单独判定")}</td></tr>')
docs=['Closeout','CustomerRawDDS_Troubleshooting','H264_Compatibility','DualSensor_UI_Guide','Board_Releases','Source_Index','Cloud_Deferred','DDS_Domains','Resumed_Test_Report']
doclinks=' · '.join(f'<a href="../../docs/HwaSimIR_P19_{n}.md">{n}</a>' for n in docs)
(base/'index.html').write_text('''<!doctype html><meta charset="utf-8"><title>P19 实测证据</title>
<style>body{max-width:1200px;margin:32px auto;padding:0 24px;font:16px/1.6 system-ui;color:#172434}a{color:#1558a6}table{width:100%;border-collapse:collapse}td,th{text-align:left;padding:8px;border-bottom:1px solid #ddd}.note{background:#fff1cf;padding:16px}img{max-width:100%}</style>
<h1>P19 功能交付与实测证据</h1><p class="note">恢复测试已修复并验证DDS三域独立配置；控制150、原视频1、客户1已部署。云重负载性能仍未解决（实测FAIL），没有因保持画质而宣称性能通过。无效云/读回实验不进入正式程序。客户现场raw、客户硬解及屏幕时延未验证。</p>
<p>'''+doclinks+'''</p><p><a href="acceptance.json">机器可读结果</a> · <a href="deployment/current.json">当前板端构建回执</a> · <a href="resumed_domains/acceptance.json">分域验收</a> · <a href="baseline/frozen_check_current.json">冻结文件核对</a> · <a href="deliverables/HwaSimIR_P19_Customer_Domains.zip">客户接收包</a></p>
<p><a href="../../build-p19-stim/release/start_control150_video1.cmd">启动控制150 / 视频1双路UI</a> · <a href="../../docs/HwaSimIR_P19_DualSensor_UI_Guide.md">UI操作说明</a></p>
<h2>P18回滚实测</h2><p><a href="../p18/runs/p19_rollback_coarse_v2/result.json">粗路181帧</a> · <a href="../p18/runs/p19_rollback_precise_v2/result.json">精路181帧</a></p>
<h2>双路 UI（下图为旧构建11布局；构建12新增三域表单）</h2><a href="ui/scale150_final.png"><img src="ui/scale150_final.png" alt="P19 双路 UI，Qt 150%缩放"></a>
<h2>真实媒体</h2><p><a href="runs/dual120_two_rounds_final/coarse/output.mp4">粗 raw 两回合录像</a> · <a href="runs/dual120_two_rounds_final/precise/output.mp4">精 H264 两回合录像</a> · <a href="runs/mpp_candidate_retained_writer/precise/received.h264">硬解候选 clean H264 短流（硬件未验证）</a> · <a href="h264/private_sei_audit.json">真实私有 SEI 隔离检查</a> · <a href="h264/final_timing_summary.json">末帧分阶段实测</a></p>
<h2>逐项测试</h2><table><thead><tr><th>用例 / 原始结果</th><th>该用例功能或预期故障判定</th><th>单独性能结论</th></tr></thead><tbody>'''+''.join(rows)+'''</tbody></table><p>所有链接相对 logs/p19/index.html；连同工作区对应目录保存即可离线打开。客户接收 zip 为独立包，不含许可证及板端历史归档。</p>''',encoding='utf-8')
print(json.dumps({'cases':len(entries),'index':str(base/'index.html'),'phaseStatus':report['phaseStatus']}))
