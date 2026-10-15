"""Quantify weather visibility and separate fixed display curves from raw source."""
import json
import numpy as np
from PIL import Image
from p17_collect_evidence import ROOT, read_pfm

BASE=ROOT/'logs/p17/ir_retest/after'
mask=np.ones((800,800),bool)
mask[300:460,220:575]=False  # exclude target and green annotations
rows=[]
for band,extra in (('swir',''),('mwir',''),('mwir','_game')):
    for weather in ('rain','snow'):
        for seq in (900,1200):
            paths=[BASE/f'{band}_{w}{extra}_ir_v7'/'diagnostic_linear' for w in ('clear',weather)]
            rgb=[np.asarray(Image.open(next(p.glob(f'*seq{seq}_rgb8.png'))),dtype=float)[:,:,:3] for p in paths]
            raw=[read_pfm(next(p.glob(f'*seq{seq}.pfm'))) for p in paths]
            dr=(raw[1]-raw[0]).mean(2)[mask]
            dg=(rgb[1]-rgb[0]).mean(2)[mask]
            assert np.isfinite(dr).all()
            rows.append(dict(band=band.upper(),preset='Game' if extra else 'HDR',weather=weather,
                sourceSeq=seq,rawMin=float(dr.min()),rawMax=float(dr.max()),
                displayMin=float(dg.min()),displayMax=float(dg.max()),
                displayP01P99=np.percentile(dg,[1,99]).tolist(),
                scope='Weather minus Clear outside target/annotation rectangle; full images not stretched'))
comparisons=[]
for weather in ('clear','rain','snow'):
    for seq in (900,1200):
        paths=[BASE/f'mwir_{weather}{extra}_ir_v7'/'diagnostic_linear' for extra in ('','_game')]
        raw=[read_pfm(next(p.glob(f'*seq{seq}.pfm'))) for p in paths]
        delta=(raw[1]-raw[0])[mask]
        comparisons.append(dict(weather=weather,sourceSeq=seq,
            rawGameMinusHdrMaxAbsolute=float(np.abs(delta).max()),
            rawGameMinusHdrMeanAbsolute=float(np.abs(delta).mean())))
(ROOT/'logs/p17/ir_retest/weather_display_check.json').write_text(json.dumps(dict(
    comparisons=rows,displayIsolation=comparisons,excludedRectangleXYXY=[220,300,575,460],
    limitation='Sampled raw and preencode output; visibility judgment also requires decoded video review'),indent=2)+'\n')
print(json.dumps(dict(weather=rows,displayIsolation=comparisons),indent=2))
