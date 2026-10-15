"""Verify the delivered density-column cache against its unchanged parent data."""
import hashlib
import json
from pathlib import Path
import numpy as np

root = Path(__file__).resolve().parents[1]
config = root / 'HwaSim_IR/Bin/Config'
manifest = json.loads((config / 'Weather/Derived/cloud_manifest_p17.json').read_text())
n = manifest['Size']
report = []
for template in manifest['Templates'].values():
    data = (config / template['Path']).read_bytes()
    parent = (config / template['ParentPath']).read_bytes()
    current = np.frombuffer(data, np.uint8).reshape(n, n, n, 4)
    original = np.frombuffer(parent, np.uint8).reshape(n, n, n, 4)
    density = original[..., 0].astype(float) / 255
    up = (np.cumsum(density[::-1], axis=0)[::-1] - .5 * density) / n
    down = (np.cumsum(density, axis=0) - .5 * density) / n
    error = max(np.max(abs(current[..., 1] / 255 - up)),
                np.max(abs(current[..., 2] / 255 - down)))
    checks = dict(densityUnchanged=bool(np.array_equal(current[..., 0], original[..., 0])),
                  legacyAppearanceRetained=bool(np.array_equal(current[..., 3], original[..., 1])),
                  parentHashMatches=hashlib.sha256(parent).hexdigest() == template['ParentSHA256'],
                  cacheHashMatches=hashlib.sha256(data).hexdigest() == template['SHA256'],
                  columnsWithinHalfByte=bool(error <= .5 / 255 + 1e-12))
    report.append(dict(template=template['Key'], checks=checks, maxColumnError=float(error)))
out = root / 'logs/p17/review/inspection/cloud_cache_check.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2) + '\n')
assert all(all(row['checks'].values()) for row in report), report
print('PASS: four density caches, original density preserved, both light columns verified')
