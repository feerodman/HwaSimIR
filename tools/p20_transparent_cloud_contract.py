"""透明内容的局部光程反例；明确记录未通过，不能用不透明深度夹具替代。

使用解析均匀云 slab 与已认可的 straight-alpha 光源契约；不修改尾焰资源。
复现当前合成式把全部 base SI 乘整条云透射率的问题。
"""
import json,math
from pathlib import Path
background=2.;source=4.;plume=50.;alpha=.5;tau=2.
T=math.exp(-tau);cloud=source*(1-T)
current=(plume*alpha+background*(1-alpha))*T+cloud
cases=[]
for name,fraction in [('front',0.),('inside',.5),('behind',1.)]:
    frontT=math.exp(-tau*fraction);frontC=source*(1-frontT)
    expected=alpha*(frontC+frontT*plume)+(1-alpha)*(cloud+T*background)
    cases.append(dict(position=name,cloudPathFractionBeforeTransparent=fraction,currentSi=current,referenceSi=expected,absoluteError=abs(current-expected),passTolerance=1e-6,passed=abs(current-expected)<1e-6))
out=Path(__file__).resolve().parents[1]/'logs/p20/optics/transparent_contract.json'
result=dict(status='FAIL_LOCAL_TRANSPARENT_PATH',scope='解析反例；生产合成公式，非GPU尾焰回放；不代表真实辐射标定',cases=cases,remedyRequired='云前缀源项与透射必须在透明贡献实际深度参与合成，不能把云总alpha应用于全部base SI')
out.write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8');print(out);print(result['status'])
