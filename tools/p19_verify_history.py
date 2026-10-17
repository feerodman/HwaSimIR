"""Verify every archived byte/link and materialize the complete P18 rollback pair.
Old snapshot hardlinks need not be recreated 400000 times to verify the archive.
"""
import hashlib,json,os,tarfile
from pathlib import Path,PurePosixPath
root=Path(__file__).resolve().parents[1];out=root/'logs/p19/board/history_offboard'
inventory=json.loads((out/'inventory.json').read_text());archive=out/'historical_deployments.tar.gz'
digests={};source={};links=[];restore=out/'restore_complete_P18';restore.mkdir(exist_ok=False)
def safe(name):
 p=PurePosixPath(name)
 if p.is_absolute() or '..' in p.parts or ':' in name:raise RuntimeError('unsafe archive member')
 return str(p)
with tarfile.open(archive,'r:gz') as tf:
 for member in tf:
  name=safe(member.name)
  if member.isfile():
   h=hashlib.sha256()
   with tf.extractfile(member) as f:
    for b in iter(lambda:f.read(1048576),b''):h.update(b)
   digests[name]=h.hexdigest();source[name]=name
  elif member.islnk():
   target=safe(member.linkname)
   if target not in digests:raise RuntimeError('forward or missing hardlink')
   digests[name]=digests[target];source[name]=source[target]
  elif member.issym():links.append(dict(path=name,target=member.linkname))
  if name.startswith('p18/candidate4/') and name in digests:
   dest=restore/name;dest.parent.mkdir(parents=True,exist_ok=True)
   src=out/'restore_check'/source[name]
   if hashlib.sha256(src.read_bytes()).hexdigest()!=digests[name]:raise RuntimeError('restored source mismatch '+name)
   os.link(src,dest)
bad=[e['path'] for e in inventory['entries'] if e['kind']=='file' and digests.get(e['path'])!=e['sha256']]
h=hashlib.sha256()
with archive.open('rb') as f:
 for b in iter(lambda:f.read(1048576),b''):h.update(b)
report=dict(status='PASS' if not bad else 'FAIL',archive=str(archive.relative_to(root)),sha256=h.hexdigest(),verifiedFiles=len(digests),mismatches=bad,
 completeP18RestoredFiles=sum(1 for p in restore.rglob('*') if p.is_file()),completeP18Restore=str(restore.relative_to(root)),symlinks=links,
 boardMutation=False,earlierRestore='Interrupted redundant historical hardlink expansion; archive retained, all archive bytes independently rechecked')
(out/'verification.json').write_text(json.dumps(report,indent=2));print(json.dumps({k:v for k,v in report.items() if k!='symlinks'},indent=2))
if bad:raise SystemExit(2)
