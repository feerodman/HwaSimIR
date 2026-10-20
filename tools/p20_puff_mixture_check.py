"""多密度团局部光程及源项近似：独立密集参考，不冒充板端像素测试。"""
from pathlib import Path
import json,numpy as np,argparse
ap=argparse.ArgumentParser();ap.add_argument("--puffs",type=int,choices=[12,24],default=12);args=ap.parse_args()
ROOT=Path(__file__).resolve().parents[1];out=ROOT/'logs/p20/optics';out.mkdir(exist_ok=True)
contract=dict(rays=96,denseSteps=8192,seed=20201017,alphaTolerance=1e-5,siSourceTolerance=.02,whiteSourceTolerance=.05)
(out/('puff_mixture_'+str(args.puffs)+'_contract.json')).write_text(json.dumps(contract,indent=2),encoding='utf-8')
templates=np.asarray(json.loads((ROOT/('logs/p20/puff_templates_12/templates.json' if args.puffs==12 else 'logs/p20/puff_templates/templates.json')).read_text()))
rng=np.random.default_rng(contract['seed']);results=[]
for case in range(contract['rays']):
    o=np.array([0.,-3.,.2]) if case%3 else np.array([0.,-.1,.1]);ray=rng.uniform(-.55,.55,3)-o;ray/=np.linalg.norm(ray)
    groups=[(templates[case%4].copy(),np.zeros(3),.4),(templates[(case+1)%4].copy(),np.array([.25,.3,.1]),.42)]
    # 同生产结构：整云先按中心排序，各整云内部再按相机前向深度排序。
    groups.sort(key=lambda g:np.dot(g[1]-o,ray),reverse=True)
    puffs=[]
    for p,offset,gray in groups:
        p[:,:3]+=offset;p=p[np.argsort((p[:,:3]-o)@ray)[::-1]]
        for row in p:puffs.append((row,gray))
    stop=[1.,3.,8.][case%3] if np.linalg.norm(o)>1 else [0.1,.4,3.][case%3]
    points=o+(np.arange(contract['denseSteps'])[:,None]+.5)*stop/contract['denseSteps']*ray
    total=np.zeros(len(points));emission=np.zeros((len(points),2));C=np.zeros(2);A=0
    for row,gray in puffs:
        center,radius,amplitude,light,column=row[:3],row[3:6],row[6],row[7],row[8]
        origin=(o-center)/radius;d=ray/radius;n=np.linalg.norm(d);d/=n;c=-origin@d;radial=origin+c*d;q=1-radial@radial
        physical=gray-.2+.1*np.exp(-5*column/.7)+.1*np.exp(-5*column)
        shade=np.clip(light*1.4+.13*(-radial[0]+.6*radial[2])-.08*np.sqrt(max(0,1-q)),.35,1.25)
        source=np.array([physical,.62+(.98-.62)*(shade-.35)/.9])
        density=amplitude*np.maximum(0,1-np.sum(((points-center)/radius)**2,axis=1))**2*2.5
        total+=density;emission+=density[:,None]*source
        if q<=0:continue
        h=np.sqrt(q);a=max(0,c-h);b=min(c+h,stop*n)
        if b<=a:continue
        def F(s):return s*(q*q-2/3*q*s*s+s**4/5)
        alpha=-np.expm1(-max(0,F(b-c)-F(a-c))*amplitude*2.5/n)
        C=alpha*source+(1-alpha)*C;A=alpha+(1-alpha)*A
    dt=stop/contract['denseSteps'];tau=total*dt;prefix=np.cumsum(tau)-tau
    source=np.divide(emission,total[:,None],out=np.zeros_like(emission),where=total[:,None]>1e-15)
    ref=(np.exp(-prefix)*(-np.expm1(-tau)))[:,None]*source;ref=ref.sum(axis=0);refA=-np.expm1(-tau.sum())
    results.append(dict(case=case,camera=o.tolist(),direction=ray.tolist(),opaqueDistance=stop,alphaError=abs(A-refA),siError=abs(C[0]-ref[0]),whiteError=abs(C[1]-ref[1])))
maxima={k:max(x[k] for x in results) for k in ('alphaError','siError','whiteError')}
report=dict(scope='CPU双云，真实部分光程与混合介质密集参考；不含层云和透明尾焰',puffsPerCloud=args.puffs,contract=contract,maxima=maxima,rays=results,
status='PASS' if maxima['alphaError']<=contract['alphaTolerance'] and maxima['siError']<=contract['siSourceTolerance'] and maxima['whiteError']<=contract['whiteSourceTolerance'] else 'FAIL_SOURCE_APPROXIMATION')
(out/('puff_mixture_'+str(args.puffs)+'_result.json')).write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8');print(report['status'],maxima)
