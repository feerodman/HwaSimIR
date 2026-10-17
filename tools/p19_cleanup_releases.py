"""Delete only archived, revalidated historical deployment roots; preserve releases."""
import base64,gzip,hashlib,json,os,shlex,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];out=root/'logs/p19/board/cleanup';out.mkdir(parents=True,exist_ok=False)
archive=root/'logs/p19/board/history_offboard'
v=json.loads((archive/'verification.json').read_text());assert v['status']=='PASS'
assert hashlib.file_digest((archive/'historical_deployments.tar.gz').open('rb'),'sha256').hexdigest()==v['sha256']
for role in ('coarse','precise'):
 r=json.loads((root/f'logs/p18/runs/p19_rollback_{role}_v2/result.json').read_text())
 assert r['executionCompleted'] and r['savedFrames']>0
env=os.environ.copy();env.update(SSH_ASKPASS=str(root/'tools/p5_ssh_askpass.cmd'),SSH_ASKPASS_REQUIRE='force',DISPLAY='p19')
ssh=['ssh','-o','UpdateHostKeys=no','-o','ConnectTimeout=10','root@192.168.1.116']
subprocess.run(ssh+['! pgrep -x HwaSim_IR >/dev/null'],env=env,check=True)
packed=out/'inventory.json.gz';packed.write_bytes(gzip.compress((archive/'inventory.json').read_bytes()))
remoteInventory='/userdata/HwaSimIR/releases/P19/p19_cleanup_inventory.json.gz'
subprocess.run(['scp','-o','UpdateHostKeys=no',str(packed),'root@192.168.1.116:'+remoteInventory],env=env,check=True)
script=r"""
import pathlib,hashlib,json,os,shutil,gzip,subprocess
root=pathlib.Path('/userdata/HwaSimIR');assert root.resolve()==root
inventory=json.loads(gzip.decompress((root/'releases/P19/p19_cleanup_inventory.json.gz').read_bytes()))
keep={'run_precise.sh','rk3588_hwasimir_performance_mode.sh'}
names=[n for n in inventory['names'] if n not in keep]
assert not subprocess.run(['pgrep','-x','HwaSim_IR'],stdout=subprocess.DEVNULL).returncode==0
assert set(p.name for p in (root/'releases').iterdir())=={'P18','P19'}
targets=[]
for n in names:
 p=root/n
 assert p.parent==root and p.resolve()==p and not p.is_symlink() and n not in ('releases','logs','DDS','zrddslicence.lic')
 assert p.exists(),str(p)
 targets.append(p)
# Refuse any unarchived/new file in a deletion subtree, including historical logs.
expected={e['path']:e for e in inventory['entries'] if e['path'].split('/')[0] in names}
actual={};cache={}
for p in targets:
 paths=[p] if p.is_file() else list(p.rglob('*'))
 for f in paths:
  if not f.is_file() and not f.is_symlink():continue
  rel=str(f.relative_to(root));e=expected.get(rel);assert e is not None,'unarchived file '+rel
  if f.is_symlink():assert e['kind']=='symlink' and os.readlink(f)==e['target'];continue
  s=f.stat();key=(s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns)
  if key not in cache:
   h=hashlib.sha256()
   with f.open('rb') as stream:
    for b in iter(lambda:stream.read(1048576),b''):h.update(b)
   cache[key]=h.hexdigest()
  assert cache[key]==e['sha256'],'changed file '+rel
  actual[rel]=cache[key]
# Never remove a directory used by a process as cwd/executable/open file.
refs=[]
for proc in pathlib.Path('/proc').iterdir():
 if not proc.name.isdigit():continue
 for link in [proc/'cwd',proc/'exe']+list((proc/'fd').glob('*')):
  try:dest=link.resolve(strict=True)
  except (OSError,RuntimeError):continue
  if any(dest==p or p in dest.parents for p in targets):refs.append(str(link)+' -> '+str(dest))
assert not refs,refs
before=shutil.disk_usage(root)._asdict();deleted=[]
for p in targets:
 # Exact path validated above; no glob deletion and no symlink traversal.
 if p.is_dir():shutil.rmtree(p)
 else:p.unlink()
 deleted.append(str(p))
for name,target in [('current','releases/P19'),('previous','releases/P18'),('Config','current/precise/Config'),('HwaSim_IR','current/precise/HwaSim_IR')]:
 p=root/name;assert not p.exists() and not p.is_symlink();p.symlink_to(target)
switch=root/'use_release.sh'
switch.write_text('''#!/bin/sh
set -eu
case "${1:-}" in P19|P18) version="$1";; *) echo "usage: $0 P19|P18" >&2; exit 2;; esac
if pgrep -x HwaSim_IR >/dev/null; then echo "STOP and exit both renderers before switching" >&2; exit 3; fi
cd /userdata/HwaSimIR
test -d "releases/$version/coarse/Config" && test -x "releases/$version/precise/HwaSim_IR"
ln -s "releases/$version" current.next
mv -Tf current.next current
echo "current=$version"
''');switch.chmod(0o755)
current=root/'start_current.sh'
current.write_text('''#!/bin/sh
set -eu
cd /userdata/HwaSimIR
case "$(readlink current)" in releases/P19) exec ./start_p19.sh "$@";; releases/P18) exec ./start_p18.sh "$@";; *) exit 2;; esac
''');current.chmod(0o755)
legacy=root/'run_precise.sh';temp=root/'run_precise.sh.p19tmp'
temp.write_text('#!/bin/sh\nexec /userdata/HwaSimIR/start_current.sh precise "$@"\n');temp.chmod(0o755);os.replace(temp,legacy)
after=shutil.disk_usage(root)._asdict()
print(json.dumps(dict(status='PASS',deleted=deleted,verifiedFiles=len(actual),uniqueHashedFiles=len(cache),before=before,after=after,freedBytes=after['free']-before['free'],releases=['P19','P18'],afterRoot=[dict(name=p.name,symlink=os.readlink(p) if p.is_symlink() else None) for p in sorted(root.iterdir())])))
"""
(out/'cleanup_remote.py').write_text(script,encoding='utf-8')
command='python3 -c '+shlex.quote("import base64;exec(base64.b64decode('"+base64.b64encode(script.encode()).decode()+"'))")
report=json.loads(subprocess.check_output(ssh+[command],env=env));(out/'result.json').write_text(json.dumps(report,indent=2));print(json.dumps({k:v for k,v in report.items() if k not in ('afterRoot','deleted')},indent=2))
