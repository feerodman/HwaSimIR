"""导出未经增强的解码帧和中心原像素ROI；不修改生产输入/画面。"""
from pathlib import Path
import argparse,subprocess,hashlib,json
from PIL import Image
root=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--prefix',required=True);a=ap.parse_args()
out=root/'logs/p20/visual';out.mkdir(exist_ok=True)
ff=root/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe'
cases={'baseline24_cloudy_original_r1':[1,300,480,600],a.prefix+'_cloudy_original_r1':[1,300,480,600],a.prefix+'_full_mwir_coexist':[1,1200,3000,4200],a.prefix+'_full_swir_no_vd':[1,1200,3000,4200]}
records=[]
for name,frames in cases.items():
 video=root/'logs/p20/runs'/name/'precise/output.mp4'
 if not video.exists():continue
 for frame in frames:
  path=out/f'{name}_frame{frame}.png'
  subprocess.run([str(ff),'-v','error','-i',str(video),'-vf',f'select=eq(n\\,{frame-1})','-frames:v','1','-update','1','-y',str(path)],check=True)
  with Image.open(path) as image:
   w,h=image.size;box=(w//2-64,h//2-64,w//2+64,h//2+64)
   roi=path.with_name(path.stem+'_roi.png');image.crop(box).save(roi)
  records.append(dict(case=name,frameOneBased=frame,size=[w,h],roiBox=box,enhancement=False,sourceVideo=str(video.relative_to(root)).replace('\\','/'),png=path.name,pngSha256=hashlib.sha256(path.read_bytes()).hexdigest(),roi=roi.name))
(out/(a.prefix+'_manifest.json')).write_text(json.dumps(dict(scope='同输入序号前后全帧和原像素中心128x128 ROI；解码PNG未增强，ROI没有缩放；不把形态差异MAE作为光学验收。',records=records),ensure_ascii=False,indent=2),encoding='utf-8')
print('导出',len(records),'帧')
