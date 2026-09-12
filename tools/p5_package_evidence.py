"""Package actual DDS bitstreams and labeled copies; original PNG/H264 stays intact."""
from pathlib import Path
import json, subprocess
from PIL import Image, ImageDraw
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'logs/p5'
BIN=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin'

def montage(items,name,cols=3):
    items=[(label,OUT/case/'received.png') for label,case in items if (OUT/case/'received.png').exists()]
    if not items:return
    canvas=Image.new('RGB',(400*cols,430*((len(items)+cols-1)//cols)),'#16191d')
    draw=ImageDraw.Draw(canvas)
    for i,(label,path) in enumerate(items):
        x,y=400*(i%cols),430*(i//cols)
        im=Image.open(path).convert('RGB');im.thumbnail((400,400))
        canvas.paste(im,(x,y));draw.text((x+8,y+405),label,fill='white')
    canvas.save(OUT/name)

videos={}
for folder in sorted(OUT.iterdir()):
    if not folder.is_dir() or not (folder/'request.json').exists() or not (folder/'received.h264').exists():continue
    if not (folder.name.startswith('rk_') or folder.name.startswith('win_cloud_') or folder.name.startswith('win_plume_')):continue
    request=json.loads((folder/'request.json').read_text(encoding='utf-8-sig'))
    if (folder/'received.h264').stat().st_size==0:continue
    dest=folder/'received.mp4'
    if not dest.exists() or dest.stat().st_mtime < (folder/'received.h264').stat().st_mtime:
        cmd=[str(BIN/'ffmpeg.exe'),'-hide_banner','-loglevel','warning','-y','-r',str(request['rate']),'-i',str(folder/'received.h264'),'-c:v','copy','-movflags','+faststart',str(dest)]
        run=subprocess.run(cmd,capture_output=True,text=True)
        (folder/'remux.log').write_text(run.stderr,encoding='utf8')
        if run.returncode:raise RuntimeError(f'remux failed: {folder.name}')
    probe=subprocess.check_output([str(BIN/'ffprobe.exe'),'-v','error','-count_frames','-select_streams','v:0',
        '-show_entries','stream=codec_name,width,height,r_frame_rate,nb_read_frames,duration','-of','json',str(dest)],text=True)
    (folder/'video_probe.json').write_text(probe,encoding='utf8')
    videos[folder.name]=json.loads(probe)['streams']

montage([('RK / old representation','rk_legacy'),('RK / new mixed','rk_accept60'),('RK / volume OFF','rk_volumeoff')],'rk_comparison.png')
montage([('RK / new end','rk_plume_end'),('RK / cloud behind prop','rk_cloud_occluded'),
    ('RK / cloud in front of prop','rk_cloud_behind'),('RK / large preset','rk_cloud_large'),
    ('RK / small preset','rk_cloud_small'),('RK / final clouds + VFX / 600 s','rk_release_600s'),
    ('RK / far sheets only','rk_release_sheet'),('RK / local volume only','rk_cloud_volume_only')],'rk_views.png')

source=OUT/'rk_release_600s/received.png'
if not source.exists():source=OUT/'rk_receiver_final_60s/received.png'
if not source.exists():source=OUT/'rk_mixed_600s/received.png'
if not source.exists():source=OUT/'rk_accept60/received.png'
if source.exists():
    canvas=Image.new('RGB',(1160,800),'#16191d');canvas.paste(Image.open(source).convert('RGB'),(0,0))
    draw=ImageDraw.Draw(canvas)
    for box,color,label,y in [((245,290,565,485),'#51c8ed','Local volume / world-fixed',110),
        ((395,378,620,423),'#ffaa55','Soft sprites / fixed emission axis',250),
        ((621,370,689,430),'#e2e46a','Generated ordinary nozzle',390),
        ((2,530,798,798),'#9bdc94','Far cloud sheets / repeated mask',600)]:
        draw.rectangle(box,outline=color,width=2)
        draw.line([(box[2],(box[1]+box[3])//2),(805,y+5)],fill=color,width=1)
        draw.text((817,y),label,fill=color)
    draw.text((817,25),'RK3588 > MPP > DDS > Windows',fill='white')
    draw.text((817,48),'Diagnostic labels; original kept intact',fill='white')
    draw.text((817,710),'800 x 800 RGB8 decoded video',fill='white')
    draw.text((817,735),'No float-radiance claim',fill='white')
    canvas.save(OUT/'rk_mixed_annotated.png')

stats={}
on,off=OUT/'rk_accept60/received.png',OUT/'rk_volumeoff/received.png'
if on.exists() and off.exists():
    a=np.asarray(Image.open(on).convert('RGB'),dtype=np.int16)
    b=np.asarray(Image.open(off).convert('RGB'),dtype=np.int16)
    # Upper/lower parts of the cloud, excluding the animated plume strip.
    mask=np.zeros(a.shape[:2],bool);mask[300:380,250:560]=True;mask[430:490,250:560]=True
    delta=np.mean(np.abs(a-b),axis=2)
    stats['local_volume_on_off']=dict(roi_pixels=int(mask.sum()),changed_gt3=int(((delta>3)&mask).sum()),
        mean_absolute_delta=float(delta[mask].mean()),note='MPP RGB8 difference; exclusion avoids the moving plume; sheet fade also changes locally')
(OUT/'video_manifest.json').write_text(json.dumps(videos,indent=2),encoding='utf8')
(OUT/'image_comparison.json').write_text(json.dumps(stats,indent=2),encoding='utf8')
print(json.dumps(dict(videos=len(videos),comparisons=stats),indent=2))
