"""Check actual local targets from the required documents and evidence index."""
import json,re
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote,urlsplit
ROOT=Path(__file__).resolve().parents[1]
files=[ROOT/'docs'/n for n in ['HwaSimIR_P18_Closeout.md','HwaSimIR_DDS_EndToEnd_Guide.md','HwaSimIR_Customer_TX_DDS_ICD.md','HwaSimIR_P18_DualChannel_Runbook.md']]
files += [ROOT/'DDS/HwaSimIRCustomerTxVideoReceiverDemo/README.md',ROOT/'logs/p18/index.html']
class Links(HTMLParser):
    def __init__(self):super().__init__();self.links=[]
    def handle_starttag(self,tag,attrs):self.links.extend(v for k,v in attrs if k in ['src','href'])
checked=[];missing=[]
for file in files:
    if not file.exists():missing.append(dict(source=str(file),target='SOURCE_MISSING'));continue
    text=file.read_text(encoding='utf-8')
    if file.suffix=='.html':parser=Links();parser.feed(text);links=parser.links
    else:links=re.findall(r'!?\[[^\]]*\]\(([^)]+)\)',text)
    for link in links:
        link=link.strip('<>');parsed=urlsplit(link)
        if parsed.scheme or not parsed.path:continue
        path=(file.parent/unquote(parsed.path)).resolve();entry=dict(source=file.relative_to(ROOT).as_posix(),link=link,exists=path.exists())
        checked.append(entry)
        if not entry['exists']:missing.append(entry)
report=dict(result='PASS' if not missing else 'FAIL',checked=len(checked),missing=missing)
(ROOT/'logs/p18/delivery_link_check.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report,indent=2))
raise SystemExit(0 if not missing else 1)
