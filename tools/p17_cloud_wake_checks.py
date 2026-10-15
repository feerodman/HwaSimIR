"""Sampled raw/display comparisons; does not substitute for visual video review."""
import argparse
import json
import re
import numpy as np
from PIL import Image
from p17_collect_evidence import ROOT, read_pfm, save_json, read_json, sha
W=ROOT/'logs/p17/cloud_rework';OLD=ROOT/'logs/p17/ir_retest/after'

def raw(case,seq):return read_pfm(next((case/'diagnostic_linear').glob(f'*seq{seq}.pfm')))
def rgb(case,seq):return np.asarray(Image.open(next((case/'diagnostic_linear').glob(f'*seq{seq}_rgb8.png'))),dtype=float)[:,:,:3]
def manifest(v):
    return {line.split(None,1)[1].strip().replace('\\','/'):line.split()[0] for line in
        (W/f'build/{v}/source_manifest.sha256').read_text(encoding='utf-8-sig').splitlines()}

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--available',action='store_true');a=ap.parse_args()
    rows=[];cloud=[];weather=[];streaming=[]
    for band in ('mwir','swir'):
        off=W/f'after/{band}_plume0_v11';on=W/f'after/{band}_plume1_v11'
        for seq in (900,1200,3600,4200):
            r0=raw(off,seq);previousOff=raw(OLD/f'{band}_plume0_ir_v8',seq)
            assert np.array_equal(r0,previousOff),(band,seq,'engine-off changed')
            if not (on/'case_result.json').exists():continue
            r1=raw(on,seq);old=raw(OLD/f'{band}_plume1_ir_v8',seq)
            assert np.isfinite(r1).all()
            delta=(r1-r0).mean(2)
            pnew=rgb(on,seq).mean(2);pold=rgb(OLD/f'{band}_plume1_ir_v8',seq).mean(2);poff=rgb(off,seq).mean(2)
            # Outside the target/label: downstream quadrant at this late pose.
            tail=np.zeros((800,800),bool);tail[240:375,240:376]=True
            row=dict(band=band,sourceSeq=seq,engineOffRawIdentical=True,
                engineOnRawMin=float(delta.min()),engineOnRawMax=float(delta.max()),positivePixels=int((delta>1e-5).sum()),
                newRawOutsideOldFootprintPixels=int(((old.mean(2)-r0.mean(2)<=1e-5)&(delta>1e-5)).sum()))
            if seq==4200:
                for label,p in (('before',pold),('after',pnew)):
                    mask=(p-poff>3)&tail;y,x=np.where(mask)
                    row[label+'TailPixelsAbove3Gray']=int(mask.sum())
                    row[label+'TailBoundsXYXY']=[int(x.min()),int(y.min()),int(x.max()),int(y.max())] if len(x) else None
            rows.append(row)
        paths=[ROOT/f'logs/p7/p17_{band}_cloud_{m}_frozen_v12' for m in ('white','radiometric')]
        if all((p/'linear.pfm').exists() and (p/'conservation.json').exists() for p in paths):
            arrays=[read_pfm(p/'linear.pfm') for p in paths]
            pictures=[np.asarray(Image.open(p/'linear_rgb8.png'),dtype=float)[:,:,:3] for p in paths]
            diff=pictures[0].mean(2)-pictures[1].mean(2)
            identical=np.array_equal(*arrays)
            cloud.append(dict(band=band,sourceSeq=180,rawBitIdentical=bool(identical),rawMaxAbsolute=float(np.max(np.abs(arrays[0]-arrays[1]))),
                displayBrighter20Pixels=int((diff>=20).sum()),displayMaxDifference=float(diff.max()),
                fixedWindScale=0,scope='Only controlled frozen-wind pair; ordinary wind-driven runs have differing wall-clock phase'))
            assert identical,(band,'cloud display changed raw')
        elif not a.available:raise RuntimeError(f'Missing cloud isolation {band}')
        for name in ('cloudy','overcast'):
            case=ROOT/f'logs/p7/p17_{band}_{name}_white_v12'
            if not (case/'conservation.json').exists():continue
            lines=(case/'board.log').read_text(encoding='utf-8',errors='replace').splitlines()
            reference='845AC79DD2E193DE'
            events=[s for s in lines if '[World3DCloudCell]' in s and reference in s]
            samples=[json.loads(s.split('[CloudLineOfSightAudit] ',1)[1]) for s in lines if '[CloudLineOfSightAudit]' in s and reference in s]
            if name=='cloudy':
                assert events and 'action=activate' in events[-1] and samples,(case,'reference cloud not retained')
                assert samples[-1]['selectedForRender'] and samples[-1]['centralRayIntersects'],case
            counts=[int(m.group(1)) for s in lines if '[World3DCloudStreaming]' in s
                for m in [re.search(r'visibleCloudVolumes=(\d+)',s)] if m]
            streaming.append(dict(caseId=case.name,referenceCloudId=reference,events=events,loggedLineOfSightSamples=len(samples),
                lastSample=samples[-1] if samples else None,maxSelectedVolumeCount=max(counts,default=0),
                scope='Cloudy tracks the reference cloud; Overcast uses its own profile population. Logged selection/ray geometry plus decoded image review, not per-pixel target occlusion calibration'))
    mask=np.ones((800,800),bool);mask[300:460,220:575]=False
    for name in ('clear','rain','snow'):
        case=W/f'after/mwir_{name}_v11'
        if not (case/'case_result.json').exists():
            if a.available:continue
            raise RuntimeError(f'Missing weather {name}')
        for seq in (900,1200):
            r=raw(case,seq);before=raw(OLD/f'mwir_{name}_ir_v8',seq)
            assert np.array_equal(r[mask],before[mask]),(name,seq,'weather background raw changed')
            if name!='clear':assert np.array_equal(r,before),(name,seq,'precipitation raw changed')
            d=rgb(case,seq).mean(2)-rgb(W/'after/mwir_clear_v11',seq).mean(2)
            weather.append(dict(weather=name,sourceSeq=seq,rawIdenticalToV8=bool(np.array_equal(r,before)),
                rawChangedPixels=int(np.any(r!=before,axis=2).sum()),backgroundRawIdentical=bool(np.array_equal(r[mask],before[mask])),
                rawMaxAbsoluteToV8=float(np.max(np.abs(r-before))),displayMin=float(d[mask].min()),displayMax=float(d[mask].max())))
    m=[manifest(v) for v in ('v11','v12')]
    changes=[k for k in sorted(set(m[0])|set(m[1])) if m[0].get(k)!=m[1].get(k)]
    assert len(changes)==1 and changes[0].endswith('IRCloudPresentation.inl'),changes
    assert sha(ROOT/'DataDrivenTestQT/1.txt')=='f2c3db00d71012cd28336b5077e03ff8d4ed1eda2698fd900486ae9cdfdd7901'
    if not a.available:assert len(rows)==8 and len(cloud)==2 and len(weather)==6 and len(streaming)==4
    save_json(W/'version_scope.json',dict(sourceFilesChangedV11ToV12=changes,
        scope='v12 only synchronizes display-only cloud sheet wind UV. v11 cloud-disabled plume and weather paths are unchanged.',
        spriteShaderSha256={name:sha(ROOT/'HwaSim_IR/Bin/Config/GameVFX'/name) for name in ('sprite.vert','sprite.frag')}))
    save_json(W/'cloud_wake_checks.json',dict(plume=rows,cloudDisplayIsolation=cloud,weatherRegression=weather,cloudStreaming=streaming,
        originalInputSha256=sha(ROOT/'DataDrivenTestQT/1.txt'),
        limitation='Finite sampled frames and engineering appearance; engine ON also controls body heat; no all-scene calibration or sustained 60 Hz claim.'))
    print('checks written',len(rows),len(cloud),len(weather))

if __name__=='__main__':main()
