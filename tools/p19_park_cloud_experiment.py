"""Preserve the paused experiment and restore only its three production files from build 08."""
import difflib, hashlib, json, tarfile
from pathlib import Path

root = Path(__file__).resolve().parents[1]
out = root / 'logs/p19/cloud_deferred'
out.mkdir(parents=True, exist_ok=False)
archive = root / 'logs/p19/build/aarch64_08/HwaSimIR-p11-20261017-123554.source.tgz'
assert hashlib.sha256(archive.read_bytes()).hexdigest() == 'db92ca6016ce422bc3d65ecbc911a352566bacbf3e17731e5a321134538407dd'
source = root / 'HwaSim_IR/HwaSim_IR'
names = ['HwaSimIR.cpp', 'HwaSimIR.h', 'IR/IRCloudPresentation.inl']
extra = ['IR/IRCloudDrawProbe.h', 'IR/IRCloudShaderSpecialization.h']
records, patches, restore = [], [], {}
with tarfile.open(archive) as tar:
    for name in names + extra:
        path = source / name
        current = path.read_bytes()
        saved = out / 'candidate16_source' / name
        saved.parent.mkdir(parents=True, exist_ok=True)
        saved.write_bytes(current)
        original = tar.extractfile('./' + name).read() if name in names else b''
        if name in names:
            restore[name] = original
        patches.extend(difflib.unified_diff(original.decode('utf-8-sig').splitlines(True), current.decode('utf-8-sig').splitlines(True), fromfile='functional08/' + name, tofile='candidate16/' + name))
        records.append({'file':name,'candidateSha256':hashlib.sha256(current).hexdigest(),'functional08Sha256':hashlib.sha256(original).hexdigest() if original else None})
(out/'candidate16_vs_functional08.patch').write_text(''.join(patches), encoding='utf-8')
(out/'manifest.json').write_text(json.dumps({'status':'DEFERRED_BY_USER','baseline':'aarch64_08','candidate16':'BUILD_INTERRUPTED_NOT_DEPLOYED_NOT_VERIFIED','files':records}, indent=2))
# Backup and patch are durable before any production file is changed.
for name, data in restore.items():
    (source/name).write_bytes(data)
for name in extra:
    (source/name).unlink()  # Exact two new experiment-only files, already preserved above.
frozen = json.loads((root/'logs/p19/baseline/frozen_files.json').read_text())
changed, missing = [], []
for name, info in frozen['files'].items():
    path = root/name
    if not path.is_file():
        missing.append(name)
    elif hashlib.sha256(path.read_bytes()).hexdigest() != info['sha256']:
        changed.append(name)
check={'baselineHead':frozen['head'],'fileCount':len(frozen['files']),'changed':changed,'missing':missing,'cloudImplementationUserAuthorized':True,'cloudWorkStatus':'DEFERRED_BY_USER','experimentalChangesIncluded':False}
(root/'logs/p19/baseline/frozen_check_current.json').write_text(json.dumps(check, indent=2))
assert not changed and not missing, check
print(json.dumps(check))
