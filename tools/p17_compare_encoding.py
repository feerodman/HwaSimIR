"""Same-run raw/Stage6/received comparison. Not a motion-flicker score."""
import json,pathlib,re,subprocess
import numpy as np
from PIL import Image
from scipy.ndimage import binary_erosion
from p17_collect_evidence import read_pfm,sha
ROOT=pathlib.Path(__file__).resolve().parents[1]
def main():
    src=ROOT/'logs/p17/diagnostics/after/p17_hdr_mwir_narrow_fov'
    movie=next(src.glob('recording/*/output.mp4'));idx=next(src.glob('recording/*/frame_index.jsonl'))
    rows=[json.loads(s) for s in idx.read_text(encoding='utf-8-sig').splitlines()]
    assert len({(r['session'],r['generation'],r['run']) for r in rows})==1
    out=ROOT/'logs/p17/diagnostics/encoding_alignment';out.mkdir(exist_ok=True)
    report=[]
    for pfm in sorted(src.glob('diagnostic_linear/*seq*.pfm')):
        seq=int(re.search(r'_seq(\d+)\.pfm$',pfm.name)[1]);r=next(r for r in rows if int(r['sourceSeq'])==seq)
        png=out/f'sourceSeq_{seq}_decoded.png'
        subprocess.run(['ffmpeg','-nostdin','-v','error','-y','-i',str(movie),'-vf',f'select=eq(n\\,{int(r["storageIndex"])-1})','-frames:v','1',str(png)],check=True)
        raw=read_pfm(pfm);pre=np.array(Image.open(pfm.with_name(pfm.stem+'_rgb8.png')).convert('RGB'));dec=np.array(Image.open(png).convert('RGB'))
        roi=np.zeros(raw.shape[:2],bool);roi[300:500,300:500]=True
        # This is a declared crop, not an inferred model-interior mask. Include
        # cold body and hot plume; exclude green annotation from encoding stats.
        neutral=np.ptp(pre.astype(int),axis=2)<=2
        region=roi&neutral
        err=np.abs(dec.astype(float)-pre.astype(float))[region]
        safe=roi&binary_erosion(neutral,iterations=3)
        grayerr=np.abs(np.sum((dec.astype(float)-pre.astype(float))*[.299,.587,.114],axis=2))[safe]
        report.append(dict(sourceSeq=seq,identity=r,rawPfmSha256=sha(pfm),preencodePngSha256=sha(pfm.with_name(pfm.stem+'_rgb8.png')),decodedPngSha256=sha(png),floatFinite=bool(np.isfinite(raw).all()),rawRoiMin=float(raw[roi].min()),rawRoiMax=float(raw[roi].max()),neutralRoiPixels=int(region.sum()),encodingAbsoluteChannelError=dict(mean=float(err.mean()),p95=float(np.percentile(err,95)),p99=float(np.percentile(err,99)),max=float(err.max())),encodingGrayErrorAwayFromAnnotation=dict(annotationExclusionRadiusPx=3,pixels=int(safe.sum()),mean=float(grayerr.mean()),p95=float(np.percentile(grayerr,95)),p99=float(np.percentile(grayerr,99)),max=float(grayerr.max()))))
    (out/'comparison.json').write_text(json.dumps(dict(scope='same-run source identity alignment; ROI [300,300,500,500], annotation excluded; encoding error only, no temporal motion compensation',qt='submit timestamp retained, actual per-paint pixels not captured',calibration='NOT_VERIFIED_CALIBRATION',frames=report),indent=2))
    print(json.dumps(dict(frames=len(report),p99Range=[min(x['encodingAbsoluteChannelError']['p99'] for x in report),max(x['encodingAbsoluteChannelError']['p99'] for x in report)])))
if __name__=='__main__':main()
