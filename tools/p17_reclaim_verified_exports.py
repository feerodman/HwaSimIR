"""Reclaim only old board v8 exports with verified retained local originals."""
import hashlib,json,os,shlex,subprocess
from pathlib import Path

root=Path(__file__).resolve().parents[1]
items=[]
for directory in (root/'logs/p17/ir_retest/after').glob('*_ir_v8'):
    for path in (directory/'diagnostic_linear').glob('*'):
        if path.is_file() and path.suffix in ('.pfm','.png'):
            items.append(dict(name=path.name,sha256=hashlib.sha256(path.read_bytes()).hexdigest(),local=str(path)))
assert items
code='''import pathlib,hashlib,json
items=ITEMS
root=pathlib.Path('/userdata/HwaSimIR/logs').resolve()
verified=[]
for item in items:
    assert '/' not in item['name'] and '\\\\' not in item['name']
    path=(root/item['name']).resolve()
    assert path.parent==root
    if not path.exists():continue
    assert path.is_file()
    assert hashlib.sha256(path.read_bytes()).hexdigest()==item['sha256'],str(path)
    item['bytes']=path.stat().st_size
    verified.append(item)
for item in verified:(root/item['name']).unlink()
print(json.dumps(dict(result='PASS_VERIFIED_LOCAL_COPIES_RETAINED',files=verified,bytesReclaimed=sum(i['bytes'] for i in verified))))
'''.replace('ITEMS',repr(items))
env=os.environ.copy();env.update(SSH_ASKPASS=str(root/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p17')
assert env.get('HWASIMIR_SSH_PASSWORD')
result=subprocess.check_output(['ssh','-o','ConnectTimeout=10','root@192.168.1.116','python3 -c '+shlex.quote(code)],env=env,text=True,encoding='utf-8')
receipt=json.loads(result)
(root/'logs/p17/cloud_rework/reclaimed_exports.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print(json.dumps(dict(result=receipt['result'],files=len(receipt['files']),bytesReclaimed=receipt['bytesReclaimed'])))
