"""Assemble actual receive media and explicitly labelled comparisons; no retouching."""
from pathlib import Path
import hashlib
import json
import subprocess
import shutil
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'logs/p6b'
DEST=OUT/'delivery'
DEST.mkdir(exist_ok=True)
BIN=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin'
records=[]

def keep(case,name,kind='received.png'):
    source=OUT/case/kind
    if not source.exists():return
    target=DEST/name
    shutil.copy2(source,target)
    records.append(dict(file=name,source=str(source),sha256=hashlib.sha256(target.read_bytes()).hexdigest(),operation='byte-for-byte copy of actual receive artifact',request=json.loads((OUT/case/'request.json').read_text(encoding='utf-8-sig'))))

for case,name in [('rk_release_fixed_smoke','normal_world_fixed.png'),('rk_release_auto60','normal_world_auto.png'),('rk_release_normal_asset','normal_world_asset.png'),('rk_release_separated','separated_world_view.png'),('rk_release_plate','ordinary_plate_occlusion.png'),('rk_uv_auto_smoke','sheet_before.png'),('rk_release_auto_smoke','sheet_after.png'),('rk_pair_mid_legacy','old_formula_reference.png')]:keep(case,name)
for case,name in [('rk_release_pan','normal_world_pan.mp4'),('rk_release_normal_asset','normal_world_asset.mp4'),('rk_release_auto60','normal_world_auto.mp4'),('rk_release_normal_600s','long_test_first_10s.mp4')]:keep(case,name,'received.mp4')
for case,name in [('rk_deployed_runtime','deployed_default_mwir'),('rk_deployed_nvg','deployed_default_nvg')]:
    keep(case,name+'.png')
    keep(case,name+'.mp4','received.mp4')

def pair_image(left,right,name,labels):
    paths=[OUT/left/'received.png',OUT/right/'received.png']
    if not all(p.exists() for p in paths):return
    canvas=Image.new('RGB',(1600,845),'#13191c');draw=ImageDraw.Draw(canvas)
    font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',22)
    for i,p in enumerate(paths):
        canvas.paste(Image.open(p).convert('RGB'),(800*i,0));draw.text((800*i+15,810),labels[i],fill='white',font=font)
    canvas.save(DEST/name)
    records.append(dict(file=name,operation='native 800px images placed side by side with labels below',sources=[str(p) for p in paths],sha256=hashlib.sha256((DEST/name).read_bytes()).hexdigest()))

def pair_video(left,right,name):
    paths=[OUT/left/'received.mp4',OUT/right/'received.mp4']
    if not all(p.exists() for p in paths):return
    dest=DEST/name
    if not dest.exists() or max(p.stat().st_mtime for p in paths)>dest.stat().st_mtime:
        args=[str(BIN/'ffmpeg.exe'),'-v','error','-y','-i',str(paths[0]),'-i',str(paths[1]),'-filter_complex','[0:v]fps=30[a];[1:v]fps=30[b];[a][b]hstack=inputs=2:shortest=1[v]','-map','[v]','-an','-c:v','libx264','-preset','fast','-crf','18','-pix_fmt','yuv420p','-movflags','+faststart',str(dest)]
        subprocess.run(args,check=True)
    probe=subprocess.check_output([str(BIN/'ffprobe.exe'),'-v','error','-count_frames','-show_entries','stream=width,height,nb_read_frames,r_frame_rate,duration','-of','json',str(dest)],text=True)
    records.append(dict(file=name,sources=[str(p) for p in paths],operation='before on left, after on right; 30fps comparison transcode; independent native originals also delivered',probe=json.loads(probe),sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))

pair_image('rk_uv_auto_smoke','rk_release_auto_smoke','sheet_before_after.png',('Before / same Auto / grazing fade 0.035','After / same Auto / Weather grazing fade 8 degrees'))
pair_image('rk_pair_mid_legacy','rk_release_fixed_smoke','cloud_algorithm_comparison.png',('Old formula reference in the same normal world','New cache / midpoint integration / same fixed display'))
pair_image('rk_hdr_before','rk_hdr_after','display_highlight_order.png',('Before / linear 1.5 clipped before Auto','After / preserve linear 1.5 until Auto mapping'))
pair_video('rk_uv_auto_smoke','rk_release_auto_smoke','sheet_before_after.mp4')
pair_video('rk_pair_mid_legacy','rk_release_fixed_smoke','cloud_algorithm_comparison.mp4')
pair_video('rk_normal_asset60','rk_release_normal_asset','normal_asset_before_after.mp4')
(DEST/'manifest.json').write_text(json.dumps(records,indent=2,ensure_ascii=False)+'\n',encoding='utf8')
print('Delivery artifacts:',len(records))
