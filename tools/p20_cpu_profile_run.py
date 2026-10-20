"""隔离诊断：同条件双路运行期间，仅采样本次候选精跟主线程。"""
from pathlib import Path
import subprocess,os,time,base64,shlex,json,argparse,re
ap=argparse.ArgumentParser();ap.add_argument('--name',required=True);ap.add_argument('--build',required=True);args=ap.parse_args()
assert all(re.fullmatch('[A-Za-z0-9_]+',v) for v in [args.name,args.build])
root=Path(__file__).resolve().parents[1];name=args.name
env=os.environ.copy();env.update(SSH_ASKPASS=str(root/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p20')
cmd=['python','tools/p20_board_run.py','--name',name,'--receipt','logs/p20/deployment/'+args.build+'/receipt.json','--tmp-board-logs','--frames','600','--source-offset','3000','--sky','1','--band','2','--control-domain','150','--video-domain','1','--customer-domain','1','--use-deployed-domains','--use-deployed-qos']
with (root/'logs/p20'/f'{name}.log').open('w') as log:
 p=subprocess.Popen(cmd,cwd=root,env=env,stdout=log,stderr=subprocess.STDOUT)
 try:
  time.sleep(8)
  script=r'''import pathlib,subprocess,json,collections,time
target='/userdata/HwaSimIR/candidates/P20_BUILD_VALUE/precise'
deadline=time.monotonic()+15
while time.monotonic()<deadline:
 pids=[p.name for p in pathlib.Path('/proc').iterdir() if p.name.isdigit() and (p/'comm').exists() and (p/'comm').read_text().strip()=='HwaSim_IR' and str((p/'cwd').resolve())==target]
 if pids:break
 time.sleep(.1)
assert len(pids)==1,pids
pid=pids[0]
log=pathlib.Path('/tmp/hwasimir-p20-logs/CASE_VALUE/precise/board.log');deadline=time.monotonic()+40
while time.monotonic()<deadline:
 if log.exists() and '[VideoOutputPerf] channel=precise platID=1001 sensorID=2 frame=120 ' in log.read_text(errors='replace'):break
 time.sleep(.05)
else:raise RuntimeError('business frame120 not observed')
maps=pathlib.Path('/proc/'+pid+'/maps').read_text();r=subprocess.run(['/tmp/p20_cpu_sample',pid,'6'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,check=True)
counts=collections.Counter(int(x,16) for x in r.stdout.splitlines());groups=collections.defaultdict(dict)
for ip,n in counts.items():
 for line in maps.splitlines():
  f=line.split();start,end=[int(x,16) for x in f[0].split('-')]
  if start<=ip<end and len(f)>=6 and f[5].startswith('/'):
   path=f[5];offset=ip-start+int(f[2],16);groups[path][offset]=groups[path].get(offset,0)+n;break
functions=[]
for path,offsets in groups.items():
 header=subprocess.check_output(['readelf','-h',path],text=True)
 values=list(offsets)
 if 'EXEC (Executable file)' in header:
  # 主程序非PIE时恢复绝对地址。
  base=next(int(l.split()[0].split('-')[0],16)-int(l.split()[2],16) for l in maps.splitlines() if l.split()[-1]==path)
  addresses=[hex(v+base) for v in values]
 else:addresses=[hex(v) for v in values]
 result=subprocess.check_output(['addr2line','-f','-C','-e',path,*addresses],text=True).splitlines()
 aggregate=collections.Counter()
 for i,offset in enumerate(values):aggregate[result[i*2]]+=offsets[offset]
 functions.extend(dict(module=path,function=k,samples=v) for k,v in aggregate.items())
functions.sort(key=lambda x:x['samples'],reverse=True)
print(json.dumps(dict(pid=pid,maps=maps,ips=r.stdout,errors=r.stderr,samples=sum(counts.values()),functions=functions,scope='6s main-thread user CPU samples after business frame120; stripped vendor symbol names are approximate address buckets; not production FPS')))
'''.replace('BUILD_VALUE',args.build).replace('CASE_VALUE',name)
  code=base64.b64encode(script.encode()).decode()
  r=subprocess.run(['ssh','-o','UpdateHostKeys=no','-o','ConnectTimeout=10','root@192.168.1.116','python3 -c '+shlex.quote("import base64;exec(base64.b64decode('"+code+"'))")],env=env,capture_output=True,text=True,timeout=60)
  (root/'logs/p20'/f'{name}_sample_remote.log').write_text(r.stderr,encoding='utf-8')
  if r.returncode==0:(root/'logs/p20'/f'{name}_samples.json').write_text(r.stdout,encoding='utf-8')
  else:print(r.stderr)
 finally:p.wait(timeout=100)
print('board test exit',p.returncode)
