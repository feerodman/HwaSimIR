"""检查Mali原始float读回；CPU独立数值积分，容差由运行前contract确定。"""
from pathlib import Path
import argparse,json,math,numpy as np
ap=argparse.ArgumentParser();ap.add_argument('directory');a=ap.parse_args();out=Path(a.directory)
contract=json.loads((out/'contract.json').read_text(encoding='utf-8'));size=contract['width']
def smooth(a,b,x):
 q=min(1.,max(0.,(x-a)/(b-a)));return q*q*(3-2*q)
cases=[]
for distance in contract['positions']:
 modes=6 if (out/f'gas_{distance}_5.rgba32f').exists() else 5 if (out/f'gas_{distance}_4.rgba32f').exists() else 4
 images=[np.fromfile(out/f'gas_{distance}_{mode}.rgba32f',dtype=np.float32).reshape(size,size,4) for mode in range(modes)]
 pixels=[im[size//2,size//2].astype(float) for im in images]
 rx=rz=math.tan(math.radians(10))/size;norm=math.sqrt(1+rx*rx+rz*rz)
 wake=contract.get('layer',1)==2;steps=40 if wake else 24
 ray=[rx/norm,1/norm,rz/norm];enter=(distance-(2.6 if wake else 1.5))/ray[1];leave=distance/ray[1];step=(leave-enter)/steps
 gasT=1.;weighted=0.;weightSum=0.;weightedC=0.;weightedW=0.
 for i in range(steps):
  s=enter+(i+.5)*step;x=s*ray[0];y=-distance+s*ray[1];z=s*ray[2];ax=max(0.,-y)
  taper=1-smooth(.25,2.2 if wake else 1.5,ax);radius=.35+(.90 if wake else .65)*taper
  drift=.06*radius*smooth(.5,1.6,ax) if wake else 0
  dx=math.sin(ax*6)*drift;dz=math.sin(ax*6*.73+1.2)*drift;r2=((x-dx)**2+(z-dz)**2)/(radius*radius)
  density=math.exp(-ax*4*(1 if wake else .28)-r2*4)*(1-smooth(.65,1,r2))
  density*=.22*math.exp(-1.8*max(0,ax-.45)**2)*(1-smooth(1.2,2.2,ax)) if wake else 1-smooth(.55,1.5,ax)
  stepAlpha=1-math.exp(-density*step*(-math.log(.2))/2);w=gasT*stepAlpha;gasT*=1-stepAlpha
  forward=s*ray[1];dt=forward/4096
  tau=sum(max(0.,1-((rx*(j+.5)*dt/10)**2+(((j+.5)*dt-30)/10)**2+(rz*(j+.5)*dt/10)**2))**2*dt for j in range(4096))*2.5*norm/10
  cloudT=math.exp(-tau);weighted+=w*(4*(1-cloudT)+50*cloudT);weightedC+=w*4*(1-cloudT);weightedW+=w*.872*(1-cloudT);weightSum+=w
 expected=[weighted/weightSum,weightedC/weightSum,weightedW/weightSum]
 finite=all(np.isfinite(im).all() for im in images);active=int(np.count_nonzero(images[0][:,:,3]>.001))
 off=float(np.max(np.abs(images[0]-images[1])));alpha=float(np.max(np.abs(images[0][:,:,3]-images[2][:,:,3])))
 errors=[abs(pixels[2][0]-expected[0]),abs(pixels[3][0]-expected[1]),abs(pixels[3][1]-expected[2])]
 if modes>=5:errors.extend(abs(x-y) for x,y in zip(pixels[4][:3],expected))
 if modes==6:
  fullT=math.exp(-2.5*16/15);coverage=pixels[0][3]
  ref=[coverage*expected[0]+(1-coverage)*(2*fullT+4*(1-fullT)),coverage*expected[1]+(1-coverage)*4*(1-fullT),coverage*expected[2]+(1-coverage)*.872*(1-fullT)]
  merged=pixels[5];actual=[merged[0]+2*(1-merged[3]),merged[2],merged[1]]
  errors.extend(abs(x-y) for x,y in zip(actual,ref))
 ok=finite and active>100 and pixels[0][0]>1 and pixels[0][3]>.01 and off<=contract['cloudOffRgbAbsTolerance'] and alpha<=contract['alphaAbsTolerance'] and max(errors)<=contract['sourceAbsTolerance']
 cases.append(dict(distance=distance,finite=bool(finite),activePixels=active,pixels=[p.tolist() for p in pixels],reference=expected,sourceErrors=errors,cloudOffMaxError=off,alphaMaxError=alpha,result='PASS' if ok else 'FAIL'))
result=dict(result='PASS' if all(c['result']=='PASS' for c in cases) else 'FAIL',contract=contract,cases=cases)
(out/'result.json').write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8');print(json.dumps(result,indent=2,ensure_ascii=False));raise SystemExit(0 if result['result']=='PASS' else 2)
