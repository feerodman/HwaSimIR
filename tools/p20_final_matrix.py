"""固定候选的串行实机矩阵；每项保留原始结果，不把功能PASS当性能PASS。"""
from pathlib import Path
import subprocess,sys,json,time,argparse,re
root=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--match',default='.*');ap.add_argument('--prefix',default='puff18');ap.add_argument('--receipt',default='logs/p20/deployment/aarch64_18_camera_mask/receipt.json');options=ap.parse_args()
receipt=options.receipt
base=['--receipt',receipt,'--tmp-board-logs','--control-domain','150','--video-domain','1','--customer-domain','1','--use-deployed-domains','--use-deployed-qos','--render-env','HwaSimIRCloudAlgorithm=PuffHybrid']
heavy=['--frames','600','--source-offset','3000','--sky','1','--band','2']
cases=[
 ('puff18_cloudy_original_r1',heavy,True),
 ('puff18_cloudy_original_r2',heavy,True),
 ('puff18_cloudy_original_r3',heavy,True),
 ('puff18_cloudy_performance_r1',heavy+['--scheduling','performance'],True),
 ('puff18_overcast_sky5', ['--frames','600','--source-offset','3000','--sky','5','--band','2'],True),
 ('puff18_full_mwir_coexist',['--frames','4318','--sky','1','--band','2','--old-display','--old-reopen'],True),
 ('puff18_full_swir_no_vd',['--frames','4318','--sky','1','--band','0'],True),
 ('puff18_stress_310s',['--frames','18600','--source','logs/p20/stress_fixture/cloud-motion-stress.data','--sky','1','--band','2'],True),
 ('puff18_native_raw_600',heavy+['--precise-codec','raw'],True),
 ('puff18_rgb_inverse_topics',['--frames','120','--width','320','--coarse-codec','h264','--precise-codec','raw','--pixels','rgb24','--old-display'],True),
 ('puff18_gray_inverse_topics_all150',['--frames','120','--width','320','--coarse-codec','h264','--precise-codec','raw','--video-domain','150','--customer-domain','150','--wrong-domain-probe','1'],True),
 ('puff18_three_domains_150_1_2',['--frames','120','--customer-domain','2','--wrong-domain-probe','1','--old-display'],True),
 ('puff18_application_recovery',['--frames','120','--roles','coarse','--rounds','2','--application-fault','--old-display'],False),
 ('puff18_identity_customer_only',['--frames','120','--width','320','--pixels','rgb24','--customer-only','--different-models','--different-sources','--coarse-sensor','17','--precise-sensor','3','--precise-plat','2002'],True),
]
# 历史低空天气输入仅作为单独标识的回归fixture，不改原始1.txt。
for band in [0,2]:
 for sky in [0,2,3]:
  cases.append((f'puff18_lowalt_band{band}_sky{sky}',['--frames','300','--source','DataDrivenTestQT/p14_ground_truck_weather_30s.txt','--sky',str(sky),'--band',str(band)],True))
records=[]
for name,extra,analyze in cases:
 name=name.replace('puff18_',options.prefix+'_',1)
 if not re.search(options.match,name):continue
 folder=root/'logs/p20/runs'/name
 if folder.exists():
  print('EXISTING_SKIP',name,flush=True);continue
 args=base.copy()
 if '--customer-domain' in extra or '--video-domain' in extra:args.remove('--use-deployed-domains')
 cmd=[sys.executable,str(root/'tools/p20_board_run.py'),'--name',name,*args,*extra]
 print('START',name,flush=True);start=time.monotonic()
 with (root/'logs/p20'/f'{name}.log').open('w',encoding='utf-8') as log:
  run=subprocess.run(cmd,cwd=root,stdout=log,stderr=subprocess.STDOUT,timeout=900)
 if analyze and run.returncode==0:
  with (root/'logs/p20'/f'{name}_performance.log').open('w',encoding='utf-8') as log:
   subprocess.run([sys.executable,'tools/p20_analyze_performance.py',name],cwd=root,stdout=log,stderr=subprocess.STDOUT,timeout=60)
 record=dict(name=name,exitCode=run.returncode,elapsedSeconds=time.monotonic()-start)
 if (folder/'result.json').exists():record['result']=json.loads((folder/'result.json').read_text(encoding='utf-8'))['status']
 if analyze and (folder/'performance.json').exists():record['sustainedBacklog']=json.loads((folder/'performance.json').read_text(encoding='utf-8'))['sustainedBacklog']
 records.append(record);(root/'logs/p20/final_matrix_progress.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
 print('END',record,flush=True)
 if run.returncode!=0 or record.get('sustainedBacklog',False):
  print('STOP_FOR_INSPECTION',name,flush=True);break
