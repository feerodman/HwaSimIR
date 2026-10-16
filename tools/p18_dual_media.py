"""Pair actual simultaneously received recordings by logical frame ordinal.

This is a labeled evidence playback, NOT a desktop screenshot or latency test.
Both source rasters retain their native dimensions; coarse gets black padding.
"""
from pathlib import Path
import argparse, json, subprocess
from PIL import Image, ImageDraw
ROOT=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--run',default='dual_perf_on_v4');args=ap.parse_args()
RUN=ROOT/'logs/p18/runs'/args.run
OUT=ROOT/'logs/p18/visual'
FF=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe'
sources=[RUN/role/'output.mp4' for role in ['coarse','precise']]
for seq in [1200,3600,4200]:
    images=[]
    for role,source in zip(['coarse','precise'],sources):
        dest=OUT/f'dual_{seq}_{role}.png'
        subprocess.run([str(FF),'-y','-v','error','-i',str(source),'-vf',f'select=eq(n\\,{seq-1})','-vsync','0','-frames:v','1',str(dest)],check=True)
        images.append(Image.open(dest).convert('RGB'))
    sheet=Image.new('RGB',(1120,850),(16,18,22));draw=ImageDraw.Draw(sheet)
    sheet.paste(images[0],(0,290));sheet.paste(images[1],(320,50))
    draw.text((10,8),f'ACTUAL SAME-BOARD DUAL RUN / saved-media playback / logical frame {seq}',fill='white')
    draw.text((10,29),'1001/1 | TX/LDecode | native 320 Gray8',fill='white')
    draw.text((520,29),'1001/2 | TX/HD264 | native 800 H264 gray',fill='white')
    sheet.save(OUT/f'dual_{seq}_paired.png')
video=OUT/'dual_simultaneous_recordings.mp4'
subprocess.run([str(FF),'-y','-v','error','-i',str(sources[0]),'-i',str(sources[1]),
    '-filter_complex','[0:v]pad=320:800:0:240:black[l];[l][1:v]hstack=inputs=2[v]',
    '-map','[v]','-c:v','libx264','-crf','18','-preset','fast','-pix_fmt','yuv420p','-an',str(video)],check=True)
(OUT/'dual_media_provenance.json').write_text(json.dumps({'sources':[p.relative_to(ROOT).as_posix() for p in sources],
    'run':args.run,'sourceCountEach':4318,'nativeSizes':[[320,320],[800,800]],
    'method':'Recorded media paired by logical ordinal; coarse padded, no image scale or tonal changes',
    'isLiveDesktopScreenshot':False,'isLatencyEvidence':False,'video':video.relative_to(ROOT).as_posix()},indent=2),encoding='utf-8')
