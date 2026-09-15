"""Two-phase board retention: exact version groups, current + one checked rollback.

Plan is read-only. --apply additionally requires a passed acceptance receipt for
the current ELF AND Config manifest. Dependencies in system SDK paths are never
retention objects. Unrecognised, pinned, active or incomplete groups are excluded.
"""
import argparse,hashlib,json,os,re,shutil,stat,time
from pathlib import Path
COMPONENTS=('Config','HwaSim_IR','run_precise.sh','rk3588_hwasimir_performance_mode.sh')
PATTERN=re.compile(r'^(Config|HwaSim_IR|run_precise\.sh|rk3588_hwasimir_performance_mode\.sh)\.(before|new|delta)_(\d{8}-\d{6})$')
def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return h.hexdigest()
def safe(root,p):
    if p.parent!=root or p.is_symlink() or p.resolve()!=p or not p.exists():raise ValueError('unsafe direct deployment object: '+str(p))
    if os.path.ismount(str(p)) or p.stat().st_dev!=root.stat().st_dev:raise ValueError('mount excluded: '+str(p))
    if p.is_dir():
        for base,dirs,files in os.walk(p,followlinks=False):
            for n in dirs+files:
                q=Path(base)/n
                if q.is_symlink() or q.stat().st_dev!=root.stat().st_dev or os.path.ismount(str(q)):raise ValueError('nested link/mount excluded: '+str(q))
def version(root,suffix='',full=False):
    paths={k:root/(k+suffix) for k in COMPONENTS}
    for p in paths.values():
        if not p.exists() or p.is_symlink():raise ValueError('incomplete package: '+str(p))
    meta=paths['Config']/'deployment_version.env'
    d=dict(line.split('=',1) for line in meta.read_text().splitlines() if '=' in line)
    hashes={'HwaSim_IR':'ElfSha256','run_precise.sh':'LauncherSha256','rk3588_hwasimir_performance_mode.sh':'PerformanceToolSha256'}
    for k,v in hashes.items():
        if digest(paths[k])!=d[v]:raise ValueError('component hash mismatch: '+str(paths[k]))
    manifest=paths['Config']/'deployment_manifest.sha256'
    if digest(manifest)!=d['ConfigManifestSha256']:raise ValueError('manifest mismatch')
    if full:
        safe(root,paths['Config'])
        for line in manifest.read_text().splitlines():
            h,n=line.split('  ',1);p=paths['Config']/n
            if Path(n).is_absolute() or '..' in Path(n).parts or p.resolve()!=p or not p.is_file():raise ValueError('unsafe manifest entry '+n)
            if digest(p)!=h:raise ValueError('rollback dependency mismatch: '+n)
    return dict(suffix=suffix,paths={k:str(v) for k,v in paths.items()},metadata=d)
def live_references(root):
    refs=[]
    for p in Path('/proc').iterdir():
        if not p.name.isdigit():continue
        for link in [p/'exe',p/'cwd']+list((p/'fd').glob('*')):
            try:
                target=os.readlink(link)
                if target.endswith(' (deleted)'):target=target[:-10]
                if target.startswith(str(root)+'/'):refs.append(target)
            except (OSError,PermissionError):pass
    return sorted(set(refs))
