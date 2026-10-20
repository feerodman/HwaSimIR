"""新增路径检查后补齐SWIR阴天，并复测一次完整压力；保留首次尖峰记录。"""
from pathlib import Path
import subprocess,sys,json,time
root=Path(__file__).resolve().parents[1]
while True:
 p=root/'logs/p20/final_readback40_progress.json';rows=json.loads(p.read_text()) if p.exists() else []
 if any(r['exitCode'] for r in rows):raise SystemExit('前置检查失败，停止')
 if len(rows)==5:break
 time.sleep(5)
commands=[]
for role,count in [('old_receiver',27),('old_reopened',4198)]:
 directory=next((root/'logs/p20/runs/puff40_full_mwir_coexist'/role).glob('round_*'))
 commands.append(['tools/p20_validate_recording.py',str(directory),'--ffmpeg',str(root/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe'),'--expected',str(count)])
base=['tools/p20_board_run.py','--receipt','logs/p20/deployment/aarch64_40_prefix_prewarm/receipt.json','--tmp-board-logs','--control-domain','150','--video-domain','1','--customer-domain','1','--use-deployed-domains','--use-deployed-qos']
commands.extend([
 [*base,'--name','puff40_overcast_swir','--frames','600','--source-offset','3000','--sky','5','--band','0'],
 ['tools/p20_analyze_performance.py','puff40_overcast_swir'],
 [*base,'--name','puff40_stress_310s_repeat','--frames','18600','--source','logs/p20/stress_fixture/cloud-motion-stress.data','--sky','1','--band','2'],
 ['tools/p20_analyze_performance.py','puff40_stress_310s_repeat']])
records=[]
for i,command in enumerate(commands):
 print('START',i,command,flush=True)
 with (root/'logs/p20'/f'final_extra40_{i}.log').open('w',encoding='utf-8') as f:r=subprocess.run([sys.executable,*command],cwd=root,stdout=f,stderr=subprocess.STDOUT,timeout=600)
 records.append(dict(command=command,exitCode=r.returncode));(root/'logs/p20/final_extra40_progress.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
 if r.returncode:raise SystemExit('补充矩阵失败，停止检查')
 print('END',i,flush=True)
