"""Delivery copies and labelled comparisons of actual received pixels.

Independent images are byte copies, videos are lossless H.264 remuxes made by
p6c_display_evidence.py. Only explicitly named comparison videos are reencoded.
"""
from pathlib import Path
import hashlib,json,shutil,subprocess,os
import numpy as np
from PIL import Image,ImageDraw,ImageFont
os.environ.setdefault('MPLCONFIGDIR',str(Path(__file__).resolve().parents[1]/'logs/p6c/mpl_cache'))
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'logs/p6c';DEST=OUT/'delivery';DEST.mkdir(exist_ok=True)
font_config=OUT/'fontconfig.xml'
font_cache=OUT/'font_cache';font_cache.mkdir(exist_ok=True)
font_config.write_text(f'<fontconfig><dir>C:/Windows/Fonts</dir><cachedir>{font_cache.as_posix()}</cachedir></fontconfig>',encoding='utf8')
os.environ['FONTCONFIG_FILE']=str(font_config)
BIN=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin'
records=[]
def record(p,sources,meaning):
    records.append(dict(path=str(p.relative_to(ROOT)),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),sources=sources,meaning=meaning))

native={
 'chart_before_game':'before_b2_Game','chart_after_game':'precision_b2_Game',
 'chart_after_auto':'after_b2_Auto','chart_black':'polarity_Black','hdr_game':'hdr_Game','hdr_auto':'hdr_Auto',
 'large_dynamic':'large_Auto','large_frozen':'large_frozen','large_fixed':'large_Game',
 'small_dynamic':'small_Auto','constant':'flat_Auto','near_constant':'nearflat_Auto',
 'sample_alias_counterexample':'alias_Auto',
 'normal_nvg_default':'release_b1_60hz_60s','normal_mwir_default':'release_b2_60hz_60s'}
for name,case in native.items():
    for ext in ('png','mp4'):
        source=OUT/case/f'received.{ext}'
        if not source.exists():continue
        p=DEST/f'{name}.{ext}';shutil.copyfile(source,p)
        record(p,[str(source.relative_to(ROOT))],'unannotated actual RK3588 -> MPP -> DDS -> Windows; PNG byte copy / MP4 H.264 remux, no pixel edits')

font=ImageFont.truetype('C:/Windows/Fonts/arial.ttf',25)
pairs=[('display_game_before_after','before_b2_Game','precision_b2_Game','P6B Game - actual received','P6C MWIR Game - actual received'),
       ('auto_dynamic_frozen','large_Auto','large_frozen','Dynamic Auto - same input sequence','Auto frozen at sequence 40'),
       ('auto_small_large','small_Auto','large_Auto','Small bright area - Auto','Large bright area - Auto')]
for name,left,right,ll,rr in pairs:
    a=OUT/left/'received.png';b=OUT/right/'received.png'
    if not a.exists() or not b.exists():continue
    canvas=Image.new('RGB',(1600,848),'#181818');draw=ImageDraw.Draw(canvas)
    canvas.paste(Image.open(a).convert('RGB'),(0,0));canvas.paste(Image.open(b).convert('RGB'),(800,0))
    draw.text((12,809),ll,font=font,fill='white');draw.text((812,809),rr,font=font,fill='white')
    p=DEST/f'{name}.png';canvas.save(p)
    record(p,[str(a.relative_to(ROOT)),str(b.relative_to(ROOT))],'native-size received images joined; labels outside original pixels')
    a=OUT/left/'received.mp4';b=OUT/right/'received.mp4';p=DEST/f'{name}.mp4'
    if not a.exists() or not b.exists():continue
    # Fixed native dimensions, common sequence/time origin; no contrast edits.
    filters=f'[0:v]setpts=PTS-STARTPTS,pad=800:848:0:0:black,drawtext=text={ll}:x=12:y=809:fontsize=25:fontcolor=white[l];[1:v]setpts=PTS-STARTPTS,pad=800:848:0:0:black,drawtext=text={rr}:x=12:y=809:fontsize=25:fontcolor=white[r];[l][r]hstack=inputs=2[v]'
    subprocess.run([str(BIN/'ffmpeg.exe'),'-v','error','-y','-i',str(a),'-i',str(b),'-filter_complex',filters,'-map','[v]','-an','-shortest','-c:v','libx264','-crf','18','-pix_fmt','yuv420p','-movflags','+faststart',str(p)],check=True)
    record(p,[str(a.relative_to(ROOT)),str(b.relative_to(ROOT))],'side-by-side actual DDS streams, matched sequence origin, labels outside image; comparison only reencoded CRF18')

fig,axes=plt.subplots(1,2,figsize=(12,4),constrained_layout=True)
x=np.linspace(0,4,4097)
axes[0].plot(x,np.clip(x,0,1)**(1/1.8)*255,label='P6B Game formula (reference)')
axes[0].plot(x,(3*x/(1+3*x))**(1/2.2)*255,label='P6C MWIR Game formula')
for case,label in [('hdr_Game','Actual GPU'),('hdr_Auto','Actual Auto GPU')]:
    raw=np.load(OUT/case/'defined_linear.npy')[80]
    g=np.asarray(Image.open(OUT/case/'linear_rgb8.png'))[80,:,0]
    axes[0].scatter(raw[::20],g[::20],s=8,label=label)
axes[0].set(xlabel='Dimensionless test definition',ylabel='Pre-encode RGB8',title='HDR mapping: formula and actual GPU');axes[0].legend(fontsize=8)
for case,label in [('large_Auto','Large area / dynamic'),('large_frozen','Large area / frozen'),('small_Auto','Small area / dynamic')]:
    p=json.loads((OUT/case/'display_parameters.json').read_text())['mapping_history']
    axes[1].plot([int(m['sourceSeq']) for m in p],[float(m['agcGain']) for m in p],label=label)
axes[1].axvspan(60,140,alpha=.1,color='orange');axes[1].set(xlabel='Input source sequence',ylabel='Actual AGC gain',title='Bright area present at sequences 60..139');axes[1].legend(fontsize=8)
p=DEST/'mapping_and_adaptation.png';fig.savefig(p,dpi=140);plt.close(fig)
record(p,['logs/p6c/*/display_parameters.json','logs/p6c/hdr_Game/linear_rgb8.png'],'scientific plot of explicit mathematical reference and measured mapping; reference curve is not an old HDR capture')
(DEST/'manifest.json').write_text(json.dumps(records,indent=2)+'\n',encoding='utf8')
print('Display delivery artifacts:',len(records))
