"""Bind retention permission to verified final files and actual measured runs."""
import hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'logs/p10'
def digest(p):
    h=hashlib.sha256()
    with p.open('rb') as f:
        for data in iter(lambda:f.read(1024*1024),b''):h.update(data)
    return h.hexdigest()
normal=BASE/'P10_final_ui_verified60';graphics=BASE/'P10_A9_edge60'
n=json.loads((normal/'p9_results.json').read_text());g=json.loads((graphics/'p8_metrics.json').read_text())
assert n['inputAudit']['result']==n['inputProductValidation']['result']=='PASS'
assert n['newImageFpsFirst60Seconds']>=59.5 and n['controlResponseHeld'] and n['controlUnder10ms']
assert n['inputBackpressureCountMax']==0 and n['decodedSize']==[[800,800]]
assert g['inputAudit']['result']=='PASS' and g['rates']['newImage']['fps']>=59.5
assert g['ingressPeaks']['inputBackpressureCount']==0
for case in (normal,graphics):
    for p in (case/'recording').glob('*/validation.json'):assert json.loads(p.read_text())['result']=='PASS'
    assert list((case/'recording').glob('*/validation.json'))
protected=json.loads((BASE/'baseline/user_files_sha256.json').read_text(encoding='utf-8-sig'))
changed=[p for p,h in protected.items() if digest(ROOT/p)!=h];assert not changed,changed
env=dict(line.split('=',1) for line in (BASE/'deploy_A9/stage/Config/deployment_version.env').read_text().splitlines() if '=' in line)
assert digest(ROOT/'logs/p7/HwaSim_IR_P10_A9')==env['ElfSha256']
files=subprocess.check_output(['git','ls-files','-m','-o','--exclude-standard','-z'],cwd=ROOT).decode('utf-8').split('\0')
hashes={p:digest(ROOT/p) for p in sorted(set(files)) if p and (ROOT/p).is_file() and not p.endswith('.max')}
release=ROOT/'releases/windows/P9_A4'
inputs={str(p.relative_to(ROOT)):digest(p) for p in (ROOT/'DataDrivenTestQT/1.txt',release/'sender/1.txt')}
assert len(set(inputs.values()))==1
source={'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
 'userProtectedFiles':len(protected),'userModifiedByTask':changed,'changedFileSha256':hashes,
 'windowsRendererSha256':digest(ROOT/'HwaSim_IR/Bin/HwaSim_IR.exe'),
 'senderSha256':digest(release/'sender/DataDrivenTestQT.exe'),'receiverSha256':digest(release/'receiver/HwaSim_IR_VideoDisplay.exe'),
 'demoInputSha256':inputs,'deployment':env}
sdk=ROOT/'.deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin'
source['windowsFfmpegRuntimeSha256']={}
for lib in sorted(sdk.glob('*.dll')):
    h=digest(lib);assert digest(ROOT/'HwaSim_IR/Bin'/lib.name)==h
    source['windowsFfmpegRuntimeSha256'][lib.name]=h
(BASE/'source_final.json').write_text(json.dumps(source,indent=2),encoding='utf-8')
receipt={'result':'PASS','tests':['P10_A9_edge60 actual MP4/SEI/body/index and input ledger','P10_final_ui_verified60 no business overrides, actual UI, DDS/MPP, full file validation'],
 'ElfSha256':env['ElfSha256'],'ConfigManifestSha256':env['ConfigManifestSha256'],
 'normalNewImageFpsIncludingStartup':n['newImageFpsFirst60Seconds'],'normalSavedProducts':n['savedProducts'],
 'graphicsNewImageFpsIncludingStartup':g['rates']['newImage']['fps'],'userFilesUnchanged':len(protected),
 'expectedUnsupported':['VIS-SWIR production','conflicting SWIR 1.5-2.5 profile','2x and 4x MSAA in current native path'],
 'scope':'acceptance of deployed supported configuration; not physical target/sensor calibration'}
(BASE/'acceptance_receipt.json').write_text(json.dumps(receipt,indent=2),encoding='utf-8')
print(json.dumps(receipt))
