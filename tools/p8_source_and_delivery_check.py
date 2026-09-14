"""Record the real dirty checkout and validate local delivery dependencies."""
import hashlib,json,re,subprocess,zipfile
from pathlib import Path
from html.parser import HTMLParser

ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'logs/p8';OUT=BASE/'delivery'
def digest(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def git(*args):return subprocess.check_output(['git',*args],cwd=ROOT,stderr=subprocess.PIPE)
def main():
    folder=BASE/'source_identity';folder.mkdir(exist_ok=True)
    head=git('rev-parse','HEAD').decode().strip();assert head=='44ce295d61e5f4e5ccab990260e18964c4e72b11'
    paths=set(git('diff','--name-only','-z').decode().split('\0'))|set(git('ls-files','--others','--exclude-standard','-z').decode().split('\0'))
    paths=sorted(p for p in paths if p and (ROOT/p).is_file())
    files={p:dict(sha256=digest(ROOT/p),bytes=(ROOT/p).stat().st_size) for p in paths}
    (folder/'tracked_changes.patch').write_bytes(git('diff','--binary'))
    with zipfile.ZipFile(folder/'changed_source_and_documents.zip','w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for p in paths:z.write(ROOT/p,p)
    identity=hashlib.sha256(json.dumps(files,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    package=json.loads((ROOT/'releases/windows/P8_A8/manifest.json').read_text())
    manifest=dict(head=head,dirty=True,changeSetSha256=identity,files=files,boardElfSha256='b09c0f6013964dbd8d00812af60e328c8bc7a429e07b07e5192a3c4c6bb4d867',windowsRelease='P8_A8',windowsExecutables={p:h for p,h in package['files'].items() if p.endswith('.exe')},meaning='Actual checkout: HEAD plus changed/untracked bytes. No commit/reset. Original user attachment included unchanged. Source zip excludes ignored logs/release binaries and runtime licence files.')
    (folder/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    class Links(HTMLParser):
        def __init__(self):super().__init__();self.links=[]
        def handle_starttag(self,tag,attrs):
            for k,v in attrs:
                if k in ('href','src') and v:self.links.append(v)
    parser=Links();parser.feed((OUT/'index.html').read_text(encoding='utf-8'))
    missing=[]
    for link in parser.links:
        if link.startswith(('http:','https:','#','data:')):continue
        if not (OUT/link).resolve().exists():missing.append(link)
    for name in ['HwaSimIR_P8_DDS_Performance_Annotation_Closeout.md','HwaSimIR_P8_Log_Retention.md']:
        p=ROOT/'docs'/name
        for link in re.findall(r'\]\(([^)]+)\)',p.read_text(encoding='utf-8')):
            if not link.startswith(('http:','https:','#')) and not (p.parent/link).resolve().exists():missing.append(name+':'+link)
    assert not missing,missing
    validated=[]
    for p in BASE.glob('P8_*/recording/*/validation.json'):
        if (p.parent/'independent_validation.log').exists():validated.append(dict(case=p.parents[2].name,recording=p.parent.name,**json.loads(p.read_text())))
    (BASE/'saved_validation_all_cases.json').write_text(json.dumps(validated,indent=2),encoding='utf-8')
    result=dict(result='PASS',head=head,changedFiles=len(files),sourceChangeSetSha256=identity,localLinksChecked=len(parser.links),missingDependencies=missing,independentlyVerifiedRecordings=len(validated),validatedVideoFrames=sum(v.get('frames',0) for v in validated),failedFileValidations=[v['case'] for v in validated if v['result']!='PASS'])
    (OUT/'delivery_validation.json').write_text(json.dumps(result,indent=2),encoding='utf-8');print(json.dumps(result))
if __name__=='__main__':main()
