# P20 测试版本目录、启动与配置流程

更新日期：2026-10-18。本文整理当前 P20 测试所用程序及配置，不执行启动、不切换板端正式版本。

## 1. 当前版本和目录

当前测试候选为 **P20 `aarch64_40_prefix_prewarm`**。Windows 工具目录名仍带 `p19`，它们就是 P20 实测使用的程序。

| 组件 | 位置 |
|---|---|
| Windows DataDrivenTestQT | `D:\HwaSimIR\build-p19-stim\release\DataDrivenTestQT.exe` |
| Windows 客户接收端 | `D:\HwaSimIR\build-p19-customer-receiver\release\HwaSimIRCustomerTxVideoReceiverDemo.exe` |
| 板端 P20 候选根目录 | `/userdata/HwaSimIR/candidates/P20_aarch64_40_prefix_prewarm` |
| 板端粗跟程序 | 候选根目录下 `coarse/HwaSim_IR` |
| 板端精跟程序 | 候选根目录下 `precise/HwaSim_IR` |
| 板端各路运行配置 | 候选根目录下 `coarse/Config/HwaSimIRRuntime.ini`、`precise/Config/HwaSimIRRuntime.ini` |
| 板端各路网络/身份配置 | 候选根目录下 `coarse/network.ini`、`precise/network.ini` |
| Windows 板端配置导出副本 | `D:\HwaSimIR\logs\p20\configs\aarch64_40_prefix_prewarm\` |
| 测试结果入口 | [P20 结果页面](../logs/p20/index.html) |

板卡：`192.168.1.116`，SSH 用户 `root`。Windows 测试网卡地址：`192.168.1.188`。构建虚拟机不参与以下日常启动流程。

**板端 `current` 仍指向 P19，`previous` 为 P18；`start_current.sh` 启动的是 P19，不是本文的 P20 候选。** P20 使用下文隔离候选启动脚本。视觉验收及性能偏差以 [Closeout](HwaSimIR_P20_Closeout.md) 为准，本文不代表最终验收签署。

## 2. 默认双路和 DDS 域

| 路别 | platID | sensorID | 原生尺寸 | 客户格式 | 客户 Topic |
|---|---:|---:|---|---|---|
| coarse 粗跟 | 1001 | 1 | 320×320 | Gray8 raw | `TX/LDecode` |
| precise 精跟 | 1001 | 2 | 800×800 | 灰度 H.264 | `TX/HD264` |

| 链路 | 板端 Runtime.ini 设置 | 当前域 | 对端设置 |
|---|---|---:|---|
| 控制、INIT、实时激励、ACK | `[DdsProtocol] DomainId` | 150 | DataDrivenTestQT 控制域 150 |
| 原视频及配套数据 | `[DdsVideo] DomainId` | 1 | VideoDisplay 域 1 |
| 客户 TX 视频 | `[CustomerDdsVideo] DomainId` | 1 | 客户接收端 `--domain 1` |

**本流程不需要启动 VideoDisplay。** 同一客户域、同一 Topic 不要同时运行多个同角色生产者；客户四个 Topic 本身没有 platID/sensorID 字段。

## 3. 启动板端两路 HwaSim_IR

在 Windows PowerShell 执行：

```powershell
ssh root@192.168.1.116
```

登录后，在板卡 Shell 执行：

```bash
cd /userdata/HwaSimIR/candidates/P20_aarch64_40_prefix_prewarm
python3 p20_start_candidate_pair.py "$PWD"
```

脚本同时启动 coarse、precise，分别加载各自配置，并设置本候选使用的 `PuffHybrid` 云算法。看到“两路已启动；等待各自 INIT/ACK”后，保持 SSH 终端打开。

脚本检测到已有 `HwaSim_IR` 时会拒绝启动，避免同 Topic 多生产者。请先正常退出已有实例，再启动本候选。

板端日志目录由脚本打印，格式为：

```text
/tmp/hwasimir-p20-pair-YYYYmmdd_HHMMSS/
    coarse.log
    precise.log
    processes.json
    policy_before.json
    result.json             # 退出时生成，包含策略恢复结果
