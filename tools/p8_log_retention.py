"""Metadata-only inventory of the sole authorized cleanup root. No link traversal."""
import argparse,csv,json,os,shutil,stat,time,ctypes,gzip
from collections import defaultdict
from pathlib import Path

ROOT=Path('D:/HwaSimIR/logs')
MAINT=ROOT/'_maintenance'
REPARSE=0x400
MANIFESTS={}

def manifest(path):
    key=str(path)
    if key not in MANIFESTS:
        values={}
        if path.exists():
            for line in path.read_text(encoding='utf-8-sig').splitlines():
                fields=line.split(None,1)
                if len(fields)==2 and len(fields[0])==64:values[fields[1].lstrip('*')]=fields[0]
        MANIFESTS[key]=values
    return MANIFESTS[key]

def boundary(path):
    path=Path(os.path.abspath(path))
    if path==ROOT or ROOT not in path.parents:raise RuntimeError('Outside child of authorized logs root: '+str(path))
    for parent in path.parents:
        if os.lstat(parent).st_file_attributes&REPARSE:raise RuntimeError('Reparse parent: '+str(parent))
        if parent==ROOT:break
    return path

def inventory():
    assert ROOT.resolve()==ROOT and ROOT.is_dir() and not os.lstat(ROOT).st_file_attributes&REPARSE
    MAINT.mkdir(exist_ok=True)
    rows=[];skipped=[];stack=[ROOT];totals=defaultdict(int);counts=defaultdict(int)
    while stack:
        directory=stack.pop()
        with os.scandir(directory) as entries:
            for entry in entries:
                path=Path(entry.path);s=entry.stat(follow_symlinks=False)
                if s.st_file_attributes&REPARSE:
                    skipped.append(dict(path=str(path),reason='reparse_point_not_followed'));continue
                if entry.is_dir(follow_symlinks=False):stack.append(path);continue
                rel=path.relative_to(ROOT);row=dict(path=str(path),relative=rel.as_posix(),bytes=s.st_size,mtimeNs=s.st_mtime_ns)
                rows.append(row)
                for n in (1,2):
                    key='/'.join(rel.parts[:n]);totals[key]+=s.st_size;counts[key]+=1
    with (MAINT/'P8_inventory.csv').open('w',newline='',encoding='utf-8-sig') as f:
        w=csv.DictWriter(f,fieldnames=['path','relative','bytes','mtimeNs']);w.writeheader();w.writerows(rows)
    data=dict(root=str(ROOT),capturedNs=time.time_ns(),logicalBytes=sum(r['bytes'] for r in rows),files=len(rows),
        disk=shutil.disk_usage(ROOT)._asdict(),skipped=skipped,
        largest=[dict(path=k,bytes=v,files=counts[k]) for k,v in sorted(totals.items(),key=lambda p:p[1],reverse=True)[:60]])
    (MAINT/'P8_inventory_summary.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
    print(json.dumps(data))

def retained(rows):
    keep={'_maintenance','p8','_keep','p7/delivery','p7/baseline','p7/checkpoints',
        'p7/D_C4_nvg_rain600','p7/D_C4_mwir_snow600','p7/D_C4_nvg_rain60','p7/D_C4_mwir_snow60',
        'p7/C1_board_cloudy_asset60','p7/A_board_fields_2init_abs','p7/A_ui_1080_qt125_fullscreen',
        'p7/D_stop_isolation_quiet_tail','p7/D_packaged_pair20','p7/weather_source'}
    clips=json.loads((ROOT/'p7/delivery/clips.json').read_text(encoding='utf-8'))
    keep.update('p7/'+item['case'] for item in clips)
    for r in rows:
        rel=Path(r['relative']);parts=rel.parts
        if '.keep' in parts:keep.add('/'.join(parts[:parts.index('.keep')]) or '.')
        if len(parts)>1 and parts[0]=='p7' and ('illum' in parts[1].lower() or parts[1].startswith('A_')):keep.add('/'.join(parts[:2]))
    return sorted(keep)

def classify(row,keep,runroots,now):
    rel=row['relative'];parts=rel.split('/');name=parts[-1].lower()
    if any(rel==k or rel.startswith(k+'/') for k in keep):return 'KEEP','protected_evidence',parts[0]+'/'+parts[1] if len(parts)>1 else rel
    if name.endswith(('.lic','.pem','.key')) or name in ('id_rsa','id_ed25519') or 'licence' in name or 'license' in name:return 'KEEP','licence_or_key_not_touched',rel
    if now-int(row['mtimeNs'])<600*1e9:return 'KEEP','recent_or_active',rel
    recovery=any(any(word in p.lower() for word in ('rollback','recovery','restore','backup','snapshot','手工')) for p in parts)
    for i,p in enumerate(parts[:-1]):
        if p=='stage' and any('deploy' in a.lower() for a in parts[:i]):
            if recovery:
                config=ROOT/Path('/'.join(parts[:i+1]))/'Config'
                relative='/'.join(parts[i+2:]) if parts[i+1]=='Config' else ''
                original=ROOT.parent/'HwaSim_IR/Bin/Config'/relative
                old=manifest(config/'deployment_manifest.sha256').get(relative)
                current=manifest(ROOT/'p7/deploy_C4/stage/Config/deployment_manifest.sha256').get(relative)
                if old and old==current and original.is_file() and original.stat().st_size==int(row['bytes']):
                    return 'DELETE','duplicate_recovery_copy_matching_manifests','/'.join(parts[:i+1])
                return 'KEEP','potential_unique_recovery',rel
            return 'DELETE','duplicate_deployment_stage','/'.join(parts[:i+1])
    if recovery:return 'KEEP','potential_unique_recovery',rel
    if rel.startswith('p7/baseline_ui_source/') or rel=='p7/baseline_ui_source.zip':return 'DELETE','head_rebuild_duplicated_in_releases','p7/baseline_ui_source' if len(parts)>2 else rel
    for root in runroots:
        if rel.startswith(root+'/'):return 'DELETE','obsolete_generated_trial',root
    if name.endswith(('.pfm','.raw','.npy','.rgb','.rgb8','.bgr','.rgba.raw')):return 'DELETE','regenerable_full_frame_export',rel
    if int(row['bytes'])>64*1024**2 and name.endswith('.log'):return 'DELETE','obsolete_large_generated_log',rel
    if any('deploy' in p.lower() for p in parts[:-1]) and name.endswith(('.zip','.tar','.tar.gz','.tgz')):return 'DELETE','duplicate_deployment_archive',rel
    return 'KEEP','unclassified_or_small_summary',rel

def load_plan():
    with (MAINT/'P8_inventory.csv').open(encoding='utf-8-sig',newline='') as f:rows=list(csv.DictReader(f))
    keep=retained(rows)
    runroots=sorted({r['relative'].rsplit('/',1)[0] for r in rows if r['relative'].endswith('/request.json')},key=len,reverse=True)
    now=time.time_ns();groups={}
    for r in rows:
        action,reason,unit=classify(r,keep,runroots,now);r.update(action=action,reason=reason,unit=unit)
        key=(action,reason,unit)
        g=groups.setdefault(key,dict(path=str(ROOT/Path(unit)),action=action,reason=reason,bytes=0,files=0))
        g['bytes']+=int(r['bytes']);g['files']+=1
    return rows,keep,groups

def plan():
    rows,keep,groups=load_plan()
    with (MAINT/'P8_cleanup_plan.csv').open('w',newline='',encoding='utf-8-sig') as f:
        w=csv.DictWriter(f,fieldnames=['path','action','reason','bytes','files']);w.writeheader();w.writerows(groups.values())
    (MAINT/'P8_protected_paths.json').write_text(json.dumps(dict(root=str(ROOT),prefixes=keep),indent=2),encoding='utf-8')
    summary={action:dict(files=sum(g['files'] for g in groups.values() if g['action']==action),bytes=sum(g['bytes'] for g in groups.values() if g['action']==action)) for action in ('KEEP','DELETE')}
    (MAINT/'P8_plan_summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8');print(json.dumps(summary))

def delete_entry(row):
    # Open without write-sharing, then delete the verified directory entry via
    # that same handle. Existing writers and new writers cannot race this step.
    path=boundary(row['path']);s=os.lstat(path)
    if s.st_file_attributes&REPARSE:return 'SKIP_REPARSE'
    if s.st_size!=int(row['bytes']) or s.st_mtime_ns!=int(row['mtimeNs']):return 'SKIP_CHANGED'
    from ctypes import wintypes as W
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.CreateFileW.argtypes=[W.LPCWSTR,W.DWORD,W.DWORD,W.LPVOID,W.DWORD,W.DWORD,W.HANDLE];kernel.CreateFileW.restype=W.HANDLE
    kernel.GetFinalPathNameByHandleW.argtypes=[W.HANDLE,W.LPWSTR,W.DWORD,W.DWORD];kernel.GetFinalPathNameByHandleW.restype=W.DWORD
    kernel.SetFileInformationByHandle.argtypes=[W.HANDLE,ctypes.c_int,W.LPVOID,W.DWORD];kernel.SetFileInformationByHandle.restype=W.BOOL
    kernel.CloseHandle.argtypes=[W.HANDLE]
    handle=kernel.CreateFileW(str(path),0x80000000|0x10000,1,None,3,0x00200000,None)
    if handle==W.HANDLE(-1).value:return 'SKIP_OPEN_'+str(ctypes.get_last_error())
    try:
        buf=ctypes.create_unicode_buffer(32768);n=kernel.GetFinalPathNameByHandleW(handle,buf,len(buf),0)
        final=buf.value.removeprefix('\\\\?\\')
        if not n or os.path.normcase(final)!=os.path.normcase(str(path)):return 'SKIP_RESOLVED_PATH'
        flag=W.BOOL(1)
        if not kernel.SetFileInformationByHandle(handle,4,ctypes.byref(flag),ctypes.sizeof(flag)):return 'SKIP_DELETE_'+str(ctypes.get_last_error())
        return 'DELETED'
    finally:kernel.CloseHandle(handle)

def execute():
    rows,keep,groups=load_plan()
    assert (MAINT/'P8_cleanup_plan.csv').exists()
    before=shutil.disk_usage(ROOT);results={};deleted=0;bytes_deleted=0
    with gzip.open(MAINT/'P8_deleted_entries.csv.gz','wt',encoding='utf-8',newline='') as f:
        w=csv.DictWriter(f,fieldnames=['path','bytes','mtimeNs','reason','result']);w.writeheader()
        for row in rows:
            if row['action']!='DELETE':continue
            result=delete_entry(row)
            w.writerow({k:row[k] for k in ('path','bytes','mtimeNs','reason')}|dict(result=result))
            key=(row['unit'],result);g=results.setdefault(key,dict(path=str(ROOT/Path(row['unit'])),result=result,files=0,bytes=0))
            g['files']+=1;g['bytes']+=int(row['bytes'])
            if result=='DELETED':deleted+=1;bytes_deleted+=int(row['bytes'])
            if deleted and deleted%25000==0:print(json.dumps(dict(deletedFiles=deleted,logicalDeletedBytes=bytes_deleted)),flush=True)
    with (MAINT/'P8_cleanup_result.csv').open('w',newline='',encoding='utf-8-sig') as f:
        w=csv.DictWriter(f,fieldnames=['path','result','files','bytes']);w.writeheader();w.writerows(results.values())
    after=shutil.disk_usage(ROOT)
    summary=dict(deletedFiles=deleted,logicalDeletedBytes=bytes_deleted,beforeDisk=before._asdict(),afterDisk=after._asdict(),
        actualFreeSpaceIncrease=after.free-before.free,remainingInventoryLogicalBytes=sum(int(r['bytes']) for r in rows)-bytes_deleted,
        skippedFiles=sum(g['files'] for g in results.values() if g['result']!='DELETED'),linksFollowed=0,entriesTruncated=0)
    (MAINT/'P8_cleanup_summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8');print(json.dumps(summary))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('action',choices=['inventory','plan','execute']);a=p.parse_args()
    dict(inventory=inventory,plan=plan,execute=execute)[a.action]()
