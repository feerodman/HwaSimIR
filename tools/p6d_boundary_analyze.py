"""Separate fixed-step cloud output from dynamic/frozen display at the boundary."""
from pathlib import Path
import json,re,subprocess
import numpy as np
from PIL import Image,ImageDraw
from p6d_analyze_causes import pfm
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'logs/p6d'
FF=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe'
def main():
    folders=[OUT/'boundary_b1_auto8',OUT/'boundary_b1_frozen8']
    raw=[pfm(p/'linear.pfm') for p in folders]
    result=dict(source_sequence=262,steps=8,linear_max_difference=float(np.abs(raw[0]-raw[1]).max()),linear_range=[float(raw[0].min()),float(raw[0].max())],cases=[])
    canvas=Image.new('RGB',(1600,836),'#16191d');draw=ImageDraw.Draw(canvas)
    for i,p in enumerate(folders):
        log=(p/'board.log').read_text(encoding='utf-8-sig',errors='replace')
        mapping=re.findall(r'\[DisplayFrameMapping\][^\n]+',log)[-1]
        canvas.paste(Image.open(p/'received.png').convert('RGB'),(i*800,0));draw.text((i*800+12,810),p.name+' / same input sequence 262 / 8 steps',fill='white')
        if not(p/'received.mp4').exists():
            subprocess.run([str(FF),'-v','error','-y','-r','30','-i',str(p/'received.h264'),'-c:v','copy',str(p/'received.mp4')],check=True)
        proc=subprocess.Popen([str(FF),'-v','error','-i',str(p/'received.mp4'),'-vf','scale=160:160:flags=area','-pix_fmt','gray','-f','rawvideo','-'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        prev=None;diff=[];index=0;black=[]
        while True:
            data=proc.stdout.read(25600)
            if not data:break
            a=np.frombuffer(data,np.uint8).astype(np.int16)
            if prev is not None:diff.append((float(np.abs(a-prev).mean()),index))
            black.append((int((a==0).sum()),index));prev=a;index+=1
        error=proc.stderr.read().decode(errors='replace');assert proc.wait()==0,error
        peak=max((d,n) for d,n in diff if n>=30)
        result['cases'].append(dict(case=p.name,actual_mapping=mapping,largest_mean_transition_after_frame30=peak,maximum_black_pixels_in_reduced_frame=max(black),decoded_frames=index,statistics_scope='160x160 review locator; no production statistics change'))
    canvas.save(OUT/'boundary_display_comparison.png')
    (OUT/'boundary_display_isolation.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
