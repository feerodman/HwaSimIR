"""Extract actual received video frames for review; never interpolate frames."""
from pathlib import Path
import subprocess,json
import numpy as np
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'logs/p6d'
FFMPEG=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe'
def main():
    cases=[f'before_pan_b{b}' for b in (1,2)]+[f'visual_b{b}_{v}' for b in (1,2) for v in ('pair_pan','near_translate','proxy_crossing')]+[f'default_b1_{v}' for v in ('near_translate','proxy_crossing')]
    rows=[]
    for case in cases:
        folder=OUT/case;video=folder/'received.mp4'
        assert video.exists(),video
        frames=folder/'review_frames';frames.mkdir(exist_ok=True)
        canvas=Image.new('RGB',(1200,830),'#16191d');draw=ImageDraw.Draw(canvas)
        for i,t in enumerate((1,4,8,12,16,20)):
            p=frames/f'second_{t:02d}.png'
            r=subprocess.run([str(FFMPEG),'-v','error','-y','-ss',str(t),'-i',str(video),'-frames:v','1',str(p)],capture_output=True,text=True)
            assert r.returncode==0 and p.exists(),(case,t,r.stderr)
            im=Image.open(p);canvas.paste(im.resize((400,400)),(i%3*400,i//3*415));draw.text((i%3*400+8,i//3*415+401),f'{case} / received video {t}s',fill='white')
        target=OUT/(case+'_video_review.png');canvas.save(target);rows.append(dict(case=case,frames_seconds=[1,4,8,12,16,20],contact_sheet=str(target.relative_to(ROOT))))
    transitions=[]
    for case in ('default_b1_proxy_crossing','visual_b2_proxy_crossing'):
        video=OUT/case/'received.mp4'
        # This reduced image is only a video review event locator. It is not
        # the production display statistics path or a cloud correctness metric.
        proc=subprocess.Popen([str(FFMPEG),'-v','error','-i',str(video),'-vf','scale=160:160:flags=area','-pix_fmt','gray','-f','rawvideo','-'],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        previous=None;changes=[];index=0
        while True:
            data=proc.stdout.read(160*160)
            if not data:break
            assert len(data)==160*160
            a=np.frombuffer(data,np.uint8).astype(np.int16)
            if previous is not None:changes.append((float(np.abs(a-previous).mean()),index))
            previous=a;index+=1
        error=proc.stderr.read().decode(errors='replace');assert proc.wait()==0,error
        selected=[]
        for delta,i in sorted(changes,reverse=True):
            if all(abs(i-j)>30 for _,j in selected):selected.append((delta,i))
            if len(selected)==3:break
        sheet=Image.new('RGB',(1200,830),'#16191d');draw=ImageDraw.Draw(sheet)
        for col,(delta,i) in enumerate(selected):
            for row,n in enumerate((i-1,i)):
                p=OUT/case/'review_frames'/f'transition_frame_{n}.png'
                r=subprocess.run([str(FFMPEG),'-v','error','-y','-i',str(video),'-vf',f'select=eq(n\\,{n})','-frames:v','1',str(p)],capture_output=True,text=True)
                assert r.returncode==0 and p.exists(),r.stderr
                sheet.paste(Image.open(p).resize((400,400)),(col*400,row*415));draw.text((col*400+8,row*415+401),f'frame {n} / reduced mean delta {delta:.3f}',fill='white')
        sheet.save(OUT/(case+'_transitions.png'));transitions.append(dict(case=case,decoded_frames=index,largest_separated_transitions=[dict(mean_gray_delta=d,frame=i) for d,i in selected],scope='160x160 event locator only; inspect adjacent original decoded frames'))
    (OUT/'video_transition_review.json').write_text(json.dumps(transitions,indent=2)+'\n')
    (OUT/'video_review_manifest.json').write_text(json.dumps(rows,indent=2)+'\n');print('Actual video review sheets:',len(rows))
if __name__=='__main__':main()
