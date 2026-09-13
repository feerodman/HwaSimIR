"""Analytic reference and statistical checks; no production imports or calibration."""
from copy import deepcopy
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
from curve_io import load_config
from sensor_lab import run, H, C

ROOT = Path(__file__).resolve().parent
BASE = json.loads((ROOT/'synthetic_visible.json').read_text(encoding='utf8'))
checks = []


def check(name, condition, **evidence):
    checks.append(dict(name=name, passed=bool(condition), **evidence))
    if not condition:
        raise AssertionError(name)


def close(a, b):
    return math.isclose(a, b, rel_tol=2e-12, abs_tol=1e-12)


def rejected(name, config):
    try:
        run(config, 2)
    except (ValueError, KeyError, TypeError) as exc:
        check(name, True, reason=str(exc))
    else:
        check(name, False)


def main():
    out = Path(sys.argv[1] if len(sys.argv)>1 else ROOT/'results')
    out.mkdir(parents=True, exist_ok=True)
    r = run(BASE, 200000)
    # Independent closed form: constant spectral irradiance and QE, integral lambda dlambda.
    expected = 1e-5*.5*(700**2-400**2)/2*1e-9/(H*C)*(5e-6)**2*.01
    check('constant_spectrum_analytic_electrons', close(r['mean_photoelectrons'], expected), expected=expected, measured=r['mean_photoelectrons'])
    check('energy_area_exposure_units', close(r['input_energy_per_pixel_J'], .003*.01*(5e-6)**2))
    b=deepcopy(BASE);b['pixel']['exposure']['value']*=2
    doubled=run(b, 2)
    check('exposure_linearity', close(doubled['mean_photoelectrons'], expected*2) and close(doubled['mean_dark_electrons'],.4))
    b=deepcopy(BASE)
    b['illumination'].update(wavelength_unit='um',value_unit='W/m2/um',samples=[[.4,.01],[.55,.01],[.7,.01]])
    b['response'].update(value_unit='percent',samples=[[400,50],[700,50]])
    check('nm_um_and_fraction_percent_equivalence', close(run(b,2)['mean_photoelectrons'],expected))
    b=deepcopy(BASE);b['illumination']['samples']=[[500,0],[550,2e-5],[600,0]]
    triangle=run(b,2)['mean_photoelectrons']
    triangle_reference=.001*550e-9/(H*C)*.5*(5e-6)**2*.01
    check('narrow_triangle_analytic',close(triangle,triangle_reference),expected=triangle_reference,measured=triangle)
    b=deepcopy(BASE);b['response'].update(kind='relative_energy',included_factors=[])
    rel=run(b,2)
    check('relative_response_cannot_produce_electrons', 'mean_photoelectrons' not in rel and close(rel['relative_weighted_response']['value'],.0015))
    b['response']['kind']='relative_photon';rel=run(b,2)
    check('relative_photon_weighting',close(rel['relative_weighted_response']['value'],expected/((5e-6)**2*.01)))
    b=deepcopy(BASE);b['optics']=deepcopy(b['response']);b['optics']['samples']=[[400,.8],[700,.8]]
    b['illumination']['reference_plane']='before_optics_uniform_plane'
    separate=run(b,2)['mean_photoelectrons']
    del b['optics'];b['response'].update(kind='absolute_system_efficiency',included_factors=['qe','optics'],samples=[[400,.4],[700,.4]])
    check('combined_or_separate_optics_once',close(separate,expected*.8) and close(run(b,2)['mean_photoelectrons'],separate))
    b['optics']=deepcopy(BASE['response']);rejected('duplicate_optics_rejected',b)
    b=deepcopy(BASE);b['qe']=deepcopy(b['response']);rejected('duplicate_qe_rejected',b)
    for name,change in (
        ('no_extrapolation', lambda b:b['response'].update(samples=[[450,.5],[650,.5]])),
        ('no_radiance_or_pfm_as_irradiance',lambda b:b['illumination'].update(value_unit='W/m2/sr/um')),
        ('unknown_unit',lambda b:b['response'].update(value_unit='auto')),
        ('nonfinite_curve',lambda b:b['response'].update(samples=[[400,float('nan')],[700,.5]])),
        ('unsorted_curve',lambda b:b['response'].update(samples=[[700,.5],[400,.5]])),
        ('qe_above_one',lambda b:b['response'].update(samples=[[400,1.1],[700,.5]])),
        ('device_curve_not_allowed',lambda b:b['response'].update(source_type='measured')),
        ('wrong_reference_plane',lambda b:b['illumination'].update(reference_plane='before_optics_uniform_plane'))):
        b=deepcopy(BASE);change(b);rejected(name,b)
    # Six standard errors on mean; loose 2 percent variance tolerance for 200k independent samples.
    mu=expected+.2;var=mu+9
    check('shot_plus_read_noise_mean',abs(r['measured_readout_mean_electron']-mu)<6*math.sqrt(var/200000),expected=mu,measured=r['measured_readout_mean_electron'])
    check('shot_plus_read_noise_variance',abs(r['measured_readout_variance_electron2']/var-1)<.02,expected=var,measured=r['measured_readout_variance_electron2'])
    check('seed_reproducible',r==run(BASE,200000))
    b=deepcopy(BASE);b['illumination']['samples']=[[400,0],[700,0]]
    dark=run(b,200000)
    check('dark_field',dark['mean_photoelectrons']==0 and abs(dark['measured_readout_mean_electron']-.2)<6*math.sqrt(9.2/200000),mean=dark['measured_readout_mean_electron'],variance=dark['measured_readout_variance_electron2'])
    b['pixel']['exposure']['value']=0;b['pixel']['read_noise']['value']=0
    black=run(b,100)
    check('zero_exposure_black_level',black['adc_min_DN']==64 and black['adc_max_DN']==64)
    b=deepcopy(BASE);b['illumination']['samples']=[[400,.1],[700,.1]];b['pixel']['read_noise']['value']=0
    saturated=run(b,1000)
    check('well_saturation_before_adc',saturated['full_well_fraction']==1 and saturated['adc_min_DN']==4064 and saturated['adc_clipping_fraction']==0)
    b['pixel']['conversion_gain']['value']=1;adc=run(b,1000)
    check('adc_saturation_separate',adc['adc_min_DN']==4095 and adc['adc_clipping_fraction']==1)
    b=deepcopy(BASE);b['illumination']['samples']=[[400,0],[700,0]];b['pixel']['dark_current']['value']=0;b['pixel']['read_noise']['value']=0;b['pixel']['black_level']['value']=64.5
    check('explicit_half_up_quantization',run(b,2)['adc_first_16_DN']==[65,65])
    with tempfile.TemporaryDirectory(dir=out) as tmp:
        d=Path(tmp);b=deepcopy(BASE);del b['response']['samples'];b['response']['samples_file']='qe.csv'
        (d/'qe.csv').write_text('wavelength,value\n400,0.5\n700,0.5\n',encoding='utf8')
        (d/'input.json').write_text(json.dumps(b),encoding='utf8')
        loaded,sources=load_config(d/'input.json')
        check('csv_units_and_hash',close(run(loaded,2)['mean_photoelectrons'],expected) and len(sources)==2 and len(sources[1]['sha256'])==64)
        subprocess.run([sys.executable,str(ROOT/'sensor_cli.py'),str(d/'input.json'),'--output',str(out/'csv_cli.json')],check=True)
        (d/'input.json').write_text('{"schema":1,"schema":2}',encoding='utf8')
        try:load_config(d/'input.json')
        except ValueError:check('duplicate_json_keys_rejected',True)
        else:check('duplicate_json_keys_rejected',False)
    (out/'reference_results.json').write_text(json.dumps({'level':'synthetic_model_only','checks':checks,'base':r,'dark':dark,'well_saturation':saturated,'adc_saturation':adc},indent=2)+'\n',encoding='utf8')
    print(f'{len(checks)} reference checks passed; results: {out.resolve()}')


if __name__=='__main__':main()
