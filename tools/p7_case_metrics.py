"""Observed per-product timings and final counters; no planned-FPS substitution."""
from pathlib import Path
import argparse,csv,json,re,statistics

def distribution(values):
    values=sorted(values)
    if not values:return dict(count=0)
    return dict(count=len(values),mean=statistics.mean(values),p50=values[round((len(values)-1)*.5)],
                p95=values[round((len(values)-1)*.95)],p99=values[round((len(values)-1)*.99)],maximum=values[-1])

def report(directory):
    groups={k:[] for k in ['inputWaitMs','inputExecutionToCaptureMs','outputQueueEncodeMs','writerQueuePrepareMs',
        'estimatedOutputLatencyMs','clockUncertaintyMs','receiverDecodeMs','guiQueueMs','guiSubmitMs']}
    rows=[]
    observed=[]
    for path in sorted((directory/'recording').glob('*/frame_index.jsonl')):
        products=[]
        for line in path.read_text(encoding='utf-8-sig').splitlines():
            p=json.loads(line);products.append(p);row={k:p.get(k,'') for k in ['session','generation','run','frameSeq','sourceSeq']}
            for key,begin,end in [('inputWaitMs','acceptedSteadyNs','executeSteadyNs'),
                                 ('inputExecutionToCaptureMs','executeSteadyNs','captureSteadyNs'),
                                 ('outputQueueEncodeMs','captureSteadyNs','encodeSteadyNs'),
                                 ('writerQueuePrepareMs','encodeSteadyNs','writerSubmitSteadyNs'),
                                 ('receiverDecodeMs','receiveSteadyNs','decodeSteadyNs'),
                                 ('guiQueueMs','decodeSteadyNs','guiBeginSteadyNs'),
                                 ('guiSubmitMs','guiBeginSteadyNs','guiSubmitSteadyNs')]:
                if begin in p and end in p:
                    row[key]=(int(p[end])-int(p[begin]))/1.e6;groups[key].append(row[key])
            if p.get('outputLatencyEstimated'):
                row['estimatedOutputLatencyMs']=p['outputLatencyMs'];groups['estimatedOutputLatencyMs'].append(p['outputLatencyMs'])
                row['clockUncertaintyMs']=p['clockUncertaintyMs'];groups['clockUncertaintyMs'].append(p['clockUncertaintyMs'])
            rows.append(row)
        rates={}
        for name,field in [('captureFps','captureSteadyNs'),('receivedCompleteVideoFps','receiveSteadyNs'),('decodedNewImageFps','decodeSteadyNs')]:
            times=[int(p[field]) for p in products if field in p]
            if len(times)>1 and times[-1]>times[0]:
                rates[name]=(len(times)-1)*1e9/(times[-1]-times[0])
                rates[name+'IntervalMs']=distribution([(b-a)/1e6 for a,b in zip(times,times[1:])])
        observed.append(dict(recording=path.parent.name,frames=len(products),rates=rates,
            definition='Observed whole saved interval, including any requested pause; not requested FPS'))
    if rows:
        fields=list(dict.fromkeys(k for row in rows for k in row))
        with (directory/'product_timings.csv').open('w',newline='',encoding='utf-8-sig') as output:
            w=csv.DictWriter(output,fieldnames=fields);w.writeheader();w.writerows(rows)
    source=directory/('board.log' if (directory/'board.log').exists() else 'hwa.out.log')
    log=source.read_text(encoding='utf-8',errors='replace')
    counters={}
    for tag in ['SyncRoundConservation','RealtimeIngress','DdsVideoPerf','CloudRenderCallAudit']:
        matches=re.findall(r'\['+tag+r'\] ([^\r\n]+)',log)
        if matches:counters[tag]=dict(re.findall(r'(\w+)=([^ ]+)',matches[-1]))
    value=dict(case=directory.name,savedProducts=len(rows),observedRates=observed,timingMs={k:distribution(v) for k,v in groups.items()},
        counters=counters,latencyMeaning='application-clock estimate; accepted Realtime to complete receiver AU; uncertainty retained',
        inputExecutionToCaptureMeaning='interval from this input execution to image capture; asynchronous repeated states include state age, not per-frame render cost',
        missingTimingMeaning='not measured; never substituted with zero')
    (directory/'metrics.json').write_text(json.dumps(value,indent=2),encoding='utf-8')
    print(json.dumps(value))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('directory',type=Path);args=parser.parse_args();report(args.directory)
