"""Deterministic analytic sprite masks (not a photo or a calibrated measurement)."""
from pathlib import Path
import numpy as np
from PIL import Image
root=Path(__file__).resolve().parents[1]/'HwaSim_IR/Bin/Config/GameVFX'
root.mkdir(parents=True,exist_ok=True)
y,x=np.mgrid[-1:1:128j,-1:1:128j]
r=np.sqrt(x*x+y*y)
edge=np.clip((1-r)/.35,0,1); edge=edge*edge*(3-2*edge)
glow=np.exp(-3*r*r)*edge
smoke=np.exp(-2*r*r)*edge*(.78+.10*np.sin(x*11+y*7)+.07*np.cos(x*17-y*13))
Image.fromarray(np.rint(np.clip(np.concatenate([glow,smoke],axis=1),0,1)*255).astype('uint8')).save(root/'soft_sprite_atlas.png')
print(root/'soft_sprite_atlas.png')
