"""Generate color byte references with the installed Panda library, not the implementation under test."""
import struct,random
from pathlib import Path
from panda3d.core import Texture
path=Path('logs/p8/native_rgb_panda_golden.bin');cases=[];rng=random.Random(803)
for components,fmt in [(3,Texture.F_rgb),(4,Texture.F_rgba)]:
    for width,height in [(1,1),(15,1),(16,1),(17,1),(31,17),(800,800)]:
        data=bytes(rng.randrange(256) for _ in range(width*height*components))
        texture=Texture();texture.setup_2d_texture(width,height,Texture.T_unsigned_byte,fmt);texture.set_ram_image(data)
        expected=bytes(texture.get_ram_image_as('RGB'));cases.append((components,width*height,data,expected))
with path.open('wb') as f:
    f.write(struct.pack('<I',len(cases)))
    for components,pixels,data,expected in cases:f.write(struct.pack('<II',components,pixels)+data+expected)
print(path,len(cases),path.stat().st_size)
