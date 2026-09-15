"""Compact P9 evidence from actual files and monotonic per-input ledgers."""
import argparse,csv,json,re,statistics,subprocess,sys
from pathlib import Path
from p8_case_metrics import ordered_ledger,audit
from p8_validate_saved_cases import input_products
from p7_validate_recording import validate

ROOT=Path(__file__).resolve().parents[1]
FF=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe'
def dist(values):
    s=sorted(values)
    return dict(count=len(s),mean=statistics.mean(s),p99=s[min(len(s)-1,int(.99*len(s)))],maximum=s[-1]) if s else None
def collect(case):
    recordings=sorted((case/'recording').glob('*/output.mp4'))
    for movie in recordings:validate(movie.parent,FF,None)
    (case/'request.json').write_text(json.dumps(dict(simMode=1,rate=60,seconds=60,source='ordinary_UI_no_business_CLI')),encoding='utf-8')
    input_products(case)
    products=[json.loads(line) for m in recordings for line in (m.parent/'frame_index.jsonl').read_text().splitlines()]
    send=[r for r in ordered_ledger(case,'sender') if r['success']=='1']
    start=int(send[0]['beginNs']);stop=start+60_000_000_000
    new=[p for p in products if start<=int(p['decodeSteadyNs'])<stop]
    full=[float(p['outputLatencyMs']) for p in products if p.get('outputLatencyEstimated')]
    controls=list(csv.DictReader((case/'control_timing.csv').open(encoding='utf-8-sig')))
    log=(case/'board.log').read_text(encoding='utf-8',errors='replace')
    visible=[int(v) for v in re.findall(r'visibleCloudVolumes=(\d+)',log)]
    phases=[l for l in log.splitlines() if '[ControlStopPhase]' in l]
    metrics=[]
    for line in (case/'receiver.err.log').read_text(encoding='utf-8',errors='replace').splitlines():
        if '[RuntimeMetricsV2] ' in line:metrics.append(json.loads(line.split('[RuntimeMetricsV2] ',1)[1]))
    bycommand={}
    for m in metrics:
        if int(m.get('controlSequence','0')):bycommand.setdefault(m['controlCommand'],set()).add((m['controlSequence'],m['controlResponseMs']))
    hold=all(len(v)==1 for v in bycommand.values())
    stop_without_video=any(m.get('controlCommand')==3 and m.get('videoFps')==0 and m.get('available') for m in metrics)
    report=dict(inputAudit=audit(case),savedProducts=len(products),
        inputProductValidation=json.loads((case/'input_product_validation.json').read_text()),
        actualInputHz=(len(send)-1)*1e9/(int(send[-1]['beginNs'])-start),
        newImageFpsFirst60Seconds=len(new)/60,newImagesFirst60Seconds=len(new),warmupExcludedSeconds=0,
        decodedSize=sorted(set((p['width'],p['height']) for p in products)),
        controls=controls,controlResponseHeld=hold,stopVisibleWithoutVideo=stop_without_video,
        controlUnder10ms=all(r['valid']=='1' and float(r['responseMs'])<10 for r in controls),stopPhases=phases,
        latency=dict(kind='application_clock_estimate',valid=len(full),total=len(products),statsMs=dist(full),over80ms=sum(x>80 for x in full)),
        visibleCloudBudgetObservedMax=max(visible,default=0),cloudVisibilityLogs=visible,
        queueDepthMax=max(map(int,re.findall(r'maxQueueDepth=(\d+)',log)),default=0),
        inputBackpressureCountMax=max(map(int,re.findall(r'inputBackpressureCount=(\d+)',log)),default=0),
        layout=json.loads((case/'receiver_ui.png.layout.json').read_text()),
        limitations='Visibility selection is not individual-cloud pixel attribution; inspect the actual decoded pictures. No new P8 long-matrix claim.')
    (case/'p9_results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k not in ['cloudVisibilityLogs','layout']},ensure_ascii=False,indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('case',type=Path);collect(p.parse_args().case)
