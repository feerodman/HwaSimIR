"""Compare actual P5/P6 Legacy-display DDS captures without changing pixels."""
import json
import re
import subprocess
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

root = Path(__file__).resolve().parents[1]
out = root / 'logs/p6'
bin_path = root / '.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin'
names = ['p6_baseline_mixed', 'p6_migration_after']
result = {}
images = []

def fields(line):
    return dict(re.findall(r'(\w+)=([^\s]+)', line))

for name in names:
    folder = root / 'logs/p5' / name
    board = (folder / 'board.log').read_text(encoding='utf8', errors='replace')
    receiver = (folder / 'video.err.log').read_text(encoding='utf8', errors='replace')
    matches = lambda tag, text: [fields(x) for x in re.findall(r'\[' + tag + r'\][^\n]+', text)]
    active = [fields(x) for x in board.splitlines() if x.startswith('[Perf]') and 'mode=sync' in x]
    active = [x for x in active if float(x.get('udpFps', 0)) > 0 and float(x.get('renderFps', 0)) > 0]
    # Exclude ramp-up/STOP windows using the same rule for both releases.
    steady = active[1:-1] if len(active) > 2 else active
    result[name] = {
        'request': json.loads((folder / 'request.json').read_text(encoding='utf-8-sig')),
        'rounds': matches('SyncRoundConservation', board),
        'final_association': matches('DdsFrameSync', receiver)[-1],
        'timing': {key: {'interval_mean': float(np.mean([float(x[key]) for x in steady])),
                         'intervals': len(steady)} for key in ['renderFps', 'renderMs']},
    }
    source = folder / 'received.h264'
    dest = folder / 'received.mp4'
    subprocess.run([str(bin_path / 'ffmpeg.exe'), '-v', 'error', '-y', '-r', '60', '-i', str(source),
                    '-c:v', 'copy', '-movflags', '+faststart', str(dest)], check=True)
    result[name]['video'] = json.loads(subprocess.check_output([
        str(bin_path / 'ffprobe.exe'), '-v', 'error', '-count_frames', '-select_streams', 'v:0',
        '-show_entries', 'stream=width,height,nb_read_frames,duration', '-of', 'json', str(dest)], text=True))
    images.append(Image.open(folder / 'received.png').convert('RGB'))

delta = np.abs(np.asarray(images[0], dtype=np.float32) - np.asarray(images[1], dtype=np.float32))
result['decoded_difference'] = {'mean_gray8': float(delta.mean()), 'max_gray8': float(delta.max()),
                                'pixels_gt2': int((delta.mean(axis=2) > 2).sum()),
                                'scope': 'DDS capture index 180; old first-input loss means this is not identical animation phase; no gain correction'}
canvas = Image.new('RGB', (1600, 840), '#171c20')
draw = ImageDraw.Draw(canvas)
for i, (name, im) in enumerate(zip(names, images)):
    canvas.paste(im, (i * 800, 0))
    draw.text((i * 800 + 10, 810), name + ' / Legacy gamma=1', fill='white')
canvas.save(out / 'rk_legacy_migration_comparison.png')
(out / 'migration_evidence.json').write_text(json.dumps(result, indent=2), encoding='utf8')
print(json.dumps(result, indent=2))
