"""Promote the verified P18 rollback directory and create stable launch entries.
No historical deployment is deleted by this step.
"""
import base64,json,os,shlex,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'logs/p19/board/release_setup';out.mkdir(parents=True,exist_ok=False)
verification=json.loads((root/'logs/p19/board/history_offboard/verification.json').read_text());assert verification['status']=='PASS'
env=os.environ.copy();env.update(SSH_ASKPASS=str(root/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p19')
ssh=['ssh','-o','UpdateHostKeys=no','-o','ConnectTimeout=10','root@192.168.1.116']
subprocess.run(ssh+['! pgrep -x HwaSim_IR >/dev/null'],env=env,check=True)
script=r'''
import pathlib,os,json,hashlib,re
root=pathlib.Path('/userdata/HwaSimIR');old=root/'p18/candidate4';dest=root/'releases/P18'
assert old.resolve()==old and dest.parent.resolve()==root/'releases' and not dest.exists()
assert hashlib.sha256((old/'HwaSim_IR').read_bytes()).hexdigest()=='f7dddbdb4e19f4e3676f9e288255c59ee86cf41ae0817a3aa724bc09e044e40d'
os.rename(old,dest)
for version in ('P19','P18'):
 text='#!/bin/sh\nset -eu\nrole="${1:-precise}"\ncase "$role" in coarse|precise) ;; *) echo "usage: $0 coarse|precise" >&2; exit 2;; esac\nif [ "$#" -gt 0 ]; then shift; fi\nbase=/userdata/HwaSimIR/releases/'+version+'/$role\ncd "$base"\nexec env HWASIMIR_ROOT="$base" HWASIMIR_CHANNEL="$role" HWASIMIR_NETWORK_CONFIG="$base/network.ini" HWASIMIR_RUNTIME_CONFIG="$base/Config/HwaSimIRRuntime.ini" ZRDDS_HOME="$base/zrdds" ./run_precise.sh "$@"\n'
 p=root/('start_'+version.lower()+'.sh');assert not p.exists();p.write_text(text);p.chmod(0o755)
instances={}
for role in ('coarse','precise'):
 p=dest/role
 network=(p/'network.ini').read_text()
 domain=int(re.search(r'(?m)^DomainId\s*=\s*(\d+)',network).group(1))
 instances[role]=dict(root=str(p),elfSha256=hashlib.sha256((p/'HwaSim_IR').read_bytes()).hexdigest(),domain=domain)
print(json.dumps(dict(instances=instances,previous='P18',latest='P19',cleanupPerformed=False)))
'''
(out/'prepare_remote.py').write_text(script,encoding='utf-8')
command='python3 -c '+shlex.quote("import base64;exec(base64.b64decode('"+base64.b64encode(script.encode()).decode()+"'))")
receipt=json.loads(subprocess.check_output(ssh+[command],env=env));(out/'receipt.json').write_text(json.dumps(receipt,indent=2))
(root/'logs/p19/deployment/previous.json').write_text(json.dumps(receipt,indent=2))
# Original P18 harness reads this receipt; no old evidence is overwritten.
target=root/'logs/p18/deployment/p19_rollback';target.mkdir(exist_ok=False)
(target/'receipt.json').write_text(json.dumps(receipt,indent=2));print(json.dumps(receipt,indent=2))
