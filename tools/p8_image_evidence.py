"""Extract genuine saved RGB pixels and compare only matching input times.

No output pixel is painted or generated. Comparison pages use separate HTML
panels. Generic geometry references come from independently projected inputs.
"""
import argparse,csv,hashlib,json,subprocess
from pathlib import Path
import numpy as np
from PIL import Image
from p8_validate_ordinary_annotations import project,position,vertices

ROOT=Path(__file__).resolve().parents[1]
FF=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe'

def records(case):
    result=[]
    for folder in sorted((case/'recording').iterdir()):
        if not (folder/'output.mp4').exists():continue
        for n,line in enumerate((folder/'producer_annotations.jsonl').read_text(encoding='utf-8').splitlines()):
            p=json.loads(line);result.append((folder,n,p))
    return result

def extract(item,path):
    folder,n,row=item
    command=[str(FF),'-v','error','-threads','1','-i',str(folder/'output.mp4'),'-vf',f'select=eq(n\\,{n})','-frames:v','1','-fps_mode','passthrough','-pix_fmt','rgb24','-f','rawvideo','pipe:1']
    raw=subprocess.check_output(command);assert len(raw)==800*800*3
    a=np.frombuffer(raw,np.uint8).reshape(800,800,3);Image.fromarray(a).save(path)
    (path.with_suffix('.json')).write_text(json.dumps(dict(recording=str(folder.resolve().relative_to(ROOT)),storageIndex=n+1,sourceSeq=row['sourceSeq'],frameSeq=row['frameSeq'],simTimeMs=row['simTimeMs'],rawRgbSha256=hashlib.sha256(raw).hexdigest(),width=800,height=800,meaning='actual saved MP4 RGB decode; unannotated'),indent=2),encoding='utf-8')
    return a

def ordinary(case):
    from scipy.ndimage import label,find_objects
    request=json.loads((case/'request.json').read_text(encoding='utf-8-sig'))
    fixture=json.loads(Path(request['cameraInput']).read_text(encoding='utf-8'))
    count=len(fixture['SyntheticTelemetryTargets']);items=records(case);out=case/'selected_pixels';out.mkdir(exist_ok=True)
    rows=[];errors=[]
    for target in [1000,3000,5000,7000]:
        item=min(items,key=lambda x:abs(float(x[2]['simTimeMs'])%60000-target))
        a=extract(item,out/f'phase_{target//2000}.png');gray=a[:,:,0].astype(float);bg=float(np.median(gray[:100,:100]))
        labels,n=label(gray>bg+15,structure=np.ones((3,3),dtype=np.uint8))
        areas=np.bincount(labels.ravel());components=[]
        for identifier,slices in enumerate(find_objects(labels),1):
            if slices and areas[identifier]>100:
                sy,sx=slices;components.append([sx.start,sy.start,sx.stop-sx.start,sy.stop-sy.start,areas[identifier]])
        time=float(item[2]['simTimeMs']);phase=int((time%60000)/2000)%4
        if phase!=0:continue # Other phases retain raw images plus full coordinate tests.
        for i in range(count):
            x,y,z,_=position(i,count,time);uv=[project((x+u,y+v,z+w)) for u,v,w in vertices(i)]
            expect=np.array([min(p[0] for p in uv),min(p[1] for p in uv),max(p[0] for p in uv),max(p[1] for p in uv)])
            boxes=[np.array([s[0],s[1],s[0]+s[2]-1,s[1]+s[3]-1]) for s in components]
            observed=min(boxes,key=lambda b:np.max(abs(b-expect)));error=int(np.max(abs(observed-expect)))
            if error>2:errors.append(dict(objectIndex=i,kind='decoded_body_bbox',errorPx=error))
            body=float(np.median(gray[max(0,int(observed[1])):min(800,int(observed[3])+1),max(0,int(observed[0])):min(800,int(observed[2])+1)]))
            for marker,offset in [('A',(0,-1.01,0)),('B',(.5,-1.01,.5))]:
                px,py=project((x+offset[0],y+offset[1],z+offset[2]));local=float(gray[py-2:py+3,px-2:px+3].max())
                if local<body+4:errors.append(dict(objectIndex=i,kind='decoded_marker_missing',marker=marker))
                rows.append(dict(sourceSeq=item[2]['sourceSeq'],objectIndex=i,marker=marker,x=px,y=py,bboxPixelError=error,markerMax=local,bodyMedian=body,topLeftOrigin=True,sourceWidth=800,sourceHeight=800))
    with (case/'ordinary_decoded_pixels.csv').open('w',newline='',encoding='utf-8-sig') as f:
        w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
    result=dict(result='PASS' if rows and not errors else 'FAIL',markerChecks=len(rows),errors=errors,scope='Actual decoded phase-0 body extents and asymmetric marker positions, independent input pinhole reference; all four phase images retained; no real-equipment semantics')
    (case/'ordinary_pixel_validation.json').write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps(result))

