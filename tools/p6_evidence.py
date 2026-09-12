"""Measured P6 logs, same-frame linear/display/decoded values, and delivery media.

The PFM contains common-scaled scene values, not physical radiance. Its rows are
the Panda texture-store order, matching the PNG after cropping the valid viewport.
Timing samples are interval averages; no per-frame P95 or cross-host latency claim.
"""
from pathlib import Path
import json, re, subprocess
import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'logs/p6'
BIN = ROOT / '.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin'

def fields(line):
    return dict(re.findall(r'(\w+)=([^\s]+)', line))

def entries(tag, text):
    return [fields(x) for x in re.findall(r'\[' + tag + r'\][^\n]+', text)]

def last(tag, text):
    return (entries(tag, text) or [{}])[-1]

def read(path):
    return path.read_text(encoding='utf-8-sig', errors='replace') if path.exists() else ''

def rgb(path):
    return np.asarray(Image.open(path).convert('RGB'), dtype=np.float32)

def pfm(path):
    with path.open('rb') as f:
        assert f.readline().strip() == b'PF'
        w, h = map(int, f.readline().split())
        scale = float(f.readline())
        a = np.frombuffer(f.read(), '<f4' if scale < 0 else '>f4')
    return a.reshape(h, w, 3)

def montage(items, name, cols=3):
    items = [(label, OUT / case / 'received.png') for label, case in items
             if (OUT / case / 'received.png').exists()]
    if not items:
        return
    canvas = Image.new('RGB', (400 * cols, 435 * ((len(items)+cols-1)//cols)), '#16191d')
    draw = ImageDraw.Draw(canvas)
    for i, (label, path) in enumerate(items):
        x, y = 400*(i % cols), 435*(i//cols)
        im = Image.open(path).convert('RGB'); im.thumbnail((400, 400))
        canvas.paste(im, (x, y)); draw.text((x+8, y+407), label, fill='white')
    canvas.save(OUT / name)

cases, displays, videos = {}, {}, {}
for folder in sorted(OUT.iterdir()):
    if not folder.is_dir() or not (folder/'request.json').exists():
        continue
    req = json.loads(read(folder/'request.json'))
    board = read(folder/'board.log')
    log = board or (read(folder/'hwa.out.log') + read(folder/'hwa.err.log'))
    receiver = read(folder/'video.err.log')
    stim = read(folder/'stim.err.log') + read(folder/'stim2.err.log')
    rounds = entries('SyncRoundConservation', log)
    source = entries('StimFinal', stim)
    product_rounds = entries('DdsFrameProductsFinal', log)
    counts = dict(source=source, rounds=rounds, ingress=last('RealtimeIngress', log),
                  sender=last('DdsVideoPerf', log), products=last('DdsFrameProductsFinal', log),
                  receiver=last('DdsVideoReceiverPerf', receiver), association=last('DdsFrameSync', receiver),
                  products_per_round=product_rounds)
    rejected_timing = []
    def numbers(rows, key):
        values=[]
        for row in rows:
            if key not in row: continue
            value=row[key]
            if not re.fullmatch(r'[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?',value):
                rejected_timing.append(dict(field=key,raw=value,reason='malformed_or_interleaved_log'));continue
            parsed=float(value)
            if np.isfinite(parsed):values.append(parsed)
            else:rejected_timing.append(dict(field=key,raw=value,reason='non_finite'))
        return values
    total = sum(int(x.get('acceptedRealtime', 0)) for x in rounds)
    source_total = sum(int(x.get('successfulRealtimeWrites', 0)) for x in source)
    zero = [('ingress','inputQueueOverflow'), ('ingress','inputOverwritten'), ('ingress','sourceSeqGapCount'),
            ('ingress','currentQueueDepth'), ('products','writeErrors'), ('sender','writeErrors'), ('sender','droppedSamples'),
            ('receiver','ddsErrors'), ('association','mismatch'), ('association','pendingMeta'), ('association','pendingAnnotation')]
    errors = [x for x in log.splitlines() if re.search(r':(?:display[^ ]*|gsg|gobj)\(error\)|GL error 0x|\[(?:LinearReadback|P6LinearCapture|NozzleAttachment)\]\[ERROR\]', x)]
    transport_errors = [x for x in stim.splitlines() if '[StimDrain][ERROR]' in x or '[StimDDS][ERROR]' in x or 'DDS start failed' in x]
    association_history = entries('DdsFrameSync', receiver)
    conserved = bool(rounds and len(source)==len(rounds) and total==source_total and
        all(s.get('successfulRealtimeWrites')==r.get('acceptedRealtime') for s,r in zip(source,rounds)) and
        all(x.get('acceptedRealtime')==x.get('lastCapturedSourceSeq') and
            all(x.get(k)=='0' for k in ['inputMinusCaptured','queueDepth','staleFramePublished']) for x in rounds) and
        all(counts[g].get(k)=='0' for g,k in zero) and
        all(counts['ingress'].get(k)==rounds[-1].get('acceptedRealtime') for k in
            ['ddsRealtimeReceived','ddsRealtimeCallbackCount','appRealtimeQueued','appRealtimeConsumed']) and
        len(product_rounds)==len(rounds) and all(
            p.get('videoMeta')==p.get('annotation')==r.get('acceptedRealtime') and p.get('writeErrors')=='0'
            for p,r in zip(product_rounds,rounds)) and
        all(a.get('mismatch','0')=='0' for a in association_history) and
        counts['sender'].get('sentSamples')==counts['receiver'].get('receivedSamples')==str(total) and
        counts['association'].get('video')==counts['association'].get('meta')==counts['association'].get('annotation')==rounds[-1].get('acceptedRealtime') and not errors and not transport_errors)
    perf = [fields(x) for x in log.splitlines() if x.startswith('[Perf]') and 'mode=sync' in x]
    active = [x for x in perf if (numbers([x],'udpFps') or [0])[0]>0]
    steady = active[1:-1] if len(active)>2 else active
    timing = {}
    for key in ['renderMs','sceneUpdateMs','irUpdateMs','readbackMs','renderFps','udpFps','stage6AgcStatsMs']:
        a = numbers(steady,key)
        if a: timing[key] = dict(window_mean=float(np.mean(a)), window_min=min(a), window_max=max(a), windows=len(a))
    output = entries('VideoOutputPerf', log)
    for key in ['bgrToNv12Ms','mppEncodeMs','ddsPublishCallMs','frameTotalMs']:
        a = numbers(output,key)
        if a: timing[key] = dict(sparse_sample_mean=float(np.mean(a)), sample_max=max(a), samples=len(a))
    peak = lambda pattern: max([int(x) for x in re.findall(pattern, log)] or [0])
    cases[folder.name] = dict(request=req, counts=counts, conservation_pass=conserved,
        release=read(folder/'release.sha256'),
        stop_status=entries('StimStopStatus',read(folder/'stim.out.log')+read(folder/'stim2.out.log')),
        transport_errors=transport_errors,
        rejected_timing_samples=rejected_timing,
        ui_fps=entries('LiveReceivedFps',receiver),
        ui_captures=entries('ResponsiveUiCapture',receiver),
        nozzle_attachments=entries('NozzleAttachment',log),
        visible_max=peak(r'visibleCloudVolumes=(\d+)'), input_queue_peak=peak(r'inputQueueDepthMax=(\d+)'),
        source_lag_peak=peak(r'sourceSeqLag=(\d+)'), graphics_errors=errors[:10], timing=timing,
        timing_scope='interval averages with first/last active windows omitted, or explicitly sparse output samples; not GPU query/P95',
        cross_machine_latency='unmeasured: clocks not calibrated')
    if req['scene']=='display' and all((folder/n).exists() for n in ['linear.pfm','linear_rgb8.png','received.png']):
        raw=pfm(folder/'linear.pfm'); g=rgb(folder/'linear_rgb8.png'); d=rgb(folder/'received.png')
        raw=raw[-800:,:800]
        # Values are read from the unique effective-source logs, not assumed from request.
        selected={x['field']:float(x['value']) for x in entries('DisplayEffective',log) if 'field' in x}
        # WARN follows DisplayEffective for explicit overrides.
        selected.update({x['field']:float(x['value']) for x in [fields(y) for y in log.splitlines() if '[DisplayEffective][WARN]' in y] if 'field' in x})
        gain, offset, gamma = selected.get('Gain',1), selected.get('OffsetGray',0)/255, selected.get('Gamma',1)
        pred=np.clip(raw*gain+offset,0,1)**(1/gamma)
        if not selected.get('WhiteHot',1):pred=1-pred
        monotone=g[80,:,0]
        signs=np.diff(monotone)*(1 if selected.get('WhiteHot',1) else -1)
        displays[folder.name]=dict(raw_shape=list(raw.shape), raw_min=float(raw.min()),raw_max=float(raw.max()),
            effective=selected, fixed_prediction_mae_gray8=float(np.abs(pred*255-g).mean()) if not selected.get('Automatic',0) else None,
            decoded_mae_gray8=float(np.abs(d-g).mean()), decoded_error_p99=float(np.percentile(np.abs(d-g),99)),
            fixed_monotone_violations=int((signs<0).sum()),
            gray_samples=[dict(x=x,linear=float(raw[80,x,0]),gpu=int(g[80,x,0]),dds=int(d[80,x,0])) for x in [0,50,100,200,400,600,799]],
            units='common-scaled linear scene / pre-encode RGB8 / actual DDS decoded RGB8')
    bitstream=folder/'received.h264'; dest=folder/'received.mp4'
    if bitstream.exists() and bitstream.stat().st_size and folder.name.startswith('rk_'):
        if not dest.exists() or dest.stat().st_mtime<bitstream.stat().st_mtime:
            r=subprocess.run([str(BIN/'ffmpeg.exe'),'-v','warning','-y','-r',str(req['rate']),'-i',str(bitstream),'-c:v','copy','-movflags','+faststart',str(dest)],capture_output=True,text=True)
            (folder/'remux.log').write_text(r.stderr,encoding='utf8')
            if r.returncode:raise RuntimeError(folder.name)
        info=subprocess.check_output([str(BIN/'ffprobe.exe'),'-v','error','-count_frames','-select_streams','v:0','-show_entries','stream=codec_name,width,height,r_frame_rate,nb_read_frames,duration,color_range,color_space','-of','json',str(dest)],text=True)
        (folder/'video_probe.json').write_text(info,encoding='utf8');videos[folder.name]=json.loads(info)

contributions={}
for kind in ['linear_rgb8.png','received.png']:
    smoke=OUT/'rk_plume_smoke2_smoke'/kind; off=OUT/'rk_plume_smoke2_off'/kind
    if smoke.exists() and off.exists():
        delta=np.abs(rgb(smoke)-rgb(off)).mean(axis=2)
        contributions['smoke2_'+kind]=dict(changed_gt2=int((delta>2).sum()),mean_delta=float(delta.mean()),
            max_delta=float(delta.max()),scope='same final Config, ordinary smoke ON versus OFF; no target thermal change')
for name in ['rk_two_off1','rk_two_off2','rk_release_two_off1','rk_release_two_off2']:
    base=OUT/('rk_release_two_pixels/linear_rgb8.png' if 'release_' in name else 'rk_two_pixels/linear_rgb8.png'); off=OUT/name/'linear_rgb8.png'
    if base.exists() and off.exists():
        delta=np.abs(rgb(base)-rgb(off)).mean(axis=2); roi=np.zeros(delta.shape,bool);roi[160:440,:]=True
        contributions[name]=dict(changed_gt2=int(((delta>2)&roi).sum()), mean_delta=float(delta[roi].mean()),
            left_mean=float(delta[180:420,20:395].mean()),right_mean=float(delta[180:420,405:780].mean()),
            scope='Mali pre-encode RGB8, VFX disabled, identical positions; upper cloud ROI excludes sheets')

sheet_base=OUT/'rk_release_mixed_raw/linear_rgb8.png'
sheet_off=OUT/'rk_release_no_sheet/linear_rgb8.png'
if sheet_base.exists() and sheet_off.exists():
    delta=np.abs(rgb(sheet_base)-rgb(sheet_off)).mean(axis=2)[650:780,50:450]
    contributions['release_far_sheet']=dict(changed_gt2=int((delta>2).sum()),mean_delta=float(delta.mean()),max_delta=float(delta.max()),scope='bottom-left Mali RGB8 ROI; excludes local clouds and nozzle')

montage([(f'RK / {n} local clouds',f'rk_cloud{n}') for n in [1,2,4]],'rk_cloud_counts.png')
montage([(f'Final RK / {n} local clouds',f'rk_release_cloud{n}') for n in [1,2,4]],'rk_release_cloud_counts.png')
montage([(f'Final smoke2 RK / {n} local clouds',f'rk_release2_cloud{n}') for n in [1,2,4]],'rk_release2_cloud_counts.png')
montage([('RK / old environment art / Game display','rk_release_legacy_art'),('RK / Weather textures / Game display','rk_release2_cloud2')],'rk_environment_comparison.png',2)
montage([('RK / both clouds','rk_two_pixels'),('RK / cloud 1 OFF','rk_two_off1'),('RK / cloud 2 OFF','rk_two_off2')],'rk_cloud_contributors.png')
montage([('Final RK / both clouds','rk_release_two_pixels'),('Final RK / cloud 1 OFF','rk_release_two_off1'),('Final RK / cloud 2 OFF','rk_release_two_off2')],'rk_release_cloud_contributors.png')
montage([(f'RK / {v}',f'rk_cloud_{v}') for v in ['overlap','translate','entry','occluded','behind','large','sheets','volumes']],'rk_cloud_views.png',4)
montage([(f'RK / {era} / {v}',f'rk_plume_{era}_{v}') for era in ['before','after'] for v in ['end','side','oblique']],'rk_plume_comparison.png')
montage([(f'RK / {era} / {v}',f'rk_plume_{era}_{v}') for era in ['before','smoke2'] for v in ['end','side','oblique']],'rk_plume_smoke2_comparison.png')
montage([(f'RK / {v}',f'rk_plume_after_{v}') for v in ['near','far','occluded','orbit','glow','smoke']],'rk_plume_views.png')
montage([(f'RK / {v}',f'rk_display_{v}') for v in ['Legacy','Game','Auto','Black','gain05','NIR']],'rk_display_profiles.png')
montage([(f'RK / {asset} / {view}',f'rk_nozzle_{asset}_{view}') for asset in ['f35','f22','aim120','aim9x'] for view in ['end','side','oblique']],'rk_nozzle_attachment_views.png')
montage([('RK / old attachment','rk_nozzle_f35_before'),('RK / mesh nozzle attachment','rk_nozzle_f35_oblique')],'rk_nozzle_attachment_comparison.png',2)
(OUT/'evidence.json').write_text(json.dumps(dict(cases=cases,display=displays,cloud_contributions=contributions,videos=videos),indent=2),encoding='utf8')
print(json.dumps(dict(cases=len(cases),conserved=[k for k,v in cases.items() if v['conservation_pass']],display=displays,cloud_contributions=contributions),indent=2))
