"""Free board storage only after byte-identical completed P18 audit copies exist."""
import argparse,hashlib,json,os,subprocess,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
env=os.environ.copy();env.update(SSH_ASKPASS=str(ROOT/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p18')
if not env.get('HWASIMIR_SSH_PASSWORD'):raise RuntimeError('credential environment required')
ap=argparse.ArgumentParser();ap.add_argument('--deployment',choices=['candidate3','candidate4'],default='candidate3');args=ap.parse_args()
receipt=json.loads((ROOT/'logs/p18/deployment'/args.deployment/'receipt.json').read_text());released=[];pending=[]
ledger=ROOT/'logs/p18/audit_release.jsonl'
already={(r['remote'],r['sha256']) for r in (json.loads(s) for s in ledger.read_text().splitlines())} if ledger.exists() else set()
for case in (ROOT/'logs/p18/runs').iterdir():
    result=case/'result.json'
    if not result.exists():continue
    r=json.loads(result.read_text());o=r.get('options',{})
    if o.get('deployment')!=args.deployment:continue
    audits=[]
    if r.get('boardExit') in [0,124] and (case/'sender_audit').exists():audits.append((o['role'],case/'sender_audit'))
    if r.get('functionalCountResult')=='PASS':audits += [(role,case/role/'sender_audit') for role in ['coarse','precise'] if (case/role/'sender_audit').exists()]
    for role,audit in audits:
        remote=receipt['instances'][role]['root']+'/logs/'+case.name
        if not re.fullmatch('/userdata/HwaSimIR/p18/'+args.deployment+'/(coarse|precise)/logs/[a-zA-Z0-9_]+',remote):raise RuntimeError('path scope')
        for file in audit.iterdir():
            if not file.is_file() or not re.fullmatch('[a-zA-Z0-9_.-]+',file.name):continue
            digest=hashlib.sha256(file.read_bytes()).hexdigest();target=remote+'/'+file.name
            if (target,digest) in already:continue
            pending.append(dict(remote=target,local=str(file),sha256=digest,bytes=file.stat().st_size))
# Exact files only, verified against complete local copies. One remote POSIX
# shell compares each SHA before removal; no recursive deletion or path glob.
commands=[]
for index,row in enumerate(pending):
    target=row['remote'];digest=row['sha256']
    commands.append(f"if [ -f '{target}' ] && [ \"$(sha256sum '{target}'|cut -d' ' -f1)\" = '{digest}' ]; then rm -- '{target}' && printf '%s\\n' '{index}'; fi")
if commands:
    # Binary stdin preserves LF for the remote shell on Windows.
    completed=subprocess.run(['ssh','-o','ConnectTimeout=10','root@192.168.1.116','sh -s'],input=('\n'.join(commands)+'\n').encode('utf-8'),env=env,capture_output=True)
    output=completed.stdout.decode('utf-8')
    released=[pending[int(line)] for line in output.splitlines() if line.strip().isdigit()]
with (ROOT/'logs/p18/audit_release.jsonl').open('a') as f:
    for row in released:f.write(json.dumps(row)+'\n')
print('verified copied files released',len(released),'bytes',sum(r['bytes'] for r in released))
if commands and completed.returncode:raise RuntimeError(completed.stderr.decode('utf-8',errors='replace'))
