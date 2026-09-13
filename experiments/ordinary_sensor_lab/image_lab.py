"""Independent synthetic photon arrays. No production image or renderer inputs.

Spectral bases are integrated by the tested pixel core at all curve breakpoints.
Spatial coefficients are evaluated in global pixel coordinates, optionally in
tiles; no wavelength cube or monochromatic substitution is used.
"""
from copy import deepcopy
import hashlib
import math
import platform

import numpy as np
import scipy
from scipy.ndimage import convolve

from sensor_lab import run as pixel_reference, number, VERSION, curve

SCHEMA = 'ordinary-sensor-image-1'
LAYERS = ('prnu', 'dsnu', 'psf', 'photo_shot', 'dark_shot', 'read_noise')
BANDS = {'visible': 'synthetic_visible_photon_pixel', 'nir': 'synthetic_nir_photon_pixel',
         'mwir': 'synthetic_mwir_photon_pixel'}


def integer(value, name, low, high):
    if isinstance(value, bool) or not isinstance(value, int) or not low <= value <= high:
        raise ValueError(f'{name}: integer {low}..{high} required')
    return value


def digest(array):
    return hashlib.sha256(np.ascontiguousarray(array).tobytes()).hexdigest()


def unit_value(item, unit, name):
    if item.get('unit') != unit:
        raise ValueError(f'{name}: explicit {unit} required')
    return number(item['value'], name)


