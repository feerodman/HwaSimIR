"""Independent byte/count/pixel checks on saved real DDS receipts."""
import hashlib,json,subprocess,math
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
FF=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe'
def rows(path):return [json.loads(s) for s in path.read_text().splitlines() if s.strip()]
def main():
    results=[]
    for directory in (ROOT/'logs/p18/runs').iterdir():
        if not (directory/'result.json').exists() or not (directory/'sender_audit').exists():continue
        run=json.loads((directory/'result.json').read_text());entry={'name':directory.name}
        if run.get('boardExit',0) not in [0,124] and not (run.get('boardExit')==143 and run.get('options',{}).get('rounds',1)>1 and run.get('executionCompleted')):
            entry.update(result='INCOMPLETE_FAILED_RUN',boardExit=run.get('boardExit'),reason='Transport/process failure; partial audit cannot establish complete delivery')
            results.append(entry);continue
        audit=[r for p in (directory/'sender_audit').glob('customer_round*.jsonl') for r in rows(p)]
        options=run.get('options',{});entry['sentCompleteFrames']=len(audit);entry['sentSamples']=sum(r['samples'] for r in audit)
        for rxdir in [directory/'customer',directory/'customer_reconnected']:
            if not (rxdir/'summary.json').exists():continue
            receiverAudit=audit
            if options.get('second_width'):
                receiverRound=2 if rxdir.name=='customer_reconnected' else 1
                receiverAudit=[r for r in audit if r['round']==receiverRound]
            rx=json.loads((rxdir/'summary.json').read_text());incoming=rows(rxdir/'receive.jsonl');samples=[r for r in incoming if r['kind']=='sample'];frames=[r for r in incoming if r['kind']=='frame']
            check=dict(receivedSamples=len(samples),decodedFrames=len(frames),rejected=rx['rejectedSamples'],failed=rx['failed'],decodeErrors=rx['decodeErrors'])
            if rx['codec']=='raw':
                expected=rx['width']*rx['height']*(1 if rx['pixelFormat']=='gray8' else 3)
                check['allRawBytesCorrect']=all(r['bytes']==expected for r in samples);check['bytesPerFrame']=expected
                check['frameIds']={'first':samples[0]['frameId'] if samples else None,'last':samples[-1]['frameId'] if samples else None,
                    'continuous':all(b['frameId']==a['frameId']+1 for a,b in zip(samples,samples[1:]))}
                check['rawHashesEqualCompleteSender']=[r['sha256'] for r in receiverAudit]==[r['sha256'] for r in samples]
            else:
                sentFiles=list((directory/'sender_audit').glob('*.h264'))
                received=(rxdir/'received.h264').read_bytes()
                check['allFragmentLengthsValid']=all(1<=r['bytes']<=1400 for r in samples)
                check['fragmentCountMatchesAuSizes']=sum(math.ceil(r['bytes']/1400) for r in audit)==len(samples)
                check['cleanStreamByteIdentical']=len(sentFiles)==1 and sentFiles[0].read_bytes()==received
                check['privateUuidOccurrences']=received.count(bytes.fromhex('48776153696d49529a724f10816d0201'))
                check['senderSharedEncodingFrames']=sum(r['sharedCleanAu'] for r in audit)
            if options.get('color_card'):
                img=Image.open(rxdir/'first.png').convert('RGB');w,h=img.size
                values=[img.getpixel((int((i+.5)*w/6),y)) for y in [h//4,3*h//4] for i in range(6)]
                rgb=[(255,0,0),(0,255,0),(0,0,255),(0,0,0),(128,128,128),(255,255,255)]
                expected=[tuple(v//f for v in c) for f in [1,2] for c in rgb]
                if rx['pixelFormat']=='gray8':expected=[((77*c[0]+150*c[1]+29*c[2]+128)>>8,)*3 for c in expected]
                maxError=max(abs(v-e) for actual,target in zip(values,expected) for v,e in zip(actual,target))
                card=dict(samples=values,expected=expected,maxAbsoluteError=maxError,rangeOrderOrientationPass=maxError<=(0 if rx['codec']=='raw' else 4),diagnosticNotIRScene=True)
                if rx['codec']=='h264' and rx['pixelFormat']=='gray8':
                    yuv=subprocess.check_output([str(FF),'-v','error','-i',str(rxdir/'received.h264'),'-frames:v','1','-pix_fmt','yuv420p','-f','rawvideo','pipe:1'])
                    chroma=yuv[w*h:];card['chromaRange']=[min(chroma),max(chroma)];card['neutralChromaPass']=min(chroma)==max(chroma)==128
                check['colorCard']=card
            entry[rxdir.name]=check
        (directory/'byte_pixel_analysis.json').write_text(json.dumps(entry,indent=2),encoding='utf-8');results.append(entry)
    (ROOT/'logs/p18/tests/production_byte_pixel_results.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
    print(json.dumps(results,indent=2))
if __name__=='__main__':main()
