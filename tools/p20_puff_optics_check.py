"""解析局部光程对独立密集积分参考；这是数值测试，不冒充板端像素证据。"""
from pathlib import Path
import json, math
import numpy as np

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'logs/p20/optics';OUT.mkdir(parents=True,exist_ok=True)
contract=dict(schema='p20.optics.contract.v1',seed=20201017,rays=320,
    referenceSteps=16384,transmissionAbsTolerance=0.0001,normalizedSourceAbsTolerance=0.02,
    relations=['front','inside','behind','edge','camera_inside'],
    measuredHardwarePixels=False,sourceLayers=6)
# 判据在生成观测结果之前落盘，不能根据失败结果放宽。
(OUT/'contract.json').write_text(json.dumps(contract,indent=2),encoding='utf-8')
templates=np.array(json.loads((ROOT/'logs/p20/puff_templates/templates.json').read_text(encoding='utf-8')))
rng=np.random.default_rng(contract['seed'])

def intersect(puffs,origin,direction,end):
    o=(origin-puffs[:,:3])/puffs[:,3:6]
    v=direction/puffs[:,3:6]
    speed=np.linalg.norm(v,axis=1);d=v/speed[:,None]
    center=-np.sum(o*d,axis=1)
    q=1-np.sum((o+d*center[:,None])**2,axis=1)
    h=np.sqrt(np.maximum(0,q))
    enter=np.maximum(0,(center-h)/speed);leave=np.minimum(end,(center+h)/speed)
    valid=(q>0)&(leave>enter)
    return speed,center,q,enter,leave,valid

def analytic(puffs,origin,direction,end,sources):
    speed,center,q,enter,leave,valid=intersect(puffs,origin,direction,end)
    if not valid.any():return 1.,0.
    front=enter[valid].min();back=leave[valid].max()
    T=1.;L=0.
    for layer in range(6):
        lo=front+(back-front)*layer/6;hi=front+(back-front)*(layer+1)/6
        a=np.maximum(lo,enter)*speed-center;b=np.minimum(hi,leave)*speed-center
        def F(s):return s*(q*q-2/3*q*s*s+s**4/5)
        tau=np.where(valid&(b>a),np.maximum(0,F(b)-F(a))/speed*puffs[:,6]*2.5,0)
        total=tau.sum();opacity=-np.expm1(-total)
        L+=T*opacity*np.dot(tau,sources)/max(total,1e-15);T*=1-opacity
    return T,L

rows=[]
for i in range(contract['rays']):
    puffs=templates[i%4]
    origin=rng.normal(size=3);origin=origin/np.linalg.norm(origin)*(0.25 if i%5==4 else 2.2)
    target=rng.uniform(-.55,.55,3);direction=target-origin;distance=np.linalg.norm(direction);direction/=distance
    end=[.5,distance,4.,distance,2.][i%5]
    sources=np.clip(puffs[:,7]*1.4,.35,1.25)
    T,L=analytic(puffs,origin,direction,end,sources)
    steps=contract['referenceSteps'];dt=end/steps;t=(np.arange(steps)+.5)*dt
    points=origin+t[:,None]*direction
    q=np.sum(((points[:,None,:]-puffs[None,:,:3])/puffs[None,:,3:6])**2,axis=2)
    density=np.maximum(0,1-q)**2*puffs[None,:,6]*2.5
    tau=density.sum(axis=1)*dt
    source=density@sources/np.maximum(density.sum(axis=1),1e-15)
    before=np.exp(-np.r_[0,np.cumsum(tau[:-1])])
    referenceT=math.exp(-tau.sum());referenceL=np.sum(before*(-np.expm1(-tau))*source)
    rows.append(dict(ray=i,relation=contract['relations'][i%5],T=T,L=L,
        transmissionError=abs(T-referenceT),sourceError=abs(L-referenceL),finite=bool(np.isfinite([T,L]).all())))
maxT=max(x['transmissionError'] for x in rows);maxL=max(x['sourceError'] for x in rows)
result=dict(contract=contract,maximumTransmissionError=maxT,maximumSourceError=maxL,
    status='PASS' if maxT<=contract['transmissionAbsTolerance'] and maxL<=contract['normalizedSourceAbsTolerance']
    and all(x['finite'] and 0<=x['T']<=1 for x in rows) else 'FAIL',
    limitations=['未验证 GPU 深度纹理、小目标像素和不同整云的合成','未完成视觉验收'],rays=rows)
(OUT/'result.json').write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8')
print(json.dumps({k:v for k,v in result.items() if k!='rays'},ensure_ascii=False,indent=2))
raise SystemExit(0 if result['status']=='PASS' else 2)
