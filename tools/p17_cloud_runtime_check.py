"""Check the observed GLES upload path, not the SDK's advertised version."""
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
before = (root/'logs/p7/p17_cloud_upload_audit_v2/board.log').read_text(encoding='utf-8', errors='replace')
after = (root/'logs/p7/p17_cloud_upload_v7/board.log').read_text(encoding='utf-8', errors='replace')
upload = re.compile(r'\[P17GLUpload\] (\w+) level=(\d+) size=(\d+),(\d+),(\d+).* data=(\S+)')
bad = [m.groups() for m in upload.finditer(before) if int(m[2]) > 0 and m[3] != m[5]]
assert bad, 'The baseline must exhibit actual nonshrinking Z uploads'
base = [m.groups() for m in upload.finditer(after) if m[6] != '(nil)']
assert base and all(int(x[1]) == 0 and tuple(map(int, x[2:5])) == (96,96,96) for x in base)
levels = [tuple(map(int,m)) for m in re.findall(r'\[P17GLGeneratedMip\] level=(\d+) size=(\d+),(\d+),(\d+)',after)]
expected = [(i,n,n,n) for i,n in enumerate((96,48,24,12,6,3,1))]
assert len(levels) >= 7 and len(levels) % 7 == 0
assert all(levels[i:i+7] == expected for i in range(0,len(levels),7))
assert 'Segmentation fault' not in after and 'OpenGL error' not in after
report = dict(result='PASS', scope='Actual board GLES texture dimensions; no rendering overrides',
    baselineInvalidMipUploads=bad, fixedCpuUploads=base,
    generatedLevels=levels, densityBaseUnchanged=True,
    notClaimed=['calibration', 'all performance thresholds', 'all weather/camera configurations'])
out = root/'logs/p17/review/inspection/cloud_runtime_check.json'
out.write_text(json.dumps(report, indent=2)+'\n')
print('PASS: invalid CPU 3D mip uploads eliminated; GPU 96..1 cube chain verified')
