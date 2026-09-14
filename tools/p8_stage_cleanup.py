"""Remove only verified duplicate Config staging entries inside logs/p8.

Separate evidence and free-space measurement from the original P8 cleanup.
No traversal of links, file truncation, or deletion of recordings/builds.
"""
import csv,gzip,json,os,shutil,time
from pathlib import Path
from p8_log_retention import ROOT,MAINT,REPARSE,boundary,delete_entry,manifest

def main():
    final=ROOT/'p8/deploy_A7/stage/Config'
    if not (ROOT/'p8/deployment_integrity_final.log').exists():raise RuntimeError('Final board/rollback verification receipt required')
    receipt=(ROOT/'p8/deployment_integrity_final.log').read_text(encoding='utf-8-sig')
    if receipt.count('[P8Integrity] result=PASS')!=3:raise RuntimeError('Three complete snapshots must verify first')
    current=manifest(final/'deployment_manifest.sha256');assert current
    rows=[];kept=0
    for deploy in sorted((ROOT/'p8').glob('deploy_*')):
        stage=deploy/'stage/Config'
        if not stage.is_dir() or os.lstat(stage).st_file_attributes&REPARSE:continue
        boundary(stage)
        previous=manifest(stage/'deployment_manifest.sha256')
        if not previous:continue
        for name in ['deployment_manifest.sha256','deployment_version.env']:
            p=stage/name
            if p.exists():shutil.copy2(p,deploy/('retained_'+name))
        stack=[stage]
        while stack:
            directory=stack.pop()
            with os.scandir(directory) as entries:
                for e in entries:
                    s=e.stat(follow_symlinks=False)
                    if s.st_file_attributes&REPARSE:kept+=1;continue
                    if e.is_dir(follow_symlinks=False):stack.append(Path(e.path));continue
                    p=Path(e.path);relative=p.relative_to(stage).as_posix()
                    if relative in previous and previous[relative]==current.get(relative) and time.time_ns()-s.st_mtime_ns>600e9 and 'licen' not in relative.lower():
                        rows.append(dict(path=str(boundary(p)),bytes=s.st_size,mtimeNs=s.st_mtime_ns,reason='same_hash_as_fully_verified_current_board_config',manifestSha256=previous[relative]))
                    else:kept+=1
    plan=MAINT/'P8_stage_cleanup_plan.csv.gz'
    if plan.exists():raise RuntimeError('Refusing to overwrite the staged-cleanup audit')
    with gzip.open(plan,'wt',encoding='utf-8',newline='') as f:
        w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
    before=shutil.disk_usage(ROOT);count=0;deleted=0;skipped=0
    with gzip.open(MAINT/'P8_stage_deleted_entries.csv.gz','wt',encoding='utf-8',newline='') as f:
        w=csv.DictWriter(f,fieldnames=list(rows[0])+['result']);w.writeheader()
        for r in rows:
            status=delete_entry(r);w.writerow(dict(r,result=status))
            if status=='DELETED':count+=1;deleted+=r['bytes']
            else:skipped+=1
    after=shutil.disk_usage(ROOT)
    result=dict(deletedFiles=count,logicalDeletedBytes=deleted,actualFreeSpaceIncrease=after.free-before.free,beforeDisk=before._asdict(),afterDisk=after._asdict(),skippedFiles=skipped,retainedUniqueOrControlEntries=kept,scope='Only newly created duplicate logs/p8/deploy_*/stage/Config entries; all videos and diagnostic cases retained',linksFollowed=0,entriesTruncated=0)
    (MAINT/'P8_stage_cleanup_summary.json').write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps(result))

if __name__=='__main__':main()
