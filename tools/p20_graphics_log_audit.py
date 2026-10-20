"""追加图形错误核验，保留早期原始回执；计数通过不代表图形正确。"""
from pathlib import Path
import re,json
root=Path(__file__).resolve().parents[1];records=[]
pattern=re.compile(r'Assertion failed:|Failed to compile|Shader compilation failed|GL error|GLSL.*error',re.I)
for p in sorted((root/'logs/p20/runs').glob('*/result.json')):
 result=json.loads(p.read_text(encoding='utf-8-sig'));roles={}
 for role in ('coarse','precise'):
  log=p.parent/role/'board/board.log'
  if not log.exists():continue
  errors=[line for line in log.read_text(errors='replace').splitlines() if pattern.search(line)]
  roles[role]=dict(count=len(errors),first=errors[:3])
 records.append(dict(case=p.parent.name,originalFunctionalStatus=result.get('status'),graphicsLogStatus='FAIL' if any(v['count'] for v in roles.values()) else 'PASS' if roles else 'NOT_VERIFIED',roles=roles))
(root/'logs/p20/graphics_log_audit.json').write_text(json.dumps(records,indent=2),encoding='utf-8')
print(json.dumps([r for r in records if r['graphicsLogStatus']!='PASS'],indent=2))
