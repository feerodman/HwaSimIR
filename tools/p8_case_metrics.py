"""Bounded P8 summaries. Producer/local clock windows; never infer sender identity from sourceSeq."""
import argparse,csv,json,re,statistics
from pathlib import Path
from collections import Counter
from difflib import SequenceMatcher
from p7_case_metrics import distribution

def csv_rows(path):
    with path.open(encoding='utf-8-sig',newline='') as stream:
        # A second MinGW sender process may reuse the SDK-independent audit
        # filename. Append headers delimit that process; retain every data row
        # and sort by the measured local time rather than its reset ordinal.
        reader=csv.DictReader(stream)
        return [r for r in reader if r!={k:k for k in reader.fieldnames}]
def lines(path):
    with path.open(encoding='utf-8-sig') as stream:return [json.loads(s) for s in stream if s.strip()]
def ordered_ledger(directory,role):
    rows=[r for p in directory.glob('input_'+role+'_*.csv') for r in csv_rows(p)]
    return sorted(rows,key=lambda r:(int(r['beginNs']),int(r['ordinal'])))
def audit(directory):
    send=[r for r in ordered_ledger(directory,'sender') if r['success']=='1']
    accepted=ordered_ledger(directory,'accepted')
    executed=ordered_ledger(directory,'execute')
    if not send or not accepted:return {'result':'NOT_MEASURED','reason':'No stable-field sender/accepted ledgers'}
    a=[r['digestFNV1a64'] for r in send];b=[r['digestFNV1a64'] for r in accepted];c=[r['digestFNV1a64'] for r in executed]
    diff=[]
    for op,i,j,k,l in SequenceMatcher(None,a,b,autojunk=False).get_opcodes():
        if op!='equal':diff.append(dict(operation=op,senderFirstOrdinal=i+1,senderLastOrdinal=j,boardFirstOrdinal=k+1,boardLastOrdinal=l))
    result=dict(result='PASS' if a==b==c else 'FAIL',successfulWrites=len(a),accepted=len(b),executed=len(c),
        orderAndMultiplicityMatch=a==b,acceptedToExecutedMatch=b==c,differences=diff,
        senderWriteMs=distribution([(int(r['endNs'])-int(r['beginNs']))/1e6 for r in send]),
        meaning='FNV1a64 of explicit canonical input fields; ordered occurrences compared, not sets; checksum is diagnostic, not authentication')
    (directory/'input_audit_comparison.json').write_text(json.dumps(result,indent=2),encoding='utf-8');return result

