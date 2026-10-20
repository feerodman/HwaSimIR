"""同一精跟PID连续800→320→800 INIT；真实SDK、独立客户接收和标准EOS。
仅借用指定已完成测试的启动配置，不修改原始输入/部署身份/协议。
"""
from pathlib import Path
import argparse,subprocess,os,json,re,shlex,shutil,time,hashlib
from p20_runtime_policy import RuntimePolicy
root=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--name',required=True);ap.add_argument('--receipt',required=True);ap.add_argument('--reference-run',required=True);a=ap.parse_args()
assert re.fullmatch('[A-Za-z0-9_]+',a.name)
out=root/'logs/p20/runs'/a.name;out.mkdir(exist_ok=False);ref=root/'logs/p20/runs'/a.reference_run
receipt=json.loads((root/a.receipt).read_text(encoding='utf-8'));remote=receipt['instances']['precise']['root'];audit='/tmp/hwasimir-p20-logs/'+a.name
assert remote.startswith('/userdata/HwaSimIR/candidates/P20_')
env=os.environ.copy();env.update(SSH_ASKPASS=str(root/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p20',QT_FORCE_STDERR_LOGGING='1')
rx=root/'build-p19-customer-receiver/release';stim=root/'build-p19-stim/release';ff=root/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin';env['PATH']=str(rx)+os.pathsep+env['PATH'];opts=['-o','UpdateHostKeys=no','-o','ConnectTimeout=10']
def ssh(cmd):return subprocess.check_output(['ssh',*opts,'root@192.168.1.116',cmd],env=env,text=True,encoding='utf-8')
ssh('! pgrep -x HwaSim_IR >/dev/null')
qos=out/'qos.xml';shutil.copy2(ref/'qos.xml',qos)
source=out/'ordinary_resize_source.data';source.write_text('\n'.join((ref/'ordinary_replay_subset.csv').read_text(encoding='utf-8').splitlines()[:121])+'\n',encoding='utf-8')
senderIni=out/'sender.ini';senderIni.write_text(re.sub(r'(?m)^QosFile=.*$','QosFile='+qos.as_posix(),(ref/'sender.ini').read_text(encoding='utf-8')),encoding='utf-8')
processes=[];files=[];policy=None;result=dict(status='FAIL',build=receipt['build'],phases=[],performanceClaim=False)
def launch(cmd,cwd,log,custom=None):
 f=log.open('wb');files.append(f);si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
 p=subprocess.Popen(cmd,cwd=cwd,env=custom or env,stdout=f,stderr=subprocess.STDOUT,startupinfo=si);processes.append(p);return p
try:
 policy=RuntimePolicy(ssh,out,remote,'original');ssh('mkdir '+shlex.quote(audit))
 command='cd '+shlex.quote(remote)+' && timeout -s TERM -k 10s 360s env '+ ' '.join(shlex.quote(k+'='+v) for k,v in dict(HWASIMIR_ROOT=remote,HWASIMIR_CHANNEL='precise',HWASIMIR_RUNTIME_CONFIG=remote+'/Config/HwaSimIRRuntime.ini',HWASIMIR_NETWORK_CONFIG=remote+'/network.ini',ZRDDS_HOME=remote+'/zrdds',RenderPresentationMode='HeadlessOffscreen',HwaSimIRExitOnStop='false',HwaSimIRLocalRecordingEnable='false',HwaSimIRCustomerPixelFormat='gray8',HwaSimIRCustomerAuditPrefix=audit+'/customer',HwaInputAuditDirectory=audit,HwaStageAuditDirectory=audit).items())+policy.environment()+' ./run_precise.sh >'+shlex.quote(audit+'/board.log')+' 2>&1'
 launch(['ssh',*opts,'root@192.168.1.116','sh -c '+shlex.quote(command)],root,out/'transport.log');time.sleep(4)
 pid=ssh('pgrep -x HwaSim_IR').strip();assert pid.isdigit();result['pid']=int(pid)
 assert ssh('readlink /proc/'+pid+'/cwd').strip()==remote
 for number,width in enumerate([800,320,800],1):
  phase=out/('phase'+str(number)+'_'+str(width));phase.mkdir();shutil.copy2(rx/'zrddslicence.lic',phase/'zrddslicence.lic')
  cfg=json.loads((ref/'dual.json').read_text(encoding='utf-8'));cfg.update(source=str(source),qos=qos.as_posix())
  for item in cfg['channels']:
   item.update(enabled=item['role']=='precise',source=str(source),pixelFormat='gray8');item['init'].update(trackerSensorWidth=width,trackerSensorHeight=width,h264En=1,saveMP4En=0)
  dual=phase/'dual.json';dual.write_text(json.dumps(cfg,ensure_ascii=False,indent=2),encoding='utf-8')
  receiver=launch([str(rx/'HwaSimIRCustomerTxVideoReceiverDemo.exe'),'--domain',str(receipt['instances']['precise']['domains']['customerVideo']),'--qos',str(qos),'--topic','TX/HD264','--codec','h264','--width',str(width),'--height',str(width),'--pixel-format','gray8','--fps','60','--output',str(phase),'--display','0','--audit','1','--timeout-ms','75000','--idle-ms','0','--exit-after-ends','1','--ffmpeg',str(ff/'ffmpeg.exe')],phase,phase/'receiver.log')
  sendEnv=env.copy();sendEnv['HwaInputAuditDirectory']=str(phase)
  sender=launch([str(stim/'DataDrivenTestQT.exe'),'--network-config='+str(senderIni),'--channels-json='+str(dual),'--rounds=1','--duration-sec=15'],stim,phase/'sender.log',sendEnv)
  senderExit=sender.wait(timeout=85);rxExit=receiver.wait(timeout=35)
  subprocess.run(['scp',*opts,'-r','root@192.168.1.116:'+audit,str(phase/'board')],env=env,check=True)
  summary=json.loads((phase/'summary.json').read_text(encoding='utf-8'))
  probe=json.loads(subprocess.check_output([str(ff/'ffprobe.exe'),'-v','error','-count_frames','-select_streams','v:0','-show_entries','stream=width,height,nb_read_frames','-of','json',str(phase/'output.mp4')]))['streams'][0]
  identity=ssh('pgrep -x HwaSim_IR').strip()==pid
  received=hashlib.sha256((phase/'received.h264').read_bytes()).hexdigest();matches=any(hashlib.sha256(p.read_bytes()).hexdigest()==received for p in (phase/'board').glob('customer_round*.h264'))
  ok=senderExit==0 and rxExit==0 and identity and summary['decodedFrames']==120 and int(probe['nb_read_frames'])==120 and probe['width']==width and probe['height']==width and matches
  record=dict(width=width,samePid=identity,decodedFrames=summary['decodedFrames'],savedFrames=int(probe['nb_read_frames']),cleanStreamIdentical=matches,status='PASS' if ok else 'FAIL');result['phases'].append(record)
  assert ok, record
 log=(phase/'board/board.log').read_text(encoding='utf-8',errors='replace');result['runtimeInitCountOne']='runtimeInitCount=1' in log and 'runtimeInitCount=2' not in log
 result['graphicsErrors']=[line for line in log.splitlines() if re.search('Assertion failed:|GL error|Shader compilation failed|Failed to compile',line,re.I)]
 result['status']='PASS' if result['runtimeInitCountOne'] and not result['graphicsErrors'] else 'FAIL'
except Exception as error:result['error']=str(error)
finally:
 try:ssh('for p in $(pgrep -x HwaSim_IR); do [ "$(readlink /proc/$p/cwd)" = '+shlex.quote(remote)+' ] && kill -TERM $p; done; true')
 except Exception:pass
 for p in processes:
  try:p.wait(timeout=5)
  except subprocess.TimeoutExpired:p.terminate();p.wait()
 for f in files:f.close()
 if policy:result['systemPolicyRestored']=policy.restore()
 (out/'result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(result,ensure_ascii=False))
raise SystemExit(0 if result['status']=='PASS' else 2)
