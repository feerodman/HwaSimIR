"""Package the completed functional work, with explicit paused/unverified boundaries."""
import argparse, hashlib, json, re, shutil, subprocess, zipfile
from datetime import datetime, timezone
from pathlib import Path

root = Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--name',required=True);ap.add_argument('--build',required=True);ap.add_argument('--customer',default='HwaSimIR_P19_Customer_Domains');a=ap.parse_args()
assert all(re.fullmatch('[A-Za-z0-9_]+',x) for x in (a.name,a.build,a.customer))
out = root/'logs/p19/deliverables'/a.name
out.mkdir(parents=True, exist_ok=False)

def copy(path, relative=None):
    path = Path(path)
    assert path.is_file(), path
    assert path.suffix.lower() != '.lic' and 'licence' not in path.name.lower(), path
    dest = out/(relative or path.relative_to(root))
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(path, dest)

source_suffixes = {'.c','.cc','.cpp','.cxx','.h','.hpp','.inl','.idl','.ui','.qrc','.rc','.pro','.pri','.cmake','.vcxproj','.filters','.sln','.md','.ico'}
for folder in ('DDS','DataDrivenTestQT','HwaSim_IR/HwaSim_IR','Shared'):
    for path in (root/folder).rglob('*'):
        if path.is_file() and (path.suffix.lower() in source_suffixes or path.name == 'CMakeLists.txt'):
            copy(path)
copy(root/'HwaSim_IR/HwaSim_IR.sln')
for path in (root/'tools').glob('p19_*'):
    if path.suffix in {'.py','.cpp','.c'}: copy(path)
for name in ('stage0_build.ps1','p11_rk3588_source_build.ps1','p18_single_run.py','p5_ssh_askpass.cmd'):
    copy(root/'tools'/name)
for path in (root/'docs').glob('HwaSimIR_P19_*.md'):
    copy(path)
for name in ('HwaSimIR_DDS_EndToEnd_Guide.md','HwaSimIR_Customer_TX_DDS_ICD.md','HwaSimIR_P18_DualChannel_Runbook.md'):
    copy(root/'docs'/name)
for path in (root/'ConfigExamples/P19').rglob('*'):
    if path.is_file(): copy(path)

# Actual final Qt UI, dependency DLLs, ordinary input and its existing LUT resources.
ui = root/'build-p19-stim/release'
for path in ui.rglob('*'):
    if path.is_file() and (path.suffix.lower() == '.dll' or path.name in {'DataDrivenTestQT.exe','1.txt','NetworkConfig.ini','start_domain1.cmd','start_control150_video1.cmd'} or 'Config' in path.relative_to(ui).parts):
        copy(path)
# Renderer is an update for the existing matched Panda/resources installation.
for name in ('HwaSim_IR.exe','avcodec-62.dll','avformat-62.dll','avutil-60.dll','swresample-6.dll','swscale-9.dll','opencv_world440.dll','ZRDDSCpp_VS2015.dll'):
    copy(root/'HwaSim_IR/Bin'/name)
copy(root/'logs/p19/deliverables'/(a.customer+'.zip'))
for path in (root/'logs/p19/build'/a.build).iterdir():
    if path.is_file(): copy(path)
copy(root/'logs/p19/build/windows_domains_receipt.json')
for name in ('build_windows_09_domains_clean.log','build_stim_12_domains.log','build_receiver_05.log','build_minimal_03.log','build_minimal_cmake_arm_v2.log','build_standalone_receiver.log','minimal_arm_live.log'):
    copy(root/'logs/p19'/name)

# Small result/diagnostic records from every case, including failures; selected real media.
for folder in ('runs','tests','ui','h264','resumed_domains','resumed_cloud'):
    for path in (root/'logs/p19'/folder).rglob('*'):
        if path.is_file() and path.suffix.lower() in {'.json','.log','.png'} and path.stat().st_size <= 8_000_000:
            copy(path)
for case in ('dual120_two_rounds_final','domains19_telemetry_two','domains24_ini_rounds','mpp_candidate_retained_writer'):
    for path in (root/'logs/p19/runs'/case).rglob('*'):
        if path.is_file() and path.suffix.lower() in {'.mp4','.h264'}: copy(path)
