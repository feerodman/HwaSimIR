"""Fit a labelled engineering grey-gas model; never publish it as MODTRAN LUT."""
import csv, hashlib, json
from pathlib import Path
import numpy as np
from scipy.optimize import least_squares

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'logs/p20m3/20261021_engineering'
OUT.mkdir(parents=True, exist_ok=True)
src = ROOT/'HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv'
rows = list(csv.DictReader(src.open(encoding='utf-8-sig')))
lines = ['P20M3_GREY_GAS_1']
report = {'quality':'ENGINEERING_APPROX', 'source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),
 'scope':'Clear standard atmosphere, no cloud. Effective band coefficients, not line-by-line equivalence.',
 'aerosol':'Koschmieder 3.912/visibility, Angstrom exponent 1.3, assumed single scatter albedo 0.9 and 1.2 km scale height',
 'thermal':'LTE absorption only; ISA lapse rate 6.5 K/km to 11 km, then 216.65 K',
 'solar':'5772 K blackbody normalized to 1361 W/m2 at 1 AU, one Earth-distance factor; 0.30-2.50 um heating integral',
 'bands':{}}
for band, lo, hi in [('SWIR',1.1,2.5),('MWIR',3.,5.)]:
    selected = [r for r in rows if r['band']==band and r['humidity_profile'].startswith('scaled_mls_surface_rh')
        and r['observer_alt_km']==r['target_alt_km'] and float(r['range_km'])>=.1]
    # Deduplicate solar dimension: propagation is independent of illumination.
    selected = list({tuple(r[k] for k in ('observer_alt_km','range_km','humidity_profile','visibility_km')):r for r in selected}.values())
    h=np.array([float(r['target_alt_km'])*1000 for r in selected]); d=np.array([float(r['range_km'])*1000 for r in selected])
    rh=np.array([float(r['humidity_profile'].split('rh')[-1]) for r in selected]); vis=np.array([float(r['visibility_km']) for r in selected])
    ref=np.array([float(r['tau_up']) for r in selected])
    if not np.all((ref>0)&(ref<1)): raise ValueError('Unidentifiable transmission anchor')
    scat=3.912/(vis*1000)*(0.55/((lo+hi)/2))**1.3*np.exp(-h/1200)
    def predict(p):
        k=np.exp(p[:3]); w=np.exp(np.r_[p[3:5],0]); w/=sum(w)
        # Dry gas and water vapour have separate declared scale heights.
        scale=.15*np.exp(-h/8000)+.85*(rh/60)*np.exp(-h/2000)
        return np.exp(-d[:,None]*(k[None,:]*scale[:,None]+scat[:,None]))@w
    fit=least_squares(lambda p:predict(p)-ref,np.r_[np.log([1e-5,3e-4,.01]),[0,0]],max_nfev=1500)
    k=np.exp(fit.x[:3]); w=np.exp(np.r_[fit.x[3:5],0]); w/=sum(w)
    lines.append(' '.join(format(v,'.17g') for v in [lo,hi,*k,*w]))
    report['bands'][band]={'absorption_m_inverse':k.tolist(),'weights':w.tolist(),
       'fit_max_tau_absolute_error':float(max(abs(predict(fit.x)-ref))),
       'anchors':[{'case':r['case_id'],'range_km':r['range_km'],'tau':r['tau_up'],'sources':r['source_case_ids']} for r in selected],
       'extrapolation':'Above 1 km uses explicit dry/wet scale heights; 2-50 km uses three-grey-gas transport. NOT strict validated.'}
path=OUT/'engineering_atmosphere.txt'; path.write_text('\n'.join(lines)+'\n',encoding='ascii')
report['parameter_sha256']=hashlib.sha256(path.read_bytes()).hexdigest()
(OUT/'parameter_provenance.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k!='bands'},indent=2))
