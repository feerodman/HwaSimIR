"""Production Qt single/dual provider coverage of every unmodified customer row."""
import configparser,csv,hashlib,json,os,subprocess
from pathlib import Path
R=Path(__file__).resolve().parents[1];out=R/'logs/p20m3/20261021_engineering/preflight';out.mkdir(exist_ok=True)
stim=R/'build-p20m3-stim/release';network=stim/'NetworkConfig.ini'
c=configparser.ConfigParser(interpolation=None);c.read(network,encoding='utf-8-sig');qos=c['DdsProtocol']['QosFile'];domain=c.getint('DdsProtocol','DomainId')
records=[]
for name in ('circleDataAll.csv','circleDataAll2.csv','circleDataAll3.csv'):
 source=R/'1020测试内容'/name;rows=list(csv.reader(source.open(encoding='utf-8-sig')))[1:]
 channels=[]
 for role,band,width,sensor in [('coarse',2,320,1),('precise',0,800,2)]:
  channels.append(dict(enabled=True,role=role,platID=1001,sensorID=sensor,source=str(source),followSource=True,model=34,followModel=True,pixelFormat='gray8',platformFromSource=True,init=dict(trackerSensorBand=band,trackerSensorWidth=width,trackerSensorHeight=width,simMode=1,videoFps=60,envVisibility=6000,envHumidity=85)))
 config=out/(source.stem+'.json');config.write_text(json.dumps(dict(version=19,domain=domain,source=str(source),model=34,stepMs=1000/60,qos=qos,observeStatus=True,channels=channels),indent=2))
 cmd=[str(stim/'DataDrivenTestQT.exe'),'--network-config='+str(network),'--channels-json='+str(config),'--validate-dual']
 env=os.environ.copy();env['QT_FORCE_STDERR_LOGGING']='1';si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
 result=subprocess.run(cmd,cwd=stim,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=90,startupinfo=si)
 (out/(source.stem+'.log')).write_bytes(result.stdout)
 records.append(dict(source=str(source),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),rows=len(rows),source_span_ms=float(rows[-1][0])-float(rows[0][0]),wall_deadline_span_ms=(len(rows)-1)*1000/60,exit_code=result.returncode,command=cmd))
 print(name,len(rows),result.returncode,flush=True)
(out/'results.json').write_text(json.dumps(records,indent=2));raise SystemExit(any(r['exit_code'] for r in records))
