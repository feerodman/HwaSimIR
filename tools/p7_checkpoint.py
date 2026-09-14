"""Copy changed source and shipped Windows products without touching Git state."""
import argparse, hashlib, json, shutil, subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[1]
if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('name')
    args = parser.parse_args()
    dest = root / 'logs/p7/checkpoints' / args.name
    if dest.exists():
        raise SystemExit('Checkpoint exists; never overwrite evidence.')
    names = subprocess.check_output(['git', 'diff', '--name-only'], cwd=root).decode().splitlines()
    names += subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard'], cwd=root).decode().splitlines()
    names += ['HwaSim_IR/Bin/HwaSim_IR.exe', 'HwaSim_IR_VideoDisplay/x64/Release/HwaSim_IR_VideoDisplay.exe',
              'HwaSim_IR_VideoDisplay/x64/Release/qss/style.css',
              'build-DataDrivenTestQT-codex-mingw73_64-Release/release/DataDrivenTestQT.exe']
    manifest = {}
    for name in sorted(set(names)):
        src = root / name
        if not src.is_file():
            continue
        output = dest / name
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, output)
        digest = hashlib.sha256(src.read_bytes()).hexdigest()
        assert hashlib.sha256(output.read_bytes()).hexdigest() == digest
        manifest[name] = digest
    (dest/'manifest.json').write_text(json.dumps(dict(head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root).decode().strip(),
        sourceAndBinaryMayHaveSeparateBuildRevisions=True,files=manifest), indent=2), encoding='utf-8')
    print(dest)
