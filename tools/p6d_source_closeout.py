"""Record the dirty production source without staging or changing the checkout."""
from pathlib import Path
import hashlib,json,subprocess,zipfile
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'logs/p6d'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    baseline=json.loads((OUT/'baseline/source.json').read_text(encoding='utf-8-sig'))
    head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    assert head==baseline['head']
    attachment='docs/HwaSimIR_P6D_Cloud_Compositing_Visual_Closeout_Codex.md'
    with zipfile.ZipFile(OUT/'baseline/preexisting_untracked.zip') as z:
        names=z.namelist();entry=next(n for n in names if n.replace('\\','/').endswith(attachment))
        assert z.read(entry)==(ROOT/attachment).read_bytes()
    changed=subprocess.check_output(['git','diff','--name-only'],cwd=ROOT,text=True).splitlines()
    untracked=subprocess.check_output(['git','ls-files','--others','--exclude-standard'],cwd=ROOT,text=True).splitlines()
    paths=sorted(set(changed+untracked))
    files=[dict(path=p,size=(ROOT/p).stat().st_size,sha256=sha(ROOT/p)) for p in paths]
    production=[r for r in files if r['path'].startswith('HwaSim_IR/')]
    identity=hashlib.sha256(json.dumps(production,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    result=dict(head=head,production_dirty_fingerprint=identity,production_files=production,changed_and_new_files=files,
                preexisting_attachment_preserved=True,user_changes_reset=False,commit_created=False)
    (OUT/'source_final.json').write_text(json.dumps(result,indent=2)+'\n')
    (OUT/'source_final.patch').write_bytes(subprocess.check_output(['git','diff','--binary'],cwd=ROOT))
    with zipfile.ZipFile(OUT/'source_changes.zip','w',compression=zipfile.ZIP_DEFLATED) as z:
        for p in paths:z.write(ROOT/p,p)
    print('HEAD',head,'production dirty fingerprint',identity,'files',len(files))
if __name__=='__main__':main()
