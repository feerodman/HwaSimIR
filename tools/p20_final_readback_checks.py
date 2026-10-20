"""完整矩阵退出后串行执行读回审计/探针/同PID尺寸重建，避免双测试争用板卡。"""
from pathlib import Path
import subprocess,sys,json,time,argparse
root=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--start-index',type=int,default=0);a=ap.parse_args()
expected=20
while True:
 p=root/'logs/p20/final_matrix_progress.json'
 rows=json.loads(p.read_text()) if p.exists() else []
 rows=[r for r in rows if r['name'].startswith('puff40_')]
 if any(r.get('exitCode')!=0 or r.get('sustainedBacklog',False) for r in rows):raise SystemExit('矩阵失败，停止补充测试')
 if len(rows)==expected:break
 time.sleep(5)
receipt='logs/p20/deployment/aarch64_40_prefix_prewarm/receipt.json'
base=['--receipt',receipt,'--tmp-board-logs','--control-domain','150','--video-domain','1','--customer-domain','1','--use-deployed-domains','--use-deployed-qos']
commands=[
 ['tools/p20_board_run.py','--name','diagnostic40_rgb_audit',*base,'--frames','120','--source-offset','3000','--sky','1','--band','2','--old-display','--render-env','HwaSimIRDirectRgbReadbackAudit=1'],
 ['tools/p20_board_run.py','--name','diagnostic40_readback_timing',*base,'--frames','600','--source-offset','3000','--sky','1','--band','2','--render-env','HwaSimIRDirectRgbTiming=1'],
 ['tools/p20_readback_summary.py','diagnostic40_readback_timing'],
 ['tools/p20_analyze_performance.py','diagnostic40_readback_timing'],
 ['tools/p20_samepid_resize.py','--name','extra40_samepid_resize','--receipt',receipt,'--reference-run','puff40_cloudy_600']]
progress=root/'logs/p20/final_readback40_progress.json'
records=json.loads(progress.read_text())[:a.start_index] if a.start_index else []
for i,command in enumerate(commands):
 if i<a.start_index:continue
 print('START',command,flush=True)
 with (root/'logs/p20'/f'final_readback40_{i}.log').open('w',encoding='utf-8') as f:
  r=subprocess.run([sys.executable,*command],cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=400)
 records.append(dict(command=command,exitCode=r.returncode))
 (root/'logs/p20/final_readback40_progress.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
 if r.returncode:raise SystemExit('补充测试失败，请检查日志')
 print('END',i,flush=True)
