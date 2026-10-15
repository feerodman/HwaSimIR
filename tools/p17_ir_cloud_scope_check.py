"""Separate camera-fixture intent from observed cloud streaming evidence."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
reference = 'D4CB564594352F28'
rows = []
for band in ('swir', 'mwir'):
    for weather in ('clear', 'cloudy', 'overcast'):
        case = f'p17_{band}_{weather}_above_ir_v8'
        path = ROOT / 'logs/p7' / case / 'board.log'
        lines = path.read_text(encoding='utf-8', errors='replace').splitlines()
        events = [s for s in lines if '[World3DCloudCell]' in s and reference in s]
        selected = [int(m.group(1)) for s in lines if '[World3DCloudStreaming]' in s
                    for m in [re.search(r'visibleCloudVolumes=(\d+)', s)] if m]
        rows.append(dict(caseId=case, referenceCloud=reference, referenceCloudEvents=events,
            referenceCloudLastState=events[-1] if events else 'NOT_ACTIVATED',
            maxSelectedVolumeCount=max(selected, default=0),
            countScope='Selected for rendering; count alone is not pixel contribution proof',
            targetInsideRenderedReferenceCloud='NOT_VERIFIED',
            targetCloudOcclusion='NOT_VERIFIED'))
out = ROOT / 'logs/p17/ir_retest/cloud_scope_check.json'
out.write_text(json.dumps(dict(result='RETEST_SCOPE_LIMITED', cases=rows,
    fixtureProvenance='Frozen cloud_above.json Scope text described intent. Runtime evidence supersedes its actual F22 in cloud phrase; original input is retained unchanged.',
    visualReview='Decoded SWIR cloud structure is visible; MWIR cloudy structure remains weak and overcast is more uniform. No contrast enhancement was applied.',
    acceptedScope='Above-cloud ordinary Clear/Cloudy/Overcast comparison inside the existing formal atmospheric query domain; no in-cloud camera or target-occlusion acceptance'),
    indent=2) + '\n', encoding='utf-8')
print(out)
