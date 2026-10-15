"""Bind depth A/B measurements to identical accepted geometry and input hashes."""
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
results = []
for model in ('aim120', 'f22'):
    paths = [root / f'logs/p17/review/after/{model}_{mode}_v4' for mode in ('linear', 'log')]
    plans = [json.loads((p/'case_result.json').read_text(encoding='utf-8-sig')) for p in paths]
    for key in ('inputSha256', 'pixelAngleUrad', 'environment', 'targetTypeProtocolCode',
                'senderExeSha256', 'boardPreflight', 'engineStateOverride', 'band', 'weather'):
        assert plans[0][key] == plans[1][key], (model, key)
    assert plans[0]['depthIsolation']['linearReference'] and not plans[1]['depthIsolation']['linearReference']
    assert all(p['depthIsolation']['nearOverrideM'] == 0 for p in plans)
    sidecars = [sorted((p/'diagnostic_linear').glob('*_geometry.json')) for p in paths]
    assert len(sidecars[0]) == len(sidecars[1]) == 20
    for aa, bb in zip(*sidecars):
        assert json.loads(aa.read_text()) == json.loads(bb.read_text()), (aa, bb)
    values = [json.loads((p/'reprojection/temporal.json').read_text()) for p in paths]
    assert all(v['effectiveMovingPairs'] == 18 for v in values)
    results.append(dict(model=model, sameBuildAndInputs=True, geometryExactlyMatched=True,
                        nearM=1, farM=200000, diagnosticFov=True,
                        captureWindows=[[3600, 3615], [4200, 4203]],
                        method=values[0]['method'], linear=values[0], logarithmic=values[1],
                        scope='Depth root-cause A/B; residuals include resampling/coverage/physical changes. Not all flicker is declared fixed.'))
out = root / 'logs/p17/review/inspection/depth_comparison.json'
out.write_text(json.dumps(results, indent=2)+'\n')
for r in results:
    print(r['model'], 'P99', r['linear']['preencodeInterior']['p99'],
          '->', r['logarithmic']['preencodeInterior']['p99'])
