# P20M3 尾焰与尾部热源修复、双路启动说明

客户电脑拷贝运行请使用 [客户运行目录与环境说明](HwaSimIR_P20M3_Client_Run_Environment.md)，其中提供可迁移运行包与启动脚本。

## 本轮结果

最新板端候选：`/userdata/HwaSimIR/candidates/P20_P20M3_SWIR_MWIR_Engineering_20261021_v6`。Windows renderer仍为 `D:\HwaSimIR\build-p20m3-renderer\bin\HwaSim_IR.exe`，已重建。旧v5/P19/P20保留。

黑色原因：首次收到 `engineState=1` 时，尾焰和尾部热源被初始化为环境温度。原固定几何抓图的仿真时间为0，温度无法积分升高，冷尾焰仍有不透明度，尾部区域又替换了原表面辐射，因此呈暗色。这不是MODTRAN缺表。此前将该图解释为发动机关闭不准确；原日志实际为engineState=1。

修复首次状态语义：首次观察到已开启发动机，局部尾焰/热源按配置工作温度初始化；已经观察到关闭后再开启，仍保留原热惯性和协议仿真时间。发动机关闭仍关闭局部贡献。未改整机温度、显示增益、灰度底值或亮斑控制。AIM120D此次样本尾焰核心1800 K、外层1100 K，尾部热源760 K，均来自既有配置，不新增任意升温。

验证：68项物理/状态单元检查PASS；Windows SWIR/MWIR短时GPU抓图；板卡Mali双路各90帧、渲染/解码/保存计数守恒，图形错误0。仅加入尾部热源修复后，Windows同场景288个像素增加辐亮度，SWIR/MWIR最大增量21.797918/425.41943 W/(m²·sr·μm)。这是实际GPU PFM差值，不是CPU标量冒充图像。SWIR的760 K尾部热源远弱于1800 K尾焰，显示亮度不保证与MWIR相同。

本轮按用户要求不做60 FPS／≤80 ms长测，不能把90帧图像验证当作性能PASS。之前v5性能失败记录保留。工程质量仍为ENGINEERING_APPROX，严格全域MODTRAN精度PENDING，真实标定NOT_VERIFIED_CALIBRATION。

## 启动顺序和配置

依次执行：板端双路 → Windows两个customer-receiver → DataDrivenTestQT。不要同时启动旧候选或第二组同Topic renderer。

| 通道 | 波段 | 分辨率 | 编码 | 接收Topic | 控制Domain | 视频Domain | platID / sensorID |
|---|---|---|---|---|---|---|---|
| coarse | MWIR 3–5 μm | 320×320 | raw gray8 | TX/LDecode | 150 | 1 | 1001 / 1 |
| precise | SWIR 1.1–2.5 μm | 800×800 | H264 gray8 | TX/HD264 | 150 | 1 | 1001 / 2 |

发送配置为60 Hz累计deadline、videoFps=60、simMode=1；这只是运行设置，不代表性能认证已通过。默认回放 `circleDataAll3.csv`，保留原10 ms仿真时间偏移，60 Hz发送属于减速回放。

## 1. 板端双路启动命令

Windows终端先登录板卡（root，使用已授权密码）：

```powershell
ssh root@192.168.1.116
```

在板卡SSH终端执行：

```sh
python3 /userdata/HwaSimIR/candidates/P20_P20M3_SWIR_MWIR_Engineering_20261021_v6/p20_start_candidate_pair.py /userdata/HwaSimIR/candidates/P20_P20M3_SWIR_MWIR_Engineering_20261021_v6
```

保持该终端开启。终端显示两路启动与 `/tmp/hwasimir-p20-pair-时间戳` 日志目录；两路等待各自INIT。程序使用v6 ELF及v6配置，不跳转旧P20程序。若提示已有renderer，先由原启动终端停止原实例。

## 2. customer-receiver双路启动命令

以下两段分别放在两个Windows PowerShell终端运行。接收窗口开启后再操作Qt。`--display 1`显示图像，`--audit 1`保存审计；每次使用新时间戳输出目录。这里视频Domain是1，不要误设为控制Domain150。

**终端A：coarse MWIR 320×320，raw。**

```powershell
Set-Location D:\HwaSimIR\build-p20m3-customer-receiver\release
$rxOut = 'D:\HwaSimIR\logs\p20m3\manual_' + (Get-Date -Format yyyyMMdd_HHmmss) + '\coarse'
New-Item -ItemType Directory -Path $rxOut -Force | Out-Null
.\HwaSimIRCustomerTxVideoReceiverDemo.exe --topic TX/LDecode --codec raw --pixel-format gray8 --width 320 --height 320 --fps 60 --domain 1 --qos D:\HwaSimIR\HwaSim_IR\Bin\Config\DDS\ZRDDS_PROTOCOL_QOS.xml --output $rxOut --display 1 --audit 1 --timeout-ms 0 --idle-ms 4000 --exit-after-ends 0 --ffmpeg D:\HwaSimIR\.deps\ffmpeg-n8.1-win64-gpl-shared\ffmpeg-n8.1-latest-win64-gpl-shared-8.1\bin\ffmpeg.exe
```