for case in ('p19_rollback_coarse_v2','p19_rollback_precise_v2'):
    for path in (root/'logs/p18/runs'/case).rglob('*'):
        if path.is_file() and path.suffix.lower() in {'.json','.log'} and path.stat().st_size <= 8_000_000: copy(path)
for name in ('index.html','acceptance.json','PROGRESS.md','board/functional_handoff.log','board/resumed_handoff.json','board/cleanup/result.json','board/p17_offboard/verification.json','board/history_offboard/verification.json','deployment/current.json','deployment/functional_handoff/receipt.json','baseline/frozen_check_current.json','baseline/frozen_files.json','cloud_deferred/manifest.json','cloud_deferred/candidate16_vs_functional08.patch'):
    copy(root/'logs/p19'/name)
(out/'README.md').write_text(f'''# P19 恢复测试交付

DDS三条链路可独立配置域，板端控制150、原视频1、客户1。打开 `logs/p19/index.html` 和 `docs/HwaSimIR_P19_Resumed_Test_Report.md` 查看实测。云重负载精跟降速/秒级积压仍未解决，本包不表示整个P19通过。客户现场raw、客户硬解、屏幕呈现时延仍未验证。

- 双路UI：`build-p19-stim/release/start_control150_video1.cmd`，默认粗320raw、精800H264，分别INIT并等待两份ACK；新增三个域的独立导出。全1入口仍保留。
- 客户接收包：`logs/p19/deliverables/{a.customer}.zip`；客户Demo域1，与控制域150无须一致。本包不含许可证。
- 板端构建、ELF、源码回执：`logs/p19/build/{a.build}`；已部署P19。两终端分别运行 `/userdata/HwaSimIR/start_current.sh coarse` 和 `precise`。退出两路后 `use_release.sh P18` 回滚；P18自身域1。参见Board_Releases文档。
- Windows renderer更新：`HwaSim_IR/Bin/HwaSim_IR.exe`，继续使用原匹配Panda运行库、模型、Config和SDK授权。此包不是独立模型资源安装包。
- 三域说明：`docs/HwaSimIR_P19_DDS_Domains.md`；直接可用配置在 `ConfigExamples/P19/control150_video1`。改域后重启，环境变量优先INI，检查日志中的有效值与来源。

无效云/读回候选已撤出生产源码，原云、尾焰、HDR、深度成像路径保留；失败性能记录继续保留。原始输入与模型未修改，原四客户IDL/Topic不变，不自动commit/push。P17及历史板外完整归档仍在用户原工作区，本包仅含其校验摘要，不复制其中许可证。开发机绝对数据路径应在UI选择本机文件后保存。

`manifest.json`含逐文件SHA256。功能、图像、性能与真实客户互通分别判定。
''',encoding='utf-8')
status=subprocess.check_output(['git','status','--short'],cwd=root,text=True)
(out/'workspace_status.txt').write_text(status,encoding='utf-8')
manifest=[]
for path in sorted(out.rglob('*')):
    if path.is_file():
        with path.open('rb') as f: sha=hashlib.file_digest(f,'sha256').hexdigest()
        manifest.append({'path':path.relative_to(out).as_posix(),'bytes':path.stat().st_size,'sha256':sha})
(out/'manifest.json').write_text(json.dumps({'createdUtc':datetime.now(timezone.utc).isoformat(),'phaseStatus':'DDS_DOMAINS_VERIFIED_CLOUD_PERFORMANCE_UNRESOLVED','files':manifest},indent=2),encoding='utf-8')
archive=out.with_suffix('.zip')
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for path in sorted(out.rglob('*')):
        if path.is_file(): z.write(path,path.relative_to(out.parent))
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
with archive.open('rb') as f: digest=hashlib.file_digest(f,'sha256').hexdigest()
receipt={'package':str(archive),'files':len(manifest),'bytes':archive.stat().st_size,'sha256':digest,'zipIntegrity':'PASS','licensesIncluded':False}
(archive.with_suffix('.receipt.json')).write_text(json.dumps(receipt,indent=2))
print(json.dumps(receipt))
