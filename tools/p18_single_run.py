"""Production sender-to-independent-receiver coverage beyond the dual main case."""
import argparse,json,os,re,shutil,subprocess,time,shlex,psutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--name',required=True);ap.add_argument('--deployment',default='candidate4')
    ap.add_argument('--role',choices=['coarse','precise'],default='precise');ap.add_argument('--width',type=int,default=800)
    ap.add_argument('--codec',choices=['raw','h264'],default='h264');ap.add_argument('--pixels',choices=['gray8','rgb24'],default='gray8')
    ap.add_argument('--band',choices=['SWIR','MWIR'],default='MWIR');ap.add_argument('--customer-only',action='store_true')
    ap.add_argument('--color-card',action='store_true');ap.add_argument('--old-display',action='store_true')
    ap.add_argument('--angle',type=float,default=25);ap.add_argument('--engine',type=int,default=-1);ap.add_argument('--reject',action='store_true')
    ap.add_argument('--weather',type=int,default=0);ap.add_argument('--late-seconds',type=int,default=0)
    ap.add_argument('--input-file',default='DataDrivenTestQT/1.txt');ap.add_argument('--disabled',action='store_true')
    ap.add_argument('--absent',action='store_true');ap.add_argument('--slow-ms',type=int,default=0)
    ap.add_argument('--reconnect',action='store_true');ap.add_argument('--old-reopen',action='store_true')
    ap.add_argument('--short-seconds',type=int,default=0)
    ap.add_argument('--receiver-build',default='build-p18-customer-receiver-v4');ap.add_argument('--callback-block-ms',type=int,default=0)
    ap.add_argument('--pause-receiver-seconds',type=int,default=0)
    ap.add_argument('--hold-dds-samples-seconds',type=int,default=0)
    ap.add_argument('--camera-file',default='');ap.add_argument('--freeze-geometry',action='store_true')
    ap.add_argument('--rounds',type=int,default=1);ap.add_argument('--second-width',type=int,default=0)
    ap.add_argument('--init-h264',type=int,choices=[0,1],default=None)
    ap.add_argument('--stimulator-build',default='build-DataDrivenTestQT-codex-mingw73_64-Release')
    ap.add_argument('--render-env',action='append',default=[]);a=ap.parse_args()
    if not 0<=a.pause_receiver_seconds<=15:raise RuntimeError('receiver pause must be 0..15 seconds')
    if a.hold_dds_samples_seconds and not (15<=a.hold_dds_samples_seconds<=60 and a.absent and a.role=='precise' and a.codec=='raw'):
        raise RuntimeError('DDS hold requires --absent --role precise --codec raw and duration 15..60')
    if any(not re.fullmatch('[A-Za-z][A-Za-z0-9_]*=[A-Za-z0-9_./,:-]+',v) for v in a.render_env):raise RuntimeError('unsafe render env')
    if not re.fullmatch('[a-zA-Z0-9_]+',a.name):raise RuntimeError('unsafe case name')
    out=ROOT/'logs/p18/runs'/a.name;out.mkdir(parents=True,exist_ok=False)
    receipt=json.loads((ROOT/'logs/p18/deployment'/a.deployment/'receipt.json').read_text());remote=receipt['instances'][a.role]['root']
    env=os.environ.copy();env.update(SSH_ASKPASS=str(ROOT/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p18',QT_FORCE_STDERR_LOGGING='1')
    if not env.get('HWASIMIR_SSH_PASSWORD'):raise RuntimeError('missing SSH credential environment')
    opts=['-o','UpdateHostKeys=no','-o','ConnectTimeout=10']
    def ssh(cmd):return subprocess.check_output(['ssh',*opts,'root@192.168.1.116',cmd],env=env,text=True,encoding='utf-8')
    ssh('! pgrep -x HwaSim_IR >/dev/null')
    rxdir=ROOT/a.receiver_build/'release';stimdir=ROOT/a.stimulator_build/'release'
    sensor=1 if a.role=='coarse' else 2;topic='TX/'+(('LD264' if sensor==1 else 'HD264') if a.codec=='h264' else ('LDecode' if sensor==1 else 'HDecode'))
    config=(ROOT/'logs/p18/before/p18_before_swir_25urad_p17v12/DataDrivenTestQT.NetworkConfig.ini').read_text(encoding='utf-8-sig')
    for key,value in dict(sensorID=sensor,trackerSensorBand=0 if a.band=='SWIR' else 2,envSky=a.weather).items():config=re.sub(r'(?m)^'+key+'=.*$',key+'='+str(value),config)
    (out/'sender.ini').write_text(config,encoding='utf-8')
    fields=dict(trackerSensorWidth=a.width,trackerSensorHeight=a.width,h264En=(a.codec=='h264' if a.init_h264 is None else bool(a.init_h264)),saveMP4En=a.old_display)
    (out/'sensor.json').write_text(json.dumps(fields),encoding='utf-8')
    audit=remote+'/logs/'+a.name;ssh(f"mkdir '{audit}'")
    processes=[];streams=[];commands={};results=dict(options=vars(a),topic=topic,actualCustomerInterop='NOT_VERIFIED',performance='NOT_VERIFIED')
    def launch(command,cwd,log,customEnv=env,visible=False):
        f=open(out/(log+'.out.log'),'wb');g=open(out/(log+'.err.log'),'wb');streams.extend([f,g])
        si=subprocess.STARTUPINFO();si.dwFlags|=subprocess.STARTF_USESHOWWINDOW;si.wShowWindow=1 if visible else 0
        p=subprocess.Popen(command,cwd=cwd,env=customEnv,stdout=f,stderr=g,startupinfo=si);processes.append(p);commands[log]=command;return p
    def receiver(restarted=False):
        d=out/('customer_reconnected' if restarted else 'customer');d.mkdir();shutil.copy2(rxdir/'zrddslicence.lic',d/'zrddslicence.lic')
        width=a.second_width if restarted and a.second_width else a.width
        return launch([str(rxdir/'HwaSimIRCustomerTxVideoReceiverDemo.exe'),'--topic',topic,'--codec',a.codec,'--pixel-format',a.pixels,'--width',str(width),'--height',str(width),'--fps','60',
            '--domain','150','--qos',str(rxdir/'ZRDDS_PROTOCOL_QOS.xml'),'--output',str(d),'--idle-ms','10000','--timeout-ms',('40000' if a.reconnect and not restarted else '200000'),'--slow-ms',str(a.slow_ms),'--display','1','--ffmpeg',str(rxdir/'ffmpeg.exe')]+(['--callback-block-ms',str(a.callback_block_ms)] if a.callback_block_ms else []),d,'receiver_reconnected' if restarted else 'receiver',visible=True)
    started=time.monotonic();rx=None;vd=None;paused=None;holder=None
    try:
        if not (a.reject or a.disabled or a.absent) and not a.late_seconds:rx=receiver()
        if a.hold_dds_samples_seconds:
            testdir=ROOT/'build-p18-contract-test/release'
            holder=launch([str(testdir/'customer_tx_contract_test.exe'),str(rxdir/'ZRDDS_PROTOCOL_QOS.xml'),'--hold-raw-reader',str(a.hold_dds_samples_seconds)],testdir,'holding_reader')
        if a.old_display:
            oldH264=(a.codec=='h264') if a.init_h264 is None else bool(a.init_h264)
            if a.role!='precise' or not oldH264 or a.customer_only:raise RuntimeError('old-display fixture requires precise INIT H264 with old DDS enabled')
            vdDir=ROOT/'HwaSim_IR_VideoDisplay/x64/Release';vdenv=env.copy();vdenv['P7RecordingRoot']=str(out/'old_recording');vdenv['P5DdsVideoPath']=str(out/'old_received.h264');vdenv['P5DdsVideoSamples']='4500'
            vdenv['P6ReceiverUiDump']=str(out/'old_ui.png');vdenv['P6ReceiverUiResponsiveCapture']='1'
            vd=launch([str(vdDir/'HwaSim_IR_VideoDisplay.exe'),'--acceptance-exit-ms','40000' if a.old_reopen else '130000'],vdDir,'old_receiver',vdenv,True)
        timeout=48 if a.reject else 100*a.rounds+90
        extra=f"HwaSimIRCustomerVideoMode={a.codec} HwaSimIRCustomerPixelFormat={a.pixels} HwaSimIRCustomerDiagnosticColorCard={'true' if a.color_card else 'false'}"
        if a.customer_only:extra+=' HwaSimIRDdsVideoEnable=false TcpSendVideo=false TcpSendRealtimeData=false TcpSendAnnotation=false TcpForwardInitControl=false'
        if a.disabled:extra+=' HwaSimIRCustomerVideoEnable=false'
        extra+=' '+' '.join(a.render_env)
        command=f"cd '{remote}' && timeout -s TERM -k 10s {timeout}s env HWASIMIR_ROOT='{remote}' HWASIMIR_CHANNEL={a.role} HWASIMIR_NETWORK_CONFIG='{remote}/network.ini' HWASIMIR_RUNTIME_CONFIG='{remote}/Config/HwaSimIRRuntime.ini' ZRDDS_HOME='{remote}/zrdds' RenderPresentationMode=HeadlessOffscreen HwaSimIRExitOnStop={'true' if a.rounds==1 else 'false'} HwaSimIRLocalRecordingEnable=false HwaSimIRCustomerAuditPrefix='{audit}/customer' HwaInputAuditDirectory='{audit}' HwaStageAuditDirectory='{audit}' {extra} ./run_precise.sh"
        # Keep renderer logs on the board so a transient SSH reset cannot leave
        # a closed stdout pipe in the renderer or destroy the final drain evidence.
        wrapped=command+f" >'{audit}/board.out.log' 2>'{audit}/board.err.log'; echo $? >'{audit}/board.exit'"
        board=launch(['ssh',*opts,'root@192.168.1.116','sh -c '+shlex.quote(wrapped)],ROOT,'board_transport');time.sleep(4)
        if board.poll() is not None:raise RuntimeError('renderer exited before INIT')
        stimenv=env.copy();stimenv['HwaInputAuditDirectory']=str(out)
        if a.camera_file:stimenv['WeatherCameraInput']=str((ROOT/a.camera_file).resolve())
        stimArgs=[str(stimdir/'DataDrivenTestQT.exe'),'--network-config='+str(out/'sender.ini'),'--input-file='+str(ROOT/a.input_file),'--channel='+a.role,
            '--plat-id=1001','--sensor-id='+str(sensor),'--duration-sec='+str(a.short_seconds or 110),'--sensor-fields-json='+str(out/'sensor.json'),'--sensor-pixel-angle-urad='+str(a.angle)]
        if a.engine>=0:stimArgs.append('--engine-state='+str(a.engine))
        if a.freeze_geometry:stimArgs.append('--freeze-geometry')
        if a.rounds>1:stimArgs+=['--rounds='+str(a.rounds),'--second-round-width='+str(a.second_width),'--inter-round-ms=16000']
        if a.customer_only:stimArgs.append('--no-video-status-barrier=1')
        sender=launch(stimArgs,stimdir,'sender',stimenv)
        (out/'commands.json').write_text(json.dumps(commands,indent=2),encoding='utf-8')
        if a.pause_receiver_seconds:
            if rx is None:raise RuntimeError('pause requires the receiver started by this harness')
            deadline=time.monotonic()+60
            receiveAudit=out/'customer/receive.jsonl'
            observed=[]
            while time.monotonic()<deadline and rx.poll() is None:
                if receiveAudit.exists():
                    observed=[json.loads(line) for line in receiveAudit.read_text(encoding='utf-8').splitlines() if line.endswith('}')]
                    observed=[row for row in observed if row.get('kind')=='sample']
                    if len(observed)>=60:break
                time.sleep(.1)
            if len(observed)<60:raise RuntimeError('cannot inject connected-receiver stall before 60 proven received samples')
            # Only our own newly launched receiver PID, never an unrelated app.
            paused=psutil.Process(rx.pid);paused.suspend()
            results['receiverPause']={'pid':rx.pid,'beginNs':time.perf_counter_ns(),'requestedSeconds':a.pause_receiver_seconds,
                'receivedSamplesBeforePause':len(observed),'firstReceiveNs':observed[0]['receivedNs'],'lastReceiveNsBeforePause':observed[-1]['receivedNs']}
            time.sleep(a.pause_receiver_seconds)
            paused.resume();paused=None;results['receiverPause']['resumeNs']=time.perf_counter_ns()
        if a.late_seconds:time.sleep(a.late_seconds);rx=receiver()
        if a.reconnect:
            results['initialReceiverExit']=rx.wait(timeout=70);time.sleep(3);rx=receiver(True)
        if a.second_width:
            deadline=time.monotonic()+150
            while not re.search(r'nextRound=\s*2',(out/'sender.err.log').read_text(errors='replace')):
                if sender.poll() is not None or time.monotonic()>deadline:raise RuntimeError('no next-round barrier')
                time.sleep(.25)
            results['initialReceiverExit']=rx.wait(timeout=40);rx=receiver(True)
        if a.old_reopen:
            results['initialOldReceiverExit']=vd.wait(timeout=70);time.sleep(3)
            vdenv['P7RecordingRoot']=str(out/'old_recording_reopened');vdenv['P5DdsVideoPath']=str(out/'old_reopened.h264');vdenv['P6ReceiverUiDump']=str(out/'old_ui_reopened.png')
            vd=launch([str(vdDir/'HwaSim_IR_VideoDisplay.exe'),'--acceptance-exit-ms','110000'],vdDir,'old_receiver_reopened',vdenv,True)
        (out/'commands.json').write_text(json.dumps(commands,indent=2),encoding='utf-8')
        results['senderExit']=sender.wait(timeout=100*a.rounds+55)
        if a.rounds>1:
            ssh("for p in $(pgrep -x HwaSim_IR); do [ \"$(readlink /proc/$p/cwd)\" = '"+remote+"' ] && kill -TERM $p; done; true")
        results['boardTransportExit']=board.wait(timeout=timeout+15)
        results['boardExit']=int(ssh(f"cat '{audit}/board.exit'").strip())
        if rx:results['receiverExit']=rx.wait(timeout=45)
        if holder:results['holdingReaderExit']=holder.wait(timeout=70)
        # Let the existing GUI acceptance timer close the recorder normally.
        # TerminateProcess skips MP4 trailer finalization; never count such a
        # harness interruption as a successful old recording.
        if vd:results['oldReceiverExit']=vd.wait(timeout=190)
        subprocess.run(['scp',*opts,'-r',f'root@192.168.1.116:{audit}',str(out/'sender_audit')],env=env,check=True)
        for filename in ['board.out.log','board.err.log']:shutil.copy2(out/'sender_audit'/filename,out/filename)
        if a.reject:
            text=(out/'board.out.log').read_text(errors='replace')+(out/'board.err.log').read_text(errors='replace');results['expectedRejection']='raw_capacity_exceeded' in text and '[CustomerTXRound]' not in text
        elif not (a.absent or a.disabled):
            d=out/('customer_reconnected' if a.reconnect or a.second_width else 'customer')
            results['receiver']=json.loads((d/'summary.json').read_text())
            probe=subprocess.check_output([str(ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffprobe.exe'),'-v','error','-count_frames','-select_streams','v:0','-show_entries','stream=width,height,nb_read_frames','-of','json',str(d/'output.mp4')],text=True)
            (d/'ffprobe.json').write_text(probe,encoding='utf-8');results['savedFrames']=int(json.loads(probe)['streams'][0]['nb_read_frames'])
        results['executionCompleted']=True
    except Exception as e:results['error']=str(e);raise
    finally:
        if paused is not None:
            try:paused.resume()
            except psutil.NoSuchProcess:pass
        for p in processes:
            if p.poll() is None:p.terminate()
        for f in streams:f.close()
        results['elapsedSeconds']=time.monotonic()-started
        (out/'result.json').write_text(json.dumps(results,indent=2),encoding='utf-8');print(json.dumps(results,indent=2))
if __name__=='__main__':main()
