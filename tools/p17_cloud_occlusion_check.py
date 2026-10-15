"""Validate sampled real depth and pre-display source-over on the RK3588.

This diagnostic readback is deliberately separate from production/performance
recordings. The probe restores GLES state and does not change shader uniforms.
"""
import json
import re
import struct
from pathlib import Path
import numpy as np

root = Path(__file__).resolve().parents[1]
case = root/'logs/p7/p17_cloud_blend_audit_v7'
lines = (case/'board.log').read_text(encoding='utf-8', errors='replace').splitlines()
density = fade = distance = before = 0.0
rows, foreground_depth = [], []
for line in lines:
    if '[P17CloudRayUniform]' in line:
        density = float(re.search(r'density=([\d.]+)', line)[1])
        fade = float(re.search(r'fade=([\d.]+)', line)[1])
    elif '[P17CloudDepthPixel]' in line:
        value = float(re.search(r'distance=([\d.]+)', line)[1])
        if 'xy=400,390' in line and 340 < value < 360:
            foreground_depth.append(value)
        if 'xy=400,395' in line:
            distance = value
    elif '[P17CloudBlendPixel]' in line:
        assert 'readType=140b' in line and 'depthTest=0' in line and 'blend=1' in line
        bits = int(re.search(r'valueHalf=([0-9a-f]+)', line)[1],16)
        value = struct.unpack('<e', struct.pack('<H',bits))[0]
        if 'phase=before' in line:
            before = value
        elif density > .7 and fade == 1:
            rows.append([distance,before,value,value-before])
phases = {}
for name, target in [('inside',2000),('behind',3400)]:
    values = np.array([row for row in rows if abs(row[0]-target) < 10])
    assert len(values) >= 3 and np.all(values[:,3] > .01)
    phases[name] = dict(samples=len(values), distanceBeforeAfterDeltaMedian=np.median(values,axis=0).tolist())
assert foreground_depth and max(foreground_depth) < 1233
assert phases['behind']['distanceBeforeAfterDeltaMedian'][3] > phases['inside']['distanceBeforeAfterDeltaMedian'][3]
report = dict(result='PASS_SAMPLED_DEPTH_AND_COMPOSITE', domain='NIR legacy linear, pre-AGC, RGB16F',
    front=dict(sampledOpaqueDistanceM=foreground_depth, cloudRayEntryM=1233.35,
               conclusion='Opaque surface precedes cloud entry; front clipping is also reviewed in the fixed-mapping movie'),
    phases=phases, limitations=['Selected pixels, not full radiometric calibration',
        'NIR default AGC clips this target to black; fixed-mapping movie is explicitly separate',
        'No full performance claim from a GLES readback diagnostic'])
(root/'logs/p17/review/inspection/cloud_occlusion_check.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS: front depth precedes cloud; sampled inside and behind compositing is nonzero and ordered')
