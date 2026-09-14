"""Compare actual decoded sequences by production source identity, not arrival."""
from pathlib import Path
import argparse,csv,json,subprocess
import numpy as np
from PIL import Image

def read_exact(pipe,size):
    data=bytearray()
    while len(data)<size:
        block=pipe.read(size-len(data))
        if not block:break
        data.extend(block)
    return data

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--ffmpeg',required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('cases',type=Path,nargs='+');a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    allrows=[];summary={}
    for case in a.cases:
        directory=next((case/'recording').glob('*/output.mp4')).parent
        index=[json.loads(x) for x in (directory/'frame_index.jsonl').read_text().splitlines()]
        w,h=index[0]['width'],index[0]['height']
        command=[a.ffmpeg,'-v','error','-i',str(directory/'output.mp4'),'-map','0:v:0','-an',
                 '-fps_mode','passthrough','-enc_time_base','1:1000000','-pix_fmt','gray','-f','rawvideo','-']
        child=subprocess.Popen(command,stdout=subprocess.PIPE)
        previous=None;rows=[]
        for entry in index:
            raw=read_exact(child.stdout,w*h);assert len(raw)==w*h
            image=np.frombuffer(raw,np.uint8).reshape(h,w)
            seq=int(entry['sourceSeq'])
            row=dict(case=case.name,sourceSeq=seq,frameSeq=int(entry['frameSeq']),mean=float(image.mean()),
                cloudRoiMean=float(image[h//5:h*4//5,w//8:w*7//8].mean()),
                meanStep=0. if previous is None else float(image.mean()-previous.mean()),
                pixelDeltaP95=0. if previous is None else float(np.percentile(np.abs(image.astype(float)-previous),95)))
            if seq in [1,120,238,250,260,261,262,263,275,360,480,600]:
                Image.fromarray(image).save(a.output/f'{case.name}_source_{seq:05d}.png')
            previous=image;rows.append(row)
        assert child.stdout.read(1)==b'';assert child.wait()==0
        maximum=max(rows[1:],key=lambda x:abs(x['meanStep']))
        summary[case.name]=dict(frames=len(rows),largestMeanStep=maximum)
        allrows.extend(rows)
    with (a.output/'display_sequence.csv').open('w',newline='',encoding='utf-8-sig') as out:
        writer=csv.DictWriter(out,fieldnames=allrows[0].keys());writer.writeheader();writer.writerows(allrows)
    (a.output/'summary.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig,ax=plt.subplots(figsize=(12,5))
    for case in a.cases:
        rows=[r for r in allrows if r['case']==case.name]
        ax.plot([r['sourceSeq'] for r in rows],[r['mean'] for r in rows],label=case.name)
    ax.set_xlabel('Production source sequence');ax.set_ylabel('Decoded gray mean (0-255)');ax.legend();ax.grid(alpha=.2)
    fig.tight_layout();fig.savefig(a.output/'display_mean.png',dpi=150)
    print(json.dumps(summary))
