"""Finalize a kit after copying support files and successful video verification."""
import json,pathlib
from p17_collect_evidence import sha
ROOT=pathlib.Path(__file__).resolve().parents[1]
def main():
    out=ROOT/'logs/p17/deliverables/HwaSimIR_P17'
    old=dict((line.split('  ',1)[1],line.split('  ',1)[0]) for line in (out/'manifest.sha256').read_text(encoding='utf-8').splitlines())
    data=(ROOT/'logs/p17/delivery_verification.json').read_bytes()
    verified=json.loads(data.decode('utf-16' if data.startswith(b'\xff\xfe') else 'utf-8-sig'))
    if verified['result']!='PASS':raise RuntimeError('Initial integrity/decode validation did not pass')
    for video in verified['videos']:
        digest=sha(out/video['file'])
        if old[video['file']]!=digest:raise RuntimeError('Video changed since decode verification')
        video['sha256']=digest
    (out/'media_validation.json').write_text(json.dumps(dict(result='PASS',videos=verified['videos'],scope='Fully decoded MP4, bound to unchanged video hashes; no visual/performance/calibration acceptance'),indent=2)+'\n',encoding='utf-8')
    files=sorted(p for p in out.rglob('*') if p.is_file() and p.name!='manifest.sha256')
    (out/'manifest.sha256').write_text(''.join(sha(p)+'  '+p.relative_to(out).as_posix()+'\n' for p in files),encoding='utf-8')
    print(json.dumps(dict(files=len(files),bytes=sum(p.stat().st_size for p in files),decodedMP4=len(verified['videos']))))
if __name__=='__main__':main()
