"""Build offline P17 evidence. Copies real media; no image enhancement or inferred frame IDs.

Harness PASS means acquisition completed, never visual/performance acceptance.
The original full MP4 and its per-frame identity are always retained.
"""
import argparse, hashlib, html, json, pathlib, re, shutil, subprocess
import numpy as np

ROOT=pathlib.Path(__file__).resolve().parents[1]

def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(1024*1024),b''): h.update(block)
    return h.hexdigest()

def run(args):
    return subprocess.check_output(args,encoding='utf-8',errors='replace',stderr=subprocess.STDOUT)

def read_json(path): return json.loads(path.read_text(encoding='utf-8-sig'))
def save_json(path,value): path.write_text(json.dumps(value,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
def percentile(v): return {f'p{p}':float(np.percentile(v,p)) for p in (50,95,99)} if len(v) else None

def read_pfm(p):
    with p.open('rb') as f:
        magic=f.readline().strip(); w,h=map(int,f.readline().split()); scale=float(f.readline())
        a=np.fromfile(f,dtype='<f4' if scale<0 else '>f4')
    return a.reshape(h,w,3 if magic==b'PF' else 1) # writer uses top-to-bottom rows

def inspect_case(src,dest,make_excerpt=True):
    dest.mkdir(parents=True,exist_ok=True)
    plan=read_json(src/'case_plan.json')
    result=read_json(src/'case_result.json') if (src/'case_result.json').exists() else None
    if not result or result.get('result')!='PASS':
        raise RuntimeError('Acquisition incomplete or failed; not copied as a validated production movie')
    movies=list(src.glob('recording/*/output.mp4'))
    indices=list(src.glob('recording/*/frame_index.jsonl'))
    if len(movies)!=1 or len(indices)!=1: return None
    movie,index=movies[0],indices[0]
    frames=[json.loads(s) for s in index.read_text(encoding='utf-8-sig').splitlines()]
    if not frames: return None
    identity={(r['session'],str(r['generation']),str(r['run'])) for r in frames}
    if len(identity)!=1: raise RuntimeError(f'mixed sessions: {src}')
    for name in ('sourceSeq','frameSeq','storageIndex'):
        v=[int(r[name]) for r in frames]
        if v!=list(range(v[0],v[0]+len(v))): raise RuntimeError(f'noncontinuous {name}: {src}')
    for p in src.glob('*'):
        if p.is_file() and p.suffix in ('.json','.log','.ini','.txt','.png','.jpg'): shutil.copy2(p,dest/p.name)
    for p in movie.parent.glob('*'):
        if p.is_file() and p.suffix in ('.json','.jsonl','.txt'): shutil.copy2(p,dest/p.name)
    out=dest/'output.mp4'
    if not out.exists() or out.stat().st_size!=movie.stat().st_size: shutil.copy2(movie,out)
    probe=json.loads(run(['ffprobe','-v','error','-count_frames','-select_streams','v:0','-show_entries','stream=width,height,nb_read_frames,duration,avg_frame_rate','-of','json',str(out)]))['streams'][0]
    if int(probe['nb_read_frames'])!=len(frames) or (probe['width'],probe['height'])!=(800,800): raise RuntimeError(f'frame mismatch: {src}')
    run(['ffmpeg','-nostdin','-xerror','-v','error','-i',str(out),'-f','null','-'])
    picks=[]
    for seq in (900,1200,1800,2400,3600,4200):
        matches=[r for r in frames if int(r['sourceSeq'])==seq]
        if len(matches)!=1: continue
        row=matches[0]; n=int(row['storageIndex'])-1; name=f'sourceSeq_{seq}_decoded.png'
        picks.append(dict(file=name,**row))
    if picks:
        expression='+'.join(f'eq(n\\,{int(row["storageIndex"])-1})' for row in picks)
        run(['ffmpeg','-nostdin','-xerror','-v','error','-y','-i',str(out),'-vf',
             'select='+expression,'-vsync','0','-frames:v',str(len(picks)),str(dest/'selected_%03d.png')])
        for number,row in enumerate(picks,1):
            (dest/f'selected_{number:03d}.png').replace(dest/row['file'])
    start=next((r for r in frames if int(r['sourceSeq'])==900),None)
    if make_excerpt and start and len(frames)>=2400:
        run(['ffmpeg','-nostdin','-v','error','-y','-ss',str(int(start['mp4PtsUs'])/1e6),'-i',str(out),'-t','25','-an','-c:v','libx264','-crf','18','-preset','fast',str(dest/'review_25s.mp4')])
    raw=[]; roi=[]; seqs=[]
    rawdir=dest/'diagnostic_linear'; rawdir.mkdir(exist_ok=True)
    for pfm in sorted(src.glob('diagnostic_linear/*_seq*.pfm')):
        m=re.search(r'_seq(\d+)\.pfm$',pfm.name)
        if not m: continue
        seq=int(m[1]); a=read_pfm(pfm)
        match=[r for r in frames if int(r['sourceSeq'])==seq]
        if len(match)!=1: raise RuntimeError('ambiguous raw identity')
        roi.append(a[300:500,300:500].copy()); seqs.append(seq)
        raw.append(dict(sourceSeq=seq,sourcePfmSha256=sha(pfm),identity=match[0],min=float(a.min()),max=float(a.max()),finite=bool(np.isfinite(a).all())))
        if seq in (900,1200): shutil.copy2(pfm,rawdir/pfm.name)
        png=pfm.with_name(pfm.stem+'_rgb8.png')
        if png.exists(): shutil.copy2(png,rawdir/png.name)
    if roi:
        np.savez_compressed(rawdir/'raw_roi_float32.npz',sourceSeq=np.array(seqs),roi=np.array(roi,dtype=np.float32))
        save_json(rawdir/'identity.json',dict(roiXYXY=[300,300,500,500],rowOrder='top_to_bottom',domain='raw pre-Stage6 RGBA16F SI attachment converted to float32 RGB',frames=raw,performanceEvidence=False))
    warm=[r for r in frames if 601<=int(r['sourceSeq'])<=4200]
    perf=dict(eligible=bool(plan.get('performanceRun')) and not plan.get('diagnosticCapture') and len(warm)==3600,selection='sourceSeq 601..4200; 10 s warmup then approximately 60 s at requested 60 Hz',rawExport=bool(plan.get('diagnosticCapture')),rates={},latencyEstimated=True)
    if len(warm)>1:
        for name,key in [('accepted','acceptedSteadyNs'),('renderCompleted','captureSteadyNs'),('outputSubmitted','writerSubmitSteadyNs'),('decoded','decodeSteadyNs'),('guiSubmitted','guiSubmitSteadyNs')]:
            v=np.array([int(r[key]) for r in warm],dtype=np.int64)
            perf['rates'][name]=(len(v)-1)*1e9/float(v[-1]-v[0])
            perf[name+'IntervalMs']=percentile(np.diff(v)/1e6)
        perf['renderWorkMs']=percentile([(int(r['captureSteadyNs'])-int(r['executeSteadyNs']))/1e6 for r in warm])
        perf['renderWorkScope']='executeSteadyNs to captureSteadyNs; not a standalone GPU timer'
        perf['outputLatencyMs']=percentile([r['outputLatencyMs'] for r in warm])
        perf['clockUncertaintyMs']=percentile([r['clockUncertaintyMs'] for r in warm])
        perf['queueWaitMs']=percentile([r['queueWaitMs'] for r in warm])
        receiver=(src/'receiver.err.log').read_text(encoding='utf-8',errors='replace')
        paint=re.findall(r'\[GuiPaintPerf\] reason=shutdown[^\r\n]+',receiver)
        if paint:
            values=dict(re.findall(r'(\w+)=([^ ]+)',paint[-1]))
            perf['qtPaint']=values
            perf['qtPaintRateThreshold60Met']=float(values.get('steadyPaintFps',0))>=60
            perf['paintCoverageNote']='Qt Paint-event counter; last frame identity logged, no per-paint latency distribution. Steady paint excludes first 180 products, not the 601..4200 timing window.'
        board=(src/'board.log').read_text(encoding='utf-8',errors='replace')
        perf['runtimeEvidenceLines']=[line for line in board.splitlines() if any(tag in line for tag in ('[SyncRoundConservation]','[MppInputPath]','[CloudRenderCallAudit]'))]
        perf['sampledEffectCosts']=[line for line in board.splitlines() if '[ScenePerfProbe]' in line or '[RenderPerfProbe]' in line or '[Stage7 Perf]' in line]
        perf['allocationScope']='MPP staging preallocation logged; complete per-frame allocation tracing not instrumented'
        perf['newFrameIdentity']='continuous unique sourceSeq/frameSeq/storageIndex, no skipped products in saved index'
        perf['result']='NOT_ACCEPTED' # per-paint identity and full perf/lag audit remain mandatory
        perf['rateThreshold60Met']=all(v>=60 for v in perf['rates'].values())
        perf['latencyP99Threshold80Met']=perf['outputLatencyMs']['p99']<=80
        if perf['eligible'] and (not perf['rateThreshold60Met'] or not perf.get('qtPaintRateThreshold60Met',False) or not perf['latencyP99Threshold80Met']):perf['result']='FAIL'
    mode='legacy_NIR_with_M1_comparison_not_formal_SWIR_MWIR' if plan['band']=='NIR' else 'formal_M1_SWIR_MWIR' if plan['band'] in ('SWIR','MWIR') else 'unsupported_production_band'
    manifest=dict(caseId=plan['name'],entryType=plan.get('entryType','ordinary_renderer_cli_replay'),band=plan['band'],physicsMode=mode,weather=plan['weather'],engineStateOverride=plan.get('engineStateOverride','original_input'),inputSha256=plan['inputSha256'],pixelAngleUrad=plan.get('pixelAngleUrad',25),harnessResult=result.get('result') if result else 'INCOMPLETE',visualAcceptance='NOT_ACCEPTED_REQUIRES_REVIEW',calibration='NOT_VERIFIED_CALIBRATION',mp4=probe,frames=len(frames),frameIdentity=picks,performance=perf)
    save_json(dest/'evidence.json',manifest)
    return manifest

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--output',default='logs/p17/deliverables/HwaSimIR_P17');ap.add_argument('--cases',nargs='*');a=ap.parse_args()
    out=(ROOT/a.output).resolve();out.mkdir(parents=True,exist_ok=True)
    sources=[ROOT/p for p in a.cases] if a.cases else sorted((ROOT/'logs/p17/production').glob('*/*'))+sorted((ROOT/'logs/p17/diagnostics').glob('*/*'))
    cases=[]; errors=[]
    for src in sources:
        if not (src/'case_plan.json').exists(): continue
        kind='production' if 'production' in src.parts else 'diagnostics'
        dest=out/kind/src.name
        try:
            info=inspect_case(src,dest)
            if info: cases.append((dest.relative_to(out).as_posix(),info));print(src.name,'DECODE_VERIFIED',flush=True)
        except Exception as e: errors.append(dict(caseId=src.name,error=str(e))); print(src.name,'FAILED',str(e)[:150],flush=True)
    save_json(out/'validation.json',dict(cases=len(cases),errors=errors,calibration='NOT_VERIFIED_CALIBRATION'))
    content=['<!doctype html><meta charset="utf-8"><title>HwaSimIR P17 evidence</title><style>body{font:16px system-ui;margin:30px;background:#17202b;color:#eee}a{color:#99d7ff}article{padding:18px;margin:20px 0;background:#243242}img{width:240px;height:240px;object-fit:contain}video{width:480px;max-width:100%}code{overflow-wrap:anywhere}</style><h1>HwaSimIR P17 · 实测证据 / 未整体验收</h1><p>原始正常输出，无逐图拉伸。CLI 普通渲染回放与受控诊断分列。采集 PASS 不等于视觉、性能或标定通过。原视频完整保留；25 秒评审片为派生重编码。NOT_VERIFIED_CALIBRATION。</p><p><a href="validation.json">解码检查</a> · <a href="manifest.sha256">文件哈希</a></p>']
    content.append('<nav><a href="diagnostics/plume_comparison/index.html">尾焰几何 / 显示分离对照</a> · <a href="diagnostics/flicker/index.html">闪烁诊断（未修复验收）</a> · <a href="docs/HwaSimIR_P17_Closeout.md">Closeout</a> · <a href="docs/HwaSimIR_P17_Issue_Ledger.csv">未关闭项</a></nav>')
    content.append('<p>'+ ' | '.join(f'<a href="#{html.escape(info["caseId"])}">{html.escape(info["caseId"])}</a>' for rel,info in cases)+'</p>')
    for rel,info in cases:
        safe=html.escape(rel);content.append(f'<article id="{html.escape(info["caseId"])}"><h2>{html.escape(info["caseId"])}</h2><p>{html.escape(info["entryType"])} · {info["band"]} · {info["weather"]} · engine={info["engineStateOverride"]} · {info["pixelAngleUrad"]} µrad</p><p>{info["physicsMode"]}</p><a href="{safe}/evidence.json">身份 / 性能 / 配置状态</a> · <a href="{safe}/output.mp4">完整原视频</a><br>')
        if (out/rel/'review_25s.mp4').exists(): content.append(f'<video controls preload="none" src="{safe}/review_25s.mp4"></video><br>')
        for row in info['frameIdentity']: content.append(f'<a href="{safe}/{row["file"]}"><img loading="lazy" src="{safe}/{row["file"]}" title="sourceSeq={row["sourceSeq"]}; storageIndex={row["storageIndex"]}"></a>')
        content.append('</article>')
    if errors: content.append('<h2>未通过采集/解码的案例</h2><pre>'+html.escape(json.dumps(errors,ensure_ascii=False,indent=2))+'</pre>')
    (out/'index.html').write_text('\n'.join(content),encoding='utf-8')
    files=sorted(p for p in out.rglob('*') if p.is_file() and p.name!='manifest.sha256')
    (out/'manifest.sha256').write_text(''.join(sha(p)+'  '+p.relative_to(out).as_posix()+'\n' for p in files),encoding='utf-8')

if __name__=='__main__':main()
