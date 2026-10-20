"""从已认可 P17 密度派生紧支撑云团；不覆盖原密度、模型或输入。"""
from pathlib import Path
import json, hashlib, argparse
ap=argparse.ArgumentParser();ap.add_argument("--puffs",type=int,choices=[8,12,16,24],default=12);args=ap.parse_args();COUNT=args.puffs
import numpy as np
from scipy.optimize import nnls

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / 'HwaSim_IR/Bin/Config'
OUT = ROOT / ('logs/p20/puff_templates_'+str(COUNT))
OUT.mkdir(parents=True, exist_ok=True)
manifest = json.loads((CONFIG/'Weather/Derived/cloud_manifest_p17.json').read_text(encoding='utf-8'))
templates = []
reports = []
for key, item in manifest['Templates'].items():
    raw = (CONFIG/item['Path']).read_bytes()
    n = manifest['Size']
    rgba = np.frombuffer(raw, np.uint8).reshape(n,n,n,4).astype(float)/255
    # 固定对象空间采样网格；相机移动不重建资源，也不复用屏幕历史。
    axis = (np.arange(1,n,3)+.5)/n*2-1
    z,y,x = np.meshgrid(axis,axis,axis,indexing='ij')
    xyz = np.stack([x.ravel(),y.ravel(),z.ravel()],axis=1)
    samples = rgba[1::3,1::3,1::3].reshape(-1,4)
    density = samples[:,0]
    rng = np.random.default_rng(20201017+int(key[1:]))
    centers = [xyz[np.argmax(density)]]
    for _ in range(COUNT-1):
        dist = np.min(np.sum((xyz[:,None,:]-np.array(centers)[None,:,:])**2,axis=2),axis=1)
        probability = dist*density
        centers.append(xyz[rng.choice(len(xyz),p=probability/probability.sum())])
    centers = np.array(centers)
    for _ in range(35):
        assignment = np.argmin(np.sum((xyz[:,None,:]-centers[None,:,:])**2,axis=2),axis=1)
        for i in range(COUNT):
            mask = assignment==i
            w = density[mask]
            if w.sum()>1e-9: centers[i] = np.sum(xyz[mask]*w[:,None],axis=0)/w.sum()
    radii = []
    for i,c in enumerate(centers):
        mask = assignment==i
        w = density[mask]
        variance = np.sum((xyz[mask]-c)**2*w[:,None],axis=0)/max(w.sum(),1e-9)
        r = np.maximum(.065, np.sqrt(variance)*3.5)
        # 保证整个紧支撑椭球在原代理球内，不在代理边界截掉光学厚度。
        r *= min(1.,(.985-np.linalg.norm(c))/max(r))
        radii.append(r)
    radii = np.array(radii)
    q = np.sum(((xyz[:,None,:]-centers[None,:,:])/radii[None,:,:])**2,axis=2)
    basis = np.maximum(0,1-q)**2
    amplitude = nnls(basis,density)[0]
    reconstructed = basis@amplitude
    # 质量守恒校正独立于相机；改变表示而不是减小云量来提速。
    amplitude *= density.sum()/max(reconstructed.sum(),1e-9)
    reconstructed = basis@amplitude
    puffs = []
    for i in range(COUNT):
        weight = basis[:,i]*density
        light = np.sum(samples[:,3]*weight)/max(weight.sum(),1e-9)
        column = np.sum(samples[:,1]*weight)/max(weight.sum(),1e-9)
        puffs.append([*centers[i],*radii[i],amplitude[i],light,column])
    templates.append(puffs)
    reports.append(dict(template=key,inputSha256=hashlib.sha256(raw).hexdigest(),
                        puffs=COUNT,densityMae=float(np.mean(abs(reconstructed-density))),
                        massRatio=float(reconstructed.sum()/density.sum()),
                        maxDensity=float(reconstructed.max()),visualAcceptance='PENDING'))
(OUT/'templates.json').write_text(json.dumps(templates,indent=2),encoding='utf-8')
(OUT/'derivation.json').write_text(json.dumps(reports,indent=2),encoding='utf-8')
# 两个候选分别派生并保存；默认使用24团，不把12团声称为数值等价。
paths=[ROOT/('logs/p20/puff_templates_'+str(n)+'/templates.json') for n in (12,24)]
if all(p.exists() for p in paths):
    header='// P20 世界空间云团：运行生成器 --puffs 12 和 --puffs 24 可复现。\n#pragma once\n'
    for count,path in zip((12,24),paths):
        header+='static const float kP20PuffTemplates'+str(count)+'[4][24][9] = {\n'
        for puffs in json.loads(path.read_text(encoding='utf-8')):
            header+='{\n'+''.join('{'+','.join(f'{v:.9f}f' for v in puff)+'},\n' for puff in puffs)+'},\n'
        header+='};\n'
    (ROOT/'HwaSim_IR/HwaSim_IR/IR/IRCloudPuffTemplates.h').write_text(header,encoding='utf-8-sig')
print(json.dumps(reports,indent=2))
