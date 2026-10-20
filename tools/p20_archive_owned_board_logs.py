"""用户明确授权后运行：逐文件验证本机归档，再清理板端已完成 P20 大日志。
不递归删除目录，不处理 releases、Config、程序或正在运行的 renderer。
"""
import base64,hashlib,json,os,shlex,subprocess,datetime
from pathlib import Path
root=Path(__file__).resolve().parents[1];entries=[]
for result in (root/'logs/p20/runs').glob('*/result.json'):
    data=json.loads(result.read_text())
    if not data.get('systemPolicyRestored') or data.get('options',{}).get('tmp_board_logs'):continue
    rp=data.get('options',{}).get('receipt','')
    if not rp.startswith('logs/p20/deployment/'):continue
    receipt=json.loads((root/rp).read_text())
    for role in ['coarse','precise']:
        remote=receipt['instances'][role]['root']+'/logs/'+result.parent.name
        for local in (result.parent/role/'board').glob('*'):
            if local.is_file() and local.stat().st_size>1_000_000 and local.suffix in ['.log','.csv','.jsonl','.h264']:
                entries.append(dict(remote=remote+'/'+local.name,local=str(local),sha256=hashlib.sha256(local.read_bytes()).hexdigest(),bytes=local.stat().st_size))
script='''import pathlib,hashlib,json,subprocess
assert subprocess.run(['pgrep','-x','HwaSim_IR'],stdout=subprocess.DEVNULL).returncode==1
entries=ENTRIES
for e in entries:
 p=pathlib.Path(e['remote'])
 assert str(p).startswith('/userdata/HwaSimIR/candidates/P20_') and '/logs/' in str(p)
 assert p.resolve()==p and not p.is_symlink()
 if not p.exists():e['result']='already_absent';continue
 if not p.is_file() or p.stat().st_size!=e['bytes'] or hashlib.sha256(p.read_bytes()).hexdigest()!=e['sha256']:
  e['result']='mismatch_preserved';continue
 p.unlink();e['result']='verified_local_copy_then_unlinked_file'
print(json.dumps(entries))
'''.replace('ENTRIES',repr(entries))
env=os.environ.copy();env.update(SSH_ASKPASS=str(root/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p20')
response=subprocess.check_output(['ssh','-o','UpdateHostKeys=no','root@192.168.1.116','python3 -'],input=script,env=env,text=True)
out=root/'logs/p20/board_log_archive';out.mkdir(exist_ok=True)
(out/(datetime.datetime.now().strftime('%Y%m%d_%H%M%S')+'.json')).write_text(response)
records=json.loads(response)
print('verified-and-cleaned bytes',sum(e['bytes'] for e in records if e['result']=='verified_local_copy_then_unlinked_file'),'mismatches preserved',sum(e['result']=='mismatch_preserved' for e in records))
