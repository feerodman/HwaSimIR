"""Build a local evidence gallery and losslessly remuxed, fully indexed clips."""
import argparse,csv,hashlib,html,json,shutil,subprocess
from pathlib import Path
from p7_validate_recording import product,validate
from p7_validate_conservation import audit

def identity(row):
    return tuple(str(row[k]) for k in ('session','platID','sensorID','channel','generation','run','round','frameSeq'))

def indexed_clip(case,dest,ffmpeg,start=0,seconds=8):
    source=next(iter(sorted((case/'recording').glob('*/output.mp4'))))
    original=[json.loads(s) for s in (source.parent/'frame_index.jsonl').read_text(encoding='utf-8-sig').splitlines()]
    by_key={identity(row):row for row in original}
    bodies=(source.parent/'producer_annotations.jsonl').read_bytes()
    dest.mkdir(parents=True,exist_ok=True)
    movie=dest/'output.mp4'
    subprocess.run([str(ffmpeg),'-v','error','-y','-ss',str(start),'-i',str(source),'-t',str(seconds),
        '-map','0:v:0','-c','copy','-avoid_negative_ts','make_zero',str(movie)],check=True)
    probe=ffmpeg.with_name('ffprobe.exe')
    info=json.loads(subprocess.check_output([str(probe),'-v','error','-select_streams','v:0','-show_packets','-show_streams','-of','json',str(movie)]))
    length=int(info['streams'][0].get('nal_length_size',4))
    index=[]
    with movie.open('rb') as video,(dest/'producer_annotations.jsonl').open('wb') as text:
        for ordinal,packet in enumerate(info['packets'],1):
            video.seek(int(packet['pos']));sample=video.read(int(packet['size']));offset=0;products=[]
            while offset<len(sample):
                count=int.from_bytes(sample[offset:offset+length],'big');offset+=length
                products.extend(product(sample[offset:offset+count]));offset+=count
            assert len(products)==1
            row=dict(by_key[identity(products[0])]);original_index=row['storageIndex']
            body=bodies[int(row['annotationBodyOffset']):int(row['annotationBodyOffset'])+int(row['annotationBodyBytes'])]
            assert body==products[0]['body']
            row.update(originalStorageIndex=original_index,storageIndex=ordinal,annotationBodyOffset=text.tell(),
                mp4PtsUs=round(float(packet['pts_time'])*1e6),sourceMovie=str(source.resolve()),derivedPreview=True)
            text.write(body+b'\n');index.append(row)
    (dest/'frame_index.jsonl').write_text(''.join(json.dumps(r,ensure_ascii=False,separators=(',',':'))+'\n' for r in index),encoding='utf-8')
    (dest/'provenance.json').write_text(json.dumps(dict(source=str(source.resolve()),sourceSha256=hashlib.sha256(source.read_bytes()).hexdigest(),
        operation='H.264 lossless remux; original SEI and annotation bytes preserved; only container PTS and storage index rebased',
        requestedStartSeconds=start,requestedDurationSeconds=seconds,firstProductionFrame=index[0]['frameSeq'],lastProductionFrame=index[-1]['frameSeq']),indent=2),encoding='utf-8')
    validate(dest,ffmpeg)
    return dest