def plan(root,rollback=None,refs=None):
    root=root.resolve();groups={};excluded=[]
    for p in root.iterdir():
        m=PATTERN.fullmatch(p.name)
        if not m:continue
        kind,phase,stamp=m.groups();groups.setdefault((phase,stamp),{})[kind]=p
    pinfile=root/'retention_pins.json';pins=[]
    if pinfile.exists():
        if pinfile.is_symlink():raise ValueError('pin file symlink')
        pins=json.loads(pinfile.read_text())['versions']
    refs=live_references(root) if refs is None else refs
    current=version(root);eligible={}
    for (phase,stamp),objects in sorted(groups.items()):
        reason=None
        if phase!='before' or set(objects)!=set(COMPONENTS):reason='incomplete_or_unfinished'
        if stamp in pins:reason='explicit_pin'
        if any(r==str(p) or r.startswith(str(p)+'/') for p in objects.values() for r in refs):reason='in_use'
        if reason:excluded.append(dict(phase=phase,stamp=stamp,reason=reason,paths=[str(p) for p in objects.values()]));continue
        try:
            for p in objects.values():safe(root,p)
            eligible[stamp]=version(root,'.before_'+stamp)
        except (ValueError,KeyError,OSError) as e:excluded.append(dict(phase=phase,stamp=stamp,reason=str(e)))
    selected=rollback or (max(eligible) if eligible else None)
    if selected not in eligible:raise ValueError('no complete eligible rollback; nothing can be deleted')
    checked=version(root,'.before_'+selected,full=True)
    remove=[v for stamp,v in eligible.items() if stamp!=selected]
    return dict(schema=1,current=current,rollback=checked,delete=remove,excluded=excluded,liveReferences=refs)
def execute(root,result,receipt):
    if receipt.get('result')!='PASS' or not receipt.get('tests'):raise ValueError('passed short acceptance evidence required')
    for key in ['ElfSha256','ConfigManifestSha256']:
        if receipt.get(key)!=result['current']['metadata'][key]:raise ValueError('acceptance does not identify current '+key)
    # Revalidate current and rollback immediately before destructive operations.
    version(root,full=True);version(root,result['rollback']['suffix'],full=True)
    before=os.statvfs(root).f_bavail*os.statvfs(root).f_frsize;deleted=[]
    live=live_references(root)
    for group in result['delete']:
        paths=[Path(p) for p in group['paths'].values()]
        for p in paths:
            safe(root,p)
            if any(r==str(p) or r.startswith(str(p)+'/') for r in live):raise ValueError('became active: '+str(p))
        # Remove all exact members of this one version. No wildcard shell delete.
        for p in paths:
            if p.is_dir():shutil.rmtree(p)
            else:p.unlink()
            deleted.append(str(p))
    os.sync();after=os.statvfs(root).f_bavail*os.statvfs(root).f_frsize
    result.update(deleted=deleted,dfAvailableBefore=before,dfAvailableAfter=after,actualFreedBytes=after-before,acceptance=receipt)
    return result
def main():
    p=argparse.ArgumentParser();p.add_argument('--root',default='/userdata/HwaSimIR');p.add_argument('--rollback');p.add_argument('--receipt',type=Path);p.add_argument('--apply',action='store_true');p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    root=Path(a.root)
    if root!=Path('/userdata/HwaSimIR') or root.resolve()!=root or root.is_symlink():raise ValueError('only the named board workspace is allowed')
    # Deploying and retention must not run concurrently. Never infer completion
    # from timestamps or the board's deliberately unchanged wall clock.
    import fcntl
    with (root/'.deployment.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        if (root/'.deployment_in_progress').exists():raise ValueError('unfinished deployment is protected')
        (root/'.retention_in_progress').mkdir()
        try:
            run_locked(root,a)
        finally:
            (root/'.retention_in_progress').rmdir()
def run_locked(root,a):
        result=plan(root,a.rollback)
        if a.apply:
            if not a.receipt:raise ValueError('receipt required')
            result=execute(root,result,json.loads(a.receipt.read_text()))
        result['mode']='applied' if a.apply else 'plan';result['recordedUnixTime']=time.time()
        a.output.write_text(json.dumps(result,indent=2),encoding='utf-8')
        if a.apply:
            temp=root/'retention_state.json.new';temp.write_text(json.dumps(result,indent=2));os.replace(temp,root/'retention_state.json')
        print(json.dumps(dict(mode=result['mode'],rollback=result['rollback']['suffix'],deleted=len(result.get('deleted',[])),actualFreedBytes=result.get('actualFreedBytes',0),excluded=result['excluded'])))
if __name__=='__main__':main()
