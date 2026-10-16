"""Extract identical source frame ordinals; never rescale/tone-map full frames."""
from pathlib import Path
import subprocess,json
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'logs/p18/visual';OUT.mkdir(exist_ok=True)
FF=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe'
index=[]
for band in ['swir','mwir']:
    before=next((ROOT/f'logs/p18/before/p18_before_{band}_10urad_p17v12/recording').glob('*/output.mp4'))
    after=ROOT/f'logs/p18/runs/plume_{band}_old_graceful_v3/customer/output.mp4'
    if not after.exists():continue
    for seq in [900,1200,2400,3600,4200]:
        paths=[]
        for label,source in [('before',before),('after',after)]:
            dest=OUT/f'{band}_10_{seq}_{label}.png'
            subprocess.run([str(FF),'-y','-v','error','-i',str(source),'-vf',f'select=eq(n\\,{seq-1})','-vsync','0','-frames:v','1',str(dest)],check=True)
            paths.append(dest)
        sheet=Image.new('RGB',(1600,850),(22,24,28));draw=ImageDraw.Draw(sheet)
        for x,path in zip([0,800],paths):sheet.paste(Image.open(path).convert('RGB'),(x,50))
        draw.text((20,15),f'P17v12 BEFORE | {band.upper()} | 10 urad | sourceSeq {seq}',fill='white')
        draw.text((820,15),'P18 AFTER | Same input / engine=1 / fixed display',fill='white')
        dest=OUT/f'{band}_10_{seq}_comparison.png';sheet.save(dest)
        # A labeled central crop is supplementary, not a substitute full frame.
        crop=Image.new('RGB',(1360,660),(22,24,28));d=ImageDraw.Draw(crop)
        for x,path in zip([0,680],paths):crop.paste(Image.open(path).convert('RGB').crop((180,200,520,520)).resize((680,640),Image.Resampling.NEAREST),(x,20))
        d.text((5,3),'BEFORE / fixed ROI x2 nearest',fill='white');d.text((685,3),'AFTER / fixed ROI x2 nearest',fill='white')
        crop.save(OUT/f'{band}_10_{seq}_crop.png');index.append(dict(band=band,sourceSeq=seq,before=str(before),after=str(after),comparison=str(dest)))
(OUT/'frame_index.json').write_text(json.dumps(index,indent=2))
