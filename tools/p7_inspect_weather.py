"""Read existing source formats and alpha; derived channel views are diagnostics."""
from pathlib import Path
import csv,hashlib,json
import numpy as np
from PIL import Image
root=Path(__file__).resolve().parents[1]
textures=root/'HwaSim_IR/Bin/Config/Weather/Textures'
out=root/'logs/p7/weather_source';out.mkdir(parents=True,exist_ok=True)
rows=[]
for name in ['rain.rgba','rain.sgi','rain_shaft.png','snow.rgba','cloud_scattered.png',
             'cloud3d_001.png','cloud3d_002.png','cloud3d_014.png','cloud3d_020.png']:
    path=textures/name;image=Image.open(path);rgba=np.array(image.convert('RGBA'));alpha=rgba[...,3]
    rows.append(dict(path=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        magic=path.read_bytes()[:12].hex(),format=image.format,mode=image.mode,width=image.width,height=image.height,
        alphaMin=int(alpha.min()),alphaMax=int(alpha.max()),alphaZeroFraction=float(np.mean(alpha==0)),
        alphaOpaqueFraction=float(np.mean(alpha==255)),alphaMean=float(alpha.mean())))
    # Display RGB and alpha separately; original files remain unchanged.
    Image.fromarray(rgba[...,:3]).save(out/(name+'.rgb.png'))
    Image.fromarray(alpha).save(out/(name+'.alpha.png'))
(out/'source_inventory.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
with (out/'source_inventory.csv').open('w',newline='',encoding='utf-8-sig') as f:
    writer=csv.DictWriter(f,fieldnames=rows[0].keys());writer.writeheader();writer.writerows(rows)
print(json.dumps(rows))
