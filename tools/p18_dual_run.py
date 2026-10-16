"""Real simultaneous RK3588 dual replay; no VideoDisplay process is launched."""
import argparse,datetime,hashlib,json,os,re,shutil,subprocess,time,psutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--name',required=True);ap.add_argument('--deployment',default='candidate4');ap.add_argument('--band',choices=['SWIR','MWIR'],default='SWIR')
    ap.add_argument('--customer-disabled',action='store_true');ap.add_argument('--rounds',type=int,default=1)
    ap.add_argument('--receiver-build',default='build-p18-customer-receiver-v4');a=ap.parse_args()
    if not re.fullmatch('[a-zA-Z0-9_]+',a.name):raise RuntimeError('unsafe case name')
    out=ROOT/'logs/p18/runs'/a.name;out.mkdir(parents=True,exist_ok=False)
    receipt=json.loads((ROOT/'logs/p18/deployment'/a.deployment/'receipt.json').read_text())
    env=os.environ.copy();env.update(SSH_ASKPASS=str(ROOT/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p18',QT_FORCE_STDERR_LOGGING='1')
    if not env.get('HWASIMIR_SSH_PASSWORD'):raise RuntimeError('missing SSH credential environment')
    opts=['-o','UpdateHostKeys=no','-o','ConnectTimeout=10']
    def ssh(cmd):return subprocess.check_output(['ssh',*opts,'root@192.168.1.116',cmd],env=env,text=True,encoding='utf-8')
    tasks=subprocess.check_output(['tasklist','/FO','CSV'],text=True,encoding='utf-8',errors='replace')
    if 'HwaSim_IR_VideoDisplay.exe' in tasks or 'DataDrivenTestQT.exe' in tasks:raise RuntimeError('existing VideoDisplay/stimulator must finish before isolated dual run')
    (out/'no_videodisplay_before.txt').write_text(tasks,encoding='utf-8')
    pre=ssh("set -eu; ! pgrep -x HwaSim_IR >/dev/null; date -u; df -h /userdata")
    (out/'board_preflight.log').write_text(pre,encoding='utf-8')
    rxdir=ROOT/a.receiver_build/'release';stimdir=ROOT/'build-DataDrivenTestQT-codex-mingw73_64-Release/release'
    env['PATH']=str(rxdir)+';'+env['PATH']
    config=(ROOT/'logs/p18/before/p18_before_swir_25urad_p17v12/DataDrivenTestQT.NetworkConfig.ini').read_text(encoding='utf-8-sig')
    config=re.sub(r'(?m)^trackerSensorBand=.*$',f'trackerSensorBand={0 if a.band=="SWIR" else 2}',config)
    (out/'sender.ini').write_text(config,encoding='utf-8')
    processes=[];streams=[];commands={};results={};boards={};receivers={}
    def launch(command,cwd,log,customEnv=env):
        stdout=open(out/(log+'.out.log'),'wb');stderr=open(out/(log+'.err.log'),'wb');streams.extend([stdout,stderr])
        si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=1 if log.endswith('_receiver') else 0
        p=subprocess.Popen(command,cwd=cwd,env=customEnv,stdout=stdout,stderr=stderr,startupinfo=si);processes.append(p);commands[log]=command;return p
    started=time.monotonic()
    try:
        for role,sensor,width,codec,topic in [('coarse',1,320,'raw','TX/LDecode'),('precise',2,800,'h264','TX/HD264')]:
            local=out/role;local.mkdir();shutil.copy2(rxdir/'zrddslicence.lic',local/'zrddslicence.lic')
            args=[str(rxdir/'HwaSimIRCustomerTxVideoReceiverDemo.exe'),'--topic',topic,'--codec',codec,'--pixel-format','gray8','--width',str(width),'--height',str(width),'--fps','60',
                '--domain','150','--qos',str(rxdir/'ZRDDS_PROTOCOL_QOS.xml'),'--output',str(local),'--idle-ms','12000','--timeout-ms','210000','--display','1','--ffmpeg',str(rxdir/'ffmpeg.exe')]
            if not a.customer_disabled:receivers[role]=launch(args,local,role+'_receiver')
            remote=receipt['instances'][role]['root'];audit=f'{remote}/logs/{a.name}'
            ssh(f"mkdir '{audit}'")
            extra='HwaSimIRCustomerVideoEnable=false' if a.customer_disabled else ''
            command=f"cd '{remote}' && timeout -s TERM -k 10s {100*a.rounds+100}s env HWASIMIR_ROOT='{remote}' HWASIMIR_CHANNEL={role} HWASIMIR_NETWORK_CONFIG='{remote}/network.ini' HWASIMIR_RUNTIME_CONFIG='{remote}/Config/HwaSimIRRuntime.ini' ZRDDS_HOME='{remote}/zrdds' RenderPresentationMode=HeadlessOffscreen HwaSimIRExitOnStop={'true' if a.rounds==1 else 'false'} HwaSimIRLocalRecordingEnable=false HwaSimIRCustomerAuditPrefix='{audit}/customer' HwaInputAuditDirectory='{audit}' HwaStageAuditDirectory='{audit}' {extra} ./run_precise.sh"
            boards[role]=launch(['ssh',*opts,'root@192.168.1.116',command],ROOT,role+'_board')
        # Read-only /proc sampling, separate from renderer and DDS payloads.
        monitor="i=0; while [ $i -lt "+str(100*a.rounds+90)+" ]; do echo SAMPLE $(cat /proc/uptime); for p in $(pgrep -x HwaSim_IR); do echo PID $p $(readlink /proc/$p/cwd); cat /proc/$p/stat; cat /proc/$p/status | sed -n '/VmRSS/p;/VmHWM/p;/Threads/p'; done; i=$((i+1)); sleep 1; done"
        monitorProcess=launch(['ssh',*opts,'root@192.168.1.116',monitor],ROOT,'memory_monitor')
        time.sleep(5)
        for role,p in boards.items():
            if p.poll() is not None:raise RuntimeError(role+' renderer exited before INIT')
        stimenv=env.copy();stimenv['HwaInputAuditDirectory']=str(out);stimenv['P7SenderUiDump']=str(out/'sender_ui.png')
        sender=launch([str(stimdir/'DataDrivenTestQT.exe'),'--network-config='+str(out/'sender.ini'),'--input-file='+str(ROOT/'DataDrivenTestQT/1.txt'),
            '--channel=precise','--plat-id=1001','--sensor-id=2','--dual-channel=1','--rounds='+str(a.rounds),'--duration-sec=120','--sensor-pixel-angle-urad=25','--save-mp4=0'],stimdir,'sender',stimenv)
        (out/'commands.json').write_text(json.dumps(commands,indent=2),encoding='utf-8')
        while sender.poll() is None:
            if time.monotonic()-started>100*a.rounds+85:raise RuntimeError('sender deadline exceeded')
            with (out/'windows_memory.jsonl').open('a') as memory:
                for role,process in receivers.items():
                    try:
                        proc=psutil.Process(process.pid);cpu=proc.cpu_times()
                        memory.write(json.dumps(dict(role=role,pid=process.pid,monotonicNs=time.monotonic_ns(),rssBytes=proc.memory_info().rss,userSeconds=cpu.user,systemSeconds=cpu.system))+'\n')
                    except psutil.Error:pass
            time.sleep(1)
        results['senderExit']=sender.returncode
        if a.rounds>1:
            # STOP is drained before the stimulator exits. Terminate only these
            # explicitly isolated instance PIDs after their final stopped state.
            for role in boards:
                remote=receipt['instances'][role]['root']
                ssh("for p in $(pgrep -x HwaSim_IR); do [ \"$(readlink /proc/$p/cwd)\" = '"+remote+"' ] && kill -TERM $p; done; true")
        for role,p in boards.items():results[role+'BoardExit']=p.wait(timeout=35)
        for role,p in receivers.items():results[role+'ReceiverExit']=p.wait(timeout=45)
        for role in boards:
            remote=receipt['instances'][role]['root']+'/logs/'+a.name
            subprocess.run(['scp',*opts,'-r',f'root@192.168.1.116:{remote}',str(out/role/'sender_audit')],env=env,check=True)
        ffprobe=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffprobe.exe'
        for role in receivers:
            local=out/role;summary=json.loads((local/'summary.json').read_text());results[role]=summary
            probe=json.loads(subprocess.check_output([str(ffprobe),'-v','error','-count_frames','-select_streams','v:0','-show_entries','stream=width,height,nb_read_frames','-of','json',str(local/'output.mp4')],text=True))
            (local/'ffprobe.json').write_text(json.dumps(probe,indent=2),encoding='utf-8');summary['savedFramesVerified']=int(probe['streams'][0]['nb_read_frames'])
            audits=list((local/'sender_audit').glob('customer_round*.jsonl'))
            rows=[json.loads(line) for p in audits for line in p.read_text().splitlines()];summary['senderFrames']=len(rows);summary['senderSamples']=sum(r['samples'] for r in rows)
            summary['expectedFrames']=4318*a.rounds
            if role=='precise':
                h264Streams=sorted((local/'sender_audit').glob('*.h264'));digest=hashlib.sha256()
                for stream in h264Streams:digest.update(stream.read_bytes())
                summary['cleanStreamSha256']=digest.hexdigest();summary['streamIdentical']=summary['cleanStreamSha256']==summary['streamSha256']
            else:
                incoming=[json.loads(line) for line in (local/'receive.jsonl').read_text().splitlines()];incoming=[r for r in incoming if r['kind']=='sample']
                summary['rawHashesIdentical']=[r['sha256'] for r in rows]==[r['sha256'] for r in incoming]
                summary['allRawBytes102400']=all(r['bytes']==102400 for r in incoming)
            summary['countPass']=len(rows)==summary['decodedFrames']==summary['savedFramesVerified']==4318*a.rounds and summary['senderSamples']==summary['receivedSamples']
        results['options']=vars(a)
        exitsOk=all(results[k]==0 or (a.rounds>1 and k.endswith('BoardExit') and results[k] in [143,-15]) for k in results if k.endswith('Exit'))
        results['functionalCountResult']=('NOT_APPLICABLE_customer_disabled' if a.customer_disabled else ('PASS' if all(results[r]['countPass'] for r in receivers) and exitsOk else 'FAIL'))
    except Exception as e:results['error']=str(e);results['functionalCountResult']='FAIL';raise
    finally:
        for p in processes:
            if p.poll() is None:p.terminate()
        for f in streams:f.close()
        results['elapsedSeconds']=time.monotonic()-started;results['actualCustomerInterop']='NOT_VERIFIED';results['performance']='NOT_VERIFIED';results['visual']='PENDING'
        (out/'result.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
        (out/'no_videodisplay_after.txt').write_text(subprocess.check_output(['tasklist','/FO','CSV'],text=True,encoding='utf-8',errors='replace'),encoding='utf-8')
        print(json.dumps(results,indent=2))
if __name__=='__main__':main()
