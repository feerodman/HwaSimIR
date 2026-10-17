"""Extract corresponding lossless frames and amplified differences for review.

No pass threshold is inferred from a whole-image average. Selection includes
the largest per-frame error and the endpoints of the same received sequence.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

parser = argparse.ArgumentParser()
parser.add_argument('candidate')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1] / 'logs/p19/runs'
candidate = root / args.candidate
comparison = json.loads((candidate / 'visual_raw_comparison.json').read_text(encoding='utf-8'))
out = candidate / 'visual_review'
out.mkdir(exist_ok=True)
manifest = {'reference': comparison['reference'], 'candidate': comparison['candidate'],
            'differenceScale': 8, 'visualAcceptance': 'NOT_VERIFIED', 'frames': []}
for role, stats in comparison['streams'].items():
    shape = (stats['frames'], stats['height'], stats['width'])
    reference = np.memmap(root / comparison['reference'] / role / 'received.gray8',
                          dtype=np.uint8, mode='r', shape=shape)
    current = np.memmap(candidate / role / 'received.gray8', dtype=np.uint8, mode='r', shape=shape)
    worst = max(stats['perFrame'], key=lambda x: x['mae'])['frame'] - 1
    peak = max(stats['perFrame'], key=lambda x: x['max'])['frame'] - 1
    for index in sorted({0, shape[0] // 2, shape[0] - 1, worst, peak}):
        delta = np.abs(reference[index].astype(np.int16) - current[index].astype(np.int16))
        panel = Image.new('RGB', (shape[2] * 3, shape[1] + 28), '#202020')
        draw = ImageDraw.Draw(panel)
        planes = (reference[index], current[index], np.minimum(delta * 8, 255).astype(np.uint8))
        for col, (plane, label) in enumerate(zip(planes, ('full resolution', 'candidate', 'absolute difference x8'))):
            panel.paste(Image.fromarray(plane).convert('RGB'), (col * shape[2], 28))
            draw.text((col * shape[2] + 8, 7), f'{role} frame {index + 1}: {label}', fill='white')
        name = f'{role}_{index + 1:04d}.png'
        panel.save(out / name)
        manifest['frames'].append({'file': name, 'role': role, 'frame': index + 1,
                                   'mae': float(delta.mean()), 'maximum': int(delta.max())})
    # Temporal change errors expose unstable sampling without claiming that a
    # temporal metric can replace checking the corresponding video sequence.
    temporal = []
    center_histogram = np.zeros(256, dtype=np.int64)
    radius = max(8, shape[2] // 25)
    center = (slice(shape[1] // 2 - radius, shape[1] // 2 + radius),
              slice(shape[2] // 2 - radius, shape[2] // 2 + radius))
    for index in range(shape[0]):
        delta = np.abs(reference[index][center].astype(np.int16) - current[index][center].astype(np.int16))
        center_histogram += np.bincount(delta.ravel(), minlength=256)
    manifest.setdefault('centralTargetRegion', {})[role] = {
        'scope': 'fixed central region containing tracked target; not a semantic mask',
        'radiusPixels': radius,
        'mae': float(np.dot(center_histogram, np.arange(256)) / center_histogram.sum()),
        'maximum': int(np.flatnonzero(center_histogram)[-1]),
        'fractionAbove8': float(center_histogram[9:].sum() / center_histogram.sum())}
    for index in range(1, shape[0]):
        original_change = reference[index].astype(np.int16) - reference[index - 1].astype(np.int16)
        candidate_change = current[index].astype(np.int16) - current[index - 1].astype(np.int16)
        temporal.append(float(np.abs(candidate_change - original_change).mean()))
    manifest.setdefault('temporal', {})[role] = {'meanDeltaError': float(np.mean(temporal)),
                                               'maxDeltaError': max(temporal)}
(out / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
print(json.dumps(manifest, indent=2))
