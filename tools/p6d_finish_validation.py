"""Final source/resource boundaries and same-frame pixel-impact assertions."""
from pathlib import Path
import json,hashlib,subprocess
import numpy as np
from p6d_analyze_causes import pfm
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'logs/p6d'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    rows=[];zero_rows=[];numeric_rows=[];sampling=[]
    for band in (1,2):
        volume=OUT/f'visual_b{band}_volumes/linear.pfm'
        assert volume.exists(), f'missing final volume capture for band {band}'
        both=pfm(volume)
        for case,removed in [('near','198EE8338358C182'),('far','D73EEA5ECB3EA0B8')]:
            p=OUT/f'visual_b{band}_{case}/linear.pfm';a=pfm(p);delta=np.abs(a-both)[:,:,0]
            pixels=int((delta>1.e-4).sum())
            rows.append(dict(band=band,removedCloudId=removed,changed_pixels=pixels,fraction=pixels/delta.size,maximum_difference=float(delta.max()),passed=pixels>1000))
        z=pfm(OUT/f'visual_b{band}_zero/linear.pfm');sheet=pfm(OUT/f'visual_b{band}_sheet/linear.pfm')
        zero_error=float(np.max(np.abs(z-sheet)));assert zero_error==0,'zero-density identity failed'
        zero_rows.append(dict(band=band,full_image_max_difference=zero_error,passed=True))
        n=pfm(OUT/f'visual_b{band}_numeric/linear.pfm')
        expected=[.2*(1-.5)+.8*.5,.4,.4,.4,.4,.2,.2*.6*.5+.8*(1-.6*.5),(.2*.6+.3*.4)*.4+.8*.6]
        actual=[float(np.median(n[100:700,i*100+20:i*100+80,0])) for i in range(8)]
        error=max(abs(x-y) for x,y in zip(actual,expected));assert error<.001
        numeric_rows.append(dict(band=band,expected=expected,actual=actual,max_error=error,tolerance=.001,passed=True,scope='GPU straight-alpha numerical cards, independent CPU source-over expected values'))
        low=pfm(OUT/f'visual_b{band}_base/linear.pfm');high=pfm(OUT/f'visual_b{band}_steps64/linear.pfm');d=np.abs(low-high)[:,:,0]
        sampling.append(dict(band=band,full_max=float(d.max()),roi_max=float(d[475:580,70:670].max()),roi_mean=float(d[475:580,70:670].mean()),scope='8 versus 64 steps, same new cache and frozen mapping; 64 is a numerical reference, not truth'))
    (OUT/'actual_cloud_pixel_impact.json').write_text(json.dumps(rows,indent=2)+'\n')
    assert len(rows)==4 and all(x['passed'] for x in rows),'cloud selection without actual pixel effect'
    (OUT/'final_compositing_validation.json').write_text(json.dumps(dict(zero_density=zero_rows,numerical_cards=numeric_rows,sampling_reference=sampling),indent=2)+'\n')
    profile=[]
    baseline={r['path']:r for r in json.loads((OUT/'baseline/source.json').read_text(encoding='utf-8-sig'))['files']}
    for name in ['default_NVG.json','default_MWIR.json']:
        p=ROOT/'HwaSim_IR/Bin/Config/SensorWave'/name
        original=subprocess.check_output(['git','show','95b810c477a327f44c3ba303dad99971159c6fe3:'+str(p.relative_to(ROOT)).replace('\\','/')],cwd=ROOT)
        relative=str(p.relative_to(ROOT)).replace('\\','/')
        assert baseline[relative]['sha256']==sha(p),name+' changed from initial checkout bytes'
        # Git stores LF while this checkout uses CRLF. Check exact original
        # checkout bytes above, and normalized repository content separately.
        assert original.replace(b'\r\n',b'\n')==p.read_bytes().replace(b'\r\n',b'\n'),name+' differs from HEAD'
        profile.append(dict(path=relative,sha256=sha(p),initial_checkout_bytes_unchanged=True,head_content_unchanged=True))
    config=ROOT/'HwaSim_IR/Bin/Config/Weather';old=json.loads(subprocess.check_output(['git','show','95b810c477a327f44c3ba303dad99971159c6fe3:HwaSim_IR/Bin/Config/Weather/world_cloud_game.json'],cwd=ROOT))
    new=json.loads((config/'world_cloud_game.json').read_text());assert old['World']==new['World']
    manifest=json.loads((config/'Derived/cloud_manifest_p6d.json').read_text());assets=[]
    for t in manifest['Templates'].values():
        source=ROOT/'HwaSim_IR/Bin/Config'/t['Source'];cache=ROOT/'HwaSim_IR/Bin/Config'/t['Path']
        assert sha(source)==t['SourceSHA256'];assert sha(cache)==t['SHA256']
        assets.append(dict(key=t['Key'],source=t['Source'],sourceSHA256=sha(source),cache=t['Path'],cacheSHA256=sha(cache)))
    result=dict(world_geometry_unchanged=True,profiles=profile,assets=assets,sensor_experiment_source_unchanged=not subprocess.check_output(['git','diff','--name-only','--','experiments/ordinary_sensor_lab'],cwd=ROOT).strip())
    (OUT/'final_resource_validation.json').write_text(json.dumps(result,indent=2)+'\n');print('resource boundaries passed; pixel proof rows',len(rows))
if __name__=='__main__':main()
