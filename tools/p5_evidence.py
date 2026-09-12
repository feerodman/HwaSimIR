"""Summarize measured P5 captures and logs; never substitutes FPS for image QA."""
from pathlib import Path
import json, re
import numpy as np
from PIL import Image, ImageDraw

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'logs/p5'

def load(path): return np.asarray(Image.open(path).convert('RGB')).astype(np.int16)

def montage(paths, dest, columns=3):
    paths=[(label,path) for label,path in paths if path.exists()]
    if not paths:return
    w,h=400,430
    canvas=Image.new('RGB',(columns*w,((len(paths)+columns-1)//columns)*h),'#16191d')
    draw=ImageDraw.Draw(canvas)
    for i,(label,path) in enumerate(paths):
        x,y=(i%columns)*w,(i//columns)*h
        im=Image.open(path).convert('RGB');im.thumbnail((400,400))
        canvas.paste(im,(x,y));draw.text((x+8,y+404),label,fill='white')
    canvas.save(dest)

results={}
for name in ['f22','aim120','aim9x']:
    paths=[(name+' / '+label,OUT/f'win_{name}_{suffix}'/'received.png') for suffix,label in
           [('v3','base texture'),('v1','UV'),('v2','normal'),('v6','material ID'),('band1','NIR'),('band2','MWIR'),('lookupA','iron reflectance 0.2'),('lookupB','iron reflectance 0.4')]]
    day=OUT/f'win_{name}_nir_day/received.png'
    if day.exists():paths[4]=(name+' / NIR daytime',day)
    montage(paths,OUT/f'{name}_views.png',4)
    pa,pb=OUT/f'win_{name}_lookupA/gpu_rgb8.png',OUT/f'win_{name}_lookupB/gpu_rgb8.png'
    if pa.exists() and pb.exists():
        a,b=load(pa),load(pb)
        # Reflectance diagnostic is exact grayscale before video encoding.
        mask=(a[:,:,0]==51)&(a[:,:,1]==51)&(a[:,:,2]==51)
        roi=np.zeros(mask.shape,bool);roi[250:580,140:640]=True
        mask &= roi
        changed=np.max(abs(a-b),axis=2)>3
        results[name]=dict(selected_pixels=int(mask.sum()),before_median=float(np.median(a[mask])),
            after_median=float(np.median(b[mask])),changed_selected_pixels=int((changed&mask).sum()),
            changed_outside_selected_in_roi=int((changed&~mask&roi).sum()),note='ROI includes background; inspect ID capture before attributing every change')
    pa,pb=OUT/f'rk_{name}_lookupA/gpu_rgb8.png',OUT/f'rk_{name}_lookupB/gpu_rgb8.png'
    if pa.exists() and pb.exists():
        a,b=load(pa),load(pb)
        mask=np.all(a==51,axis=2)
        roi=np.zeros(mask.shape,bool);roi[250:580,140:640]=True
        mask &= roi
        changed=np.max(abs(a-b),axis=2)>3
        results[f'rk_{name}']=dict(selected_pixels=int(mask.sum()),before_median=float(np.median(a[mask])),
            after_median=float(np.median(b[mask])),changed_selected_pixels=int((changed&mask).sum()),
            changed_outside_selected_in_roi=int((changed&~mask&roi).sum()),note='Mali RGB8 render texture before MPP; artificial iron reflectance A/B')

for name in ['win_linear_blend','win_gamma2_blend','win_gamma2_novol_fixed','rk_release_gamma2_blend']:
    path=OUT/name/'gpu_rgb8.png'
    if path.exists():results[name]=dict(center_rgb_median=np.median(load(path)[340:460,340:460],axis=(0,1)).tolist())

cases={}
for folder in sorted(OUT.iterdir()):
    if not folder.is_dir():continue
    board=folder/'board.log'
    if not board.exists():continue
    log=board.read_text(encoding='utf-8',errors='replace')
    video=(folder/'video.err.log').read_text(encoding='utf-8',errors='replace') if (folder/'video.err.log').exists() else ''
    conservation=re.findall(r'\[SyncRoundConservation\][^\n]+',log)
    frames=re.findall(r'\[DdsVideoReceiverPerf\][^\n]+',video)
    perf=[dict(re.findall(r'(\w+)=([^\s]+)',line)) for line in log.splitlines() if line.startswith('[Perf]') and 'mode=sync' in line]
    active_perf=[p for p in perf if float(p.get('udpFps','0'))>0]
    # Boundary windows include waiting for INIT/STOP and are not a steady rate.
    # Preserve the complete run logs and label the fixed boundary exclusion.
    steady_perf=active_perf[1:-1] if len(active_perf)>2 else active_perf
    timing={}
    for key in ['renderMs','sceneUpdateMs','irUpdateMs','stage7SkyGroundMs','plumeUpdateMs','readbackMs','renderFps','udpFps']:
        vals=[float(p[key]) for p in steady_perf if key in p]
        all_vals=[float(p[key]) for p in active_perf if key in p]
        if vals:timing[key]=dict(window_mean=float(np.mean(vals)),window_min=min(vals),window_max=max(vals),windows=len(vals),all_active_window_mean=float(np.mean(all_vals)),scope='first and last active interval omitted consistently for INIT/STOP boundaries')
    output_samples=[dict(re.findall(r'(\w+)=([^\s]+)',line)) for line in log.splitlines() if line.startswith('[VideoOutputPerf]')]
    for key in ['bgrToNv12Ms','mppEncodeMs','ddsPublishCallMs','ddsBackpressureMs','frameTotalMs']:
        vals=[float(p[key]) for p in output_samples if key in p]
        if vals:timing[key]=dict(sample_mean=float(np.mean(vals)),sample_min=min(vals),sample_max=max(vals),samples=len(vals),scope='sparse logged frames, not every frame')
    def final_fields(tag,text):
        matches=re.findall(r'\['+tag+r'\][^\n]+',text)
        return dict(re.findall(r'(\w+)=([^\s]+)',matches[-1])) if matches else {}
    counts=dict(conservation=final_fields('SyncRoundConservation',log),sender=final_fields('DdsVideoPerf',log),
                products=final_fields('DdsFrameProductsFinal',log),receiver=final_fields('DdsVideoReceiverPerf',video),
                frame_sync=final_fields('DdsFrameSync',video))
    expected=[counts[group].get(key) for group,key in [('conservation','acceptedRealtime'),('conservation','lastCapturedSourceSeq'),
        ('sender','sentSamples'),('products','videoMeta'),('products','annotation'),('receiver','receivedSamples'),
        ('frame_sync','video'),('frame_sync','meta'),('frame_sync','annotation')]]
    zero_fields=[counts[group].get(key) for group,key in [('conservation','inputMinusCaptured'),('conservation','queueDepth'),
        ('conservation','staleFramePublished'),('sender','writeErrors'),('sender','droppedSamples'),('products','writeErrors'),
        ('receiver','ddsErrors'),('frame_sync','mismatch'),('frame_sync','pendingMeta'),('frame_sync','pendingAnnotation')]]
    errors=[s for s in log.splitlines() if '0x506' in s or ':display(error)' in s or 'gsg(error)' in s or 'Shader input' in s]
    rounds=[dict(re.findall(r'(\w+)=([^\s]+)',line)) for line in conservation]
    round_total=sum(int(p.get('acceptedRealtime','0')) for p in rounds)
    multi_ok=(len(rounds)>1 and not errors and all(x=='0' for x in zero_fields)
        and all(p.get('acceptedRealtime')==p.get('lastCapturedSourceSeq') and p.get('inputMinusCaptured')=='0'
            and p.get('queueDepth')=='0' and p.get('staleFramePublished')=='0' for p in rounds)
        and counts['sender'].get('sentSamples')==str(round_total) and counts['receiver'].get('receivedSamples')==str(round_total)
        and counts['frame_sync'].get('video')==counts['conservation'].get('acceptedRealtime')
        and counts['frame_sync'].get('video')==counts['frame_sync'].get('meta')==counts['frame_sync'].get('annotation'))
    cases[folder.name]=dict(conservation=conservation[-1:] or ['MISSING'],receiver_final=frames[-1:] or ['MISSING'],counts=counts,
        single_round_count_pass=bool(len(rounds)==1 and expected[0] and len(set(expected))==1 and all(x=='0' for x in zero_fields) and not errors),
        multiple_init_count_pass=multi_ok,accepted_across_inits=round_total,
        conservation_all=conservation,
        visible_volumes_max=max([int(x) for x in re.findall(r'visibleCloudVolumes=(\d+)',log)] or [0]),
        graphics_errors=errors,
        input_queue_peak=max([int(x) for x in re.findall(r'inputQueueDepthMax=(\d+)',log)] or [0]),
        source_lag_peak=max([int(x) for x in re.findall(r'sourceSeqLag=(\d+)',log)] or [0]),
        receiver_decode_errors_max=max([int(x) for x in re.findall(r'h264DecodeErrors=(\d+)',video)] or [0]),
        timing=timing,timing_scope='interval averages, not per-frame GPU timings or P95')
(OUT/'evidence.json').write_text(json.dumps(dict(pixels=results,board_cases=cases),indent=2),encoding='utf8')
montage([(f'{era} / {view}',OUT/f'win_plume_{era}_{view}/received.png')
    for era in ['old','new'] for view in ['end','side','oblique']],OUT/'plume_comparison.png',3)
montage([(view,OUT/f'win_plume_new_{view}/received.png')
    for view in ['end','side','oblique','near','far','occluded']],OUT/'plume_views.png',3)
montage([(view,OUT/f'win_cloud_{view}/received.png')
    for view in ['below','above','parallax','occluded','behind','sheet_only','volume_only']],OUT/'cloud_views.png',4)
montage([(f'RK / {name} / iron {case}',OUT/f'rk_{name}_lookup{case}/received.png')
    for name in ['f22','aim120','aim9x'] for case in ['A','B']],OUT/'rk_material_pixels.png',2)
print(json.dumps(dict(pixels=results,board_case_count=len(cases)),indent=2))
