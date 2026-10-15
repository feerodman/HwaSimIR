"""Compare real before/after board outputs without contrast enhancement."""
import json
import argparse
import subprocess
import numpy as np
from PIL import Image
from p17_collect_evidence import ROOT, read_pfm

BASE=ROOT/'logs/p17/ir_retest/after'
parser=argparse.ArgumentParser();parser.add_argument('--weather-only',action='store_true');args=parser.parse_args()
OUT=ROOT/'logs/p17/ir_retest/hdr_check'
OUT.mkdir(parents=True,exist_ok=True)
mask=np.ones((800,800),bool);mask[300:460,220:575]=False
contract=json.loads((ROOT/'logs/p17/ir_retest/profile/check.json').read_text())
knee,white=contract['kneeRadiance'],contract['whiteRadiance']


def sample(weather,version,seq):
    case=BASE/f'mwir_{weather}_ir_{version}';p=case/'diagnostic_linear'
    raw=read_pfm(next(p.glob(f'*seq{seq}.pfm')))
    rgb=np.asarray(Image.open(next(p.glob(f'*seq{seq}_rgb8.png'))),dtype=float)[:,:,:3]
    decoded=OUT/f'{weather}_{version}_{seq}.png'
    movie=next(case.glob('recording/*/output.mp4'))
    subprocess.run(['ffmpeg','-nostdin','-xerror','-v','error','-y','-i',str(movie),
        '-vf',f'select=eq(n\\,{seq-1})','-frames:v','1',str(decoded)],check=True)
    return raw,rgb,np.asarray(Image.open(decoded),dtype=float)[:,:,:3]


rows=[];plume=[]
for seq in (900,1200):
    samples={}
    for weather in ('clear','rain','snow'):
        samples[weather]={v:sample(weather,v,seq) for v in ('v7','v8')}
        old,new=samples[weather]['v7'],samples[weather]['v8']
        delta=new[0]-old[0]
        assert np.isfinite(new[0]).all() and np.max(np.abs(delta[mask]))==0, (weather,seq,'raw_changed')
        prediction=(np.log1p(np.maximum(new[0].mean(2),0)/knee)/np.log1p(white/knee))**(1/2.2)*255
        error=np.abs(new[1].mean(2)-np.clip(prediction,0,255))[mask]
        assert np.percentile(error,99.9)<=1.1,(weather,seq,'GPU_curve_mismatch',np.percentile(error,99.9))
        rows.append(dict(weather=weather,sourceSeq=seq,rawOutsideTargetBitIdentical=True,
            rawWholeImageMaxAbsolute=float(np.abs(delta).max()),gpuCurveErrorP999Gray=float(np.percentile(error,99.9))))
    for weather in ('rain','snow'):
        comparison={}
        for v in ('v7','v8'):
            r=samples[weather][v];c=samples['clear'][v]
            pre=(r[1]-c[1]).mean(2)[mask];dec=(r[2]-c[2]).mean(2)[mask]
            comparison[v]=dict(preencodeMin=float(pre.min()),preencodeMax=float(pre.max()),
                decodedMin=float(dec.min()),decodedMax=float(dec.max()),
                decodedPixelsAbsAtLeast2=int((np.abs(dec)>=2).sum()))
        # Particle coverage differs between source times. Compare each sample
        # to its own unchanged-input baseline, not a fixed particle brightness.
        assert (comparison['v8']['preencodeMax']>comparison['v7']['preencodeMax'] if weather=='rain'
                else comparison['v8']['preencodeMin']<comparison['v7']['preencodeMin'])
        rows.append(dict(weather=weather,sourceSeq=seq,contrast=comparison))
for seq in (() if args.weather_only else (3600,4200)):
    old,new=sample('plume1','v7',seq),sample('plume1','v8',seq)
    saturated=[]
    for data in (old,new):
        rgb=data[1];gray=(rgb[:,:,0]==rgb[:,:,1])&(rgb[:,:,1]==rgb[:,:,2])
        saturated.append(int((gray&(rgb[:,:,0]>=255)).sum()))
    assert saturated[1]<=saturated[0],(seq,'new_full_white_clipping',saturated)
    plume.append(dict(sourceSeq=seq,oldSaturatedPixels=saturated[0],newSaturatedPixels=saturated[1],
        oldRawMax=float(old[0].max()),newRawMax=float(new[0].max()),
        rawWholeImageMaxAbsolute=float(np.abs(new[0]-old[0]).max())))
result=dict(result='PASS_SAMPLED_WEATHER_ONLY' if args.weather_only else 'PASS_SAMPLED_BOARD_HDR_FIX',weather=rows,plume=plume,
    scope='Two weather frame identities and two late plume samples; full videos separately decoded and visually reviewed',
    exclusions=['all noise/exposure settings','radiometric calibration','all plume geometries and overlaps'])
filename='hdr_weather_check.json' if args.weather_only else 'hdr_gpu_check.json'
(ROOT/'logs/p17/ir_retest'/filename).write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