class ImageExperiment:
    def __init__(self, config, seed=20260913):
        self.config = deepcopy(config)
        c = self.config
        if c.get('schema') != SCHEMA or c.get('validation_level') != 'synthetic_reference':
            raise ValueError('explicit synthetic image schema and validation level required')
        if c.get('model') != 'synthetic_photon_array' or c.get('band') not in BANDS:
            raise ValueError('this is a synthetic photon model, not a thermal detector model')
        self.seed = integer(seed, 'seed', 0, 2**64-1)
        self.width = integer(c['width'], 'width', 1, 4096)
        self.height = integer(c['height'], 'height', 1, 4096)
        self.shape = (self.height, self.width)
        if self.width*self.height > 4096*2048:
            raise ValueError('image exceeds reference memory scope')
        self.tile_rows = integer(c.get('tile_rows', 128), 'tile_rows', 1, self.height)
        self.temporal_samples = integer(c.get('temporal_samples', 8), 'temporal_samples', 1, 128)
        self.fps = number(c.get('sequence_fps', 10), 'sequence_fps', strict=True)
        if self.fps > 1000:
            raise ValueError('sequence_fps exceeds synthetic scope')
        p = c['pixel']
        self.exposure = number(p['exposure']['value'], 'exposure') * (1e-6 if p['exposure']['unit'] == 'us' else 1)
        self.dark_rate = unit_value(p['dark_current'], 'electron/pixel/s', 'dark_current')
        self.read_sigma = unit_value(p['read_noise'], 'electron_rms', 'read_noise')
        self.well = unit_value(p['full_well'], 'electron', 'full_well')
        self.gain = number(p['conversion_gain']['value'], 'conversion_gain', strict=True)
        if p['conversion_gain']['unit'] == 'electron/DN':
            self.gain = 1/self.gain
        self.black = unit_value(p['black_level'], 'DN', 'black_level')
        self.bits = integer(p['adc_bits'], 'adc_bits', 1, 16)
        self.limit = 2**self.bits-1
        bases = c['spectral_bases']
        if not isinstance(bases, list) or not 1 <= len(bases) <= 2:
            raise ValueError('one or two explicit synthetic spectral bases required by these charts')
        self.references = []
        self.base_electrons = []
        response_identity = None
        for b in bases:
            bounds = {'visible': (400.,700.), 'nir': (700.,1100.), 'mwir': (3000.,5000.)}[c['band']]
            identity = [b['response'].get('kind'), b['response'].get('included_factors')]
            for role in ('response','optics'):
                identity.append(None if role not in b else [a.tolist() for a in curve(b[role],role,bounds)])
            if response_identity is not None and identity != response_identity:
                raise ValueError('all spectral bases must share one detector response and optical path')
            response_identity = identity
            q = dict(schema=VERSION, model=BANDS[c['band']], pixel=p,
                     illumination=b['illumination'], response=b['response'])
            for key in ('optics', 'qe'):
                if key in b:
                    q[key] = b[key]
            # Reuse reference-plane/factor/unit checks and piecewise Gauss integral.
            ref = pixel_reference(q, count=2, seed=0)
            if 'mean_photoelectrons' not in ref:
                raise ValueError('relative SRF cannot generate absolute image electrons')
            self.references.append(ref)
            self.base_electrons.append(ref['mean_photoelectrons'])
        self.scene = c['scene']
        allowed = ('uniform', 'steps', 'edge', 'equal_energy_spectra', 'chart', 'moving_edge')
        if self.scene.get('kind') not in allowed:
            raise ValueError('only generated ordinary mathematical fields are accepted; no images/PFM')
        if set(self.scene) - {'kind', 'amplitude', 'temporal_rate_per_s', 'speed_pixels_per_s'}:
            raise ValueError('unknown scene field (external images/radiance are not supported)')
        self.amplitude = number(self.scene.get('amplitude', 1), 'amplitude')
        self.temporal_rate = number(self.scene.get('temporal_rate_per_s', 0), 'temporal_rate_per_s')
        self.speed = number(self.scene.get('speed_pixels_per_s', 20), 'speed_pixels_per_s')
        if self.scene['kind'] in ('equal_energy_spectra', 'chart') and len(bases) < 2:
            raise ValueError('this chart needs two distinct spectral bases')
        f = c['fixed_pattern']
        if f.get('source_type') != 'synthetic' or not f.get('source'):
            raise ValueError('fixed_pattern: synthetic provenance required')
        prnu_sigma = unit_value(f['prnu_sigma'], 'fraction_rms', 'prnu_sigma')
        dsnu_sigma = unit_value(f['dsnu_sigma'], 'electron/pixel/s_rms', 'dsnu_sigma')
        if prnu_sigma > .3 or dsnu_sigma > 1e6:
            raise ValueError('fixed pattern exceeds declared mathematical scope')
        # Namespace-specific full-frame streams: independent of tile order/frame noise.
        self.prnu = np.maximum(0, 1+self.rng(0).normal(0, prnu_sigma, self.shape))
        self.dark_rate_map = np.maximum(0, self.dark_rate+self.rng(1).normal(0, dsnu_sigma, self.shape))
        psf = c['psf']
        if psf.get('kind') != 'synthetic_discrete_irradiance_psf' or psf.get('boundary') != 'periodic':
            raise ValueError('only an explicit artificial irradiance PSF with periodic boundary is supported; no MTF files')
        self.kernel = np.asarray(psf['kernel'], dtype=np.float64)
        if self.kernel.ndim != 2 or any(n % 2 != 1 or n > 15 for n in self.kernel.shape):
            raise ValueError('PSF: odd dimensions <=15 required')
        if not np.isfinite(self.kernel).all() or (self.kernel < 0).any() or abs(self.kernel.sum()-1) > 1e-12:
            raise ValueError('PSF must be nonnegative, finite, and unit sum')
        self.layer_flags = {key: True for key in LAYERS}
        if set(c.get('layers', {})) - set(LAYERS):
            raise ValueError('unknown layer')
        for key, value in c.get('layers', {}).items():
            if not isinstance(value, bool):
                raise ValueError('layer flags must be booleans')
            self.layer_flags[key] = value
        self.identity = dict(validation_level='synthetic_reference', input_kind='spectral_irradiance',
            band=c['band'], model=c['model'], shape=list(self.shape), adc_bits=self.bits,
            storage='uint16 with unscaled actual DN; float64 electron layers',
            integration='spectral bases: all breakpoints/3-point Gauss; exposure: midpoint temporal samples',
            psf='wavelength-independent artificial discrete irradiance PSF before pixel response; periodic boundary, unit DC and conserved sum',
            pixel_area='square pitch squared with artificial fill factor 1; no microlens or angular response model',
            fixed_pattern='Gaussian PRNU factor and Gaussian additive dark-rate variation; both clipped nonnegative once per experiment',
            fixed_prnu_sha256=digest(self.prnu), fixed_dark_rate_sha256=digest(self.dark_rate_map),
            seed=seed, runtime=dict(python=platform.python_version(), numpy=np.__version__, scipy=scipy.__version__,
                                    rng='PCG64 / SeedSequence(seed, namespace, frame)', platform=platform.platform()),
            reproducibility='same recorded environment; future NumPy bit identity not promised',
            pixel_core_references=self.references)

    def rng(self, namespace, frame=0):
        return np.random.Generator(np.random.PCG64(np.random.SeedSequence([self.seed, namespace, frame])))

    def weights(self, y0, y1, time_s):
        x = (np.arange(self.width)[None, :]+.5)/self.width
        y = (np.arange(y0, y1)[:, None]+.5)/self.height
        shape = (y1-y0, self.width)
        a = np.ones(shape); b = np.zeros(shape)
        kind = self.scene['kind']
        if kind == 'steps':
            a[:] = np.floor(x*8)/7
        elif kind in ('edge', 'moving_edge'):
            edge = .5 if kind == 'edge' else .15+(self.speed*time_s/self.width) % .7
            a[:] = np.where(x < edge, .05, 1)
        elif kind == 'equal_energy_spectra':
            a[:] = x < .5; b[:] = x >= .5
        elif kind == 'chart':
            a[:] = .18
            a = np.where(y < .20, np.broadcast_to(x, shape), a)
            a = np.where((y >= .20) & (y < .40), np.broadcast_to(np.floor(x*16)/15, shape), a)
            qx=(x-.25)/.14; qy=(y-.58)/.14
            radius=qx*qx+qy*qy
            sphere=np.maximum(0, -.25*qx+.35*qy+.8*np.sqrt(np.maximum(0, 1-radius)))
            a=np.where((y>=.4)&(y<.76)&(radius<1), .03+.8*sphere, a)
            a=np.where((x>.72)&(x<.76)&(y>.45)&(y<.49), 15, a)
            # Equal-area, equal-energy spectral comparison at the bottom.
            a=np.where(y>.80, np.where(x<.5, 1, 0), a)
            b=np.where(y>.80, np.where(x<.5, 0, 1), b)
            # A moving ordinary bright edge for the short sequence.
            a=np.where((y>.68)&(y<.76)&(x<.1+(self.speed*time_s/self.width) % .75), 2, a)
        scale=self.amplitude*(1+self.temporal_rate*time_s)
        return [a*scale, b*scale] + [np.zeros(shape) for _ in range(len(self.base_electrons)-2)] if len(self.base_electrons)>1 else [a*scale]

    def expected(self, frame=0, tile_rows=None, reverse_tiles=False, flags=None):
        integer(frame, 'frame', 0, 1000000)
        flags = self.layer_flags if flags is None else flags
        tile = self.tile_rows if tile_rows is None else integer(tile_rows, 'tile_rows', 1, self.height)
        ideal = np.zeros(self.shape)
        starts = list(range(0, self.height, tile))
        if reverse_tiles:
            starts.reverse()
        for y0 in starts:
            y1=min(self.height,y0+tile)
            for k in range(self.temporal_samples):
                time_s=frame/self.fps+self.exposure*(k+.5)/self.temporal_samples
                w=self.weights(y0,y1,time_s)
                for mu, plane in zip(self.base_electrons,w):
                    ideal[y0:y1] += mu*plane/self.temporal_samples
        blurred = convolve(ideal, self.kernel, mode='wrap') if flags['psf'] else ideal.copy()
        photo = blurred*self.prnu if flags['prnu'] else blurred.copy()
        dark = (self.dark_rate_map if flags['dsnu'] else np.full(self.shape,self.dark_rate))*self.exposure
        if not np.isfinite(photo).all() or np.max(photo+dark) > 1e12:
            raise ValueError('array charge exceeds numerical experiment scope')
        return dict(ideal_photoelectrons=ideal, blurred_photoelectrons=blurred,
                    expected_photoelectrons=photo, expected_dark_electrons=dark)

    def frame(self, index=0, disable=()):
        if set(disable)-set(LAYERS):
            raise ValueError('unknown disabled layer')
        flags = dict(self.layer_flags)
        for key in disable:
            flags[key]=False
        out = self.expected(index, flags=flags)
        photo, dark = out['expected_photoelectrons'], out['expected_dark_electrons']
        shot = self.rng(10,index).poisson(photo) if flags['photo_shot'] else photo.copy()
        dark_shot = self.rng(11,index).poisson(dark) if flags['dark_shot'] else dark.copy()
        raw = shot+dark_shot
        stored = np.minimum(raw,self.well)
        read = self.rng(12,index).normal(0,self.read_sigma,self.shape) if flags['read_noise'] else np.zeros(self.shape)
        readout = stored+read
        analog_dn = readout*self.gain+self.black
        dn = np.clip(np.floor(analog_dn+.5),0,self.limit).astype(np.uint16)
        # Fixed full-code-range preview, explicitly separate from raw DN.
        preview = np.floor(dn.astype(float)/self.limit*255+.5).astype(np.uint8)
        out.update(photo_electrons=shot,dark_electrons=dark_shot,noisy_charge_electrons=raw,
                   stored_electrons=stored,read_noise_electrons=read,readout_electrons=readout,
                   raw_dn=dn,preview_u8=preview)
        stats = {key: dict(mean=float(v.mean()),variance=float(v.var()),minimum=float(v.min()),maximum=float(v.max())) for key,v in out.items()}
        return out, dict(frame=index,start_time_s=index/self.fps,exposure_s=self.exposure,layers=flags,
            well_saturated_fraction=float(np.mean(raw>=self.well)),
            adc_clipped_fraction=float(np.mean((analog_dn<0)|(analog_dn>self.limit))),
            fixed_prnu_sha256=self.identity['fixed_prnu_sha256'],fixed_dark_rate_sha256=self.identity['fixed_dark_rate_sha256'],statistics=stats)
