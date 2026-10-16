"""Remaining P18 hardware cases; preserves all attempts and stops on harness errors."""
import subprocess,sys,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
cases=[
 ('dual_perf_on_v2_retry','p18_dual_run.py','--deployment candidate3 --band SWIR'),
 ('dual_perf_off_v2','p18_dual_run.py','--deployment candidate3 --band SWIR --customer-disabled'),
 ('dual_two_rounds_v2','p18_dual_run.py','--deployment candidate3 --band SWIR --rounds 2'),
 ('late_idr_recovery_v2','p18_single_run.py','--role precise --width 800 --codec h264 --pixels gray8 --late-seconds 40 --receiver-build build-p18-customer-receiver-v3'),
 ('reconnect_idr_recovery_v2','p18_single_run.py','--role precise --width 800 --codec h264 --pixels gray8 --reconnect --receiver-build build-p18-customer-receiver-v3'),
 ('controlled_callback_timeout_v2','p18_single_run.py','--role coarse --width 320 --codec raw --pixels gray8 --short-seconds 20 --callback-block-ms 1000 --receiver-build build-p18-customer-receiver-v3'),
]
for band in ['SWIR','MWIR']:
    for weather in [0,2,3]:cases.append((f'retain_{band.lower()}_weather{weather}_v2','p18_single_run.py',f'--band {band} --weather {weather} --engine 0 --input-file DataDrivenTestQT/p14_ground_truck_weather_30s.txt --codec h264 --pixels rgb24 --receiver-build build-p18-customer-receiver-v3'))
    for weather in [1,5]:cases.append((f'retain_{band.lower()}_cloud{weather}_v2','p18_single_run.py',f'--band {band} --weather {weather} --engine 1 --camera-file logs/p17/cloud_rework/fixtures_v2/cloud_above.json --freeze-geometry --short-seconds 31 --angle 200 --codec h264 --pixels rgb24 --receiver-build build-p18-customer-receiver-v3'))
    for view in ['side','oblique','end','near','far','occluded']:
        cases.append((f'nozzle_{band.lower()}_{view}_v2','p18_single_run.py',f'--band {band} --engine 1 --short-seconds 8 --codec h264 --pixels rgb24 --render-env P5Scene=nozzle --render-env P5View={view} --render-env P5MaterialView=0 --receiver-build build-p18-customer-receiver-v3'))
results=[]
for name,script,args in cases:
    if (ROOT/'logs/p18/runs'/name).exists():print('PRESERVED',name,flush=True);continue
    print('START',name,flush=True)
    with (ROOT/'logs/p18'/f'{name}.log').open('w',encoding='utf-8') as log:
        rc=subprocess.run([sys.executable,str(ROOT/'tools'/script),'--name',name]+args.split(),cwd=ROOT,stdout=log,stderr=subprocess.STDOUT).returncode
    results.append(dict(name=name,exitCode=rc));(ROOT/'logs/p18/final_matrix_progress.json').write_text(json.dumps(results,indent=2))
    print('FINISH',name,rc,flush=True)
    if rc:raise SystemExit(rc)
