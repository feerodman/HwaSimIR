"""Parse real HwaFrameV2 products from retained original-chain captures."""
import json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];entries=[]
for directory in sorted((ROOT/'logs/p18/runs').iterdir()):
    if not (directory/'result.json').exists():continue
    for filename,target in [('old_received.h264','old_sei_check.json'),('old_reopened.h264','old_reopened_sei_check.json'),('customer/received.h264','customer_sei_check.json')]:
        path=directory/filename
        if not path.exists():continue
        parsed=json.loads(subprocess.check_output([str(ROOT/'logs/p18/tools/frame_product_check.exe'),str(path)],text=True))
        (directory/target).write_text(json.dumps(parsed,indent=2),encoding='utf-8')
        entries.append(dict(path=path.relative_to(ROOT).as_posix(),result=parsed))
(ROOT/'logs/p18/old_and_customer_sei_audit.json').write_text(json.dumps(entries,indent=2),encoding='utf-8')
print('Parsed captures:',len(entries))
