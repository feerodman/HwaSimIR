"""Inspect saved MP4 pixels against the explicit ordinary illumination phases."""
import argparse,csv,json,re,subprocess
from pathlib import Path
import numpy as np
from PIL import Image

def validate(case,ffmpeg,expect):
    rows=[];selections=(30,90,180,240,300)
    for folder in sorted((case/'recording').iterdir()):
        if not (folder/'frame_index.jsonl').exists():continue
        products=[json.loads(x) for x in (folder/'frame_index.jsonl').read_text(encoding='utf-8-sig').splitlines()]
        by_source={int(p['sourceSeq']):i for i,p in enumerate(products)}
        for seq in selections:
            if seq not in by_source:continue
            path=folder/f'illumination_source{seq}.png'
            subprocess.run([str(ffmpeg),'-v','error','-threads','1','-i',str(folder/'output.mp4'),'-vf',f'select=eq(n\\,{by_source[seq]})',
                '-frames:v','1','-update','1','-y',str(path)],check=True)
            pixels=np.asarray(Image.open(path).convert('RGB'),dtype=float).mean(axis=2)
            # Inner plate and near-facing sphere regions lie inside the known 100 mrad beam.
            plate=pixels[340:460,285:365];sphere=pixels[340:460,445:495]
            rows.append(dict(run=folder.name,sourceSeq=seq,phase='on' if seq in (90,240) else 'off',
                plateMean=float(plate.mean()),sphereMean=float(sphere.mean()),plateStd=float(plate.std()),image=str(path)))
    checks=[]
    for run in sorted(set(r['run'] for r in rows)):
        group=[r for r in rows if r['run']==run]
        for region in ('plate','sphere'):
            on=[r[region+'Mean'] for r in group if r['phase']=='on'];off=[r[region+'Mean'] for r in group if r['phase']=='off']
            if not on or not off:raise RuntimeError('Missing actual on/off MP4 evidence')
            change=float(np.mean(on)-np.mean(off))
            passed=change>1 if expect=='on' else abs(change)<2 if expect=='off' else True
            checks.append(dict(run=run,region=region,onMinusOffRGB8=change,result='PASS' if passed else 'FAIL'))
    with (case/'illumination_pixels.csv').open('w',newline='',encoding='utf-8-sig') as f:
        writer=csv.DictWriter(f,fieldnames=rows[0]);writer.writeheader();writer.writerows(rows)
    log=(case/('board.log' if (case/'board.log').exists() else 'hwa.out.log')).read_text(errors='replace')
    samples=re.findall(r'\[P7IlluminationSample\] ([^\n]+)',log)
    (case/'illumination_effective.csv').write_text('sourceSeq,protocolEn,effectiveEn,sensorBand,sourceBand,angleMrad,spotRaw,centerReference,fallback\n'+
        '\n'.join(','.join(dict(re.findall(r'(\w+)=([^ ]+)',s)).get(k,'') for k in ['sourceSeq','protocolEn','effectiveEn','sensorBand','sourceBand','angleMrad','spotRaw','centerReference','fallback']) for s in samples)+'\n')
    result=dict(result='PASS' if checks and all(x['result']=='PASS' for x in checks) else 'FAIL',expected=expect,checks=checks,
        scope='Actual saved ordinary plate/sphere pixels; full-frame Auto mapping may change their relation to background',
        synthetic='range 1000 m; rho .5; tau 1; reference illumination from effective existing configuration; not device calibration')
    (case/'illumination_validation.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('case',type=Path);p.add_argument('--ffmpeg',type=Path,required=True);p.add_argument('--expect',choices=['on','off','observe'],default='on');a=p.parse_args();validate(a.case,a.ffmpeg,a.expect)
