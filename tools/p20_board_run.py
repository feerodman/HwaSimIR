"""P20 板端双路验收；独立目录和部署收据，保留逐帧守恒证据。"""
import argparse, json, os, subprocess, time, re, shlex, shutil, hashlib, psutil, threading, base64
from pathlib import Path
from p20_runtime_policy import RuntimePolicy
ROOT=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--name',required=True);ap.add_argument('--frames',type=int,default=120)
ap.add_argument('--roles',choices=['both','coarse','precise'],default='both');ap.add_argument('--rounds',type=int,default=1)
ap.add_argument('--coarse-sensor',type=int,default=1);ap.add_argument('--precise-sensor',type=int,default=2)
ap.add_argument('--precise-plat',type=int,default=1001);ap.add_argument('--coarse-codec',choices=['raw','h264'],default='raw')
ap.add_argument('--precise-codec',choices=['raw','h264'],default='h264');ap.add_argument('--width',type=int,default=0)
ap.add_argument('--pixels',choices=['gray8','rgb24'],default='gray8');ap.add_argument('--customer-only',action='store_true')
ap.add_argument('--different-models',action='store_true');ap.add_argument('--fault-reader',action='store_true')
ap.add_argument('--render-env',action='append',default=[]);ap.add_argument('--application-fault',action='store_true')
ap.add_argument('--old-display',action='store_true');ap.add_argument('--old-reopen',action='store_true')
ap.add_argument('--transport',choices=['tcpv4','udpv4'],default='tcpv4')
ap.add_argument('--sky',type=int,choices=range(6));ap.add_argument('--different-sources',action='store_true')
ap.add_argument('--source-offset',type=int,default=0)
ap.add_argument('--source',default='DataDrivenTestQT/1.txt',help='普通数据文件，不限定名称或行数')
ap.add_argument('--control-domain',type=int,default=1)
ap.add_argument('--video-domain',type=int,default=1)
ap.add_argument('--customer-domain',type=int,default=1)
ap.add_argument('--wrong-domain-probe',type=int,default=-1)
ap.add_argument('--pin-render',action='store_true')
ap.add_argument('--use-deployed-domains',action='store_true',help='Use the actual runtime INI; assert its recorded domains match the requested test')
ap.add_argument('--use-deployed-qos',action='store_true',help='Keep the renderer INI QoS path; no per-test XML override')
ap.add_argument('--receipt',default='logs/p19/deployment/current.json')
ap.add_argument('--tmp-board-logs',action='store_true',help='后续测试日志写入 /tmp，结束后仍复制到本机；不删除旧证据')
ap.add_argument('--sender-tcp-nodelay',action='store_true',help='受控诊断：激励侧协议Writer启用SDK push_on_write/TCP_NODELAY')
ap.add_argument('--band',type=int,choices=[0,2],default=2)
ap.add_argument('--precise-band',type=int,choices=[0,2],default=None)
ap.add_argument('--fps',type=int,default=60)
ap.add_argument('--input-hz',type=float,default=None,help='Packet send rate; independent of configured video FPS')
ap.add_argument('--stim-directory',default='build-p19-stim/release')
ap.add_argument('--utc-date',default=None,help='Explicit replay start date, yyyy-MM-dd')
ap.add_argument('--utc-hour',type=float,default=None)
ap.add_argument('--direct-dds-sender',default=None,help='Independent sender executable; does not disable production stim preflight')
ap.add_argument('--diagnostic-reference-mismatch',action='store_true')
ap.add_argument('--scheduling',choices=['original','performance'],default='original')
ap.add_argument('--p20m2-60hz',action='store_true',help='Require 60 Hz input and videoFps=60; record direct-sender deadlines')
a=ap.parse_args()
if not 1<=a.fps<=240:raise SystemExit('invalid configured video FPS')
if a.input_hz is None:a.input_hz=float(a.fps)
if not 0<a.input_hz<=1000:raise SystemExit('invalid packet input rate')
if a.p20m2_60hz and (a.input_hz!=60 or a.fps!=60):
    raise SystemExit('P20M2 performance requires input-hz=60 and fps=60')
if (a.utc_date is None)!=(a.utc_hour is None):raise SystemExit('Both replay UTC date and hour are required')
if a.utc_date is not None:
    import datetime
    datetime.date.fromisoformat(a.utc_date)
    if not 0<=a.utc_hour<24:raise SystemExit('invalid UTC hour')
