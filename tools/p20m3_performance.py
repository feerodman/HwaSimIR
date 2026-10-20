"""Measured N-1 rates and every-frame latency, independent of transport exit code."""
import argparse,csv,json,re,statistics
from pathlib import Path
R=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('name');a=ap.parse_args();p=R/'logs/p20/runs'/a.name
r=json.loads((p/'result.json').read_text(encoding='utf-8'));roles=[k for k in ('coarse','precise') if k in r]
def csvs(d,pat):return [row for f in sorted(d.glob(pat)) for row in csv.DictReader(f.open(encoding='utf-8-sig'))]
def lines(f):return [json.loads(v) for v in f.read_text(encoding='utf-8').splitlines()]
def stats(v):
 if not v:return None
 v=sorted(v);return dict(n=len(v),minimum=v[0],mean=statistics.mean(v),p50=v[int(.5*(len(v)-1))],p95=v[int(.95*(len(v)-1))],p99=v[int(.99*(len(v)-1))],maximum=v[-1],over80ms=sum(x>80 for x in v))
def rate(v):return (len(v)-1)*1e9/(v[-1]-v[0]) if len(v)>1 and v[-1]>v[0] else None
stim=csvs(p,'input_sender*.csv');report=dict(schema='P20M3.Performance.1',run=a.name,configuredHz=60,streams={},quality='ENGINEERING_APPROX',rateMethod='N-1 intervals, no rounding for acceptance',clock='Stim and receive/decode use the same Windows steady clock; board timestamps used only for board-local intervals')
for index,role in enumerate(roles):
 d=p/role;render=csvs(d/'board','stage_render*.csv');execute=csvs(d/'board','input_execute*.csv');output=csvs(d/'board','stage_output*.csv');rx=lines(d/'receive.jsonl');frames=[v for v in rx if v['kind']=='frame']
 own=stim[index::len(roles)];warm=600 if len(render)>=3600 else 0
 log=(d/'board/board.log').read_text(encoding='utf-8',errors='replace')
 lat=[];bad=[];matched=len(own)==len(execute)==len(frames) and [x['digestFNV1a64'] for x in own]==[x['digestFNV1a64'] for x in execute]
 if matched:
  lat=[(int(b['decodedNs'])-int(x['beginNs']))/1e6 for x,b in zip(own,frames)]
  bad=[dict(sourceSeq=i+1,latencyMs=v) for i,v in enumerate(lat) if v>80]
  if any(v<0 for v in lat):raise ValueError('Clock or ordering mismatch')
 queue=[int(x['remainingInputDepth']) for x in render];rt=[int(x['steadyNs']) for x in render]
 senderTimes=[int(x['beginNs']) for x in own]
 intervals=[(senderTimes[i]-senderTimes[i-1])/1e6 for i in range(1,len(senderTimes))]
 slope=0
 if len(queue)>warm+2:
  xs=[(v-rt[warm])/1e9 for v in rt[warm:]];ys=queue[warm:];xm=statistics.mean(xs);ym=statistics.mean(ys)
  slope=sum((x-xm)*(y-ym) for x,y in zip(xs,ys))/sum((x-xm)**2 for x in xs)
 s=dict(inputs=len(own),executed=len(execute),rendered=len(render),decoded=len(frames),saved=r[role].get('savedFramesVerified'),
  inputHz=rate([int(x['beginNs']) for x in own]),renderFps=rate(rt),decodedFps=rate([int(x['decodedNs']) for x in frames]),
  inputIntervalsMs=stats(intervals),inputGapsOver25ms=sum(x>25 for x in intervals),
  inputSpanSeconds=(senderTimes[-1]-senderTimes[0])/1e9 if senderTimes else None,
  maxDeadlineLatenessMs=max((t-senderTimes[0])/1e6-i*1000/60 for i,t in enumerate(senderTimes)) if senderTimes else None,
  stableRenderSpanSeconds=(rt[-1]-rt[warm])/1e9 if len(rt)>warm else None,
  stableRenderFps=rate(rt[warm:]),stableDecodeFps=rate([int(x['decodedNs']) for x in frames[warm:]]),warmupFrames=warm,
  sameClockLatencyVerified=matched,latencyAll=stats(lat),latencyStable=stats(lat[warm:]),over80msFrames=bad,
  queueMax=max(queue,default=0),queueEnd=queue[-1] if queue else None,queueSlopeFramesPerSec=slope,
  stages={k:stats([float(x[k]) for x in render[warm:]]) for k in ('executeToDrawBeginMs','doFrameMs','captureTaskMs')},
  outputStages={k:stats([float(x[k]) for x in output]) for k in ('readbackMs','convertMs','encodeMs','ddsEnqueueMs','ddsBackpressureMs','metaAnnotationMs','totalMs')},
  ingressToExecuteMs=stats([(int(x['endNs'])-int(x['beginNs']))/1e6 for x in execute]),
  ingressToRenderAgeMs=stats([(int(y['steadyNs'])-int(x['beginNs']))/1e6 for x,y in zip(execute,render)]) if len(execute)==len(render) else None,
  outputFrames=len(output),outputFps=rate([int(x['steadyNs']) for x in output]),
  stopTotalMs=[float(v) for v in re.findall(r'stopTotalMs=([\d.]+)',log)],
  hardwareGpu='glVendor=ARM glRenderer=Mali' in log,
  countConserved=matched,
  strict60fpsPass=False,latency80msPass=bool(lat) and max(lat)<=80)
 # No 58/59 -> 60 rounding. Exact measured result is preserved.
 s['strict60fpsPass']=bool(s['stableRenderFps'] and s['stableRenderFps']>=60 and s['stableDecodeFps']>=60 and matched and slope<=.01 and s['queueEnd']==0)
 report['streams'][role]=s
report['performancePass']=all(s['strict60fpsPass'] and s['latency80msPass'] for s in report['streams'].values())
(p/'performance_p20m3.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps({k:{key:v for key,v in s.items() if key not in ('over80msFrames','stages')} for k,s in report['streams'].items()},indent=2))