```

## 4. 启动 Windows 客户接收端

打开两个 PowerShell 窗口。**每个窗口都先执行以下公共设置**；工作目录需保持在接收端 release 目录，以便使用随程序部署的 DLL 和 SDK 许可。

```powershell
cd D:\HwaSimIR\build-p19-customer-receiver\release
$qos = 'D:\HwaSimIR\logs\p20\configs\aarch64_40_prefix_prewarm\precise\ZRDDS_PROTOCOL_QOS.xml'
$ffmpeg = 'D:\HwaSimIR\.deps\ffmpeg-n8.1-win64-gpl-shared\ffmpeg-n8.1-latest-win64-gpl-shared-8.1\bin\ffmpeg.exe'
$out = 'D:\HwaSimIR\logs\p20\manual_' + (Get-Date -Format yyyyMMdd_HHmmss)
```

### 4.1 第一个窗口：粗跟 raw

```powershell
.\HwaSimIRCustomerTxVideoReceiverDemo.exe --domain 1 --topic TX/LDecode --codec raw --pixel-format gray8 --width 320 --height 320 --fps 60 --qos $qos --output "${out}_coarse" --display 1 --ffmpeg $ffmpeg
```

### 4.2 第二个窗口：精跟 H.264

```powershell
.\HwaSimIRCustomerTxVideoReceiverDemo.exe --domain 1 --topic TX/HD264 --codec h264 --pixel-format gray8 --width 800 --height 800 --fps 60 --qos $qos --output "${out}_precise" --display 1 --exit-after-ends 1 --ffmpeg $ffmpeg
```

等待各自出现 `[CustomerRxReady]`，核对 Topic、domain、尺寸，然后再发送激励。两路均显示图像并保存媒体，输出目录为：

```text
D:\HwaSimIR\logs\p20\manual_时间_coarse\
D:\HwaSimIR\logs\p20\manual_时间_precise\
```

上述命令默认不因 idle 或总时长自动超时。精跟使用 `--exit-after-ends 1`，正常 STOP 后收到标准 H.264 结束码，完成排空和保存后退出；粗跟在 STOP 排空后手动关闭图像窗口，以完成保存。下一回合重新启动接收端并使用新输出目录。

## 5. 启动 DataDrivenTestQT 并发送激励

另开 PowerShell，执行下面的 MWIR 双路启动命令：

```powershell
cd D:\HwaSimIR\build-p19-stim\release
.\DataDrivenTestQT.exe `
  --network-config=D:\HwaSimIR\logs\p20\runs\puff40_full_mwir_coexist\sender.ini `
  --channels-json=D:\HwaSimIR\logs\p20\runs\puff40_full_mwir_coexist\dual.json
```

这会打开交互 UI 并加载本轮实际测试配置，不会自动开始回放。配置引用同目录下的 `ordinary_replay_subset.csv`，这是测试回放数据；可在 UI 中替换为自己的数据文件。

操作顺序：

1. 确认两路发送勾选均开启，身份、角色、尺寸和压缩格式与第 2 节一致。
2. 确认公共数据源，以及两路是否跟随公共源、公共模型；需要不同模型或不同数据源时，在对应路取消跟随并选择。
3. 执行两路 INIT，确认分别收到两份成功 ACK。不要用一份广播 INIT 覆盖两种分辨率。
4. 执行 START，观察两个接收窗口和实收计数。
5. 结束时执行 STOP，等待输入、渲染和视频发送排空。

两路跟随同一公共数据源时，每 tick 只推进一次源。普通输入不要求固定文件名、固定行数或固定 SHA。角色与 sensorID 可分别配置；当前协议 Control 仍是平台级控制，同 platID 的实例会受其共同影响。

### 切换 SWIR

退出上一控制端后，用以下命令加载本轮 SWIR 配置：

```powershell
cd D:\HwaSimIR\build-p19-stim\release
.\DataDrivenTestQT.exe `
  --network-config=D:\HwaSimIR\logs\p20\runs\puff40_full_swir_no_vd\sender.ini `
  --channels-json=D:\HwaSimIR\logs\p20\runs\puff40_full_swir_no_vd\dual.json
