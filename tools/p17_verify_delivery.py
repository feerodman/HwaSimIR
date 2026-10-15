"""Offline verification of a P17 directory, including full video decoding."""
import argparse,hashlib,json,pathlib,subprocess
def main():
    ap=argparse.ArgumentParser();ap.add_argument('directory');a=ap.parse_args();root=pathlib.Path(a.directory).resolve()
    errors=[];count=0;videos=[]
    for line in (root/'manifest.sha256').read_text(encoding='utf-8').splitlines():
        expected,rel=line.split('  ',1);p=(root/rel).resolve()
        if not p.is_relative_to(root) or not p.is_file():errors.append('missing/invalid: '+rel);continue
        h=hashlib.sha256()
        with p.open('rb') as f:
            for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
        if h.hexdigest()!=expected:errors.append('hash: '+rel)
        count+=1
    for p in root.rglob('*.mp4'):
        r=subprocess.run(['ffmpeg','-nostdin','-v','error','-i',str(p),'-f','null','-'],capture_output=True,text=True)
        videos.append(dict(file=p.relative_to(root).as_posix(),decoded=r.returncode==0))
        if r.returncode:errors.append('decode: '+p.relative_to(root).as_posix())
    report=dict(filesChecked=count,videos=videos,errors=errors,result='PASS' if not errors else 'FAIL',scope='file integrity and decoding only; no visual/calibration/performance acceptance')
    print(json.dumps(report,indent=2));raise SystemExit(bool(errors))
if __name__=='__main__':main()
