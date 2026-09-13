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
OUT = ROOT / 'logs/p6b'
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
    updates=[x for x in entries('Stage6 AGC',log) if x.get('effective')=='1' and int(x.get('sourceSeq',0))>2*req['rate'] and float(x.get('stage6AgcStatsMs',0))>0]
    # The update and the frame diagnostic both log the same source sequence.
    updates=list({(x['sourceSeq'],x['stage6AgcStatsMs']):x for x in updates}.values())
    durations=numbers(updates,'stage6AgcStatsMs')
    if durations:timing['agc_update_cpu_read_and_histogram_ms']=dict(mean=float(np.mean(durations)),median=float(np.median(durations)),maximum=max(durations),updates=len(durations),scope='per statistics update: extraction, PFM conversion, CPU histogram; GPU sampling draw is in render timing')
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
    if all((folder/n).exists() for n in ['linear.pfm','linear_rgb8.png','received.png']):
        raw=pfm(folder/'linear.pfm'); g=rgb(folder/'linear_rgb8.png'); d=rgb(folder/'received.png')
        raw=raw[-800:,:800]
        # Values are read from the unique effective-source logs, not assumed from request.
        selected={x['field']:float(x['value']) for x in entries('DisplayEffective',log) if 'field' in x and re.fullmatch(r'[0-9.+-]+',x.get('value',''))}
        # WARN follows DisplayEffective for explicit overrides.
        selected.update({x['field']:float(x['value']) for x in [fields(y) for y in log.splitlines() if '[DisplayEffective][WARN]' in y] if 'field' in x})
        gain, offset, gamma = selected.get('Gain',1), selected.get('OffsetGray',0)/255, selected.get('Gamma',1)
        mapping=last('DisplayFrameMapping',log)
        before_agc=raw*gain+offset
        if mapping.get('hdrUntilAgc')!='1':before_agc=np.clip(before_agc,0,1)
        agc_gain,agc_offset=float(mapping.get('agcGain',1)),float(mapping.get('agcOffset',0))
        pred=(np.clip(before_agc*agc_gain+agc_offset,0,1) if selected.get('Automatic',0) else np.clip(before_agc,0,1))**(1/gamma)
        if not selected.get('WhiteHot',1):pred=1-pred
        monotone=g[80,:,0]
        signs=np.diff(monotone)*(1 if selected.get('WhiteHot',1) else -1)
        displays[folder.name]=dict(raw_shape=list(raw.shape), raw_min=float(raw.min()),raw_max=float(raw.max()),
            effective=selected, prediction_mae_gray8=float(np.abs(pred*255-g).mean()), prediction_max_gray8=float(np.abs(pred*255-g).max()), mapping=mapping,
            decoded_mae_gray8=float(np.abs(d-g).mean()), decoded_error_p99=float(np.percentile(np.abs(d-g),99)),
            fixed_monotone_violations=(int((signs<0).sum()) if req.get('scene')=='display' and not req.get('normal') else None),
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

# Compare the GPU samples with exact source pixel centers, and their quantized
# percentiles with a full offline histogram. This is sampling, not area averaging.
statistics={}
for folder in sorted(OUT.iterdir()):
    if not (folder/'linear_stats.pfm').exists() or not (folder/'linear.pfm').exists():continue
    raw=pfm(folder/'linear.pfm')[-800:,:800];small=pfm(folder/'linear_stats.pfm')
    n=small.shape[0];index=np.floor((np.arange(n)+.5)*800/n).astype(int)
    expected=raw[np.ix_(index,index)]
    error=np.abs(small-expected)
    def quantiles(a):
        a=np.clip(a.mean(axis=2),0,1)
        bins=np.clip(np.floor(a*255+.5),0,255).astype(int).ravel();bins.sort()
        return [float(bins[int(np.floor(p/100*(len(bins)-1)+.5))])/255 for p in [2,98]]
    fullq,smallq=quantiles(raw),quantiles(small)
    statistics[folder.name]=dict(size=n,sample_mae=float(error.mean()),sample_max_error=float(error.max()),full_histogram_quantiles=fullq,sample_histogram_quantiles=smallq,percentile_error=[smallq[i]-fullq[i] for i in range(2)],method='one nearest pixel per equal-area stratum; 256-bin histogram on clipped public display domain; not a mean-downsample percentile')
contribution_history={}
for label,base_name,hide_prefix in [('initial_midpoint','rk_pair2_fixed60','rk_pair2_hide_'),('release','rk_release_fixed_smoke','rk_release_hide_')]:
    base=OUT/base_name;contributions={}
    if not (base/'linear_rgb8.png').exists():continue
    for which in ['near','far']:
        other=OUT/(hide_prefix+which)
        if not (other/'linear_rgb8.png').exists():continue
        changes={}
        for kind in ['linear_rgb8.png','received.png']:
            a,b=rgb(base/kind),rgb(other/kind);diff=np.abs(a-b).max(axis=2)
            changes[kind]=dict(changed_pixels_ge2=int((diff>=2).sum()),fraction_ge2=float(np.mean(diff>=2)),mae=float(np.abs(a-b).mean()),max=float(diff.max()))
        contributions[which]=changes
    contribution_history[label]=dict(base=base_name,hide_prefix=hide_prefix,changes=contributions)
contributions=contribution_history.get('release',contribution_history.get('initial_midpoint',{})).get('changes',{})
montage([('RK / P6A reconstruction and phase','rk_pair_mid_legacy'),('RK / first P6B phase (rejected)','rk_pair_mid_smoke'),('RK / midpoint and smooth world horizon','rk_pair2_fixed60')],'cloud_stripe_comparison.png')
(OUT/'evidence.json').write_text(json.dumps(dict(cases=cases,display=displays,statistics=statistics,cloud_contributions=contributions,cloud_contribution_history=contribution_history,videos=videos),indent=2),encoding='utf8')
print(json.dumps(dict(cases=len(cases),conserved=[k for k,v in cases.items() if v['conservation_pass']],statistics=statistics,contributions=contributions),indent=2))
