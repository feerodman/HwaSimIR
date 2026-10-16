"""Run offline audits only after the final live tests have finished."""
import hashlib,json,shutil,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'logs/p18'
deadline=time.monotonic()+1200
while not (BASE/'final_environment.json').exists():
    if time.monotonic()>deadline:raise SystemExit('final live tests not complete')
    time.sleep(2)
jobs=[['p18_analyze_dual.py',f'logs/p18/runs/{name}'] for name in ['dual_perf_off_v4','dual_perf_on_v4','dual_final_mwir_v4']]
if (BASE/'performance_comparison.json').exists() and not (BASE/'performance_comparison_v2.json').exists():shutil.copy2(BASE/'performance_comparison.json',BASE/'performance_comparison_v2.json')
jobs += [['p18_performance_compare.py','--off-run','dual_perf_off_v4','--on-run','dual_perf_on_v4'],['p18_analyze_cases.py'],['p18_audit_old_chain.py'],['p18_media_audit.py'],['p18_dual_media.py'],['p18_preservation_check.py']]
for index,(script,*args) in enumerate(jobs):
    print('ANALYZE',script,*args,flush=True)
    with (BASE/f'final_analysis_{index:02}_{script}.log').open('w',encoding='utf-8') as log:
        subprocess.run([sys.executable,str(ROOT/'tools'/script),*args],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
files=[]
for p in sorted((ROOT/'DDS/CustomerTX/Generated').iterdir()):
    if not p.is_file():continue
    other=BASE/'tests/mingw_generated'/p.name
    files.append(dict(name=p.name,same=other.exists() and p.read_bytes()==other.read_bytes(),sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
(BASE/'tests/generated_type_comparison.json').write_text(json.dumps(dict(result='PASS' if all(p['same'] for p in files) else 'FAIL',files=files),indent=2))
with (BASE/'final_diff_check.log').open('w',encoding='utf-8') as log:subprocess.run(['git','diff','--check'],cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,check=True)
print('OFFLINE_ANALYSIS_COMPLETE',flush=True)