```

实际逐路 INIT 参数来自加载的 `dual.json`，不要只根据 `sender.ini` 的单路默认 SensorInit 判断双路波段。切换波段/尺寸等 INIT 参数，应先 STOP、RESET，再重新 INIT、等待 ACK、START。

## 6. 修改配置的流程

### 6.1 运行时配置与 UI 配置的分工

- `sender.ini`：控制端网络、控制域、QoS 和默认参数。
- `dual.json`：双路身份、角色、发送勾选、公共/逐路数据源、模型和各自完整 INIT 参数；其中也有控制域和 QoS。配套使用同一场景目录的两个文件，修改域时保持一致。
- 板端各路 `network.ini` 与 `Config/HwaSimIRRuntime.ini`：各路本地部署身份、运行和视频配置。UI 中选择身份不能代替修改 renderer 的本地部署身份。
- 本机 `logs/p20/configs/` 是配置导出副本；只修改本机副本不会自动修改板卡。

修改前保留原文件副本，优先在新目录保存自己的 sender.ini/dual.json。若改变数据源文件位置，同时检查 JSON/INI 内引用的数据和 QoS 路径。

### 6.2 修改三条 DDS 链路的域

先正常退出两路 renderer 和相关对端，然后修改**板端两路实际加载的完整 Runtime.ini**。当前配置片段如下，仅用于定位键，不能用它覆盖整份文件：

```ini
[DdsProtocol]
DomainId=150
QosFile=Config/DDS/ZRDDS_PROTOCOL_QOS.xml

[DdsVideo]
DomainId=1
QosFile=Config/DDS/ZRDDS_PROTOCOL_QOS.xml

[CustomerDdsVideo]
Enable=true
DomainId=1
```

同步修改对端：

1. DataDrivenTestQT 的 `sender.ini` 中 `[DdsProtocol] DomainId`，以及 `dual.json` 的 `domain`，对齐控制域。
2. 客户接收端 `--domain` 对齐客户视频域。
3. 如使用 VideoDisplay，其 NetworkConfig 或 `--dds-domain` 对齐原视频域。
4. 检查环境覆盖 `HwaSimIRDdsProtocolDomainId`、`HwaSimIRDdsVideoDomainId`、`HwaSimIRCustomerDomainId`；它们优先于 INI。
5. 重启相关进程，检查 `[DdsDomains]` 的实际值及来源、`[DdsRuntime]` 和实收计数。仅 STOP/RESET 不会改变已创建 Participant 的域。

域范围为 0～232；只有客户域可以填 `-1`，表示继承原视频域。三域可以独立配置，但同进程 Factory 共享一套 QoS XML；控制启用时以 `DdsProtocol.QosFile` 为共同文件，不支持靠三个路径加载三套独立全局 QoS。

继续使用本轮 QoS 文件。控制发送端日志 `[DdsStimEffectiveQos]` 应显示 `getReturn=0 pushOnWrite=1`，不要随意替换为旧 QoS 后只凭画面判断配置正确。详见 [P20 DDS 三域说明](HwaSimIR_P20_DDS_Domain_Guide.md)。

### 6.3 修改尺寸或客户像素格式

尺寸、h264En 等通过对应路 INIT 配置；客户 Mode/PixelFormat 等本地部署选项通过板端 Runtime.ini 配置。必须同步接收端的 Topic、codec、pixel-format、width、height，不能只修改接收端参数。

raw 每帧一个 Sample，最大有效数据为 640000 字节；Gray8 为宽×高字节，RGB24 为宽×高×3 字节，超出必须拒绝。例如 800×800 RGB24 超限。此快速启动流程使用 320×320 Gray8 raw 和 800×800 灰度 H.264。

## 7. 正常结束和再次启动

1. 在 DataDrivenTestQT 执行 STOP，等待排空完成。
2. 等精跟客户接收端完成末帧解码、媒体保存并自动退出；粗跟停止收帧后关闭其图像窗口，等待保存完成。不要在保存前直接强杀接收进程。
3. 若继续下一回合，重新启动客户接收端并使用新输出目录，再按 RESET → 两路 INIT → 两份 ACK → START 执行。
4. 若结束本次使用，在板端成对启动终端按 Ctrl+C。脚本退出自己启动的两个实例并恢复 CPU/GPU/DDR 策略。
5. 检查终端“两路已退出；系统策略恢复：True”和日志目录中的 `result.json`。随后可退出控制端和 SSH。

使用上述候选启动方式不会切换 `current`，P19 回退版本仍保留。需要使用 P19 时，先完整退出本候选，再按 [P20 Closeout](HwaSimIR_P20_Closeout.md) 的 release 启动/回滚说明执行。