if any(not re.fullmatch('[A-Za-z][A-Za-z0-9_]*=[A-Za-z0-9_./,:-]+',v) for v in a.render_env):raise SystemExit('invalid render environment')
if a.fault_reader and (a.roles!='coarse' or a.rounds!=2 or a.frames<900):raise SystemExit('fault test requires coarse, two rounds, >=900 frames')
if not re.fullmatch('[A-Za-z0-9_]+',a.name):raise SystemExit('invalid name')
out=ROOT/'logs/p20/runs'/a.name;out.mkdir(parents=True,exist_ok=False)
env=os.environ.copy();env.update(SSH_ASKPASS=str(ROOT/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p19',QT_FORCE_STDERR_LOGGING='1')
opts=['-o','UpdateHostKeys=no','-o','ConnectTimeout=10']
def ssh(command):return subprocess.check_output(['ssh',*opts,'root@192.168.1.116',command],env=env,text=True)
def board_audit(role):
    return ('/tmp/hwasimir-p20-logs/'+a.name+'/'+role if a.tmp_board_logs else receipt['instances'][role]['root']+'/logs/'+a.name)
rxdir=ROOT/'build-p19-customer-receiver/release';stimdir=ROOT/a.stim_directory
ffdir=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin'
env['PATH']=str(rxdir)+os.pathsep+env['PATH']
testQos=out/'qos.xml';testQos.write_text((ROOT/'HwaSim_IR/Bin/Config/DDS/ZRDDS_PROTOCOL_QOS.xml').read_text(encoding='utf-8-sig').replace('tcpv4://default//0',a.transport+'://default//0'),encoding='utf-8')
if a.sender_tcp_nodelay:
    text=testQos.read_text(encoding='utf-8')
    if '<push_on_write>true</push_on_write>' not in text:text=text.replace('<datawriter_qos name="hwasimir_protocol_writer">','<datawriter_qos name="hwasimir_protocol_writer"><protocol><push_on_write>true</push_on_write></protocol>')
    testQos.write_text(text,encoding='utf-8')
receiptPath=ROOT/a.receipt
if not receiptPath.exists():raise SystemExit('部署收据不存在')
receipt=json.loads(receiptPath.read_text())
if a.use_deployed_domains:
    for role in ('coarse','precise'):
        if a.roles in ('both',role):
            assert receipt['instances'][role]['domains']==dict(control=a.control_domain,oldVideo=a.video_domain,customerVideo=a.customer_domain), 'deployed domain receipt differs'
ssh('! pgrep -x HwaSim_IR >/dev/null')
(out/'processes_before.json').write_text(json.dumps([p.info for p in psutil.process_iter(['pid','name']) if p.info['name'] and ('HwaSim' in p.info['name'] or 'DataDriven' in p.info['name'])],indent=2))
# A legitimate renamed, shorter source; never modify 1.txt or gate on its hash.
lines=(ROOT/a.source).read_text(encoding='utf-8-sig').splitlines()
header=next(i for i,l in enumerate(lines) if l.strip().startswith('Time'))
rows=[l for l in lines[header+1:] if l.strip()]
if not 1<=a.frames<=len(rows) or not 0<=a.source_offset<=len(rows)-a.frames:raise SystemExit('frame range')
source=out/'ordinary_replay_subset.csv';source.write_text(lines[header]+'\n'+'\n'.join(rows[a.source_offset:a.source_offset+a.frames])+'\n',encoding='utf-8')
config=(ROOT/'DataDrivenTestQT/NetworkConfig.ini').read_text(encoding='utf-8-sig')
config=re.sub(r'(?m)^DomainId=.*$','DomainId='+str(a.control_domain),config)
config=re.sub(r'(?m)^QosFile=.*$','QosFile='+str(testQos).replace('\\','/'),config)
if a.utc_date is not None:
    config=re.sub(r'(?m)^UtcDate=.*$','UtcDate='+a.utc_date,config)
    config=re.sub(r'(?m)^UtcHour=.*$','UtcHour='+str(a.utc_hour),config)
(out/'sender.ini').write_text(config,encoding='utf-8')
channels=[];selected=[]
for index,role in enumerate(('coarse','precise')):
    enabled=a.roles in ('both',role);width=a.width or (320 if index==0 else 800);codec=a.coarse_codec if index==0 else a.precise_codec
    item=dict(enabled=enabled,role=role,platID=1001 if index==0 else a.precise_plat,sensorID=a.coarse_sensor if index==0 else a.precise_sensor,
              source=str(source),followSource=True,model=0x22 if index==0 else 0x33,followModel=not a.different_models,pixelFormat=a.pixels,platformFromSource=True,
              init=dict(trackerSensorWidth=width,trackerSensorHeight=width,h264En=int(codec=='h264'),saveMP4En=int(a.old_display),simMode=1,videoFps=a.fps,trackerSensorBand=a.precise_band if index==1 and a.precise_band is not None else a.band))
    channels.append(item)
    if a.sky is not None:item['init']['envSky']=a.sky
    if a.different_sources and index==1:
        other=out/'precise_alternative_data.txt';other.write_text(lines[header]+'\n'+'\n'.join(rows[10:10+a.frames])+'\n',encoding='utf-8')
        item.update(source=str(other),followSource=False)
    if enabled:selected.append((role,item,codec,width))
dual=dict(version=19,domain=a.control_domain,source=str(source),model=0x22,stepMs=1000/a.input_hz,qos=str(testQos).replace('\\','/'),observeStatus=not a.customer_only,channels=channels)
dual['diagnosticReferenceMismatch']=a.diagnostic_reference_mismatch
(out/'dual.json').write_text(json.dumps(dual,indent=2))
processes=[];files=[];commands={};results=dict(options=vars(a),status='FAIL',customerHardware='NOT_VERIFIED',actualCustomerRaw='NOT_VERIFIED',physicalAcceptance='NOT_ASSESSED',configuredVideoFps=a.fps,inputPacketHz=a.input_hz);rxs={};boards={}
def launch(command,cwd,name,customEnv=None):
    log=(out/(name+'.log')).open('wb');files.append(log);si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=0
    p=subprocess.Popen(command,cwd=cwd,env=customEnv or env,stdout=log,stderr=subprocess.STDOUT,startupinfo=si);processes.append(p);commands[name]=command;return p
start=time.monotonic()
faultThread=None
wrongProbe=None
runtimePolicy=None
try:
    runtimePolicy=RuntimePolicy(ssh,out,receipt['instances']['precise']['root'],a.scheduling)
    monitor="""import pathlib,time,json
for i in range(900):
 d={'steadyNs':time.monotonic_ns()}
 for pat in ('/sys/class/thermal/thermal_zone*/temp','/sys/class/devfreq/*/cur_freq','/sys/devices/system/cpu/cpufreq/policy*/scaling_cur_freq'):
  import glob
  for f in glob.glob(pat):
   try:d[f]=pathlib.Path(f).read_text().strip()
   except OSError:pass
 print(json.dumps(d),flush=True);time.sleep(1)
"""
    launch(['ssh',*opts,'root@192.168.1.116','python3 -u -c '+shlex.quote("import base64;exec(base64.b64decode('"+base64.b64encode(monitor.encode()).decode()+"'))")],ROOT,'board_monitor')
    if a.wrong_domain_probe>=0:
        probeDir=ROOT/'build-p19-minimal/release'
        wrongProbe=launch([str(probeDir/'CustomerRawReceiverMinimal.exe'),'--domain',str(a.wrong_domain_probe),'--qos',str(testQos),'--seconds','25'],probeDir,'wrong_domain_probe')
    if a.fault_reader:
        probe=ROOT/'build-p19-payload-probe/release'
        fault=launch([str(probe/'CustomerTxPayloadProbe.exe'),'--mode','hold','--domain',str(a.customer_domain),'--qos',str(testQos),'--hold-seconds','60'],probe,'fault_reader')
    for role,item,codec,width in selected:
        local=out/role;local.mkdir();shutil.copy2(rxdir/'zrddslicence.lic',local/'zrddslicence.lic')
        topic='TX/'+('H' if role=='precise' else 'L')+('D264' if codec=='h264' else 'Decode')
        rxs[role]=launch([str(rxdir/'HwaSimIRCustomerTxVideoReceiverDemo.exe'),'--topic',topic,'--codec',codec,'--pixel-format',a.pixels,'--width',str(width),'--height',str(width),'--fps',str(a.fps),'--domain',str(a.customer_domain),'--qos',str(testQos),'--output',str(local),'--display','0','--audit','1','--timeout-ms',str(max(90000,int(a.frames*a.rounds/a.input_hz*1000)+90000)),'--idle-ms',('25000' if a.rounds>1 else '4000') if codec=='raw' else '0','--exit-after-ends',str(a.rounds) if codec=='h264' else '0','--ffmpeg',str(ffdir/'ffmpeg.exe')],local,role+'_receiver')
        remote=receipt['instances'][role]['root'];audit=board_audit(role)
        # Identity is changed in a per-test local network file, never claimed to be sent by INIT.
        net=ssh('cat '+shlex.quote(remote+'/network.ini'));net=re.sub(r'(?m)^platID=.*$',f"platID={item['platID']}",net);net=re.sub(r'(?m)^sensorID=.*$',f"sensorID={item['sensorID']}",net)
        network=out/(role+'_network.ini');network.write_text(net)
        if a.tmp_board_logs:ssh('mkdir -p '+shlex.quote('/tmp/hwasimir-p20-logs/'+a.name))
        ssh('mkdir '+shlex.quote(audit));subprocess.run(['scp',*opts,str(network),'root@192.168.1.116:'+audit+'/network.ini'],env=env,check=True)
        subprocess.run(['scp',*opts,str(testQos),'root@192.168.1.116:'+audit+'/qos.xml'],env=env,check=True)
        extra=f"HwaSimIRCustomerPixelFormat={a.pixels} "+' '.join(a.render_env)+runtimePolicy.environment()
        if not a.use_deployed_qos:
            extra+=f' HwaSimIRDdsProtocolQosFile={audit}/qos.xml HwaSimIRDdsVideoQosFile={audit}/qos.xml'
        if not a.use_deployed_domains:
            extra+=f' HwaSimIRDdsProtocolDomainId={a.control_domain} HwaSimIRDdsVideoDomainId={a.video_domain} HwaSimIRCustomerDomainId={a.customer_domain}'
        if a.application_fault:extra+=' HwaSimIRCustomerDiagnosticFailAfterFramesOnce=60'
        if a.customer_only:extra+=' HwaSimIRDdsVideoEnable=false TcpSendVideo=false TcpSendRealtimeData=false TcpSendAnnotation=false TcpForwardInitControl=false'
        command=f"cd '{remote}' && timeout -s TERM -k 10s {max(180,int(a.frames*a.rounds/a.input_hz)+180)}s env HWASIMIR_ROOT='{remote}' HWASIMIR_CHANNEL={role} HWASIMIR_NETWORK_CONFIG='{audit}/network.ini' HWASIMIR_RUNTIME_CONFIG='{remote}/Config/HwaSimIRRuntime.ini' ZRDDS_HOME='{remote}/zrdds' RenderPresentationMode=HeadlessOffscreen HwaSimIRExitOnStop=false HwaSimIRLocalRecordingEnable=false HwaSimIRCustomerAuditPrefix='{audit}/customer' HwaInputAuditDirectory='{audit}' HwaStageAuditDirectory='{audit}' {extra} ./run_precise.sh"
        command+=f" >'{audit}/board.log' 2>&1; echo $? >'{audit}/board.exit'"
        boards[role]=launch(['ssh',*opts,'root@192.168.1.116','sh -c '+shlex.quote(command)],ROOT,role+'_transport')
    time.sleep(4)
    results['rendererPidsBefore']=ssh('pgrep -a HwaSim_IR')
    (out/'thread_placement.txt').write_text(ssh('for p in $(pgrep -x HwaSim_IR); do for t in /proc/$p/task/*/status; do grep -E "Name:|Pid:|Cpus_allowed_list:" $t; done; done'),encoding='utf-8')
    if a.pin_render:
        placement=[]
        for role,_,_,_ in selected:
            cpu=4 if role=='coarse' else 6
            remote=receipt['instances'][role]['root']
            placement.append(ssh("for p in $(pgrep -x HwaSim_IR); do [ \"$(readlink /proc/$p/cwd)\" = '"+remote+"' ] && taskset -pc "+str(cpu)+" $p; done; true"))
        (out/'render_placement.log').write_text(''.join(placement))
        (out/'thread_placement_after.txt').write_text(ssh('for p in $(pgrep -x HwaSim_IR); do for t in /proc/$p/task/*/status; do grep -E "Name:|Pid:|Cpus_allowed_list:" $t; done; done'),encoding='utf-8')
    if a.old_display:
        vdDir=ROOT/'HwaSim_IR_VideoDisplay/x64/Release'
        oldConfig=re.sub(r'(?m)^DomainId=.*$','DomainId='+str(a.video_domain),(vdDir/'NetworkConfig.ini').read_text(encoding='utf-8-sig'))
        oldIni=out/'VideoDisplay.ini';oldIni.write_text(oldConfig,encoding='utf-8')
        def old_receiver(reopened=False):
            suffix='old_reopened' if reopened else 'old_receiver'
            vdenv=env.copy();vdenv.update(P7RecordingRoot=str(out/suffix),P5DdsVideoPath=str(out/(suffix+'.h264')),P5DdsVideoSamples='10000')
            return launch([str(vdDir/'HwaSim_IR_VideoDisplay.exe'),'--network-config='+str(oldIni),'--dds-domain',str(a.video_domain),'--acceptance-exit-ms','18000' if a.old_reopen and not reopened else str(max(35000,int(a.frames*a.rounds/a.input_hz*1000)+25000))],vdDir,suffix,vdenv)
        vd=old_receiver()
    stimEnv=env.copy();stimEnv['HwaInputAuditDirectory']=str(out)
    senderExe=ROOT/a.direct_dds_sender if a.direct_dds_sender else stimdir/'DataDrivenTestQT.exe'
    senderCommand=[str(senderExe),'--network-config='+str(out/'sender.ini'),'--channels-json='+str(out/'dual.json'),'--rounds='+str(a.rounds),'--duration-sec='+str(max(15,int(a.frames/a.input_hz)+8))]
    if a.p20m2_60hz and a.direct_dds_sender:
        senderCommand+=['--p20m2-60hz','--pacing-jsonl='+str(out/'send_pacing.jsonl')]
    sender=launch(senderCommand,stimdir,'sender',stimEnv)
    if a.fault_reader:
        def induce_fault():
            events=[]
            deadline=time.monotonic()+30
            while time.monotonic()<deadline and sender.poll() is None:
                if '[P19UIRealtime]' in (out/'sender.log').read_text(errors='replace'):
                    psutil.Process(fault.pid).suspend()
                    events.append(dict(action='suspend_owned_reader_process',pid=fault.pid,elapsedSeconds=time.monotonic()-start))
                    time.sleep(8)
                    fault.kill();fault.wait()
                    events.append(dict(action='remove_fault_reader',elapsedSeconds=time.monotonic()-start))
                    break
                time.sleep(.05)
            (out/'fault_events.json').write_text(json.dumps(events,indent=2))
        faultThread=threading.Thread(target=induce_fault);faultThread.start()
    (out/'commands.json').write_text(json.dumps(commands,indent=2))
    if a.old_reopen:
        results['oldInitialExit']=vd.wait(timeout=45);vd=old_receiver(True)
    results['senderExit']=sender.wait(timeout=max(120,int(a.frames*a.rounds/a.input_hz)+120))
    results['rendererPidsAfter']=ssh('pgrep -a HwaSim_IR')
    for role,p in rxs.items():results[role+'ReceiverExit']=p.wait(timeout=35)
    if a.old_display:results['oldReceiverExit']=vd.wait(timeout=max(75,int(a.frames*a.rounds/a.input_hz)+40))
    for role,item,codec,width in selected:
        remote=receipt['instances'][role]['root'];audit=board_audit(role)
        subprocess.run(['scp',*opts,'-r','root@192.168.1.116:'+audit,str(out/role/'board')],env=env,check=True)
        summary=json.loads((out/role/'summary.json').read_text());results[role]=summary
        if (out/role/'output.mp4').exists():
            probe=json.loads(subprocess.check_output([str(ffdir/'ffprobe.exe'),'-v','error','-count_frames','-select_streams','v:0','-show_entries','stream=width,height,nb_read_frames','-of','json',str(out/role/'output.mp4')]))
            (out/role/'ffprobe.json').write_text(json.dumps(probe,indent=2));summary['savedFramesVerified']=int(probe['streams'][0]['nb_read_frames'])
        tx=[json.loads(line) for f in sorted((out/role/'board').glob('customer_round*.jsonl')) for line in f.read_text().splitlines()]
        rx=[json.loads(line) for line in (out/role/'receive.jsonl').read_text().splitlines()]
        summary['senderFrames']=len(tx)
        if codec=='raw':summary['pixelsIdentical']=[t['sha256'] for t in tx]==[r['sha256'] for r in rx if r['kind']=='sample']
        else:
            h=hashlib.sha256()
            for f in sorted((out/role/'board').glob('customer_round*.h264')):h.update(f.read_bytes())
            summary['cleanStreamIdentical']=h.hexdigest()==summary['streamSha256']
            summary['endReceiveToLastDecodedMs']=(summary['lastDecodedNs']-summary['endReceivedNs'])/1e6
            summary['endReceiveToMp4Ms']=(summary['mp4CompleteNs']-summary['endReceivedNs'])/1e6
        summary['countPass']=summary['senderFrames']==summary['decodedFrames']==summary.get('savedFramesVerified')==a.frames*a.rounds
    results['samePids']=results['rendererPidsBefore']==results['rendererPidsAfter']
    results['status']='PASS' if results['senderExit']==0 and all(results[r+'ReceiverExit']==0 and results[r]['countPass'] and results[r].get('pixelsIdentical',results[r].get('cleanStreamIdentical',False)) for r,_,_,_ in selected) else 'FAIL'
    if wrongProbe:
        code=wrongProbe.wait(timeout=30)
        probeLines=(out/'wrong_domain_probe.log').read_text(errors='replace').splitlines()
        stats=json.loads(next(line for line in reversed(probeLines) if line.startswith('{')))
        results['wrongDomainIsolation']={'domain':a.wrong_domain_probe,'exitCode':code,'receivedSamples':stats['receivedSamples'],'status':'PASS' if code==2 and stats['receivedSamples']==0 else 'FAIL'}
        if results['wrongDomainIsolation']['status']!='PASS':results['status']='FAIL'
    if a.fault_reader or a.application_fault:
        board=(out/'coarse/board/board.log').read_text(errors='replace')
        rounds=sorted((out/'coarse/board').glob('customer_round*.jsonl'))
        second=[json.loads(line) for line in rounds[-1].read_text().splitlines()] if len(rounds)>=2 else []
        received=[r for r in rx if r['kind']=='sample']
        results['faultRecovery']={'sdkWriteFailureObserved':'stage=typed_DataWriter_write' in board and 'abort_customer_round' in board and not a.application_fault,
            'applicationFailureObserved':'diagnostic_application_failure_once_NOT_SDK_RETURN_CODE' in board,
            'secondRoundFrames':len(second),'secondRoundPixelsIdentical':len(second)==a.frames and [t['sha256'] for t in second]==[r['sha256'] for r in received[-a.frames:]],
            'runtimeInitCountOne':'runtimeInitCount=1' in board,'faultKind':'explicit_application_failure_once' if a.application_fault else 'real_reader_process_suspension_no_return_code_injection'}
        keys=('applicationFailureObserved' if a.application_fault else 'sdkWriteFailureObserved','secondRoundPixelsIdentical','runtimeInitCountOne')
        results['status']='PASS' if results['samePids'] and results['senderExit']==0 and all(results['faultRecovery'][k] for k in keys) else 'FAIL'
    results['graphicsErrors']={}
    for role,_,_,_ in selected:
        log=(out/role/'board/board.log').read_text(errors='replace')
        errors=[line for line in log.splitlines() if re.search(r'Assertion failed:|Failed to compile|Shader compilation failed|GL error|GLSL.*error',line,re.I)]
        results['graphicsErrors'][role]=dict(count=len(errors),first=errors[:10])
        if errors:results['status']='FAIL'
except Exception as error:results['error']=str(error)
finally:
    if faultThread:faultThread.join(timeout=12)
    for role,item,codec,width in selected:
        remote=receipt['instances'][role]['root']
        try:ssh("for p in $(pgrep -x HwaSim_IR); do [ \"$(readlink /proc/$p/cwd)\" = '"+remote+"' ] && kill -TERM $p; done; true")
        except Exception:pass
    for p in processes:
        try:p.wait(timeout=5)
        except subprocess.TimeoutExpired:p.terminate();p.wait()
    for f in files:f.close()
    if runtimePolicy:results['systemPolicyRestored']=runtimePolicy.restore()
    results['elapsedSeconds']=time.monotonic()-start
    (out/'result.json').write_text(json.dumps(results,indent=2));print(json.dumps(results,indent=2))
raise SystemExit(0 if results['status']=='PASS' else 2)
