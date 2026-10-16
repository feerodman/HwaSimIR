"""Isolated diagnostic and resolution lifecycle checks after the main matrix."""
import subprocess,sys,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
cases=[
 ('resize_raw320_to640_v2','--role coarse --width 320 --codec raw --pixels gray8 --rounds 2 --second-width 640 --short-seconds 20 --stimulator-build build-p18-stimulator'),
 ('customer_h264_1024_v2','--role precise --width 1024 --codec h264 --pixels gray8 --short-seconds 20'),
 ('customer_gray_encoder_override_v2','--role coarse --width 320 --codec h264 --init-h264 0 --pixels gray8 --short-seconds 20 --customer-only'),
]
for band in ['SWIR','MWIR']:
    cases.append((f'plume_{band.lower()}_engine_off_v2',f'--band {band} --engine 0 --angle 10 --codec h264 --pixels rgb24'))
    for mode in ['core','halo','mask']:
        cases.append((f'diagnostic_{band.lower()}_{mode}_v2',f'--deployment diagnostic_{mode} --band {band} --engine 1 --short-seconds 8 --codec h264 --pixels rgb24 --render-env P5Scene=nozzle --render-env P5View=side --render-env P5MaterialView=0'))
    cases.append((f'nozzle_{band.lower()}_320_v2',f'--role coarse --width 320 --band {band} --engine 1 --short-seconds 8 --codec h264 --pixels rgb24 --render-env P5Scene=nozzle --render-env P5View=side --render-env P5MaterialView=0'))
results=[]
for name,args in cases:
    if (ROOT/'logs/p18/runs'/name).exists():print('PRESERVED',name,flush=True);continue
    print('START',name,flush=True)
    with (ROOT/'logs/p18'/f'{name}.log').open('w',encoding='utf-8') as log:
        rc=subprocess.run([sys.executable,str(ROOT/'tools/p18_single_run.py'),'--name',name,'--receiver-build','build-p18-customer-receiver-v3']+args.split(),cwd=ROOT,stdout=log,stderr=subprocess.STDOUT).returncode
    results.append(dict(name=name,exitCode=rc));(ROOT/'logs/p18/tail_matrix_progress.json').write_text(json.dumps(results,indent=2))
    print('FINISH',name,rc,flush=True)
    if rc:raise SystemExit(rc)
