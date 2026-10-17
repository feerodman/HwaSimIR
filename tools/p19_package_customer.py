"""Standalone customer source/binary kit. Never copy licenses, input archives or old EXEs."""
import argparse,hashlib,json,re,shutil,zipfile
from pathlib import Path
ap=argparse.ArgumentParser();ap.add_argument('--name',default='HwaSimIR_P19_Customer');a=ap.parse_args()
assert re.fullmatch('[A-Za-z0-9_]+',a.name)
root=Path(__file__).resolve().parents[1];out=root/'logs/p19/deliverables'/a.name;out.mkdir(parents=True,exist_ok=a.name=='HwaSimIR_P19_Customer')
def copy(src,dest):
 dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dest)
for name in ('CustomerRawReceiverMinimal','HwaSimIRCustomerTxVideoReceiverDemo'):
 for p in (root/'DDS'/name).iterdir():
  if p.is_file() and (p.suffix in ('.cpp','.h','.pro','.md') or p.name=='CMakeLists.txt'):copy(p,out/'DDS'/name/p.name)
for name in ('Generated','IDL'):
 for p in (root/'DDS/CustomerTX'/name).iterdir():
  if p.is_file():copy(p,out/'DDS/CustomerTX'/name/p.name)
for name in ('CustomerVideoContract.h','AnnexBEndScanner.h'):copy(root/'DDS/CustomerTX'/name,out/'DDS/CustomerTX'/name)
for name in ('HwaSimIR_P19_CustomerRawDDS_Troubleshooting.md','HwaSimIR_P19_H264_Compatibility.md','HwaSimIR_P19_DDS_Domains.md'):
 copy(root/'docs'/name,out/'docs'/name)
configs=root/'ConfigExamples/P19'
for p in configs.rglob('*'):
 if p.is_file():copy(p,out/'ConfigExamples/P19'/p.relative_to(configs))
for build,exe in [('build-p19-customer-receiver','HwaSimIRCustomerTxVideoReceiverDemo.exe'),('build-p19-minimal','CustomerRawReceiverMinimal.exe')]:
 d=root/build/'release';copy(d/exe,out/'Windows'/exe)
 for p in d.glob('*.dll'):copy(p,out/'Windows'/p.name)
 for name in ('platforms','imageformats','styles'):
  if (d/name).exists():
   for p in (d/name).glob('*.dll'):copy(p,out/'Windows'/name/p.name)
for name in ('ZRDDS_PROTOCOL_QOS.xml','ffmpeg.exe'):copy(root/'build-p19-customer-receiver/release'/name,out/'Windows'/name)
copy(root/'logs/p19/build/CustomerRawReceiverMinimal.aarch64',out/'RK3588/CustomerRawReceiverMinimal')
copy(root/'HwaSim_IR/Bin/Config/DDS/ZRDDS_PROTOCOL_QOS.xml',out/'RK3588/ZRDDS_PROTOCOL_QOS.xml')
copy(root/'logs/p19/runs/mpp_candidate_retained_writer/precise/received.h264',out/'media/clean_mpp_baseline_l32_800x800_60.h264')
for p in (root/'logs/p19/h264/p19_mpp_candidate').iterdir():
 if p.is_file():copy(p,out/'media/analysis'/p.name)
copy(root/'logs/p19/h264/private_sei_audit.json',out/'media/private_sei_audit.json')
(out/'README.md').write_text('''# P19 客户接收包

先看 `DDS/CustomerRawReceiverMinimal/main.cpp` 和同目录README；确认320×320 Gray8每Sample长度102400。完整显示/录像工程位于 `DDS/HwaSimIRCustomerTxVideoReceiverDemo`，只订阅四个客户类型/Topic，不依赖HwaSim控制、Status、Meta、Annotation或私有SEI。

## Windows快速运行

本包为Qt5.12.12 / MinGW7.3 x64 / 匹配ZRDDS版本。提供自己的有效SDK许可，按SDK规定放在工作目录；本包不含任何许可证内容。不要混用VS版ZRDDSCpp.dll。默认最小例子Domain1，完整Demo命令显式指定1。

```text
cd Windows
CustomerRawReceiverMinimal.exe --domain 1 --qos ZRDDS_PROTOCOL_QOS.xml --seconds 15
HwaSimIRCustomerTxVideoReceiverDemo.exe --topic TX/LDecode --codec raw --pixel-format gray8 --width 320 --height 320 --fps 60 --domain 1 --qos ZRDDS_PROTOCOL_QOS.xml --output coarse_new --display 1 --idle-ms 4000 --ffmpeg ffmpeg.exe
HwaSimIRCustomerTxVideoReceiverDemo.exe --topic TX/HD264 --codec h264 --pixel-format gray8 --width 800 --height 800 --fps 60 --domain 1 --qos ZRDDS_PROTOCOL_QOS.xml --output precise_new --display 1 --exit-after-ends 1 --ffmpeg ffmpeg.exe
```

两个接收器在两个终端同时运行。每次用新的output目录，避免覆盖证据。raw没有结束字段，本地idle-ms只用于有限录像退出，不证明发送端STOP；正常连续raw接收可保持idle-ms=0并关窗口退出。H264中途暂停不重置，正常STOP标准结束NAL会排空末帧；没有结束码的中断明确报告incomplete。不要将短包/零长度Sample视为帧边界。

源代码构建见各README。完整Demo需要设置ZRDDS_MINGW730_ROOT、FFMPEG_ROOT再qmake/make；FFmpeg头文件/导入库由匹配SDK环境提供。最小例子CMake只需要C++11、Threads、ZRDDS SDK及本包generated；已独立交叉构建并在RK3588实收222帧（无缺口/重复/无效样本），Windows qmake构建不链接Qt。

RK3588最小程序在 `RK3588/`；使用板端匹配ZRDDS SDK和许可，`LD_LIBRARY_PATH=<SDK>/lib ./CustomerRawReceiverMinimal --domain 1 --qos ZRDDS_PROTOCOL_QOS.xml`。

配置说明在 `ConfigExamples/P19`，保留现场全1/实验全150，新增control150_video1（控制150、原视频1、客户1）。客户接收器只须匹配CustomerDdsVideo域；-1继承DdsVideo域。原P18无此分域扩展。详见docs中的DDS域说明。其中导出数据源路径是开发机示例，必须在UI选择接收方本机合法文件后保存。候选硬解INI与800×800@60短clean流在配置/media目录。客户未知硬件解码能力未验证；SPS/PPS与原SDK返回码报告见docs。该包不包含客户硬件通过证明。
''',encoding='utf-8')
manifest=[]
for p in sorted(out.rglob('*')):
 if p.is_file() and p.name!='manifest.json':
  assert p.suffix.lower()!='.lic' and 'licence' not in p.name.lower()
  manifest.append(dict(path=p.relative_to(out).as_posix(),bytes=p.stat().st_size,sha256=hashlib.file_digest(p.open('rb'),'sha256').hexdigest()))
(out/'manifest.json').write_text(json.dumps(manifest,indent=2))
archive=out.with_suffix('.zip')
with zipfile.ZipFile(archive,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for p in sorted(out.rglob('*')):
  if p.is_file():z.write(p,p.relative_to(out.parent))
with zipfile.ZipFile(archive) as z:assert z.testzip() is None
receipt=dict(package=str(archive),files=len(manifest),sha256=hashlib.file_digest(archive.open('rb'),'sha256').hexdigest(),zipIntegrity='PASS',licensesIncluded=False)
archive.with_suffix('.receipt.json').write_text(json.dumps(receipt,indent=2));print(json.dumps(receipt))
