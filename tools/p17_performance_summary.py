"""Identity-based 60 s timing summary. Qt paint is a separate logged scope."""
import json,pathlib,re
import numpy as np
ROOT=pathlib.Path(__file__).resolve().parents[1]
def main():
    reports=[]
    for index in sorted((ROOT/'logs/p17/production/performance').glob('*/recording/*/frame_index.jsonl')):
        case=index.parents[2]
        rows=[json.loads(line) for line in index.read_text(encoding='utf-8-sig').splitlines()]
        warm=[r for r in rows if 601<=int(r['sourceSeq'])<=4200]
        if len(warm)!=3600:raise RuntimeError('Missing timing products')
        receiver=(case/'receiver.err.log').read_text(encoding='utf-8',errors='replace')
        paint=re.findall(r'\[GuiPaintPerf\] reason=shutdown[^\r\n]+',receiver)
        if not paint:raise RuntimeError('Missing actual Qt paint count')
        qt=dict(re.findall(r'(\w+)=([^ ]+)',paint[-1]))
        rates={k:(len(warm)-1)*1e9/(int(warm[-1][v])-int(warm[0][v])) for k,v in {'accepted':'acceptedSteadyNs','captured':'captureSteadyNs','output':'writerSubmitSteadyNs','decode':'decodeSteadyNs','guiSubmit':'guiSubmitSteadyNs'}.items()}
        p=lambda values:{'p95':float(np.percentile(values,95)),'p99':float(np.percentile(values,99))}
        latency=p([r['outputLatencyMs'] for r in warm])
        paintFps=float(qt['steadyPaintFps'])
        reports.append(dict(caseId=case.name,sourceSeqWindow=[601,4200],warmupSeconds=10,requestedHz=60,
            frames=len(rows),rates=rates,qtPaintFps=paintFps,qtPaint=qt,outputLatencyEstimatedMs=latency,
            renderExecuteToCaptureMs=p([(int(r['captureSteadyNs'])-int(r['executeSteadyNs']))/1e6 for r in warm]),
            queueWaitMs=p([r['queueWaitMs'] for r in warm]),clockUncertaintyMs=p([r['clockUncertaintyMs'] for r in warm]),
            paintMergedProducts=int(qt['submittedFrames'])-int(qt['paintedFrames']),
            result='FAIL' if paintFps<60 or min(rates.values())<60 or latency['p99']>80 else 'NOT_ACCEPTED_PENDING_PAINT_LATENCY',
            latencyScope='estimated input acceptance to GUI submission, not measured scanout/paint latency',
            qtScope='Qt Paint events, steady excludes first 180 products; not the sourceSeq timing window'))
    out=ROOT/'logs/p17/performance_summary.json';out.write_text(json.dumps(reports,indent=2)+'\n')
    print(json.dumps(reports,indent=2))
if __name__=='__main__':main()
