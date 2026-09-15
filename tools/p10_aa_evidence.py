"""Local, same-frame AA comparisons. No image enhancement or radiance claims."""
import hashlib,json,re,csv,statistics
from pathlib import Path
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'logs/p10';OUT=BASE/'aa';OUT.mkdir(exist_ok=True)
MODES=['off','msaa2','msaa4','edge']
CASES={m:BASE/(f'P10_A7_{m}' if m!='edge' else 'P10_A9_edge60') for m in MODES}
images={m:np.asarray(Image.open(CASES[m]/'linear_rgb8.png').convert('RGB')) for m in MODES}
off=images['off'].astype(float)
rows=[]
for mode in MODES:
    case=CASES[mode];log=(case/'board.log').read_text(encoding='utf-8',errors='replace')
    metrics=json.loads((case/'p8_metrics.json').read_text())
    a=images[mode].astype(float);delta=np.abs(a-off)
    row={'mode':mode,'case':case.name,'sourceSeq':180,'rawPFMSha256':hashlib.sha256((case/'linear.pfm').read_bytes()).hexdigest(),
         'preEncodeDifferentPixelsVsOff':int(np.any(delta>0,axis=2).sum()),
         'flatBackgroundMaxDifference8bit':float(delta[20:180,100:700].max()),
         'sphereInteriorMaxDifference8bit':float(delta[260:280,375:420].max()),
         'allImageBordersMaxDifference8bit':float(max(delta[0].max(),delta[-1].max(),delta[:,0].max(),delta[:,-1].max())),
         'checkerMeanAbsoluteDifference8bit':float(delta[220:365,120:255].mean()),
         'inputHz':metrics['actualSenderHz'],'newImageFpsIncludingStartup':metrics['rates']['newImage']['fps'],
         'renderMedianMs':metrics['stages']['render']['doFrameMs']['p50'],
         'fboAudit':[l for l in log.splitlines() if '[P10Fbo]' in l],
         'displayMapping':[l for l in log.splitlines() if '[DisplayFrameMapping]' in l],
         'fileValidation':json.loads(next((case/'recording').glob('*/validation.json')).read_text())['result'],
         'inputProducts':json.loads((case/'input_product_validation.json').read_text())['result']}
    rows.append(row)
for region,box in [('sphere',(320,210,480,365)),('checker',(95,200,285,385))]:
    a=Image.fromarray(images['off']).crop(box);b=Image.fromarray(images['edge']).crop(box)
    # Exact nearest-neighbour enlargement for defect inspection; originals kept.
    result=Image.new('RGB',(a.width*4,a.height*2))
    result.paste(a.resize((a.width*2,a.height*2),Image.Resampling.NEAREST),(0,0))
    result.paste(b.resize((b.width*2,b.height*2),Image.Resampling.NEAREST),(a.width*2,0))
    result.save(OUT/f'{region}_off_left_edge_right_2x.png')
(OUT/'comparison.json').write_text(json.dumps({'pixelScope':'sourceSeq180 pre-encode RGB8, left Off/right EdgeAA; all raw common-scaled PFM hashes must match','rows':rows},indent=2),encoding='utf-8')
with (OUT/'comparison.csv').open('w',newline='',encoding='utf-8-sig') as f:
    keys=[k for k,v in rows[0].items() if not isinstance(v,list)];w=csv.DictWriter(f,fieldnames=keys);w.writeheader();w.writerows({k:r[k] for k in keys} for r in rows)
assert len({r['rawPFMSha256'] for r in rows})==1
assert all(r['flatBackgroundMaxDifference8bit']==0 and r['sphereInteriorMaxDifference8bit']==0 for r in rows)
assert all(r['allImageBordersMaxDifference8bit']==0 for r in rows)
print(json.dumps({'rawInputSame':True,'flatRegionsUnchanged':True,'rows':[{k:v for k,v in r.items() if k not in ('fboAudit','displayMapping')} for r in rows]},indent=2))
