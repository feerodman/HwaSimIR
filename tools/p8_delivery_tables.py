"""Produce scoped delivery tables from preserved observations, never plans."""
import csv,json,re
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'logs/p8';OUT=BASE/'delivery'
def load(p):return json.loads(p.read_text(encoding='utf-8-sig')) if p.exists() else {}
def write(name,rows):
    keys=list(dict.fromkeys(k for r in rows for k in r))
    with (OUT/name).open('w',newline='',encoding='utf-8-sig') as f:
        w=csv.DictWriter(f,fieldnames=keys);w.writeheader();w.writerows(rows)
def value(d,*keys):
    for key in keys:d=d.get(key,{}) if isinstance(d,dict) else {}
    return d if not isinstance(d,dict) else ''
def main():
    OUT.mkdir(exist_ok=True);performance=[];upstream=[];annotations=[];delays=[];files=[];stages=[]
    for case in sorted(BASE.glob('P8_*')):
        if not case.is_dir() or not (case/'request.json').exists():continue
        q=load(case/'request.json');m=load(case/'p8_metrics.json');c=load(case/'conservation.json');a=load(case/'ordinary_annotation_validation.json');px=load(case/'ordinary_pixel_validation.json');la=load(case/'latency_validation.json');ip=load(case/'input_product_validation.json')
        recordings=list((case/'recording').glob('*/output.mp4'));validations=[load(p.parent/'validation.json') for p in recordings]
        fv='PASS' if validations and all(v.get('result')=='PASS' for v in validations) else ('FAIL' if any(v.get('result')=='FAIL' for v in validations) else 'NOT_MEASURED')
        for movie,v in zip(recordings,validations):
            files.append(dict(case=case.name,recording=movie.parent.name,result=v.get('result','NOT_MEASURED'),frames=v.get('frames',''),mp4Sha256=v.get('movieSha256',''),movie=str(movie.relative_to(OUT.parent)),scope='one recording only; do not multiply case counters by this row count'))
        peaks=m.get('ingressPeaks',{});audit=m.get('inputAudit',{});ld=m.get('latency',{});wait=m.get('inputWaitMs',{})
        performance.append(dict(case=case.name,host=q.get('host'),band=q.get('band'),weather=q.get('weather'),requestedInputHz=q.get('rate'),actualInputHz=m.get('actualSenderHz',''),actualNewImageFps=value(m,'rates','newImage','fps'),measurementSeconds=value(m,'rates','newImage','windowSeconds'),warmupSeconds=value(m,'rates','newImage','warmupSeconds'),requestedDurationSeconds=q.get('seconds'),simMode=q.get('simMode'),outputFps=q.get('outputFps'),recordingEnabled=q.get('saveMp4'),recordings=len(recordings),indexedFrames=m.get('products',''),realtime60Hz=m.get('realtime60Hz','NOT_MEASURED'),inputConservation=c.get('result','NOT_MEASURED'),inputOrderedFields=audit.get('result','NOT_MEASURED'),acceptedToSavedProducts=ip.get('result','NOT_MEASURED'),fileCorrectness=fv,coordinateTest=a.get('result','NOT_MEASURED'),lastWriteToLastReceiveMs=m.get('drainAfterLastWriteMs',''),maxInputDepth=peaks.get('maxQueueDepth',''),fullQueueBackpressure=peaks.get('inputBackpressureCount',''),overflow=peaks.get('inputQueueOverflow',''),overwrite=peaks.get('inputOverwritten',''),inputWaitP95Ms=wait.get('p95',''),inputWaitP99Ms=wait.get('p99',''),inputWaitMaxMs=wait.get('maximum',''),windowLatencyValid=ld.get('valid',''),windowLatencyTotal=ld.get('total',''),windowLatencyP99Ms=value(ld,'distributionMs','p99'),windowLatencyMaxMs=value(ld,'distributionMs','maximum'),windowLatencyOver80Ms=ld.get('over80Ms',''),thermalMaxC=m.get('thermalMaxC',''),scope='case aggregate once; stage stats include startup; rates use stated window; async last-write tail includes continued free-running output before STOP'))
        upstream.append(dict(case=case.name,result=c.get('result','NOT_MEASURED'),senderSuccessCounts=json.dumps(c.get('successfulWriterCounts',[])),boardAcceptedCounts=json.dumps(c.get('boardAcceptedCounts',[])),acceptedMinusSender=json.dumps(c.get('acceptedMinusWriter',[])),orderedInputResult=audit.get('result','NOT_MEASURED'),successfulOrderedWrites=audit.get('successfulWrites',''),accepted=audit.get('accepted',''),executed=audit.get('executed',''),differences=json.dumps(audit.get('differences',[])),transportEvents=len(c.get('transportEvents',[])),acceptedProductResult=ip.get('result','NOT_MEASURED'),savedFrames=ip.get('savedProducts',''),scope='one case aggregate; accepted sequence alone is not an upstream sender sequence'))
        if a:
            annotations.append(dict(case=case.name,fileCorrespondence=fv,fieldAndCoordinateResult=a['result'],checkedObjectFrames=a['checkedObjectFrames'],coordinateErrors=a['errorCount'],decodedPixelResult=px.get('result','NOT_MEASURED'),decodedMarkerChecks=px.get('markerChecks',''),sourceWidth=800,sourceHeight=800,origin='top_left',clipping='source viewport 0..799',invalid='V1 omits invalid/invisible objects or points without reason codes',uiScaling='presentation only; saved source stays 800x800',semanticCalibration='NOT_MEASURED: synthetic points only; equipment meanings uncalibrated'))
        if la:
            delays.append(dict(case=case.name,result=la['result'],valid=la['valid'],total=la['total'],validRatio=la['validRatio'],p99Ms=value(la,'wholeSession','p99'),maxMs=value(la,'wholeSession','maximum'),over80Ms=la['over80Ms'],afterFirst5sP99Ms=value(la,'afterFirst5s','p99'),afterFirst5sMaxMs=value(la,'afterFirst5s','maximum'),uncertaintyP99Ms=value(la,'uncertaintyMs','p99'),uncertaintyMaxMs=value(la,'uncertaintyMs','maximum'),meaning=la['definition']))
        for stage,fields in m.get('stages',{}).items():
            for name,v in fields.items():
                if isinstance(v,dict):stages.append(dict(case=case.name,stage=stage,metric=name,**v,scope=m.get('stageStatisticsScope','older diagnostic stage scope; see case metrics')))
    p7=ROOT/'logs/p7/D_C4_mwir_snow600/latency_validation.json'
    if p7.exists():
        la=load(p7);delays.append(dict(case='P7_D_C4_mwir_snow600',result=la['result'],valid=la['valid'],total=la['total'],validRatio=la['validRatio'],p99Ms=value(la,'wholeSession','p99'),maxMs=value(la,'wholeSession','maximum'),over80Ms=la['over80Ms'],afterFirst5sP99Ms=value(la,'afterFirst5s','p99'),afterFirst5sMaxMs=value(la,'afterFirst5s','maximum'),uncertaintyP99Ms=value(la,'uncertaintyMs','p99'),uncertaintyMaxMs=value(la,'uncertaintyMs','maximum'),meaning=la['definition']))
    for name,rows in [('performance_and_files.csv',performance),('upstream_conservation.csv',upstream),('annotation_scope_and_checks.csv',annotations),('latency_validity_and_outliers.csv',delays),('recording_files.csv',files),('local_stage_timings.csv',stages)]:write(name,rows)
    print(json.dumps(dict(cases=len(performance),recordings=len(files),coordinateCases=len(annotations))))
if __name__=='__main__':main()
