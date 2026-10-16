"""Hash local deliverables and verified source anchors; excludes runtime licences."""
import argparse,hashlib,json,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'logs/p18'
ap=argparse.ArgumentParser();ap.add_argument('--index-only',action='store_true');args=ap.parse_args()
anchors={
 'HwaSim_IR/HwaSim_IR/HwaSimIR.cpp':['BuildRuntimeConfigCandidatePaths','LoadNetworkConfig','LoadD2VideoOutputConfig','CustomerDdsVideoPublisher::validate','capture_task'],
 'HwaSim_IR/HwaSim_IR/TcpCommThread.cpp':['startOutputRound','stopOutputRound','sendFrameThreadFunc','sendFramePacket','HwaFrameV2::insert','m_customerPublisher'],
 'HwaSim_IR/HwaSim_IR/TcpCommThread_Linux.cpp':['startOutputRound','stopOutputRound','sendFrameThreadFunc','sendFramePacket','HwaFrameV2::insert','m_customerPublisher'],
 'HwaSim_IR/HwaSim_IR/Video/CustomerDdsVideoPublisher.cpp':['bool CustomerDdsVideoPublisher::validate','bool CustomerDdsVideoPublisher::start','bool CustomerDdsVideoPublisher::beginRound','bool CustomerDdsVideoPublisher::submit','bool CustomerDdsVideoPublisher::endRound','void run()'],
 'DDS/CustomerTX/CustomerTxWriter.h':['bool open(','bool send(','bool drain('],
 'DDS/CustomerTX/CustomerVideoContract.h':['imageBytes(','convertBgr(','fragments(','validLength('],
 'DDS/Runtime/DdsRuntimeManager.cpp':['DDSIF::Init','DDSIF::CreateDP','DDSIF::Finalize'],
 'DDS/Protocol/DdsStimClient.cpp':['sendInit','sendControl','sendRealtime','waitForRunningStatus','waitForStopStatus'],
 'DataDrivenTestQT/mainwindow.cpp':['enableDualCustomerChannels','sendInitCommand','sendRealTimeData','prepareNextAutoRound'],
 'DDS/HwaSimIRCustomerTxVideoReceiverDemo/main.cpp':['void push(','void packet(','void run(','void flush(','bool saveMedia(','on_process_sample'],
 'DDS/HwaSimIRCustomerTxVideoReceiverDemo/CustomerAvSha256.h':['CustomerAvSha256()','void update(','std::string finishHex('],
 'DDS/CustomerTX/IDL/CustomerTxVideo.idl':['struct HD264Stream','struct LD264Stream','struct HDDecoder','struct LDDecoder'],
 'DataDrivenTestQT/main.cpp':['int main('],
 'HwaSim_IR/Bin/Config/GameVFX/sprite.vert':['void main'],
 'HwaSim_IR/Bin/Config/GameVFX/sprite.frag':['void main'],
 'HwaSim_IR/HwaSim_IR/IR/P5GraphicsTest.inl':['bindOpaqueFixtureDepth','UpdateP5GraphicsTestScene'],
}
index=[]
for name,queries in anchors.items():
    source=(ROOT/name).read_text(encoding='utf-8-sig').splitlines()
    index.append(dict(path=name,absolute=str(ROOT/name),anchors=[dict(query=q,lines=[i+1 for i,s in enumerate(source) if q in s]) for q in queries]))
(BASE/'source_index.json').write_text(json.dumps(index,indent=2),encoding='utf-8')
if args.index_only:
    print(json.dumps([dict(path=row['path'],missing=[a['query'] for a in row['anchors'] if not a['lines']]) for row in index if any(not a['lines'] for a in row['anchors'])],indent=2));raise SystemExit(0)
status=subprocess.check_output(['git','status','--short'],cwd=ROOT,text=True,encoding='utf-8')
(BASE/'final_git_status.txt').write_text(status,encoding='utf-8')
files=set(BASE.rglob('*'))
changed=subprocess.check_output(['git','diff','--name-only'],cwd=ROOT,text=True,encoding='utf-8').splitlines()
files.update(ROOT/n for n in changed)
for folder in ['DDS/CustomerTX','DDS/HwaSimIRCustomerTxVideoReceiverDemo','docs/HwaSimIR_P18_Codex_Package','build-p18-customer-receiver-v4/release']:
    files.update((ROOT/folder).rglob('*'))
files.update(ROOT/n for n in ['docs/HwaSimIR_P18_Closeout.md','docs/HwaSimIR_DDS_EndToEnd_Guide.md','docs/HwaSimIR_Customer_TX_DDS_ICD.md','docs/HwaSimIR_P18_DualChannel_Runbook.md',
    'HwaSim_IR/HwaSim_IR/Video/CustomerDdsVideoPublisher.cpp','HwaSim_IR/HwaSim_IR/Video/CustomerDdsVideoPublisher.h',
    'HwaSim_IR/Bin/HwaSim_IR.exe','build-DataDrivenTestQT-codex-mingw73_64-Release/release/DataDrivenTestQT.exe'])
files.update((ROOT/'tools').glob('p18_*'))
entries=[]
for path in sorted(files):
    if not path.is_file() or path.name in ['delivery_manifest.json','zrddslicence.lic'] or '__pycache__' in path.parts:continue
    digest=hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda:f.read(4*1024*1024),b''):digest.update(block)
    entries.append(dict(path=path.relative_to(ROOT).as_posix(),bytes=path.stat().st_size,sha256=digest.hexdigest()))
report=dict(schema='HwaSimIR.P18.LocalDeliveryManifest.1',head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
    workspace=str(ROOT),files=entries,exclusions=['runtime zrddslicence.lic','self manifest','Python cache'],
    actualCustomerInterop='NOT_VERIFIED',note='Local evidence inventory; not a public distribution of commercial SDK licences')
(BASE/'delivery_manifest.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('Hashed files:',len(entries),'bytes:',sum(r['bytes'] for r in entries))
