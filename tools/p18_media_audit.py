"""Count decoded saved frames independently; never infer counts from MP4 FPS."""
import concurrent.futures, json, subprocess
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
PROBE = ROOT / '.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffprobe.exe'

def inspect(path):
    entry = {'path': path.relative_to(ROOT).as_posix(), 'bytes': path.stat().st_size}
    command = [str(PROBE), '-v', 'error', '-count_frames', '-select_streams', 'v:0',
               '-show_entries', 'stream=width,height,codec_name,pix_fmt,nb_read_frames,r_frame_rate', '-of', 'json', str(path)]
    result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8')
    entry.update(exitCode=result.returncode, stderr=result.stderr)
    try:
        stream = json.loads(result.stdout)['streams'][0]
        entry.update(stream)
        entry['decodedSavedFrames'] = int(stream['nb_read_frames'])
        summary = path.parent / 'summary.json'
        if summary.exists():
            receiver = json.loads(summary.read_text(encoding='utf-8'))
            entry['receiverDecodedFrames'] = receiver['decodedFrames']
            entry['countMatchesReceiver'] = entry['decodedSavedFrames'] == receiver['decodedFrames']
        index = path.parent / 'frame_index.jsonl'
        if index.exists():
            entry['oldFrameIndexRows'] = sum(bool(line.strip()) for line in index.read_text(encoding='utf-8').splitlines())
            entry['countMatchesOldIndex'] = entry['decodedSavedFrames'] == entry['oldFrameIndexRows']
        entry['readable'] = result.returncode == 0 and not result.stderr
    except (KeyError, IndexError, ValueError):
        entry['readable'] = False
    return entry

def main():
    files = sorted((ROOT / 'logs/p18/runs').rglob('output.mp4'))
    files += sorted((ROOT / 'logs/p18/before').glob('*_p17v12/recording/**/output.mp4'))
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        entries = list(pool.map(inspect, files))
    destination = ROOT / 'logs/p18/media_audit.json'
    destination.write_text(json.dumps(entries, indent=2), encoding='utf-8')
    print(json.dumps({'media': len(entries), 'unreadable': [r['path'] for r in entries if not r['readable']],
                      'countMismatch': [r['path'] for r in entries if r.get('countMatchesReceiver') is False or r.get('countMatchesOldIndex') is False]}, indent=2))

if __name__ == '__main__':
    main()
