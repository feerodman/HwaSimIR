"""Release only P18 diagnostic exports already copied and hash-verified locally."""
import hashlib,json,os,re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
env=os.environ.copy();env.update(SSH_ASKPASS=str(ROOT/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p18')
if not env.get('HWASIMIR_SSH_PASSWORD'):raise RuntimeError('SSH credential environment missing')
rows=[]
for p in (ROOT/'logs/p18').glob('before/*/diagnostic_linear/*'):
    if not p.is_file() or not re.fullmatch(r'p17_p18_before_[A-Za-z0-9_]+_linear_seq[0-9]+(?:_rgb8\.png|\.pfm)',p.name):continue
    digest=hashlib.sha256(p.read_bytes()).hexdigest();remote='/userdata/HwaSimIR/logs/'+p.name
    command=f"if test -f '{remote}'; then test $(sha256sum '{remote}' | cut -d' ' -f1) = {digest} && rm -- '{remote}' && echo released; else echo already_absent; fi"
    result=subprocess.check_output(['ssh','-o','UpdateHostKeys=no','-o','ConnectTimeout=10','root@192.168.1.116',command],env=env,text=True).strip()
    rows.append(dict(local=str(p),remote=remote,sha256=digest,result=result))
out=ROOT/'logs/p18/diagnostic_release.jsonl'
with out.open('a',encoding='utf-8') as f:
    for row in rows:f.write(json.dumps(row)+'\n')
print('verified exported files',len(rows))
