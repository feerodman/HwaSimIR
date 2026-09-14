"""Separate measured stage intervals from one-second GUI FPS observations."""
import csv,json,re,statistics
from pathlib import Path
from p7_validate_conservation import read,audit
from p7_case_metrics import report

root=Path('logs/p7');rows=[]
for case in sorted(root.glob('D_cost_*')):
    if not case.is_dir():continue
    report(case);conservation=audit(case)
    metrics=json.loads((case/'metrics.json').read_text())
    video=read(case/'video.err.log')+read(case/'video.out.log')
    samples=[float(v) for v in re.findall(r'\[LiveReceivedFps\] fps=([\d.]+)',video) if float(v)>0]
    # Startup and final partial windows are deliberately excluded; this is not
    # a substitute for the exact saved-interval FPS when files are available.
    windows=samples[2:-2] if len(samples)>4 else []
    board=read(case/'board.log');perf=re.findall(r'\[VideoOutputPerf\] ([^\r\n]+)',board)
    parsed=[dict(re.findall(r'(\w+)=([^ ]+)',line)) for line in perf]
    row=dict(case=case.name,conservation=conservation['result'],inputCounts=str(conservation['successfulWriterCounts']),
        guiPositiveWindowMedianFps=statistics.median(windows) if windows else '',guiWindows=len(windows))
    for key in ['bgrToNv12Ms','mppEncodeMs','ddsPublishCallMs','frameTotalMs']:
        values=[float(p[key]) for p in parsed if key in p]
        row[key+'LoggedMedian']=statistics.median(values) if values else ''
    for key in ['inputExecutionToCaptureMs','outputQueueEncodeMs','receiverDecodeMs','guiQueueMs','guiSubmitMs']:
        row[key+'SavedMean']=metrics['timingMs'][key].get('mean','')
    rates=metrics['observedRates']
    row['exactSavedNewImageFps']=rates[0]['rates'].get('decodedNewImageFps','') if rates else ''
    row['cumulativeInputBackpressureMs']=metrics['counters'].get('RealtimeIngress',{}).get('inputBackpressureWaitMs','')
    row['emptyMeaning']='not measured; save-disabled cases have no per-file timings'
    rows.append(row)
with (root/'cost_isolation.csv').open('w',newline='',encoding='utf-8-sig') as f:
    writer=csv.DictWriter(f,fieldnames=rows[0]);writer.writeheader();writer.writerows(rows)
print(json.dumps(rows,indent=2))