**终端B：precise SWIR 800×800，H264。**

```powershell
Set-Location D:\HwaSimIR\build-p20m3-customer-receiver\release
$rxOut = 'D:\HwaSimIR\logs\p20m3\manual_' + (Get-Date -Format yyyyMMdd_HHmmss) + '\precise'
New-Item -ItemType Directory -Path $rxOut -Force | Out-Null
.\HwaSimIRCustomerTxVideoReceiverDemo.exe --topic TX/HD264 --codec h264 --pixel-format gray8 --width 800 --height 800 --fps 60 --domain 1 --qos D:\HwaSimIR\HwaSim_IR\Bin\Config\DDS\ZRDDS_PROTOCOL_QOS.xml --output $rxOut --display 1 --audit 1 --timeout-ms 0 --idle-ms 0 --exit-after-ends 1 --ffmpeg D:\HwaSimIR\.deps\ffmpeg-n8.1-win64-gpl-shared\ffmpeg-n8.1-latest-win64-gpl-shared-8.1\bin\ffmpeg.exe
```

首次未收到数据时raw不会因4秒空闲退出；收到数据后连续4秒空闲会排空并退出。H264收到本轮结束标记后排空退出。再次回放前重启两个接收器；这是单轮接收命令。关闭接收窗口也可退出，正常完成时优先等待STOP排空和输出文件写完。

## 3. DataDrivenTestQT启动命令

在另一个Windows PowerShell终端执行：

```powershell
Set-Location D:\HwaSimIR
& .\build-p20m3-stim\release\DataDrivenTestQT.exe --network-config=D:/HwaSimIR/build-p20m3-stim/release/P20M3_Board_NetworkConfig.ini --channels-json=D:/HwaSimIR/build-p20m3-stim/release/P20M3_Board_Dual.json
```

这是交互模式，不自动开始发送。双路界面确认coarse/precise均启用、source为所需CSV，依次RESET、INIT，等待两路ACK及状态就绪，再START；结束按STOP。完整目标键保持0x22/plat3/target3，不用0x33随意替换。发动机开启控制尾焰和尾部热源；关闭输入不应显示强制热源。

专用配置已准备好；若以后重新构建Qt导致默认配置被复制，可在工程根目录重新执行：

```powershell
F:\Programs\anaconda3\python.exe tools/p20m3_package_windows.py
F:\Programs\anaconda3\python.exe tools/p20m3_prepare_launch.py
```

重建配置会恢复默认CSV3、工程策略、UTC日期2026-10-21/03:30。回放实际日期不同，应修改专用 `P20M3_Board_NetworkConfig.ini` 的 `[Demo] UtcDate/UtcHour`；不要把发送墙钟节拍当作协议仿真时间。

## 停止与回滚

1. Qt STOP，等待H264结束标记和接收器排空，raw约4秒后退出。
2. 板端启动终端按Ctrl+C，成对停止本脚本拥有的实例并恢复系统性能策略。
3. 若要回滚v5，在旧实例全部停止后执行：

```sh
python3 /userdata/HwaSimIR/candidates/P20_P20M3_SWIR_MWIR_Engineering_20261021_v6/p20_start_candidate_pair.py /userdata/HwaSimIR/candidates/P20_P20M3_SWIR_MWIR_Engineering_20261021_v5
```

v5保留修复前尾焰/热源初始化行为。原P19/P20入口未覆盖。

## 短时自动复核（不必手动启动上述程序）

以下入口会自行启动/停止板端、Qt和两个接收器，不能与手动启动的一组并行运行：

```powershell
Set-Location D:\HwaSimIR
$env:HWASIMIR_SSH_PASSWORD = Read-Host '板卡 root 密码'
F:\Programs\anaconda3\python.exe tools/p20m3_run.py --case smoke --name ('p20m3_hotfix_'+(Get-Date -Format yyyyMMdd_HHmmss)) --utc-date 2026-10-21 --utc-hour 3.5
```

## 证据与身份

- 修复源码：`IR/IREnginePlumeModel.cpp`、`IR/IRTemperatureModel.cpp`；回归：`tools/p11_plume_physics_unit.cpp`。
- ARM ELF SHA-256：`7e0b6b706639570507472ea484efafe42061a36a2ed4c40ba2981eeece8faa78`。
- 构建/部署：`logs/p20m3/20261021_engineering/build_plume_rear_fix_arm/build_receipt.json`、`deployment_v6/receipt.json`。
- Windows短测：`windows_plume_rear_fix_swir`、`windows_plume_rear_fix_mwir`，汇总 `plume_rear_fix_windows_evidence.json`。
- 板卡短测：`logs/p20/runs/p20m3_v6_plume_rear_capture/result.json`、`physical_evidence_summary.json`。
- 实际SWIR图：[SWIR](../logs/p20/runs/p20m3_v6_plume_rear_capture/precise/board/linear_rgb8.png)。实际MWIR图：[MWIR](../logs/p20/runs/p20m3_v6_plume_rear_capture/coarse/board/linear_rgb8.png)。相同目录的 `linear.pfm` 为AGC前SI浮点数据。
- `plume_rear_fix_acceptance.json` 为本次专用验收；不重写v5长测结果。
