"""Independent ordinary-chart definitions and same-sequence display evidence."""
from pathlib import Path
import json,re
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'logs/p6c'


def pfm(p):
    with p.open('rb') as f:
        assert f.readline().strip()==b'PF'
        w,h=map(int,f.readline().split());scale=float(f.readline())
        return np.frombuffer(f.read(),'<f4' if scale<0 else '>f4').reshape(h,w,3)


def fields(s):return dict(re.findall(r'(\w+)=([^\s]+)',s))


def definition(case,seq,w=800,h=800):
    x=np.broadcast_to((np.arange(w)[None,:]+.5)/w,(h,w))
    y=np.broadcast_to(1-(np.arange(h)[:,None]+.5)/h,(h,w))
    g=np.full((h,w),.18)
    g=np.where(y>.8,x,g);g=np.where((y<=.8)&(y>.6),.08*x,g)
    g=np.where((y<=.6)&(y>.4),.2+.005*np.floor(x*16),g)
    g=np.where((x>.85)&(x<.90)&(y>.26)&(y<.31),1.5,g)
    light=np.array([-.4,.6,1.]);light/=np.linalg.norm(light)
    for center,base,gain in [(.25,.03,.27),(.6,.06,.5)]:
        qx=(x-center)/.08;qy=(y-.1)/.08;r=qx*qx+qy*qy
        v=base+gain*np.maximum(0,qx*light[0]+qy*light[1]+np.sqrt(np.maximum(0,1-r))*light[2])
        g=np.where((y<=.2)&(r<1),v,g)
    if case==1:g=4*x
    if case==2:g=np.full((h,w),.25)
    if case==3:g=.25+.0001*x
    if case in (4,5):
        g=.02+.18*x
        if 60<=seq<140:g=np.where((y>.2)&(y<.8)&(x>(.985 if case==4 else .55)),2+2*y,g)
    if case==6:
        g=np.where(np.mod(np.floor(x*800),25)<2,2.,.1)
        g=np.where((x>.501)&(x<.504)&(y>.501)&(y<.504),8.,g)
    if case==7:g=3+.05*x
    if case==8:g=.4+.05*x
    return g


def main():
    result={}
    for folder in sorted(OUT.iterdir()):
        if not (folder/'request.json').exists():continue
        req=json.loads((folder/'request.json').read_text(encoding='utf-8-sig'))
        if req.get('scene')!='display' or req.get('normal'):continue
        log=(folder/'board.log').read_text(encoding='utf8',errors='replace') if (folder/'board.log').exists() else ''
        mappings=[fields(x) for x in log.splitlines() if x.startswith('[P6CMapping]')]
        effective=[fields(x) for x in log.splitlines() if x.startswith('[DisplayEffective]')]
        parameters=dict(source_sequence=req['dumpSeq'],mapping_history=mappings,effective_sources=effective,
                        definition_case=req.get('displayCase',0),unit='dimensionless common scaled linear scene input; no radiance units')
        (folder/'display_parameters.json').write_text(json.dumps(parameters,indent=2)+'\n',encoding='utf8')
        row={}
        if (folder/'linear.pfm').exists() and (folder/'linear_rgb8.png').exists():
            raw=pfm(folder/'linear.pfm')[-800:,:800,0].astype(float)
            g=np.array(Image.open(folder/'linear_rgb8.png'))[:,:,0].astype(float)
            d=np.array(Image.open(folder/'received.png'))[:,:,0].astype(float)
            defined=definition(req.get('displayCase',0),req['dumpSeq'])
            np.save(folder/'defined_linear.npy',defined,allow_pickle=False)
            sources={v['field']:float(v['value']) for v in effective}
            affine=raw*sources.get('Gain',1)+sources.get('OffsetGray',0)/255
            np.save(folder/'affine_display_input.npy',affine,allow_pickle=False)
            row.update(definition_to_float_mae=float(np.abs(raw-defined).mean()),definition_to_float_max=float(np.abs(raw-defined).max()),
                       defined_min=float(defined.min()),defined_max=float(defined.max()),raw_min=float(raw.min()),raw_max=float(raw.max()),
                       raw_unique_values=len(np.unique(raw)),gpu_unique_values=len(np.unique(g)),
                       gpu_white_fraction=float(np.mean(g==255)),gpu_black_fraction=float(np.mean(g==0)),
                       decoded_mae=float(np.abs(d-g).mean()),decoded_max=float(np.abs(d-g).max()))
            rows=[80,240,400] if req.get('displayCase',0)==0 else [80]
            if req.get('displayCase',0) in (0,1,2,3,7,8):
                sign=1 if sources.get('WhiteHot',1) else -1
                row['fixed_or_frozen_row_inversions']={str(y):int(np.sum(sign*np.diff(g[y])<0)) for y in rows}
                row['decoded_row_inversions']={str(y):int(np.sum(sign*np.diff(d[y])<0)) for y in rows}
            if (folder/'linear_stats.pfm').exists():
                sample=pfm(folder/'linear_stats.pfm')[:,:,0].astype(float)
                n=sample.shape[0];ix=np.floor((np.arange(n)+.5)*800/n).astype(int)
                exact=raw[ix[:,None],ix[None,:]]
                sm=sample*sources.get('Gain',1)+sources.get('OffsetGray',0)/255
                # Independent nearest rank on each declared population.
                q=lambda a:[float(np.sort(a.ravel())[int(np.floor(p*.01*(a.size-1)+.5))]) for p in [2,98]]
                row['sampling']=dict(nearest_pixel_error=float(np.abs(exact-sample).max()),sample_quantiles=q(sm),full_quantiles=q(affine),sample_max=float(sample.max()),full_max=float(raw.max()),not_full_percentile_equivalence=True)
        if mappings:
            frozen=[m for m in mappings if req.get('freezeAfter',0)>0 and int(m['sourceSeq'])>req['freezeAfter']]
            row['frozen_mapping_constant']=len({(m['agcGain'],m['agcOffset']) for m in frozen})<=1 if frozen else None
            first=[m for m in mappings if m['sourceSeq']=='1']
            row['init_first_mappings']=first
        result[folder.name]=row
    (OUT/'display_validation.json').write_text(json.dumps(result,indent=2,allow_nan=False)+'\n',encoding='utf8')
    print(json.dumps(result,indent=2))


if __name__=='__main__':main()
