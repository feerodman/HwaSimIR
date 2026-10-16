"""Capture immutable P18 input and pre-change source/resource identity."""
import hashlib,json,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'logs/p18/baseline'
def sha(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return h.hexdigest()
def main():
    OUT.mkdir(parents=True,exist_ok=True)
    if (OUT/'identity.json').exists():raise RuntimeError('Baseline already captured; do not overwrite')
    tracked=subprocess.check_output(['git','ls-files','-z'],cwd=ROOT).decode('utf-8').split('\0')
    paths=[ROOT/p for p in tracked if p and (p.startswith('HwaSim_IR/Bin/Config/') or p.startswith('DDS/IDL/') or p=='DataDrivenTestQT/1.txt')]
    paths+=list((ROOT/'docs/HwaSimIR_P18_Codex_Package').rglob('*'))
    paths+=[ROOT/'HwaSim_IR/Bin/HwaSim_IR.exe',ROOT/'HwaSim_IR_VideoDisplay/x64/Release/HwaSim_IR_VideoDisplay.exe',ROOT/'build-DataDrivenTestQT-codex-mingw73_64-Release/release/DataDrivenTestQT.exe']
    hashes={p.relative_to(ROOT).as_posix():dict(sha256=sha(p),bytes=p.stat().st_size,absolute=str(p.resolve())) for p in paths if p.is_file()}
    for name in ('sprite.vert','sprite.frag'):
        shutil.copy2(ROOT/'HwaSim_IR/Bin/Config/GameVFX'/name,OUT/name)
    shutil.copy2(ROOT/'HwaSim_IR/Bin/Config/IRPlume/engine_plume_profiles.json',OUT/'engine_plume_profiles.json')
    d=dict(head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
        status=subprocess.check_output(['git','status','--short'],cwd=ROOT,text=True),cwd=str(ROOT),files=hashes)
    (OUT/'identity.json').write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('baseline files',len(hashes),'head',d['head'])
if __name__=='__main__':main()
