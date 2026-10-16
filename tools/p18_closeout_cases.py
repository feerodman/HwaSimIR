"""Final real fault/lifecycle/recording cases; preserves earlier failed attempts."""
import subprocess,sys,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
if not (ROOT/'logs/p18/deployment/candidate4/receipt.json').exists():raise SystemExit('candidate4 must be built and verified first')
cases=[]
for band in ['SWIR','MWIR']:
    for view in ['side','oblique','end','near','far','occluded']:
        cases.append((f'nozzle_{band.lower()}_{view}_v3','p18_single_run.py',f'--band {band} --engine 1 --short-seconds 4 --codec h264 --pixels rgb24 --render-env P5Scene=nozzle --render-env P5View={view} --render-env P5MaterialView=0'))
    for mode in ['core','halo','mask']:
        cases.append((f'diagnostic_{band.lower()}_{mode}_v3','p18_single_run.py',f'--deployment diagnostic_{mode}_v3 --band {band} --engine 1 --short-seconds 4 --codec h264 --pixels rgb24 --render-env P5Scene=nozzle --render-env P5View=side --render-env P5MaterialView=0'))
cases += [
 ('paused_customer_transport_v3','p18_single_run.py','--role precise --width 800 --codec raw --init-h264 1 --pixels gray8 --old-display --short-seconds 20 --pause-receiver-seconds 8'),
 ('customer_absent_graceful_v3','p18_single_run.py','--old-display --absent'),
 ('old_reopen_graceful_v3','p18_single_run.py','--old-display --old-reopen --pixels rgb24'),
 ('plume_swir_old_graceful_v3','p18_single_run.py','--band SWIR --angle 10 --engine 1 --pixels rgb24 --old-display'),
 ('plume_mwir_old_graceful_v3','p18_single_run.py','--band MWIR --angle 10 --engine 1 --pixels rgb24 --old-display'),
 ('dual_perf_off_v3','p18_dual_run.py','--band SWIR --customer-disabled'),
 ('dual_perf_on_v3','p18_dual_run.py','--band SWIR'),
 ('dual_final_mwir_v3','p18_dual_run.py','--band MWIR'),
]
results=[]
for name,script,args in cases:
    if (ROOT/'logs/p18/runs'/name).exists():print('PRESERVED',name,flush=True);continue
    print('START',name,flush=True)
    with (ROOT/'logs/p18'/f'{name}.log').open('w',encoding='utf-8') as log:
        rc=subprocess.run([sys.executable,str(ROOT/'tools'/script),'--name',name,'--deployment','candidate4','--receiver-build','build-p18-customer-receiver-v3']+args.split(),cwd=ROOT,stdout=log,stderr=subprocess.STDOUT).returncode
    results.append(dict(name=name,exitCode=rc));(ROOT/'logs/p18/closeout_matrix_progress.json').write_text(json.dumps(results,indent=2))
    print('FINISH',name,rc,flush=True)
    if rc:raise SystemExit(rc)