def visual(base):
    output=base/'delivery';output.mkdir(exist_ok=True);rows=[]
    for label in ['rain','snow']:
        cases={k:base/f'P8_visual_{label}_{k}' for k in ['192','128','hide_first','hide_second']}
        bytime={k:{round(float(i[2]['simTimeMs']),5):i for i in records(c)} for k,c in cases.items()}
        common=set.intersection(*(set(m) for m in bytime.values()));assert common,'No identical simulation input times'
        t=min(common,key=lambda t:abs(t%60000-4000));images={}
        for k in cases:
            images[k]=extract(bytime[k][t],output/f'{label}_{k}.png')
            if k in ['192','128']:
                movie=bytime[k][t][0]/'output.mp4'
                subprocess.run([str(FF),'-v','error','-y','-ss','1','-i',str(movie),'-t','6','-an','-c:v','copy',str(output/f'{label}_{k}_6s.mp4')],check=True)
        for k in ['192','hide_first','hide_second']:
            d=np.abs(images['128'].astype(np.int16)-images[k].astype(np.int16)).max(axis=2)
            ys,xs=np.where(d>2)
            rows.append(dict(weather=label,comparison='128_vs_'+k,simTimeMs=t,changedPixelsOver2=int((d>2).sum()),maximumRgb8Difference=int(d.max()),changedBounds=f'{xs.min()},{ys.min()},{xs.max()},{ys.max()}' if len(xs) else '',scope='Decoded local difference, includes codec effects; display frozen within band, identical input time and normal world identity'))
    with (output/'weather_pixel_comparisons.csv').open('w',newline='',encoding='utf-8-sig') as f:
        w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
    print(json.dumps(rows))

def normal(base):
    base=base.resolve();out=base/'delivery'
    for band,case_name in [('nvg','P8_final_NVG_rain600'),('mwir','P8_final_MWIR_snow600')]:
        case=base/case_name;folder=next((case/'recording').iterdir())
        rows=[json.loads(s) for s in (folder/'frame_index.jsonl').read_text().splitlines()]
        n=len(rows)//2;row=rows[n];pts=int(row['mp4PtsUs'])/1e6
        raw=subprocess.check_output([str(FF),'-v','error','-threads','1','-ss',f'{pts:.6f}','-i',str(folder/'output.mp4'),'-frames:v','1','-pix_fmt','rgb24','-f','rawvideo','pipe:1'])
        with (folder/'all_frame_correspondence.csv').open(encoding='utf-8-sig') as f:golden=list(csv.DictReader(f))[n]['rgbMd5']
        assert hashlib.md5(raw).hexdigest()==golden,'Accurate seek did not return the verified selected frame'
        Image.fromarray(np.frombuffer(raw,np.uint8).reshape(800,800,3)).save(out/f'normal_{band}.png')
        (out/f'normal_{band}.json').write_text(json.dumps(dict(case=case_name,storageIndex=n+1,frameProduct=row,rgbMd5=golden,meaning='unannotated verified MP4 frame, formal default settings'),indent=2),encoding='utf-8')
        subprocess.run([str(FF),'-v','error','-y','-ss',f'{pts:.6f}','-i',str(folder/'output.mp4'),'-t','6','-an','-c:v','copy',str(out/f'normal_{band}_6s.mp4')],check=True)
    before=base/'P8_ordinary2_windows_A4';after=base/'P8_ordinary2_windows_A6_retry'
    b={round(float(i[2]['simTimeMs']),5):i for i in records(before)};a={round(float(i[2]['simTimeMs']),5):i for i in records(after)}
    checks=list(csv.DictReader((before/'ordinary_coordinate_checks.csv').open(encoding='utf-8-sig')))
    candidates=[r for r in checks if round(float(r['timeMs']),5) in b and round(float(r['timeMs']),5) in a]
    worst=max(candidates,key=lambda r:float(r['bboxErrorPx']));time=round(float(worst['timeMs']),5)
    for label,item in [('before',b[time]),('after',a[time])]:
        extract(item,out/f'annotation_{label}.png')
        (out/f'annotation_{label}_body.json').write_text(json.dumps(item[2],indent=2),encoding='utf-8')
    (out/'annotation_comparison_reference.json').write_text(json.dumps(worst,indent=2),encoding='utf-8')
    print(json.dumps(dict(normalFrames='PASS',annotationComparisonTimeMs=time,originalBBoxErrorPx=worst['bboxErrorPx'])))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('mode',choices=['ordinary','visual','normal']);p.add_argument('path',type=Path);a=p.parse_args();globals()[a.mode](a.path)
