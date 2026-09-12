"""Extract real received video frames for temporal visual review."""
from pathlib import Path
import subprocess
from PIL import Image, ImageDraw

repo = Path(__file__).resolve().parents[1]
root = repo / 'logs/p6'
ffmpeg = repo / '.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe'
for name in ['rk_plume_before_orbit', 'rk_plume_smoke2_orbit', 'rk_release_translate', 'rk_release_entry', 'rk_pause']:
    folder = root / name
    video = folder / 'received.mp4'
    if not video.exists():
        continue
    canvas = Image.new('RGB', (1600, 435), '#16191d')
    draw = ImageDraw.Draw(canvas)
    for i, seconds in enumerate([0, 2, 4, 6]):
        frame = folder / f'video_at_{seconds}s.png'
        subprocess.run([str(ffmpeg), '-v', 'error', '-y', '-ss', str(seconds), '-i', str(video),
                        '-frames:v', '1', str(frame)], check=True)
        if not frame.exists():
            continue
        im = Image.open(frame).convert('RGB'); im.thumbnail((400, 400))
        canvas.paste(im, (i * 400, 0))
        draw.text((i * 400 + 8, 408), f'{name} / playback {seconds}s', fill='white')
    canvas.save(folder / 'temporal_review.png')
    print(folder / 'temporal_review.png')
