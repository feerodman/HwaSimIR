"""Measured nonzero pixel influence in matched common-scaled linear captures.

This is a rendered footprint, not a hardware fragment-invocation counter.
The unchanged batch uses one draw and a prefix of the same immutable quads.
"""
import csv,json,re
from pathlib import Path
import numpy as np
from PIL import Image
from p7_validate_visual_pairs import pfm

BASE=Path(__file__).resolve().parents[1]/'logs/p8'
def main():
    rows=[]
    for weather in ['rain','snow']:
        cases={n:BASE/f'P8_pixels_{weather}_{n}' for n in [0,128,192]}
        images={n:pfm(case/'linear.pfm') for n,case in cases.items()}
        controls=[]
        for n,case in cases.items():
            q=json.loads((case/'request.json').read_text(encoding='utf-8-sig'));e=json.loads((case/'effective_environment.json').read_text(encoding='utf-8-sig'))
            log=(case/'board.log').read_text(encoding='utf-8',errors='replace')
            if n:assert f'[PrecipitationBatch] particles={n} draws=1 vertices={n*4}' in log
            controls.append((q['cameraInput'],q['dumpSeq'],e['P6DFrozenGain'],e['P6DFrozenOffset']))
        assert len(set(controls))==1
        for n in [192,128]:
            d=np.max(abs(images[n]-images[0]),axis=2)
            # Multiple thresholds expose numerical dust / very soft edges.
            body=next((cases[n]/'recording').glob('*/producer_annotations.jsonl'))
            actual=json.loads(body.read_text(encoding='utf-8').splitlines()[239])
            zero_body=next((cases[0]/'recording').glob('*/producer_annotations.jsonl'))
            zero_actual=json.loads(zero_body.read_text(encoding='utf-8').splitlines()[239])
            row=dict(weather=weather,particles=n,vertices=n*4,triangles=n*2,drawCalls=1,sourceSeq=240,simTimeMs=actual['simTimeMs'],zeroReferenceSimTimeMs=zero_actual['simTimeMs'],timeDifferenceMs=actual['simTimeMs']-zero_actual['simTimeMs'],
                changedLinearPixels1e6=int((d>1e-6).sum()),changedLinearPixels1e5=int((d>1e-5).sum()),changedLinearPixels1e4=int((d>1e-4).sum()),maxLinearDifference=float(d.max()),scope='Actual common-scaled linear GPU capture versus zero particles, frozen mapping; not physical radiance or a GPU hardware invocation counter')
            raw=np.asarray(Image.open(cases[n]/'linear_rgb8.png').convert('RGB'),dtype=np.int16)
            zero=np.asarray(Image.open(cases[0]/'linear_rgb8.png').convert('RGB'),dtype=np.int16)
            row['preencodeRgbChangedOver1']=int((np.max(abs(raw-zero),axis=2)>1).sum());rows.append(row)
        old,new=rows[-2:];new['linearFootprintRatioTo192']=new['changedLinearPixels1e5']/old['changedLinearPixels1e5']
        new['result']='OBSERVATION_WITH_INPUT_TIME_DIFFERENCE'
        new['limitation']='Do not use as strict same-input causal proof; use exact full-input matched DDS RGB evidence separately.'
    with (BASE/'delivery/precipitation_coverage.csv').open('w',newline='',encoding='utf-8-sig') as f:
        keys=list(dict.fromkeys(k for r in rows for k in r));w=csv.DictWriter(f,fieldnames=keys);w.writeheader();w.writerows(rows)
    (BASE/'precipitation_coverage.json').write_text(json.dumps(rows,indent=2),encoding='utf-8');print(json.dumps(rows))
if __name__=='__main__':main()
