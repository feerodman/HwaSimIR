"""Sample the end of ordinary production plume movies without tone changes."""
import json,subprocess
from pathlib import Path
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'logs/p18/visual';index=[]
ff=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe'
for band in ['swir','mwir']:
    folder=OUT/f'{band}_temporal';folder.mkdir(exist_ok=True)
    source=ROOT/f'logs/p18/runs/plume_{band}_old_graceful_v3/customer/output.mp4'
    subprocess.run([str(ff),'-y','-v','error','-i',str(source),'-vf','select=gte(n\\,3599)*not(mod(n-3599\\,60))','-vsync','0','-frames:v','12',str(folder/'frame_%02d.png')],check=True)
    sheet=Image.new('RGB',(1280,1065),(18,22,29));draw=ImageDraw.Draw(sheet)
    for i,p in enumerate(sorted(folder.glob('frame_*.png'))):
        x=i%4*320;y=i//4*355;draw.text((x+5,y+8),f'{band.upper()} / sourceSeq {3600+i*60} / 800 -> 320 preview',fill='white')
        sheet.paste(Image.open(p).convert('RGB').resize((320,320)),(x,y+35))
    target=OUT/f'{band}_motion_contact.png';sheet.save(target)
    index.append(dict(band=band,source=str(source),sourceSeq=list(range(3600,4261,60)),preview=str(target),fullFrames=str(folder),tonalChanges=False,previewRescaled=True))
(OUT/'temporal_review_index.json').write_text(json.dumps(index,indent=2),encoding='utf-8')
