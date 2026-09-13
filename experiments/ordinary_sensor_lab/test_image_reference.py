"""Analytic, independent quadrature and stochastic tests of the 2D experiment."""
from copy import deepcopy
import json
import math
from pathlib import Path
import sys
import tempfile

import numpy as np
from PIL import Image
from scipy.integrate import quad

from image_cli import execute, load_image_config
from image_lab import ImageExperiment, LAYERS, digest

ROOT=Path(__file__).resolve().parent
BASE=json.loads((ROOT/'synthetic_nir_image.json').read_text())
checks=[]


def check(name, passed, **values):
    checks.append(dict(name=name,passed=bool(passed),**values))
    if not passed:
        raise AssertionError(name)


def close(a,b):
    return np.allclose(a,b,rtol=3e-13,atol=2e-10)


def small():
    c=deepcopy(BASE);c.update(width=31,height=29,tile_rows=7)
    c['scene']={'kind':'uniform','amplitude':1}
    c['layers']={key:False for key in LAYERS}
    return c


def reject(name, change):
    c=small();change(c)
    try:
        ImageExperiment(c).frame()
    except (ValueError,TypeError,KeyError):
        check(name,True)
    else:
        check(name,False)


def main():
    out=Path(sys.argv[1]) if len(sys.argv)>1 else ROOT/'test_output'
    out.mkdir(parents=True,exist_ok=True)
    c=small();e=ImageExperiment(c);a,st=e.frame()
    check('array_matches_verified_single_pixel',close(a['expected_photoelectrons'],e.references[0]['mean_photoelectrons']))
    # Independent SI constants and adaptive integration: no calls to production integrator.
    lo,hi=700.,1100.;amp=.00008
    integrand=lambda w: (2*amp*(hi-w)/(hi-lo))*(.15+.6*(w-lo)/(hi-lo))*(w*1e-9)/(6.62607015e-34*299792458.)
    expected,error=quad(integrand,lo,hi,epsabs=1e-5,epsrel=1e-12)
    expected*=25e-12*.01
    check('independent_adaptive_spectral_integral',close(a['expected_photoelectrons'],expected),expected_electrons=expected,measured_electrons=float(a['expected_photoelectrons'][0,0]),quadrature_relative_error_estimate=error/(expected/(25e-12*.01)))
    b=deepcopy(c);b['pixel']['exposure']['value']*=2
    check('noiseless_exposure_linearity',close(ImageExperiment(b).frame()[0]['expected_photoelectrons'],2*a['expected_photoelectrons']))
    b=deepcopy(c);b['pixel']['exposure']={'value':10000,'unit':'us'}
    b['pixel']['conversion_gain']={'value':1/.6,'unit':'electron/DN'}
    for basis in b['spectral_bases']:
        for role in ('illumination','response'):
            q=basis[role];q['wavelength_unit']='um'
            q['samples']=[[x/1000,y*(1000 if role=='illumination' else 100)] for x,y in q['samples']]
            q['value_unit']='W/m2/um' if role=='illumination' else 'percent'
    converted=ImageExperiment(b).frame()[0]
    check('spectral_exposure_and_conversion_units_equivalent',close(converted['expected_photoelectrons'],a['expected_photoelectrons']) and np.array_equal(converted['raw_dn'],a['raw_dn']))
    b=small();b['scene']['kind']='equal_energy_spectra';v=ImageExperiment(b);q=v.frame()[0]['expected_photoelectrons']
    energies=[r['input_irradiance_W_m2'] for r in v.references]
    check('equal_energy_distinct_spectra_remain_distinct',close(*energies) and q[0,-1]>q[0,0]*1.5,irradiances_W_m2=energies,electrons=[float(q[0,0]),float(q[0,-1])])
    b=small();b['scene'].update(amplitude=0)
    check('zero_light_preserves_dark_only',np.max(ImageExperiment(b).frame()[0]['expected_photoelectrons'])==0)
    b['pixel']['exposure']['value']=0
    zero=ImageExperiment(b).frame()[0]
    check('zero_exposure_black_level',np.max(zero['noisy_charge_electrons'])==0 and np.all(zero['raw_dn']==64))
    reject('negative_scene_rejected',lambda b:b['scene'].update(amplitude=-1))
    reject('radiance_rejected',lambda b:b['spectral_bases'][0]['illumination'].update(value_unit='W/m2/sr/nm'))
    reject('relative_srf_rejected_for_absolute_array',lambda b:b['spectral_bases'][0]['response'].update(kind='relative_energy'))
    reject('duplicate_optics_rejected_at_detector_plane',lambda b:b['spectral_bases'][0].update(optics=deepcopy(b['spectral_bases'][0]['response'])))
    reject('included_photon_energy_factor_rejected',lambda b:b['spectral_bases'][0]['response'].update(included_factors=['qe','photon_energy']))
    reject('unimplemented_mtf_rejected',lambda b:b['psf'].update(kind='measured_mtf'))
    reject('unknown_scene_image_input_rejected',lambda b:b['scene'].update(image='production.pfm'))
    reject('nonunit_psf_rejected',lambda b:b['psf'].update(kernel=[[2]]))
    reject('mixed_detector_responses_rejected',lambda b:b['spectral_bases'][1]['response']['samples'][0].__setitem__(1,.2))
    mwir=json.loads((ROOT/'synthetic_mwir_image.json').read_text());mwir.update(width=9,height=7,tile_rows=3)
    mwir['scene']={'kind':'uniform'};mwir['layers']={k:False for k in LAYERS}
    mwir_expected=quad(lambda w:(.000003*(5000-w)/2000)*(.15+.6*(w-3000)/2000)*(w*1e-9)/(6.62607015e-34*299792458.),3000,5000,epsrel=1e-12)[0]*25e-12*.01
    check('mwir_independent_integral',close(ImageExperiment(mwir).expected()['expected_photoelectrons'],mwir_expected),expected_electrons=mwir_expected)
    # Global coordinates and full-frame RNG streams keep tiles/order from changing any pixel.
    b=small();b['scene']['kind']='chart';b['layers']={k:True for k in LAYERS}
    v=ImageExperiment(b,543);x=v.expected(tile_rows=1);y=v.expected(tile_rows=13,reverse_tiles=True)
    check('tile_boundaries_and_reverse_order_exact',all(np.array_equal(x[k],y[k]) for k in x))
    _,s0=v.frame(0);_,s1=v.frame(1)
    check('fixed_patterns_stable_across_frames',s0['fixed_prnu_sha256']==s1['fixed_prnu_sha256'] and s0['fixed_dark_rate_sha256']==s1['fixed_dark_rate_sha256'])
    again=ImageExperiment(b,543)
    check('same_environment_seed_reproducible',np.array_equal(v.frame(2)[0]['raw_dn'],again.frame(2)[0]['raw_dn']))
    check('frame_noise_changes',not np.array_equal(v.frame(0)[0]['read_noise_electrons'],v.frame(1)[0]['read_noise_electrons']))
    # Statistical sample: 120 frames x 32x32 = 122880 independent observations.
    b=small();b.update(width=32,height=32);b['layers'].update(photo_shot=True,dark_shot=True,read_noise=True)
    v=ImageExperiment(b,1001);samples=np.concatenate([v.frame(i)[0]['readout_electrons'].ravel() for i in range(120)])
    mu=v.references[0]['mean_photoelectrons']+v.dark_rate*v.exposure;var=mu+v.read_sigma**2
    check('noise_mean_six_standard_errors',abs(samples.mean()-mu)<6*math.sqrt(var/samples.size),samples=int(samples.size),expected=mu,measured=float(samples.mean()))
    check('noise_variance_statistical_interval',abs(samples.var(ddof=1)/var-1)<.025,expected=var,measured=float(samples.var(ddof=1)))
    # Fixed pattern means/variances are separate from temporal noise.
    b=small();b.update(width=256,height=256,tile_rows=31)
    v=ImageExperiment(b,600)
    check('prnu_distribution',abs(v.prnu.mean()-1)<6*.02/256 and abs(v.prnu.var()/.02**2-1)<.025)
    check('dsnu_distribution_nonnegative',np.min(v.dark_rate_map)>=0 and abs(v.dark_rate_map.var()/16-1)<.025)
    b=small();b['layers']['psf']=True
    q=ImageExperiment(b).expected()
    check('psf_preserves_uniform_dc',close(q['ideal_photoelectrons'],q['blurred_photoelectrons']))
    b['scene']['kind']='edge';q=ImageExperiment(b).expected()
    check('psf_conserves_sum_and_softens_edge',close(q['ideal_photoelectrons'].sum(),q['blurred_photoelectrons'].sum()) and not np.array_equal(q['ideal_photoelectrons'],q['blurred_photoelectrons']))
    b=small();b['scene']['amplitude']=1000
    q=ImageExperiment(b).frame()[0]
    check('well_saturation_before_read_and_adc',np.all(q['stored_electrons']==20000) and np.all(q['raw_dn']==12064))
    b['pixel']['conversion_gain']['value']=2;q=ImageExperiment(b).frame()[0]
    check('adc_clipping_separate',np.all(q['raw_dn']==16383) and np.all(q['stored_electrons']==20000))
    b=small();b['scene']['amplitude']=0;b['pixel']['dark_current']['value']=0;b['pixel']['black_level']['value']=64.5
    q=ImageExperiment(b).frame()[0]
    check('quantization_half_up_not_bankers',np.all(q['raw_dn']==65))
    b=small();b['scene']['temporal_rate_per_s']=2;v=ImageExperiment(b);q=v.expected(3)
    # The midpoint exposure integration is exact for this independent affine time field.
    expected_time=expected*(1+2*(3/v.fps+v.exposure/2))
    check('within_exposure_time_integration',close(q['expected_photoelectrons'],expected_time))
    with tempfile.TemporaryDirectory(dir=out) as tmp:
        d=Path(tmp);cfg=d/'synthetic_test.json';b=small();cfg.write_text(json.dumps(b),encoding='utf8')
        m=execute(cfg,d/'images',frames=3,seed=55,ablations=True)
        raw=np.load(d/'images/frame_0000/raw_dn.npy');png=np.asarray(Image.open(d/'images/frame_0000/raw_dn_uint16.png'))
        preview=np.asarray(Image.open(d/'images/frame_0000/preview_u8.png'))
        check('raw_14bit_DN_unscaled_in_uint16',raw.dtype==np.uint16 and np.array_equal(raw,png) and raw.max()<16384)
        check('preview_separate_fixed_code_range',np.array_equal(preview,np.floor(raw.astype(float)/16383*255+.5).astype(np.uint8)))
        check('sequence_and_layer_artifacts',len(m['sequence'])==3 and len(m['ablations'])==5 and (d/'images/synthetic_preview_sequence.png').exists())
    (out/'image_reference_results.json').write_text(json.dumps(dict(validation_level='synthetic_reference',checks=checks),indent=2)+'\n',encoding='utf8')
    print(len(checks),'2D checks passed')


if __name__=='__main__':main()
