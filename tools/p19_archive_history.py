"""Archive an inventoried exact deployment whitelist off-board. No board cleanup."""
import base64, hashlib, json, os, shlex, subprocess, tarfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
out=root/'logs/p19/board/history_offboard';out.mkdir(parents=True,exist_ok=False)
env=os.environ.copy();env.update(SSH_ASKPASS=str(root/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p19')
ssh=['ssh','-o','UpdateHostKeys=no','-o','ConnectTimeout=10','root@192.168.1.116']
script=r'''
import pathlib,hashlib,json,os,re
root=pathlib.Path('/userdata/HwaSimIR')
names=[]
for p in sorted(root.iterdir()):
 n=p.name
 if n in ('Config','Config_new','HwaSim_IR','HwaSim_IR_new','p18','run_precise.sh','rk3588_hwasimir_performance_mode.sh') or n.startswith(('Config.before_','HwaSim_IR.before_')) or re.fullmatch(r'p16_[a-z0-9_]+_rollback_[0-9-]+',n):names.append(n)
cache={};entries=[]
def record(p):
 s=p.lstat();rel=str(p.relative_to(root))
 if p.is_symlink():entries.append(dict(path=rel,kind='symlink',target=os.readlink(p)));return
 if p.is_file():
  key=(s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns)
  if key not in cache:
   h=hashlib.sha256()
   with p.open('rb') as f:
    for b in iter(lambda:f.read(1048576),b''):h.update(b)
   cache[key]=h.hexdigest()
  entries.append(dict(path=rel,kind='file',bytes=s.st_size,sha256=cache[key]))
for n in names:
 p=root/n
 assert p.parent==root and not p.is_symlink(),str(p)
 record(p)
 if p.is_dir():
  for base,dirs,files in os.walk(p,followlinks=False):
   for n in files:record(pathlib.Path(base)/n)
   for n in dirs:
    d=pathlib.Path(base)/n
    if d.is_symlink():record(d)
print(json.dumps(dict(root=str(root),names=names,entries=entries,uniqueFileContents=len(cache))))
'''
(out/'inventory_remote.py').write_text(script,encoding='utf-8')
command='python3 -c '+shlex.quote("import base64;exec(base64.b64decode('"+base64.b64encode(script.encode()).decode()+"'))")
inventory=json.loads(subprocess.check_output(ssh+[command],env=env))
(out/'inventory.json').write_text(json.dumps(inventory,indent=2))
archive=out/'historical_deployments.tar.gz'
with archive.open('xb') as f:
 subprocess.run(ssh+['cd /userdata/HwaSimIR && tar -czf - -- '+ ' '.join(shlex.quote(n) for n in inventory['names'])],env=env,stdout=f,check=True)
restore=out/'restore_check';restore.mkdir()
seen={};pending=[];links=[]
with tarfile.open(archive,'r:gz') as tf:
 for member in tf:
  dest=(restore/member.name).resolve()
  if not dest.is_relative_to(restore.resolve()):raise RuntimeError('unsafe archive path')
  if member.isfile():
   dest.parent.mkdir(parents=True,exist_ok=True);h=hashlib.sha256()
   with tf.extractfile(member) as src,dest.open('xb') as dst:
    for b in iter(lambda:src.read(1048576),b''):dst.write(b);h.update(b)
   seen[member.name]=h.hexdigest()
  elif member.islnk():pending.append((member.name,member.linkname))
  elif member.issym():links.append(dict(path=member.name,target=member.linkname))
while pending:
 remaining=[]
 for name,target in pending:
  if target not in seen:remaining.append((name,target));continue
  src=(restore/target).resolve();dst=(restore/name).resolve()
  if not src.is_relative_to(restore.resolve()) or not dst.is_relative_to(restore.resolve()):raise RuntimeError('unsafe hardlink')
  dst.parent.mkdir(parents=True,exist_ok=True);os.link(src,dst);seen[name]=seen[target]
 if len(remaining)==len(pending):raise RuntimeError('unresolved archive hardlinks')
 pending=remaining
bad=[e['path'] for e in inventory['entries'] if e['kind']=='file' and seen.get(e['path'])!=e['sha256']]
h=hashlib.sha256()
with archive.open('rb') as f:
 for b in iter(lambda:f.read(1048576),b''):h.update(b)
result=dict(status='PASS' if not bad else 'FAIL',archive=str(archive.relative_to(root)),sha256=h.hexdigest(),restoredFiles=len(seen),mismatches=bad,symlinks=links,boardMutation=False)
(out/'verification.json').write_text(json.dumps(result,indent=2));print(json.dumps({k:v for k,v in result.items() if k!='symlinks'},indent=2),flush=True)
if bad:raise SystemExit(2)
