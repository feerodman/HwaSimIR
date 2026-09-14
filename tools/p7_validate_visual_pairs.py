"""Local defect/pixel influence checks on actual GPU dumps and received pixels.

The PFM domain is public scaled scene-linear gray, never physical radiance.
"""
import argparse,csv,json
from pathlib import Path
import numpy as np
from PIL import Image
from scipy.ndimage import label

def pfm(path):
    with path.open('rb') as f:
        kind=f.readline().strip();assert kind in (b'PF',b'Pf')
        line=f.readline()
        while line.startswith(b'#'):line=f.readline()
        width,height=map(int,line.split());scale=float(f.readline())
        data=np.frombuffer(f.read(),dtype='<f4' if scale<0 else '>f4')
        return np.flipud(data.reshape(height,width,3 if kind==b'PF' else 1)).copy()

def compare(base,other,domain,threshold):
    a=pfm(base/'linear.pfm') if domain=='common_linear' else np.asarray(Image.open(base/'received.png').convert('RGB'),dtype=float)
    b=pfm(other/'linear.pfm') if domain=='common_linear' else np.asarray(Image.open(other/'received.png').convert('RGB'),dtype=float)
    assert a.shape==b.shape
    difference=np.max(np.abs(a-b),axis=2);mask=difference>threshold
    components,count=label(mask);areas=np.bincount(components.ravel())[1:]
    rows=[]
    for y in range(0,a.shape[0],100):
        for x in range(0,a.shape[1],100):
            tile=difference[y:y+100,x:x+100]
            rows.append(dict(x=x,y=y,changed=int((tile>threshold).sum()),maximum=float(tile.max()),p99=float(np.percentile(tile,99))))
    return dict(domain=domain,threshold=threshold,changedPixels=int(mask.sum()),maximum=float(difference.max()),
        largestChangedRegion=int(areas.max()) if count else 0,tiles=rows)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,default=Path('logs/p7'));a=p.parse_args()
    results=[];tiles=[]
    for weather in ('rain','snow'):
        base=a.root/f'D_pixels_{weather}_both'
        for variant in ('hide_first','hide_second','off'):
            other=a.root/f'D_pixels_{weather}_{variant}'
            for domain,threshold in [('common_linear',1e-5),('DDS_RGB8',3)]:
                result=compare(base,other,domain,threshold)
                for row in result.pop('tiles'):tiles.append(dict(weather=weather,variant=variant,domain=domain,**row))
                result.update(weather=weather,variant=variant,base=str(base),other=str(other))
                result['result']='PASS' if result['changedPixels']>100 else 'FAIL'
                results.append(result)
    # Same source frame at 4 s, above the 2500 m rain ceiling: no rain influence.
    result=compare(a.root/'D_rain_altitude_on',a.root/'D_rain_altitude_off','common_linear',1e-5)
    result.pop('tiles');result.update(weather='rain',variant='above_ceiling',result='PASS' if result['changedPixels']==0 else 'FAIL')
    results.append(result)
    (a.root/'visual_pixel_validation.json').write_text(json.dumps(dict(results=results,pfmMeaning='common_scaled_scene_linear_not_physical_radiance'),indent=2),encoding='utf-8')
    with (a.root/'visual_pixel_tiles.csv').open('w',newline='',encoding='utf-8-sig') as f:
        w=csv.DictWriter(f,fieldnames=tiles[0]);w.writeheader();w.writerows(tiles)
    print(json.dumps(results))
    if any(r['result']!='PASS' for r in results):raise SystemExit(1)