def report(directory,warmup=5,seconds=60):
    products=[r for p in sorted((directory/'recording').glob('*/frame_index.jsonl')) for r in lines(p)]
    send=[r for r in ordered_ledger(directory,'sender') if r['success']=='1']
    rates={};window_products=[]
    if products:
        for name,field in [('capture','captureSteadyNs'),('received','receiveSteadyNs'),('newImage','decodeSteadyNs')]:
            times=[int(r[field]) for r in products]
            # Windows sender and receiver use the same QPC steady clock. No board/Windows subtraction.
            origin=int(send[0]['beginNs']) if send and name!='capture' else times[0]
            start=origin+int(warmup*1e9);stop=start+int(seconds*1e9)
            selected=[r for r in products if start<=int(r[field])<stop]
            if name=='newImage':window_products=selected
            rates[name]=dict(fps=len(selected)/seconds,frames=len(selected),windowSeconds=seconds,
                origin='sender_local_monotonic' if send and name!='capture' else 'first_product_local_monotonic',
                warmupSeconds=warmup,stopDrainExcluded=True)
    elif send:
        decoded=ordered_ledger(directory,'received')
        start=int(send[0]['beginNs'])+int(warmup*1e9);stop=start+int(seconds*1e9)
        if decoded:
            for name,field in [('received','beginNs'),('newImage','endNs')]:
                selected=[r for r in decoded if start<=int(r[field])<stop]
                rates[name]=dict(fps=len(selected)/seconds,frames=len(selected),windowSeconds=seconds,
                    origin='sender_local_monotonic',warmupSeconds=warmup,stopDrainExcluded=True,
                    source='receiver_identified_new_image_audit_independent_of_recording')
    valid=[r for r in window_products if r.get('outputLatencyEstimated')]
    lat=[float(r['outputLatencyMs']) for r in valid]
    waits=[(int(r['executeSteadyNs'])-int(r['acceptedSteadyNs']))/1e6 for r in window_products]
    stages={}
    for prefix in ['render','output','recording']:
        rows=[r for p in sorted(directory.glob('stage_'+prefix+'_*.csv')) for r in csv_rows(p)]
        if rows:
            complete=[r for r in rows if None not in r and all(v is not None for v in r.values())]
            stages[prefix]={k:distribution([float(r[k]) for r in complete]) for k in rows[0] if k and (k.endswith('Ms') or 'Depth' in k)}
            stages[prefix]['incompleteRows']=len(rows)-len(complete)
    temperatures=[r for p in directory.glob('thermal.jsonl') for r in lines(p)]
    thermal=[t for r in temperatures for t in r.get('temperaturesC',{}).values()]
    log=(directory/'board.log').read_text(encoding='utf-8',errors='replace') if (directory/'board.log').exists() else ''
    counts=re.findall(r'\[RealtimeIngress\] ([^\r\n]+)',log)
    last=dict(re.findall(r'(\w+)=([^ ]+)',counts[-1])) if counts else {}
    # STOP resets interval statistics. Preserve the peak from all log snapshots.
    snapshots=[dict(re.findall(r'(\w+)=([^ ]+)',s)) for s in counts]
    peaks={k:max([float(s.get(k,0)) for s in snapshots] or [0]) for k in ['maxQueueDepth','inputBackpressureCount','inputBackpressureWaitMs','maxInputBackpressureWaitMs','inputQueueOverflow','inputOverwritten']}
    per_second=[]
    if products:
        origin=int(send[0]['beginNs']) if send else int(products[0]['decodeSteadyNs'])
        bins={}
        for r in products:bins.setdefault(int((int(r['decodeSteadyNs'])-origin)//1000000000),[]).append(r)
        for sec,rows in sorted(bins.items()):
            waits_s=[(int(r['executeSteadyNs'])-int(r['acceptedSteadyNs']))/1e6 for r in rows]
            valid_s=[r for r in rows if r.get('outputLatencyEstimated')]
            per_second.append(dict(second=sec,newFrames=len(rows),inputWaitMeanMs=statistics.mean(waits_s),inputWaitMaxMs=max(waits_s),estimatedLatencyMaxMs=max([r['outputLatencyMs'] for r in valid_s] or [-1])))
        with (directory/'per_second.csv').open('w',newline='',encoding='utf-8-sig') as f:
            w=csv.DictWriter(f,fieldnames=per_second[0]);w.writeheader();w.writerows(per_second)
    result=dict(case=directory.name,products=len(products),rates=rates,inputWaitMs=distribution(waits),stages=stages,
        inputAudit=audit(directory),finalIngress=last,ingressPeaks=peaks,stageStatisticsScope='whole case including startup; rate window separately defined',thermalMaxC=max(thermal) if thermal else None,
        latency=dict(result='ESTIMATED' if valid else 'NOT_MEASURED',valid=len(valid),total=len(window_products),
            validRatio=len(valid)/len(window_products) if window_products else 0,over80Ms=sum(x>80 for x in lat),distributionMs=distribution(lat)))
    result['realtime60Hz']='PASS' if rates and rates['newImage']['fps']>=59.5 and peaks['inputBackpressureCount']==0 and result['inputAudit'].get('result')=='PASS' else 'FAIL_OR_INCOMPLETE'
    request=json.loads((directory/'request.json').read_text(encoding='utf-8-sig')) if (directory/'request.json').exists() else {}
    if request.get('rate')!=60 or request.get('simMode')!=1 or request.get('pauseStart',-1)>=0 or request.get('seconds',0)<65:
        result['realtime60Hz']='NOT_APPLICABLE_TO_THIS_REGRESSION'
    if send:
        result['actualSenderHz']=(len(send)-1)*1e9/(int(send[-1]['beginNs'])-int(send[0]['beginNs'])) if len(send)>1 else None
        arrival_times=[int(p['receiveSteadyNs']) for p in products] or [int(r['beginNs']) for r in ordered_ledger(directory,'received')]
        result['drainAfterLastWriteMs']=max(0,max(arrival_times)-int(send[-1]['endNs']))/1e6 if arrival_times else None
        result['drainMeaning']='last successful sender API return to last complete receiver AU, same Windows monotonic clock; not exact STOP callback duration'
    (directory/'p8_metrics.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps(result));return result
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('directory',type=Path);p.add_argument('--seconds',type=float,default=60);p.add_argument('--warmup',type=float,default=5);a=p.parse_args();report(a.directory,a.warmup,a.seconds)
