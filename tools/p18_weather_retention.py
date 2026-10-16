"""Measure saved weather differences without changing tone mapping or source pixels."""
from pathlib import Path
import json, subprocess
import numpy as np
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'logs/p18/visual/weather';OUT.mkdir(parents=True,exist_ok=True)
FF=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe'
results=[]
for band in ['swir','mwir']:
    for seq in [900,1200]:
        frames=[];names=[]
        for weather in [0,2,3]:
            name=f'retain_{band}_weather{weather}_v2'
            if band=='swir' and weather==3:name='retain_swir_weather3_retry'
            source=ROOT/f'logs/p18/runs/{name}/customer/output.mp4'
            if not source.exists():break
            dest=OUT/f'{band}_{weather}_{seq}.png'
            subprocess.run([str(FF),'-y','-v','error','-i',str(source),'-vf',f'select=eq(n\\,{seq-1})','-vsync','0','-frames:v','1',str(dest)],check=True)
            frames.append(Image.open(dest).convert('RGB'));names.append(name)
        if len(frames)!=3:continue
        mask=np.ones((800,800),bool);mask[280:520,180:620]=False
        clear=np.asarray(frames[0].convert('L')).astype(np.int16)
        stats=[]
        for weather,img in zip([2,3],frames[1:]):
            diff=(np.asarray(img.convert('L')).astype(np.int16)-clear)[mask]
            stats.append(dict(weather=weather,minGrayDelta=int(diff.min()),maxGrayDelta=int(diff.max()),
                absAtLeast2Pixels=int((abs(diff)>=2).sum()),backgroundPixels=int(mask.sum()),
                caveat='Decoded lossy stream difference includes compression error; not every differing pixel is a particle'))
        sheet=Image.new('RGB',(2400,850),(20,24,30));d=ImageDraw.Draw(sheet)
        for x,img,title in zip([0,800,1600],frames,['CLEAR','RAIN','SNOW']):
            sheet.paste(img,(x,50));d.text((x+10,15),f'{band.upper()} {title} sourceSeq={seq}; unchanged fixed display',fill='white')
        sheet.save(OUT/f'{band}_{seq}_clear_rain_snow.png')
        results.append(dict(band=band,sourceSeq=seq,cases=names,stats=stats,displayRescaling=False))
(ROOT/'logs/p18/weather_retention.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
print(json.dumps(results,indent=2))
