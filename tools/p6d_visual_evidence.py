"""Unretouched local crops and slices, plus fixed-mapping DDS comparison panels."""
from pathlib import Path
import json
import numpy as np
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'logs/p6d';CONFIG=ROOT/'HwaSim_IR/Bin/Config'
def main():
    for band in (1,2):
        panels=[]
        for kind,case in [('before',f'isolate_b{band}_base'),('after',f'visual_b{band}_base')]:
            p=OUT/case/'received.png';assert p.exists(),p
            im=Image.open(p).convert('RGB');im.crop((70,475,670,580)).save(OUT/case/'defect_crop.png')
            panels.append(im)
        canvas=Image.new('RGB',(1600,836),'#16191d');draw=ImageDraw.Draw(canvas)
        for i,im in enumerate(panels):canvas.paste(im,(800*i,0));draw.text((800*i+12,810),f'Band {band}: '+('BEFORE' if i==0 else 'AFTER')+' / same sequence 90 and frozen mapping',fill='white')
        canvas.save(OUT/f'comparison_b{band}.png')
        crop=Image.new('RGB',(600,260),'#16191d');draw=ImageDraw.Draw(crop)
        for i,im in enumerate(panels):crop.paste(im.crop((70,475,670,580)),(0,i*130));draw.text((8,i*130+108),'BEFORE' if i==0 else 'AFTER',fill='white')
        crop.save(OUT/f'defect_comparison_b{band}.png')
    source_slices()
    print('Fixed-mapping comparisons and source slices saved without tone changes')

def source_slices():
    old=json.loads((CONFIG/'Weather/Derived/cloud_manifest.json').read_text());new=json.loads((CONFIG/'Weather/Derived/cloud_manifest_p6d.json').read_text())
    canvas=Image.new('RGB',(6*240,4*268),'#16191d');draw=ImageDraw.Draw(canvas)
    for row,key in enumerate(new['Templates']):
        t=new['Templates'][key];source=Image.open(CONFIG/t['Source']).convert('RGBA')
        a=np.frombuffer((CONFIG/old['Templates'][key]['Path']).read_bytes(),np.uint8).reshape(96,96,96,4)
        b=np.frombuffer((CONFIG/t['Path']).read_bytes(),np.uint8).reshape(96,96,96,4)
        items=[('PNG RGB',source.convert('RGB')),('PNG alpha',source.getchannel('A')),('old R XZ',Image.fromarray(a[:,48,:,0])),('new R XZ',Image.fromarray(b[:,48,:,0])),('new G XZ',Image.fromarray(b[:,48,:,1])),('new R XY',Image.fromarray(b[48,:,:,0]))]
        for col,(label,im) in enumerate(items):
            canvas.paste(im.convert('RGB').resize((240,240),Image.Resampling.NEAREST),(col*240,row*268))
            draw.text((col*240+6,row*268+244),key+' '+label,fill='white')
    canvas.save(OUT/'source/final_slices.png')
if __name__=='__main__':
    import sys
    source_slices() if '--slices-only' in sys.argv else main()