def main(root,ffmpeg):
    dest=root/'delivery';dest.mkdir(exist_ok=True)
    selected=[('nvg_rain','D_C4_nvg_rain600',20,8,'RK3588 / NVG Auto / 双云、雨、普通游戏资源和尾焰'),
        ('mwir_snow','D_C4_mwir_snow600',20,8,'RK3588 / MWIR Game / 双云、雪、普通游戏资源和尾焰'),
        ('vfx_before','D_vfx_side_legacy',0,6,'RK3588 / 普通喷口 / Legacy / Game固定显示'),
        ('vfx_after','D_vfx_side',0,6,'RK3588 / 普通喷口 / Enhanced / 同一显示'),
        ('rain_before','D_rain_geometry_before',0,8,'RK3588 / 旧雨线几何 / 冻结映射'),
        ('rain_after','D_rain_geometry_after',0,8,'RK3588 / C4细雨线 / 同一冻结映射'),
        ('cloud_before','C3_cloud_before_band1',0,10,'Windows / 旧云缓存 / 普通世界同输入冻结显示'),
        ('cloud_after','C3_cloud_after_band1',0,10,'Windows / P7云缓存 / 同输入冻结显示'),
        ('cloud_mwir_before','C3_cloud_before_band2',0,10,'Windows / MWIR旧云缓存 / 冻结显示'),
        ('cloud_mwir_after','C3_cloud_after_band2',0,10,'Windows / MWIR新云缓存 / 同输入冻结显示')]
    figures=[];clip_records=[]
    for name,case_name,start,seconds,title in selected:
        case=root/case_name
        if not case.exists():raise RuntimeError('Required evidence missing '+str(case))
        clip=indexed_clip(case,dest/'clips'/name,ffmpeg,start,seconds)
        raw=case/'received.png'
        if raw.exists():shutil.copy2(raw,dest/(name+'.png'))
        figures.append(f'<figure><figcaption>{html.escape(title)}</figcaption><a href="{name}.png"><img src="{name}.png"></a><video controls preload="metadata" src="clips/{name}/output.mp4"></video><p><a href="clips/{name}/all_frame_correspondence.csv">短片逐帧对应表</a> · <a href="../{case_name}/">完整原始案例</a></p></figure>')
        clip_records.append(dict(name=name,case=case_name,title=title,**json.loads((clip/'validation.json').read_text())))
    for name,source,title in [('receiver_ui',root/'A_ui_1080_qt125_fullscreen/receiver_ui.png','实际1920×1080全屏，Qt缩放1.25；不是4K'),
        ('sender_ui',root/'A_board_fields_2init_abs/sender_ui.png','实际发送器27字段控件，非默认值回读试验')]:
        shutil.copy2(source,dest/(name+'.png'))
        figures.insert(0,f'<figure><figcaption>{title}</figcaption><a href="{name}.png"><img src="{name}.png"></a></figure>')
    (dest/'clips.json').write_text(json.dumps(clip_records,ensure_ascii=False,indent=2),encoding='utf-8')
    rows=[]
    for case in sorted(root.iterdir()):
        if not case.is_dir() or not (case/'metrics.json').exists():continue
        data=json.loads((case/'metrics.json').read_text(encoding='utf-8-sig'))
        conservation=audit(case)
        ingress=data['counters'].get('RealtimeIngress',{})
        latency=data['timingMs']['estimatedOutputLatencyMs']
        for r in data.get('observedRates',[]):
            verified=case/'recording'/r['recording']/'validation.json'
            result=json.loads(verified.read_text()) if verified.exists() else {}
            rows.append(dict(case=case.name,recording=r['recording'],frames=r['frames'],receivedNewImageFps=r['rates'].get('decodedNewImageFps',''),
                upstreamConservation=conservation['result'],writerCounts=str(conservation['successfulWriterCounts']),boardAcceptedCounts=str(conservation['boardAcceptedCounts']),
                inputBackpressureCount=ingress.get('inputBackpressureCount',''),cumulativeInputBackpressureMs=ingress.get('inputBackpressureWaitMs',''),
                maximumInputBackpressureMs=ingress.get('maxInputBackpressureWaitMs',''),maxInputQueue=ingress.get('maxQueueDepth',''),
                latencyEstimatedSamples=latency['count'],estimatedLatencyP99Ms=latency.get('p99',''),estimatedLatencyMaxMs=latency.get('maximum',''),
                fullFileValidation=result.get('result','NOT_VERIFIED'),movieSha256=result.get('movieSha256','')))
    with (dest/'performance_and_files.csv').open('w',newline='',encoding='utf-8-sig') as f:
        writer=csv.DictWriter(f,fieldnames=rows[0]);writer.writeheader();writer.writerows(rows)
    text='''<!doctype html><meta charset="utf-8"><title>P7 实际接收证据</title>
<style>body{max-width:1400px;margin:30px auto;padding:0 24px;font:16px/1.6 system-ui;background:#101922;color:#e9eff4}a{color:#87cbff}.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:24px}figure{margin:0;background:#1c2935;padding:16px}img,video{display:block;width:100%;height:auto;margin-top:12px}figcaption{font-weight:600}@media(max-width:850px){.grid{grid-template-columns:1fr}}</style>
<h1>P7 实际接收图、短片与逐帧文件</h1>
<p>原图无标注；视频来自实际接收 MP4 的无损封装选段。每段短片另有按原生产身份生成并独立验证的正文、索引及对应表。完整录像和原始计时仍保留在各案例目录。普通喷口与冻结显示对照为明确测试入口；正常世界场景与其分开列出。</p>
<p>30 Hz 双云雨/雪各10分钟已运行并逐帧验证18000帧；60 Hz组合存在持续板端排队，NVG60秒轮还发生上游TCP重连后少182条输入。该轮已到达的MP4/正文/索引正确，但整条输入链路失败。云近景偏软、雪雾低对比仍是图像限制。4K物理实屏未测。</p>
<p><a href="performance_and_files.csv">性能及完整文件结果CSV</a> · <a href="../visual_pixel_validation.json">逐云/降水像素影响</a> · <a href="../../../docs/HwaSimIR_P7_UI_Telemetry_Recording_Weather_Closeout.md">完整实施报告</a></p><div class="grid">'''
    (dest/'index.html').write_text(text+'\n'.join(figures)+'</div>',encoding='utf-8')
    print(dest/'index.html')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path('logs/p7'));p.add_argument('--ffmpeg',type=Path,required=True);a=p.parse_args();main(a.root,a.ffmpeg)
