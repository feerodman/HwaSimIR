"""Scientific views of synthetic layer arrays; raw arrays and previews stay untouched."""
from pathlib import Path
import json,hashlib,subprocess,os
import numpy as np
os.environ.setdefault('MPLCONFIGDIR',str(Path(__file__).resolve().parents[1]/'logs/p6c/mpl_cache'))
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'logs/p6c'
FFMPEG=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe'
records=[]
for band in ('nir','mwir'):
    base=OUT/f'synthetic_{band}_800_final';dest=base/'views';dest.mkdir(exist_ok=True)
    keys=[('expected_photoelectrons','Expected photoelectrons','electron / pixel'),
          ('expected_dark_electrons','Expected dark signal','electron / pixel'),
          ('noisy_charge_electrons','Photo + dark shot charge','electron / pixel'),
          ('readout_electrons','After full well and read noise','electron / pixel'),
          ('raw_dn','Raw 14-bit digital codes','DN (uint16, no rescaling)')]
    fig,axes=plt.subplots(2,3,figsize=(15,9),constrained_layout=True)
    for ax,(key,title,unit) in zip(axes.flat,keys):
        a=np.load(base/'frame_0000'/f'{key}.npy',allow_pickle=False)
        im=ax.imshow(a,cmap='viridis',vmin=0,vmax=16383 if key=='raw_dn' else None)
        ax.set_title(title);ax.set_xlabel('pixel x');ax.set_ylabel('pixel y')
        fig.colorbar(im,ax=ax,label=unit,shrink=.8)
    ax=axes.flat[-1]
    pattern=np.load(base/'fixed_prnu_factor.npy',allow_pickle=False)
    im=ax.imshow(pattern,cmap='coolwarm',vmin=.94,vmax=1.06)
    ax.set_title('Stationary artificial PRNU');fig.colorbar(im,ax=ax,label='factor',shrink=.8)
    fig.suptitle(f'Synthetic {band.upper()} photon array / diagnostic color scales / not device calibration')
    fig.savefig(dest/'synthetic_layer_views.png',dpi=120);plt.close(fig)
    fig,axes=plt.subplots(1,5,figsize=(18,4),constrained_layout=True)
    for ax,name in zip(axes,('expectation_only','psf_only','fixed_pattern_only','shot_only','full')):
        dn=np.load(base/'ablations'/name/'raw_dn.npy',allow_pickle=False)
        ax.imshow(dn,cmap='gray',vmin=0,vmax=16383);ax.set_title(name);ax.axis('off')
    fig.suptitle('Same synthetic frame / identical DN scale / layer switches')
    fig.savefig(dest/'synthetic_layer_comparison.png',dpi=120);plt.close(fig)
    video=dest/'synthetic_preview.mp4'
    subprocess.run([str(FFMPEG),'-v','error','-y','-framerate','10','-i',str(base/'frame_%04d/preview_u8.png'),'-frames:v','40','-c:v','libx264','-crf','18','-pix_fmt','yuv420p','-movflags','+faststart',str(video)],check=True)
    for p in dest.iterdir():
        if p.is_file():records.append(dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),meaning='diagnostic view or preview video; raw values remain in original NPY/PNG files'))
(OUT/'sensor_view_manifest.json').write_text(json.dumps(records,indent=2)+'\n',encoding='utf8')
print('Synthetic diagnostic views:',len(records))
