"""Sequential real-board cases. Credentials are inherited, never written here."""
import subprocess,sys,json,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
cases=[
 ('precise_h264_rgb_shared_v2','--role precise --width 800 --codec h264 --pixels rgb24 --band SWIR --old-display'),
 ('coarse_h264_rgb_colorcard_v2','--role coarse --width 320 --codec h264 --pixels rgb24 --band SWIR --color-card'),
 ('reject_raw800_rgb_v2','--role precise --width 800 --codec raw --pixels rgb24 --reject'),
 ('reject_raw1024_gray_v2','--role precise --width 1024 --codec raw --pixels gray8 --reject'),
 ('old1024_customer_disabled_v2','--role precise --width 1024 --codec h264 --pixels gray8 --disabled --old-display'),
 ('customer_late_v2','--role precise --width 800 --codec h264 --pixels gray8 --late-seconds 40'),
 ('customer_reconnect_v2','--role precise --width 800 --codec h264 --pixels gray8 --reconnect'),
 ('customer_absent_v2','--role precise --width 800 --codec h264 --pixels gray8 --absent --old-display'),
 ('customer_slow_v2','--role coarse --width 320 --codec raw --pixels gray8 --slow-ms 10'),
 ('customer_overload_v2','--role precise --width 800 --codec raw --pixels gray8 --slow-ms 50'),
 ('old_reopen_customer_continuous_v2','--role precise --width 800 --codec h264 --pixels rgb24 --old-display --old-reopen'),
 ('plume_swir_10_after_v2','--role precise --width 800 --codec h264 --pixels rgb24 --band SWIR --engine 1 --angle 10 --old-display'),
 ('plume_mwir_10_after_v2','--role precise --width 800 --codec h264 --pixels rgb24 --band MWIR --engine 1 --angle 10 --old-display'),
]
results=[]
for name,args in cases:
    out=ROOT/'logs/p18/runs'/name
    if out.exists():print('PRESERVED existing',name,flush=True);continue
    command=[sys.executable,str(ROOT/'tools/p18_single_run.py'),'--name',name]+args.split()
    print('START',name,flush=True);start=time.monotonic()
    with (ROOT/'logs/p18'/f'{name}.log').open('w',encoding='utf-8') as log:
        rc=subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT).returncode
    results.append(dict(name=name,exitCode=rc,elapsed=time.monotonic()-start))
    (ROOT/'logs/p18/extended_cases_progress.json').write_text(json.dumps(results,indent=2))
    print('FINISH',name,rc,flush=True)
    if rc:print('Inspect failed evidence before continuing; stopping batch.',flush=True);break
