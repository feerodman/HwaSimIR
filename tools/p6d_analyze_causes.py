"""Same-band, same-frame local defect measurements; no image tone changes."""
from pathlib import Path
import json,csv,re
import numpy as np
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'logs/p6d'
def pfm(p):
    with p.open('rb') as f:
        assert f.readline().strip()==b'PF'
        w,h=map(int,f.readline().split());scale=float(f.readline())
        return np.frombuffer(f.read(),'<f4' if scale<0 else '>f4').reshape(h,w,3)
def rgb(p):return np.asarray(Image.open(p).convert('RGB'),float)
def stats(a):return dict(min=float(a.min()),max=float(a.max()),mean=float(a.mean()),p99=float(np.percentile(a,99)))
def main():
    rows=[];numeric=[]
    evidence=json.loads((OUT/'evidence.json').read_text()) if (OUT/'evidence.json').exists() else {'cases':{}}
    interpretation={
        'base':'同帧冻结映射基线；黑纹已存在于显示前公共缩放浮点。',
        'art1':'R 不变只关闭人工明暗后黑带消失，支持 G/人工明暗为主因；不是最终外观。',
        'uniform':'均匀密度和固定源色不产生原纹理黑带；代理轮廓仍是几何近似。',
        'volumes':'关闭片云仍有原黑带，排除片云为该内部黑带的唯一原因。',
        'near':'仅保留近云仍有黑带，排除必须双云才产生。',
        'far':'远云自身保留方向性结构，说明它也有源/挤出问题。',
        'background':'背景参考，用于分离云造成的像素变化。',
        'sheet':'无体积云参考；与零密度比较揭示圆形挖空副作用。',
        'steps8':'实际循环上限 64，实际 uniform 8；基线低步数局部误差。',
        'steps16':'增加步数减轻采样斑点，方向性暗带仍存在。',
        'steps32':'高步数保留平滑带状结构，反对仅靠步数修复。',
        'steps64':'实际 64 次上限参考仍有方向性软带，支持密度挤出也需要修改。',
        'density':'R 通道作为源的透明可视化；不冒充直接二维物理密度。',
        'alpha':'单位源色的透明可视化，可结合背景估计合成透明度。',
        'source':'原 G 通道作为源的透明可视化；同时保留 RG 体切片。',
        'zero':'原零密度仍改变片云：圆形代理挖空有误；修复后两波段全图差值为 0。',
        'numeric':'GPU source-over 与独立 CPU 解析式一致，未发现重复乘 alpha。'}
    roi=np.s_[475:580,70:670,0]
    for band in (1,2):
        base=OUT/f'isolate_b{band}_base'
        if not (base/'linear.pfm').exists():continue
        ref=pfm(base/'linear.pfm');items=[]
        for case in ('base','art1','uniform','volumes','near','far','background','sheet','steps8','steps16','steps32','steps64','density','alpha','source','zero','numeric'):
            p=OUT/f'isolate_b{band}_{case}'
            verified=OUT/f'verified_b{band}_{case}'
            if (verified/'linear.pfm').exists():p=verified
            if not(p/'linear.pfm').exists():continue
            a=pfm(p/'linear.pfm');enc=rgb(p/'linear_rgb8.png');dec=rgb(p/'received.png')
            log=(p/'board.log').read_text(encoding='utf-8-sig',errors='replace')
            ray=re.findall(r'visibleCloudVolumes=[1-9]\d* averageRaySteps=([\d.]+)',log)
            perf=evidence['cases'].get(p.name,{}).get('timing',{}).get('renderMs',{})
            mapping=re.findall(r'\[DisplayFrameMapping\][^\n]+',log)
            r=dict(band=band,case=case,sequence=90,local_defect_roi='x70:670 y475:580',linear_roi_min=float(a[roi].min()),linear_roi_max=float(a[roi].max()),linear_roi_mean=float(a[roi].mean()),
                   display_roi_black_pixels=int((enc[roi]<1).sum()),decode_roi_black_pixels=int((dec[roi]<1).sum()),linear_roi_max_abs_difference=float(np.abs(a-ref)[roi].max()),
                   codec_roi_max_abs_difference=float(np.abs(enc-dec)[roi].max()),codec_roi_p99_abs_difference=float(np.percentile(np.abs(enc-dec)[roi],99)),
                   pixel_impact_fraction=float(np.mean(np.abs(a[:,:,0]-ref[:,:,0])>1.e-4)),actual_average_ray_steps=ray[-1] if ray else 0,
                   measured_render_ms_interval_mean=perf.get('window_mean','unavailable'),timing_scope='diagnostic case including explicit capture; not production throughput',
                   actual_mapping=mapping[-1] if mapping else '',supports_or_excludes=interpretation[case],evidence_directory=p.name)
            rows.append(r);items.append((case,p/'received.png'))
            Image.open(p/'received.png').crop((70,475,670,580)).save(p/'defect_crop.png')
            if case=='numeric':
                # Independent closed-form source-over arithmetic, not shader outputs.
                expected=[.2*(1-.5)+.8*.5,.4,.4,.4,.4,.2,.2*(1-.4)*(1-.5)+.8*(1-(1-.4)*(1-.5)),(.2*(1-.4)+.3*.4)*(1-.6)+.8*.6]
                actual=[float(np.median(a[100:700,i*100+20:i*100+80,0])) for i in range(8)]
                error=max(abs(x-y) for x,y in zip(actual,expected))
                numeric.append(dict(band=band,expected=expected,actual=actual,tolerance=.001,max_error=error,passed=error<.001))
        canvas=Image.new('RGB',(1200,430*((len(items)+2)//3)),(25,25,25));draw=ImageDraw.Draw(canvas)
        for i,(name,path) in enumerate(items):
            x,y=i%3*400,i//3*430;canvas.paste(Image.open(path).resize((400,400)),(x,y));draw.text((x+8,y+405),f'Band {band} / {name}',fill='white')
        canvas.save(OUT/f'cause_b{band}.png')
    if rows:
        with (OUT/'cause_matrix.csv').open('w',newline='',encoding='utf-8-sig') as f:
            w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
    (OUT/'numeric_blend.json').write_text(json.dumps(numeric,indent=2)+'\n')
    print(json.dumps({'cases':len(rows),'numeric':numeric},indent=2))
if __name__=='__main__':main()
