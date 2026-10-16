"""Labeled contact sheets of actual received frames, originals remain alongside."""
import json
from pathlib import Path
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'logs/p18/visual';index=[]
for band in ['swir','mwir']:
    groups=[('views',[f'nozzle_{band}_{view}_v3' for view in ['side','oblique','end','near','far','occluded']]),
            ('layers',[f'diagnostic_{band}_{mode}_v3' for mode in ['core','halo','mask']])]
    for kind,names in groups:
        sheet=Image.new('RGB',(1440,520*((len(names)+2)//3)),(20,24,30));d=ImageDraw.Draw(sheet);present=[]
        for i,name in enumerate(names):
            path=ROOT/f'logs/p18/runs/{name}/customer/last.png';x=(i%3)*480;y=(i//3)*520
            d.text((x+8,y+8),name,fill='white');d.text((x+8,y+26),'DIAGNOSTIC ONLY / preview scaled 800->480',fill='white')
            if path.exists():
                sheet.paste(Image.open(path).convert('RGB').resize((480,480)),(x,y+40));present.append(path.relative_to(ROOT).as_posix())
            else:d.text((x+30,y+100),'NOT YET CAPTURED',fill='#ffae9e')
        if present:
            dest=OUT/f'{band}_{kind}_contact.png';sheet.save(dest)
            index.append(dict(band=band,group=kind,sources=present,image=dest.relative_to(ROOT).as_posix(),diagnosticOnly=True,displayRescaledForContactSheet=True))
(OUT/'diagnostic_contact_index.json').write_text(json.dumps(index,indent=2),encoding='utf-8')
