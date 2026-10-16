"""Verify the generated opaque fixture uses the same log depth as the plume."""
import json,subprocess
from pathlib import Path
from PIL import Image,ImageDraw
import numpy as np
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'logs/p18/visual'
FF=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe';rows=[]
for band in ['swir','mwir']:
    sources=[ROOT/f'logs/p18/runs/nozzle_{band}_occluded_{v}/customer/output.mp4' for v in ['v2','v3']]
    if not all(p.exists() for p in sources):continue
    sheet=Image.new('RGB',(1600,850),(20,24,30));d=ImageDraw.Draw(sheet);samples=[]
    for i,(version,source) in enumerate(zip(['v2','v3'],sources)):
        dest=OUT/f'{band}_occluded_{version}_240.png'
        subprocess.run([str(FF),'-y','-v','error','-i',str(source),'-vf','select=eq(n\\,239)','-vsync','0','-frames:v','1',str(dest)],check=True)
        img=Image.open(dest).convert('RGB');roi=np.asarray(img.convert('L'))[350:450,350:450]
        samples.append(dict(version=version,minGray=int(roi.min()),maxGray=int(roi.max()),stdGray=float(roi.std())))
        sheet.paste(img,(i*800,50));d.text((i*800+10,15),f'{band.upper()} DIAGNOSTIC opaque depth {version} / same sourceSeq 240',fill='white')
    sheet.save(OUT/f'{band}_occlusion_depth_comparison.png')
    rows.append(dict(band=band,sourceSeq=240,roi=[350,350,450,450],samples=samples,
        opaqueInteriorUniform=samples[1]['maxGray']-samples[1]['minGray']<=1,
        scope='Generated diagnostic nozzle/cap only. No production model or sensor tone curve changes.'))
(ROOT/'logs/p18/occlusion_diagnostic.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
print(json.dumps(rows,indent=2))
