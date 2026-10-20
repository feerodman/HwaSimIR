"""Strict structured trace and actual GPU linear image summaries (not CPU pixel truth)."""
import argparse, hashlib, json, math
from pathlib import Path
import numpy as np
from collections import Counter

ROOT=Path(__file__).resolve().parents[1]
def unique(pairs):
    d={}
    for k,v in pairs:
        if k in d: raise ValueError('duplicate JSON field '+k)
        d[k]=v
    return d
def trace(path):
    rows=[json.loads(s,object_pairs_hook=unique) for s in path.read_text(encoding='utf-8').splitlines()]
    seen=set(); forced=0; targets=[]; backgrounds=[]
    for r in rows:
        key=tuple(r[k] for k in ('runId','generation','sourceSeq','targetKey','band'))
        if key in seen:raise ValueError('duplicate frame/target trace '+str(key))
        seen.add(key)
        assert r['datasetId'].startswith('P20M3_GREY_GAS_1:')
        for k,v in r['components'].items():
            assert set(v)=={'value','unit','status'} and v['unit'] and math.isfinite(v['value']), (k,v)
        if r['targetKey']=='ENVIRONMENT':backgrounds.append(r)
        else:
            targets.append(r)
            forced+=int(r['input']['forcedBlackDueToAtmosphereMiss'])
    return dict(records=len(rows),targetRecords=len(targets),environmentRecords=len(backgrounds),
        forcedBlackDueToAtmosphereMiss=forced,
        overallStatuses=sorted(set(r['overall'] for r in rows)),
        cameraStatusCounts=dict(Counter(r['cameraStatus'] for r in targets)),
        backgroundDistinctComponentSnapshots=len(set(json.dumps(r['components'],sort_keys=True) for r in backgrounds)),
        targetCounts=sorted(set(r['input'].get('targetNumValid') for r in backgrounds)),
        note='One record per evaluated target and per environment frame; hidden/no-target frames classified independently.')
def pfm(path):
    with path.open('rb') as f:
        kind=f.readline().strip();w,h=map(int,f.readline().split());scale=float(f.readline());channels=3 if kind==b'PF' else 1
        v=np.frombuffer(f.read(),dtype='<f4' if scale<0 else '>f4').reshape(h,w,channels)[::-1]*abs(scale)
    assert np.isfinite(v).all() and (v>=0).all()
    def stats(x):return dict(min=float(x.min()),max=float(x.max()),mean=float(x.mean()),std=float(x.std()))
    return dict(width=w,height=h,channels=channels,unit='W/(m^2 sr um)',source='GPU SI render target PFM; before display/AGC and annotations',
        all=stats(v),center40=stats(v[h//2-20:h//2+20,w//2-20:w//2+20]),corner40=stats(v[:40,:40]),
        sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        note='Center ROI includes target/background mixture; no inferred segmentation or CPU scalar substitution.')
def main():
    ap=argparse.ArgumentParser();ap.add_argument('directory',type=Path);a=ap.parse_args();d=a.directory
    report={'schema':'P20M3.Evidence.1','traces':{},'gpuLinearFrames':{}}
    for f in d.rglob('physical.jsonl'):report['traces'][str(f.relative_to(d))]=trace(f)
    for f in d.rglob('linear.pfm'):report['gpuLinearFrames'][str(f.relative_to(d))]=pfm(f)
    (d/'physical_evidence_summary.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
