"""Read published tables and bounded native output metadata; never run MODTRAN."""
from pathlib import Path
import collections,csv,hashlib,json,re
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'HwaSim_IR/Bin/Config/Atmosphere/MODTRAN';OUT=ROOT/'logs/p10/coverage';OUT.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p):return list(csv.DictReader(p.open(encoding='utf-8-sig',newline='')))
manifest=BASE/'processed/manifest.csv';formal=BASE/'processed/band_lut_si.csv';m=rows(manifest);f=rows(formal)
coverage={}
for band in sorted({v['band'] for v in m}):
    r=[v for v in m if v['band']==band]
    coverage[band]=dict(cases=len(r),ranges=sorted({(v['wavelength_low_um'],v['wavelength_high_um']) for v in r}),modes=sorted({v['mode'] for v in r}),status=sorted({v['status'] for v in r}))
raw=BASE/'raw/audit_mwir_standard_20260906/mwir_std_trans/MODOUT2.txt'
lines=raw.read_text(errors='replace').splitlines();metadata=lines[:35]+lines[-3:]
(OUT/'native_mwir_metadata.txt').write_text('\n'.join(metadata),encoding='utf-8')
sources=[]
for p in [manifest,formal,raw]:sources.append(dict(path=str(p),bytes=p.stat().st_size,sha256=sha(p)))
spectra=[]
for name in ['path_lut_spectral.csv','sky_lut_spectral.csv','solar_lut_spectral.csv']:
    p=BASE/'processed'/name
    with p.open('rb') as stream:
        head=stream.read(1800);stream.seek(max(0,p.stat().st_size-1800));tail=stream.read()
    spectra.append(dict(path=str(p),bytes=p.stat().st_size,head=head.decode(errors='replace'),tail=tail.decode(errors='replace'),scan='bounded head/tail only; full-band availability follows manifest, not filename'))
dependencies=[]
base=ROOT/'HwaSim_IR/Bin/Config/SensorWave'
for name in ['default_VIS-SWIR.json','default_SWIR.json','default_MWIR.json']:
    def visit(x,path=''):
        if isinstance(x,dict):
            for k,v in x.items():yield from visit(v,path+'.'+k if path else k)
        elif isinstance(x,list):
            for i,v in enumerate(x):yield from visit(v,path+'['+str(i)+']')
        elif isinstance(x,str) and '$(PRESAGIS_ONDULUS_IR_22_0)' in x:yield path,x
    for field,value in visit(json.loads((base/name).read_text(encoding='utf-8-sig'))):
        p=Path(value.replace('$(PRESAGIS_ONDULUS_IR_22_0)',r'D:\Presagis\Suite22\Ondulus_IR_22_0'))
        dependencies.append(dict(profile=name,field=field,source=value,resolved=str(p),exists=p.is_file(),consumedInHwaSimIR=False,distribution='not copied; licence not inferred'))
report=dict(formalBands=dict(collections.Counter(v['band'] for v in f)),formalResponseModes=sorted({v['response_mode'] for v in f}),radianceUnits=sorted({v['radiance_unit'] for v in f}),coverage=coverage,sources=sources,spectralSamples=spectra,externalDependencies=dependencies,
    conclusions={'MWIR':'Existing 3-5 um rectangular-band means, limited original atmosphere/scene axes; not a measured device SRF.',
    'SWIR':'No formal 1.5-2.5 or 1.1-2.5 um table in this manifest. NIR means and band labels cannot be reused.',
    'VIS-SWIR':'No matching 0.5-1.7 um formal coverage. NIR 0.7-1.1 spectra alone do not span it.',
    'rawReuse':'Existing full spectral rows are potentially reusable only inside their measured spectral/scene coverage. Native MWIR sample is not SWIR coverage.',
    'runtime':'NIR/MWIR implementations exist; production enablement is independent of table availability. No new atmospheric grid generated or deployed.'})
(OUT/'coverage.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(formalBands=report['formalBands'],manifestBands=list(coverage),dependencies=len(dependencies),newCasesRun=0)))
