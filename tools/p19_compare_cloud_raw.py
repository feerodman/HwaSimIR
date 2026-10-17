"""Paired lossless raw comparison, with per-frame errors and source identity checks."""
import argparse,hashlib,json
from pathlib import Path
import numpy as np
root=Path(__file__).resolve().parents[1];ap=argparse.ArgumentParser();ap.add_argument('reference');ap.add_argument('candidate');args=ap.parse_args()
base=root/'logs/p19/runs';a=base/args.reference;b=base/args.candidate
ra=json.loads((a/'result.json').read_text());rb=json.loads((b/'result.json').read_text())
assert ra['status']==rb['status']=='PASS'
sourceA=(a/'ordinary_replay_subset.csv').read_bytes();sourceB=(b/'ordinary_replay_subset.csv').read_bytes();assert sourceA==sourceB
report={'reference':args.reference,'candidate':args.candidate,'sourceSha256':hashlib.sha256(sourceA).hexdigest(),'scope':'uncompressed Gray8 before video coding; numeric comparison, visual review separate','streams':{}}
for role in ('coarse','precise'):
 if role not in ra or role not in rb:continue
 x=ra[role];y=rb[role]
 assert x['pixelFormat']==y['pixelFormat']=='gray8' and x['codec']==y['codec']=='raw'
 assert x['width']==y['width'] and x['height']==y['height'] and x['decodedFrames']==y['decodedFrames']
 shape=(x['decodedFrames'],x['height'],x['width']);av=np.memmap(a/role/'received.gray8',dtype=np.uint8,mode='r',shape=shape);bv=np.memmap(b/role/'received.gray8',dtype=np.uint8,mode='r',shape=shape)
 frames=[];hist=np.zeros(256,dtype=np.int64)
 for i in range(shape[0]):
  d=np.abs(av[i].astype(np.int16)-bv[i].astype(np.int16));hist+=np.bincount(d.ravel(),minlength=256)
  frames.append({'frame':i+1,'mae':float(d.mean()),'max':int(d.max()),'fractionAbove1':float((d>1).mean())})
 total=int(hist.sum());report['streams'][role]={'frames':shape[0],'width':shape[2],'height':shape[1],'mae':float(np.dot(hist,np.arange(256))/total),'max':int(np.flatnonzero(hist)[-1]),'fractionAbove1':float(hist[2:].sum()/total),'errorHistogram':hist.tolist(),'perFrame':frames}
(b/'visual_raw_comparison.json').write_text(json.dumps(report,indent=2));print(json.dumps({r:{k:v for k,v in m.items() if k not in ('perFrame','errorHistogram')} for r,m in report['streams'].items()},indent=2))
