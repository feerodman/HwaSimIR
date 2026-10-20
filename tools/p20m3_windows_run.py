"""Run the isolated real Windows renderer and production Qt entry, with GPU capture."""
import argparse,configparser,json,os,shutil,subprocess,time
from pathlib import Path
R=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--band',type=int,choices=[0,2],required=True);ap.add_argument('--name',required=True);ap.add_argument('--frames',type=int,default=180);ap.add_argument('--source',default='1020测试内容/circleDataAll3.csv');ap.add_argument('--lifecycle',action='store_true');ap.add_argument('--renderer-directory',default='build-p20m3-renderer/bin');ap.add_argument('--legacy-single-ui',action='store_true');ap.add_argument('--rounds',type=int,default=1);ap.add_argument('--utc-hour',type=float,default=3.5);ap.add_argument('--reference-mismatch',action='store_true');a=ap.parse_args()
out=R/'logs/p20m3/20261021_engineering'/a.name;out.mkdir(exist_ok=False)
render=R/a.renderer_directory;stim=R/'build-p20m3-stim/release'
source=out/'input.csv';lines=(R/a.source).read_text(encoding='utf-8-sig').splitlines();source.write_text('\n'.join(lines[:a.frames+1])+'\n',encoding='utf-8')
sender=out/'sender.ini';shutil.copy2(stim/'NetworkConfig.ini',sender)
c=configparser.ConfigParser(interpolation=None);c.optionxform=str;c.read(sender,encoding='utf-8-sig');qos=c['DdsProtocol']['QosFile']
c['Demo']['UtcHour']=str(a.utc_hour)
with sender.open('w',encoding='utf-8') as f:c.write(f)
network=out/'network.ini';network.write_text('[Identity]\nchannel=precise\nplatID=1001\nsensorID=2\nacceptSensorBroadcast=true\n[UDP]\nlocalIp=0.0.0.0\nlocalPort=8893\nremoteIp=127.0.0.1\nremotePort=9998\n[TCP]\nserverIp=127.0.0.1\nserverPort=5555\n')
channel=dict(enabled=True,role='precise',platID=1001,sensorID=2,source=str(source),followSource=True,model=0x22,followModel=True,pixelFormat='gray8',platformFromSource=True,
 init=dict(trackerSensorWidth=800,trackerSensorHeight=800,h264En=1,saveMP4En=0,simMode=1,videoFps=60,trackerSensorBand=a.band,envVisibility=6000,envHumidity=85))
dual=dict(version=19,domain=151,source=str(source),model=0x22,stepMs=1000/60,qos=qos,observeStatus=True,channels=[channel,dict(enabled=False,role='coarse',platID=1001,sensorID=1)])
dual['engineeringLifecycle']=a.lifecycle
dual['diagnosticReferenceMismatch']=a.reference_mismatch
(out/'dual.json').write_text(json.dumps(dual,indent=2))
env=os.environ.copy();env.update(HWASIMIR_RUNTIME_CONFIG=str(render/'Config/HwaSimIRRuntime.ini'),HWASIMIR_RUN_ID=a.name,HWASIMIR_PHYSICAL_TRACE=str(out/'physical.jsonl'),LinearDiagnosticPath=str(out/'linear'),LinearDiagnosticSeq='60',Stage5OutputFrameDump='true',Stage5OutputFrameDumpEvery='60',Stage5OutputFrameDumpPath=str(out/'display.png'),EnableStage5RadianceDebug='false',QT_FORCE_STDERR_LOGGING='1',HwaInputAuditDirectory=str(out),HwaStageAuditDirectory=str(out),HwaSimIRExitOnStop='true')
env['PATH']=str(render)+';F:/Programs/Panda3D-1.10.15-x64/bin;'+env['PATH']
if a.rounds>1:env['HwaSimIRExitOnStop']='false'
si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
commands={};processes=[];files=[]
def start(name,cmd,cwd,custom):
 f=(out/(name+'.log')).open('wb');files.append(f);commands[name]=cmd
 p=subprocess.Popen(cmd,cwd=cwd,env=custom,stdout=f,stderr=subprocess.STDOUT,startupinfo=si);processes.append(p);return p
try:
 renderer=start('renderer',[str(render/'HwaSim_IR.exe'),'--channel','precise','--network-config',str(network)],render,env)
 time.sleep(4)
 if renderer.poll() is not None:raise RuntimeError('renderer startup exit '+str(renderer.returncode))
 command=[str(stim/('P20MDirectDds.exe' if (a.lifecycle or a.reference_mismatch) else 'DataDrivenTestQT.exe')),'--network-config='+str(sender),'--channels-json='+str(out/'dual.json'),'--duration-sec='+str(max(10,int(a.frames/60)+5)),'--rounds='+str(a.rounds)]
 if a.legacy_single_ui:
  (out/'sensor.json').write_text(json.dumps({k:v for k,v in channel['init'].items() if k.startswith('trackerSensor') or k in ('h264En','saveMP4En')}))
  command+=['--legacy-single-ui','--channel=precise','--input-file='+str(source),'--plat-id=1001','--sensor-id=2','--sim-mode=1','--video-fps=60','--send-step-ms=16.666666666667','--sensor-band='+str(a.band),'--sensor-fields-json='+str(out/'sensor.json')]
 qt=start('qt',command,stim,env)
 code=qt.wait(timeout=(100+a.frames/60)*a.rounds)
 try:renderer.wait(timeout=15)
 except subprocess.TimeoutExpired:pass
 (out/'result.json').write_text(json.dumps(dict(commands=commands,qt_exit=code,renderer_exit=renderer.poll(),status='EXECUTED_NOT_YET_ASSESSED'),indent=2))
finally:
 for p in processes:
  if p.poll() is None:p.terminate();p.wait(timeout=15)
 for f in files:f.close()
print(out)
