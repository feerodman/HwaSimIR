"""Report all valid estimated delays and actual local-stage contributions.

The cross-host estimate is carried by the production index; no system clocks
are subtracted here. Cold-start samples remain in the full-session result.
"""
import argparse,csv,json,sys
from pathlib import Path
from p7_case_metrics import distribution

def report(case):
    products=[]
    for path in sorted((case/'recording').glob('*/frame_index.jsonl')):
        with path.open(encoding='utf-8-sig') as f:products.extend(json.loads(s) for s in f if s.strip())
    pairs=[('acceptedToExecuteMs','acceptedSteadyNs','executeSteadyNs'),('executeToCaptureMs','executeSteadyNs','captureSteadyNs'),('captureToEncodeMs','captureSteadyNs','encodeSteadyNs'),('encodeToSubmitMs','encodeSteadyNs','writerSubmitSteadyNs'),('receiverDecodeMs','receiveSteadyNs','decodeSteadyNs'),('guiQueueMs','decodeSteadyNs','guiBeginSteadyNs'),('guiSubmitMs','guiBeginSteadyNs','guiSubmitSteadyNs')]
    rows=[];origins={}
    for p in products:
        identity=(p['session'],p['generation'],p['run'])
        origin=origins.setdefault(identity,int(p['acceptedSteadyNs']))
        r={k:p[k] for k in ['session','generation','run','sourceSeq','frameSeq']}
        r.update(secondsSinceFirstAccepted=(int(p['acceptedSteadyNs'])-origin)/1e9,estimated=bool(p.get('outputLatencyEstimated')),estimatedDelayMs=p.get('outputLatencyMs'),uncertaintyMs=p.get('clockUncertaintyMs'))
        for name,a,b in pairs:r[name]=(int(p[b])-int(p[a]))/1e6 if a in p and b in p else ''
        r['over80Ms']=r['estimated'] and r['estimatedDelayMs']>80
        rows.append(r)
    valid=[r for r in rows if r['estimated']]
    summary=dict(result='ESTIMATED' if valid else 'NOT_MEASURED',total=len(rows),valid=len(valid),validRatio=len(valid)/len(rows) if rows else 0,
        wholeSession=distribution([r['estimatedDelayMs'] for r in valid]),afterFirst5s=distribution([r['estimatedDelayMs'] for r in valid if r['secondsSinceFirstAccepted']>=5]),
        uncertaintyMs=distribution([r['uncertaintyMs'] for r in valid]),over80Ms=sum(r['over80Ms'] for r in rows),
        definition='producer accepted input to receiver complete video AU; application-clock estimate, not exact synchronized-clock latency; excludes pre-acceptance DDS backlog')
    (case/'latency_validation.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    for name,selected in [('latency_all_frames.csv',rows),('latency_over80ms.csv',[r for r in rows if r['over80Ms']])]:
        if rows:
            with (case/name).open('w',newline='',encoding='utf-8-sig') as f:
                w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(selected)
    print(json.dumps(summary));return summary
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('case',type=Path);report(p.parse_args().case)
