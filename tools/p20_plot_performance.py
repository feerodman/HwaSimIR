"""将完整逐帧记录画成独立性能图；不挑区间、不删除尖峰。"""
import argparse,csv,json,os
from pathlib import Path
os.environ.setdefault('MPLCONFIGDIR',str(Path(__file__).resolve().parents[1]/'logs/p20/matplotlib_cache'))
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
plt.rcParams['font.sans-serif']=['Microsoft YaHei','SimHei','DejaVu Sans']
plt.rcParams['axes.unicode_minus']=False
root=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('case');a=ap.parse_args()
folder=root/'logs/p20/runs'/a.case
fig,axes=plt.subplots(3,1,figsize=(12,8),sharex=True,constrained_layout=True)
for role in ['coarse','precise']:
    rows=list(csv.DictReader((folder/role/'performance_frames.csv').open()))
    t=[float(r['elapsedSeconds']) for r in rows]
    label='粗跟' if role=='coarse' else '精跟'
    axes[0].plot(t,[int(r['remainingInputFrames']) for r in rows],label=label,lw=.7)
    axes[1].plot(t,[float(r['enqueueToRenderedAgeMs']) for r in rows],label=label,lw=.6,alpha=.8)
    p=json.loads((folder/'performance.json').read_text())['streams'][role]
    windows=[r for r in p['oneSecondWindows'] if not r['partialLast']]
    axes[2].plot([r['second'] for r in windows],[r['frames'] for r in windows],label=label,lw=.8)
axes[0].set_ylabel('输入队列（帧）');axes[1].set_ylabel('板端入队至渲染（ms）')
axes[2].set_ylabel('完整1秒窗口的新帧数');axes[2].set_xlabel('经过时间（秒）')
axes[2].axhline(60,color='black',ls='--',lw=.7)
for ax in axes:ax.legend();ax.grid(alpha=.25)
fig.suptitle(a.case+'\n全部帧，未剔除预热或尖峰')
fig.savefig(folder/'performance_plot.png',dpi=150);plt.close(fig)
print(folder/'performance_plot.png')
