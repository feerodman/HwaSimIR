"""Read actual saved pixels, independently of the SEI/body/index comparison."""
import argparse
import csv
import json
import subprocess
from pathlib import Path
import numpy as np


def validate(folder, ffmpeg):
    rows=[json.loads(line) for line in (folder/'frame_index.jsonl').read_text().splitlines()]
    bodies=(folder/'producer_annotations.jsonl').read_bytes()
    w,h=rows[0]['width'],rows[0]['height']
    assert (w,h)==(800,800), 'This explicit chart definition is 800 x 800.'
    # Fixed external test definition, including known hand-checked positions.
    assert 100+(1*3)%560 == 103 and 100+(31*3)%560 == 193
    process=subprocess.Popen([str(ffmpeg),'-v','error','-i',str(folder/'output.mp4'),
        '-an','-fps_mode','passthrough','-enc_time_base','1:1000000','-pix_fmt','gray','-f','rawvideo','pipe:1'],stdout=subprocess.PIPE)
    results=[]
    for row in rows:
        data=process.stdout.read(w*h)
        assert len(data)==w*h
        frame=np.frombuffer(data,dtype=np.uint8).reshape(h,w)
        background=float(np.median(frame[15:25,15:25]))
        bright=float(np.median(frame[45:65,365:395]))
        assert bright-background>40, ('chart not visible',row['frameSeq'],background,bright)
        threshold=(background+bright)/2
        code=sum((int(np.median(frame[45:65,44+24*k:60+24*k])>threshold)<<k) for k in range(12))
        seq=int(row['sourceSeq'])
        assert code==seq%4096, ('pixel/production identity mismatch',row['frameSeq'],seq,code)
        strip=np.mean(frame[310:350],axis=0)>threshold
        columns=np.flatnonzero(strip)
        assert len(columns)>0
        left,right=int(columns[0]),int(columns[-1])+1
        expected_left=100+((seq%4096)*3)%560
        assert abs(left-expected_left)<=1 and abs(right-(expected_left+40))<=1, (seq,left,right,expected_left)
        body=json.loads(bodies[int(row['annotationBodyOffset']):int(row['annotationBodyOffset'])+row['annotationBodyBytes']])
        chart=body['diagnosticChart']
        assert chart['sourceSeq']==seq and chart['code12']==code and chart['left']==expected_left
        assert (chart['top'],chart['width'],chart['height'])==(300,40,60)
        results.append(dict(frameSeq=row['frameSeq'],sourceSeq=seq,decodedCode=code,
            measuredLeft=left,measuredRight=right,expectedLeft=expected_left,expectedRight=expected_left+40,
            bodyMatchesRaster=True))
    assert process.stdout.read(1)==b''
    assert process.wait()==0
    with (folder/'pixel_annotation_alignment.csv').open('w',newline='',encoding='utf-8-sig') as out:
        writer=csv.DictWriter(out,fieldnames=results[0]);writer.writeheader();writer.writerows(results)
    report=dict(result='PASS',frames=len(rows),scope='actual decoded pixels and independent ordinary moving rectangle definition',
        synthetic=True,productionScene=False)
    (folder/'pixel_validation.json').write_text(json.dumps(report,indent=2))
    print(json.dumps(report))


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('directory',type=Path);parser.add_argument('--ffmpeg',type=Path,required=True)
    args=parser.parse_args();validate(args.directory,args.ffmpeg)
