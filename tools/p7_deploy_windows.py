"""Build immutable paired Qt runtime releases and atomically select one.

No existing executable, shared inode or user configuration is overwritten.
The rollback executables are rebuilt from recorded HEAD, not original bytes.
"""
import argparse, hashlib, json, os, shutil, subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'releases/windows'
PLUGINS=('bearer','iconengines','imageformats','platforms','styles','translations')

def digest(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest() if hasattr(hashlib,'file_digest') else hashlib.sha256(f.read()).hexdigest()

def copy(src,dst):
    dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)
    assert digest(src)==digest(dst)

def build(name,baseline=False):
    final=BASE/name;stage=BASE/(name+'.staging')
    if final.exists() or stage.exists():raise RuntimeError('Release exists; refusing to overwrite '+str(final))
    stage.mkdir(parents=True)
    before=ROOT/'logs/p7/baseline_ui_source'
    receiver=ROOT/'HwaSim_IR_VideoDisplay/x64/Release'
    sender=ROOT/'build-DataDrivenTestQT-codex-mingw73_64-Release/release'
    for kind,source,exe in [('receiver',receiver,'HwaSim_IR_VideoDisplay.exe'),('sender',sender,'DataDrivenTestQT.exe')]:
        dest=stage/kind
        for p in source.glob('*.dll'):copy(p,dest/p.name)
        for folder in PLUGINS:
            if (source/folder).exists():shutil.copytree(source/folder,dest/folder)
        for p in (source/'Config').rglob('*'):
            if p.is_file():copy(p,dest/'Config'/p.relative_to(source/'Config'))
        copy(source/'zrddslicence.lic',dest/'zrddslicence.lic')
        binary=source/exe
        if baseline:
            binary=before/('HwaSim_IR_VideoDisplay/x64/Release' if kind=='receiver' else 'build_sender/release')/exe
        copy(binary,dest/exe)
        config=(before if baseline else ROOT)/('HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/NetworkConfig.ini' if kind=='receiver' else 'DataDrivenTestQT/NetworkConfig.ini')
        # Keep the actual final receiver recording policy shipped beside its EXE.
        if kind=='receiver' and not baseline:config=source/'NetworkConfig.ini'
        copy(config,dest/'NetworkConfig.ini')
        if kind=='sender':copy((before if baseline else ROOT)/'DataDrivenTestQT/1.txt',dest/'1.txt')
        else:copy(ROOT/('logs/p7/baseline/receiver-style.css' if baseline else 'HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/qss/style.css'),dest/'qss/style.css')
    files={str(p.relative_to(stage)).replace('\\','/'):digest(p) for p in sorted(stage.rglob('*')) if p.is_file() and p.name!='zrddslicence.lic'}
    runtime={str(p.relative_to(stage)).replace('\\','/'):digest(p) for p in stage.rglob('zrddslicence.lic')}
    value=dict(version=name,head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip(),
        baselineRebuiltFromHead=baseline,originalBinaryBytesPreserved=False if baseline else None,
        runtimeDependencies='Existing local Qt, DDS, OpenCV and FFmpeg DLLs; local licensed runtime only',files=files,
        runtimeManagedInitialHashes=runtime,
        runtimeManagedMeaning='Copied and hash-verified initially. DDS SDK may update its own licence state at runtime; never rewrite, restore or edit it during integrity checks.')
    (stage/'manifest.json').write_text(json.dumps(value,indent=2),encoding='utf-8')
    stage.rename(final);verify(name);return value

def verify(name):
    folder=(BASE/name).resolve()
    if folder.parent!=BASE.resolve():raise RuntimeError('Invalid release path')
    manifest=json.loads((folder/'manifest.json').read_text())
    for relative,expected in manifest['files'].items():
        if digest(folder/relative)!=expected:raise RuntimeError('Hash mismatch '+relative)
    return manifest

def select(name):
    verify(name);temporary=BASE/'current.json.new'
    temporary.write_text(json.dumps(dict(version=name),indent=2),encoding='utf-8')
    os.replace(temporary,BASE/'current.json')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('action',choices=['build','select','verify'])
    parser.add_argument('name');parser.add_argument('--baseline',action='store_true');args=parser.parse_args()
    BASE.mkdir(parents=True,exist_ok=True)
    value=build(args.name,args.baseline) if args.action=='build' else verify(args.name)
    if args.action=='select':select(args.name)
    print(json.dumps(dict(action=args.action,release=args.name,result='PASS',files=len(value['files']))))
