"""Retest the receiver timing-gap fix and hash optimization against real ZRDDS."""
import hashlib,json,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
deadline=time.monotonic()+600
while not (ROOT/'logs/p18/runs/dual_final_mwir_v3/result.json').exists():
    if time.monotonic()>deadline:raise SystemExit('previous board matrix has not completed')
    time.sleep(2)
if not json.loads((ROOT/'logs/p18/runs/dual_final_mwir_v3/result.json').read_text()).get('functionalCountResult')=='PASS':
    raise SystemExit('inspect previous board run before continuing')
testdir=ROOT/'build-p18-contract-test/release'
rx=ROOT/'build-p18-customer-receiver-v4/release'
contractLog=ROOT/'logs/p18/contract_test_qos_final.log'
if not contractLog.exists():
    with contractLog.open('w',encoding='utf-8') as log:
        subprocess.run([str(testdir/'customer_tx_contract_test.exe'),str(rx/'ZRDDS_PROTOCOL_QOS.xml')],cwd=testdir,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=60)
if '"unitAndTypedRoundtrip":"PASS"' not in contractLog.read_text():raise SystemExit('contract test did not pass')
for topic in ['HD264','LD264']:
    print('START gap fixture',topic,flush=True)
    folder=ROOT/f'logs/p18/tests/incremental_{topic}_v4_gap'
    if not folder.exists():
        with (ROOT/f'logs/p18/incremental_{topic}_v4_gap.log').open('w',encoding='utf-8') as log:
            subprocess.run(['powershell','-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT/'tools/p18_incremental_receiver_test.ps1'),'-Topic',topic,'-ReceiverBuild','build-p18-customer-receiver-v4','-Suffix','_v4_gap','-PauseMs','3000'],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True,timeout=60)
    summary=json.loads((folder/'summary.json').read_text())
    samples=[json.loads(line) for line in (folder/'receive.jsonl').read_text().splitlines() if json.loads(line)['kind']=='sample']
    gap=max((b['receivedNs']-a['receivedNs'])/1e6 for a,b in zip(samples,samples[1:]))
    sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    probe=json.loads(subprocess.check_output([str(rx/'ffmpeg.exe').replace('ffmpeg.exe','ffprobe.exe') if (rx/'ffprobe.exe').exists() else str(ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffprobe.exe'),'-v','error','-count_frames','-show_entries','stream=nb_read_frames','-of','json',str(folder/'output.mp4')],text=True))
    ok=summary['decodedFrames']==120 and not summary['failed'] and summary['decodeErrors']==0 and gap>=2900 and sha(folder/'received.h264')==sha(ROOT/'logs/p18/tests/diagnostic_color_120.h264') and int(probe['streams'][0]['nb_read_frames'])==120
    (folder/'verification.json').write_text(json.dumps(dict(result='PASS' if ok else 'FAIL',maximumInterSampleGapMs=gap,decoded=summary['decodedFrames'],saved=int(probe['streams'][0]['nb_read_frames']),streamSha256=sha(folder/'received.h264')),indent=2))
    if not ok:raise SystemExit('gap fixture verification failed '+topic)
cases=[
 ('paused_connected_customer_v4','p18_single_run.py','--role precise --width 320 --codec raw --init-h264 1 --pixels gray8 --old-display --short-seconds 20 --pause-receiver-seconds 8'),
 ('old_reopen_graceful_v4','p18_single_run.py','--old-display --old-reopen --pixels rgb24'),
 ('raw800_receiver_v4','p18_single_run.py','--role precise --width 800 --codec raw --pixels gray8 --short-seconds 20'),
 ('ld264_rgb_receiver_v4','p18_single_run.py','--role coarse --width 320 --codec h264 --pixels rgb24 --color-card --short-seconds 20'),
 ('late_idr_recovery_v4','p18_single_run.py','--late-seconds 22'),
 ('reconnect_idr_recovery_v4','p18_single_run.py','--reconnect'),
 ('dual_perf_off_v4','p18_dual_run.py','--band SWIR --customer-disabled'),
 ('dual_perf_on_v4','p18_dual_run.py','--band SWIR'),
 ('dual_final_mwir_v4','p18_dual_run.py','--band MWIR'),
]
results=[]
for name,script,args in cases:
    if (ROOT/'logs/p18/runs'/name).exists():raise SystemExit('preserve existing evidence; inspect '+name)
    print('START',name,flush=True)
    with (ROOT/'logs/p18'/f'{name}.log').open('w',encoding='utf-8') as log:
        rc=subprocess.run([sys.executable,str(ROOT/'tools'/script),'--name',name,'--deployment','candidate4','--receiver-build','build-p18-customer-receiver-v4']+args.split(),cwd=ROOT,stdout=log,stderr=subprocess.STDOUT).returncode
    results.append(dict(name=name,exitCode=rc));(ROOT/'logs/p18/receiver_v4_matrix_progress.json').write_text(json.dumps(results,indent=2))
    print('FINISH',name,rc,flush=True)
    if rc:raise SystemExit(rc)
