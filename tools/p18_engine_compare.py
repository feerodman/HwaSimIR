"""Match ordinary engine ON/OFF by source ordinal, including trajectory tail."""
import json,subprocess
from pathlib import Path
from PIL import Image,ImageDraw
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'logs/p18/visual'
FF=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe';entries=[]
for band in ['swir','mwir']:
    for seq in [1200,3600,4200]:
        sheet=Image.new('RGB',(1600,850),(20,24,30));d=ImageDraw.Draw(sheet);sources=[]
        for i,state in enumerate(['on','off']):
            case=f'plume_{band}_10_after_v2' if state=='on' else f'plume_{band}_engine_off_v2'
            source=ROOT/f'logs/p18/runs/{case}/customer/output.mp4';dest=OUT/f'{band}_{seq}_engine_{state}.png'
            subprocess.run([str(FF),'-y','-v','error','-i',str(source),'-vf',f'select=eq(n\\,{seq-1})','-vsync','0','-frames:v','1',str(dest)],check=True)
            sheet.paste(Image.open(dest).convert('RGB'),(800*i,50));d.text((800*i+10,16),f'{band.upper()} engine={state.upper()} sourceSeq={seq} / 10 urad / fixed display',fill='white')
            sources.append(source.relative_to(ROOT).as_posix())
        path=OUT/f'{band}_{seq}_engine_comparison.png';sheet.save(path)
        entries.append(dict(band=band,sourceSeq=seq,sources=sources,image=path.relative_to(ROOT).as_posix(),
            note='Engine switch also affects existing body heat sources; whole-image difference is not pure plume radiance'))
(OUT/'engine_comparison_index.json').write_text(json.dumps(entries,indent=2),encoding='utf-8')
