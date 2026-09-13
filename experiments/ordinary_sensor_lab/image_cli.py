"""Generate independent synthetic electron images, raw codes and a preview sequence."""
import argparse
import hashlib
import json
from pathlib import Path
import time

import numpy as np
from PIL import Image

from curve_io import load_config, resolve_curve_files
from image_lab import ImageExperiment, integer


def load_image_config(path):
    config, sources = load_config(path)
    for basis in config.get('spectral_bases', []):
        sources.extend(resolve_curve_files(basis, path))
    return config, sources


def save_frame(folder, arrays, complete=True):
    folder.mkdir(parents=True, exist_ok=True)
    keys = arrays.keys() if complete else ('raw_dn', 'preview_u8')
    for key in keys:
        a = arrays[key]
        if key == 'preview_u8':
            Image.fromarray(a).save(folder/'preview_u8.png')
        else:
            np.save(folder/(key+'.npy'), a, allow_pickle=False)
        if key == 'raw_dn':
            Image.fromarray(a).save(folder/'raw_dn_uint16.png')


def execute(config_path, output, frames=12, seed=20260913, ablations=False):
    integer(frames, 'frames', 1, 1000)
    config, sources = load_image_config(config_path)
    lab = ImageExperiment(config, seed)
    if output.exists() and any(output.iterdir()):
        raise ValueError('output must be a new or empty directory; preserve prior experiment identities')
    output.mkdir(parents=True, exist_ok=True)
    begin = time.perf_counter()
    np.save(output/'fixed_prnu_factor.npy', lab.prnu, allow_pickle=False)
    np.save(output/'fixed_dark_rate_e_per_pixel_s.npy', lab.dark_rate_map, allow_pickle=False)
    sequence = []
    previews = []
    for i in range(frames):
        arrays, stats = lab.frame(i)
        save_frame(output/f'frame_{i:04d}', arrays, complete=i == 0)
        sequence.append(stats)
        previews.append(Image.fromarray(arrays['preview_u8']))
    # Lossless animated PNG is a preview only; the raw DN series remains separate.
    if frames > 1:
        previews[0].save(output/'synthetic_preview_sequence.png',save_all=True,
                         append_images=previews[1:],duration=round(1000/lab.fps),loop=0)
    comparisons = {}
    if ablations:
        variants = {
            'expectation_only': ('prnu','dsnu','psf','photo_shot','dark_shot','read_noise'),
            'psf_only': ('prnu','dsnu','photo_shot','dark_shot','read_noise'),
            'fixed_pattern_only': ('photo_shot','dark_shot','read_noise'),
            'shot_only': ('prnu','dsnu','psf','read_noise'),
            'full': ()}
        for name, disabled in variants.items():
            arrays, stats = lab.frame(0,disable=disabled)
            save_frame(output/'ablations'/name,arrays)
            comparisons[name] = stats
    metadata = dict(identity=lab.identity,inputs=sources,config=config,sequence=sequence,
                    ablations=comparisons,elapsed_seconds=time.perf_counter()-begin,
                    preview='fixed DN / (2^bits-1) * 255 rounded; no AGC or gamma; not raw data',
                    layer_units={'*photoelectrons':'electron/pixel','*electrons':'electron/pixel',
                                 'raw_dn':'DN, actual effective ADC bits in uint16 container',
                                 'preview_u8':'preview code only'},
                    implementation={f:hashlib.sha256(Path(__file__).with_name(f).read_bytes()).hexdigest()
                                    for f in ('sensor_lab.py','curve_io.py','image_lab.py','image_cli.py')})
    artifacts=[]
    for p in sorted(output.rglob('*')):
        if p.is_file() and p.name != 'manifest.json':
            artifacts.append(dict(path=p.relative_to(output).as_posix(),bytes=p.stat().st_size,
                                  sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
    metadata['artifacts']=artifacts
    (output/'manifest.json').write_text(json.dumps(metadata,indent=2,allow_nan=False)+'\n',encoding='utf8')
    return metadata


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('config',type=Path);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--frames',type=int,default=12);p.add_argument('--seed',type=int,default=20260913)
    p.add_argument('--ablations',action='store_true');args=p.parse_args()
    try:
        result=execute(args.config,args.output,args.frames,args.seed,args.ablations)
    except (ValueError,TypeError,KeyError,OSError) as exc:
        p.exit(2,'Invalid synthetic image experiment: '+str(exc)+'\n')
    print(json.dumps(dict(output=str(args.output.resolve()),frames=len(result['sequence']),
                         level='synthetic_reference',elapsed_seconds=result['elapsed_seconds'])))


if __name__=='__main__':
    main()
