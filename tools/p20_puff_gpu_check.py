"""将实际GPU片元与独立解析参考比较；输入深度来自实际小目标几何。"""
from pathlib import Path
import json, math, sys
import numpy as np
ROOT=Path(__file__).resolve().parents[1]
if '--directory' in sys.argv:OUT=ROOT/sys.argv[sys.argv.index('--directory')+1]
else:OUT=ROOT/('logs/p20/gpu_fixture_fullpath_half' if '--template12' in sys.argv and '--half-float' in sys.argv else 'logs/p20/gpu_fixture_fullpath' if '--template12' in sys.argv else 'logs/p20/gpu_fixture_half' if '--half-float' in sys.argv else 'logs/p20/gpu_fixture')

contract=json.loads((OUT/'contract.json').read_text(encoding='utf-8'))
cases=json.loads((OUT/'cases.json').read_text(encoding='utf-8'))
results=[]
for case in cases:
    name=case['name'];w=case['width'];h=case['height']
    assert case['componentBytes'] in (2,4) and w==h==800
    half=case['componentBytes']==2
    actual=np.fromfile(OUT/(name+('.rgba16f' if half else '.rgba32f')),np.float16 if half else np.float32).reshape(h,w,4).astype(float)
    depth=np.fromfile(OUT/(name+'.depth.rgb8'),np.uint8).reshape(h,w,3).astype(float)
    encoded=(depth[:,:,0]*65536+depth[:,:,1]*256+depth[:,:,2])/16777215
    opaque=np.exp2(encoded*(math.log2(1001)-math.log2(1.1))+math.log2(1.1))-1
    xs=(2*(np.arange(w)+.5)/w-1)*math.tan(math.radians(case.get('fov',45)/2))
    zs=(2*(np.arange(h)+.5)/h-1)*math.tan(math.radians(case.get('fov',45)/2))
    x,z=np.meshgrid(xs,zs);direction=np.stack([x,np.ones_like(x),z],axis=2)
    direction/=np.linalg.norm(direction,axis=2)[:,:,None]
    tau=np.zeros((h,w))
    for puff in case.get('puffs',[[0,0,0,1,1,1,1,.7,.1]]):
        radius=np.array(puff[3:6]);origin=(np.array([case.get('cameraX',0)/10,case['cameraY']/10,0])-np.array(puff[:3]))/radius
        localDirection=direction/radius;inverseRadius=np.linalg.norm(localDirection,axis=2);localDirection/=inverseRadius[:,:,None]
        center=-np.sum(origin*localDirection,axis=2)
        radial=origin+center[:,:,None]*localDirection;q=1-np.sum(radial*radial,axis=2)
        half=np.sqrt(np.maximum(0,q));enter=np.maximum(0,center-half)
        leave=np.minimum(center+half,opaque*inverseRadius/(10*direction[:,:,1]))
        def F(s):return s*(q*q-2/3*q*s*s+s**4/5)
        tau+=np.where((q>0)&(leave>enter),np.maximum(0,F(leave-center)-F(enter-center))*2.5*puff[6]/inverseRadius,0)
    alpha=-np.expm1(-tau);reference=.4*alpha
    mask=encoded<.99999
    result=dict(name=name,finite=bool(np.isfinite(actual).all()),targetCoveredPixels=int(mask.sum()),
        alphaMaxError=float(np.max(abs(actual[:,:,3]-alpha))),sourceMaxError=float(np.max(abs(actual[:,:,0]-reference))),
        targetAlphaMean=float(actual[:,:,3][mask].mean()) if mask.any() else None,
        targetDepthMedian=float(np.median(opaque[mask])) if mask.any() else None)
    result['status']='PASS' if result['finite'] and mask.sum()==case['targetPixels']**2 and result['alphaMaxError']<=contract['alphaAbsTolerance'] and result['sourceMaxError']<=contract['sourceAbsTolerance'] else 'FAIL'
    if contract.get('verifyCloudSourceB'):
        result['cloudSourceBMaxError']=float(np.max(abs(actual[:,:,2]-reference)))
        if result['cloudSourceBMaxError']>contract['sourceAbsTolerance']:result['status']='FAIL'
    results.append(result)
report=dict(result='PASS' if all(x['status']=='PASS' for x in results) else 'FAIL',cases=results,
            backend=contract['backend'],boardResult=('PASS' if all(x['status']=='PASS' for x in results) else 'FAIL') if contract['backend'].startswith('RK3588') else 'NOT_VERIFIED',visualAcceptance='PENDING',
            scope='解析光程、真实不透明深度、1/3/7像素目标；相同源函数，不覆盖多介质源项排序或尾焰')
(OUT/'result.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
print(json.dumps(report,indent=2,ensure_ascii=False))
raise SystemExit(0 if report['result']=='PASS' else 2)
