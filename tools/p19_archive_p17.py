"""Read-only board backup. Credentials come only from the caller's environment."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'logs/p19/board/p17_offboard'
OUT.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
env.update(SSH_ASKPASS=str(ROOT / 'tools/p5_ssh_askpass.cmd'),
           SSH_ASKPASS_REQUIRE='force', DISPLAY='p19')
ssh = ['ssh', '-o', 'UpdateHostKeys=no', '-o', 'ConnectTimeout=10', 'root@192.168.1.116']

def remote(command):
    return subprocess.check_output(ssh + [command], env=env)

# tar traverses Config_new, but never follows symbolic links. No board file is removed.
archive = OUT / 'user_p17_v2.tar.gz'
if archive.exists():
    raise SystemExit('Refuse to overwrite an existing archive')
manifest = remote("cd /userdata/HwaSimIR && find Config_new -type f -exec sha256sum {} + && sha256sum HwaSim_IR_new")
(OUT / 'board_sha256.txt').write_bytes(manifest)
with archive.open('xb') as target:
    subprocess.run(ssh + ['cd /userdata/HwaSimIR && tar -czf - Config_new HwaSim_IR_new run_precise.sh rk3588_hwasimir_performance_mode.sh'],
                   env=env, stdout=target, check=True)

expected = dict((line.split('  ', 1)[1], line.split('  ', 1)[0])
                for line in manifest.decode().splitlines())
restore = OUT / 'restore_check'
restore.mkdir(exist_ok=False)
seen = {}
links = []
with tarfile.open(archive, 'r:gz') as tf:
    for member in tf:
        path = (restore / member.name).resolve()
        if not path.is_relative_to(restore.resolve()):
            raise RuntimeError('unsafe archive path')
        if member.isfile():
            path.parent.mkdir(parents=True, exist_ok=True)
            # Materialize actual restored bytes, then compare to the board manifest.
            with tf.extractfile(member) as source, path.open('xb') as target:
                while True:
                    block = source.read(1024 * 1024)
                    if not block:
                        break
                    target.write(block)
            seen[member.name] = hashlib.sha256(path.read_bytes()).hexdigest()
        elif member.issym() or member.islnk():
            links.append({'path': member.name, 'target': member.linkname})
missing = [name for name, digest in expected.items() if seen.get(name) != digest]
report = {'status': 'PASS' if not missing else 'FAIL', 'archive': str(archive.relative_to(ROOT)),
          'sha256': hashlib.sha256(archive.read_bytes()).hexdigest(), 'verifiedFiles': len(expected),
          'mismatches': missing, 'links': links, 'restoredFiles': len(seen),
          'scope': 'P17 ELF, complete Config_new and existing launcher/network files; system DDS/Mali/licence remain on board',
          'boardMutation': False}
(OUT / 'verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2), flush=True)
if missing:
    raise SystemExit(2)
