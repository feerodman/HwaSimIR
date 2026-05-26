# HwaSimIR Windows Runbook

本手册是阶段 0 的基线运行入口。目标是让后续修改红外全链路模型前，先能稳定复现当前行为：HwaSimIR 接收 DataDrivenTestQT 的 UDP 初始化、控制和实时激励数据，Panda3D 更新场景，并通过 TCP/JPEG 输出 8-bit 画面。

## 1. 基线版本

- 基线日期：2026-05-25
- 基线提交：`1aa6bdd3f179626d83f368f38e12ccbb3108f1a4`
- 当前主程序：`ConsoleApplication1_LLA/ConsoleApplication1/HwaSimIR.cpp`
- 激励端：`DataDrivenTestQT/mainwindow.cpp`
- 输出形式：TCP 长度头 + JPEG，当前发送线程会把画面缩放为 `800x800`

## 2. Windows 依赖

- Visual Studio，支持 `v140` 平台工具集
- Windows SDK 8.1 或兼容 SDK
- Panda3D C++ SDK 或本地编译库，并设置 `PANDA3D_DIR`
- OpenCV 4.4.0，当前 VS 工程默认引用 `D:\Environment\opencv2-440`
- Qt 5.12.12，建议使用 Qt Creator 打开 `DataDrivenTestQT/DataDrivenTestQT.pro`
- Eigen 已随 HwaSimIR 工程目录携带

不要把本机绝对依赖路径写入核心代码。后续如果需要调整路径，优先用环境变量、VS 属性页或 CMake cache。

## 3. 运行目录

HwaSimIR 推荐工作目录：

```powershell
cd D:\HwaSimIR\ConsoleApplication1_LLA\Bin
```

原因：

- 模型、SensorWave JSON、配置文件位于 `ConsoleApplication1_LLA/Bin/Config`
- 当前红外材质和大气表读取逻辑会尝试 `../../materials` 和 `../../transmittance`

DataDrivenTestQT 推荐工作目录：

```powershell
cd D:\HwaSimIR\DataDrivenTestQT
```

原因：

- 当前激励端构造时读取 `./1.txt`

## 4. 通信基线

HwaSimIR：

- UDP 监听：`0.0.0.0:8888`
- UDP 初始化应答默认回发：运行时根据 DataDrivenTestQT 来源地址更新
- TCP 视频流客户端：连接 `127.0.0.1:5555`
- 渲染模式：同步渲染，当前代码中 `SetRenderMode(true, 0)`，即 1 组 UDP 实时数据驱动 1 帧

DataDrivenTestQT：

- 本地 UDP：`127.0.0.1:9999`
- 目标 UDP：`127.0.0.1:8888`
- 初始化指令：`0x36`
- 控制指令：`0x41`
- 实时数据：`0x38`

TCP 视频接收端不是阶段 0 必需项；如果没有服务端监听 `127.0.0.1:5555`，HwaSimIR 的 TCP 线程会持续重连，不应阻塞 UDP 初始化和场景更新。

## 5. DataDrivenTestQT 当前默认激励

- `trackerSensorBand = 2`，当前注释语义为中波红外
- `trackerSensorWidth = 640`
- `trackerSensorHeight = 512`
- 横向 FOV UI 默认：`0.1`
- 纵向 FOV UI 默认：`0.1`
- `videoFps = 30`
- 实时发送步长 UI 默认：`25 ms`
- 实时数据目标数：`targetNumValid = 3`
- 实时轨迹文件：`DataDrivenTestQT/1.txt`

注意：初始化结构体在 HwaSimIR 和 DataDrivenTestQT 侧存在历史差异，阶段 0 只冻结当前可运行基线，不在本阶段重构协议结构。

## 6. 手动冒烟流程

1. 启动可选 TCP/JPEG 接收端，监听 `127.0.0.1:5555`。
2. 从 Visual Studio 打开 `ConsoleApplication1_LLA/ConsoleApplication1.sln`。
3. 选择当前可用配置，建议先用 `Release|x64`。
4. 设置 HwaSimIR 工作目录为 `D:\HwaSimIR\ConsoleApplication1_LLA\Bin`，启动 HwaSimIR。
5. 从 Qt Creator 打开 `DataDrivenTestQT/DataDrivenTestQT.pro`。
6. 确认 DataDrivenTestQT 工作目录为 `D:\HwaSimIR\DataDrivenTestQT`，启动激励端。
7. 点击 `复位`，HwaSimIR 控制台应打印控制指令。
8. 点击 `初始化`，DataDrivenTestQT 应收到 `0x37` 初始化应答，HwaSimIR 控制台应打印 Stage0 初始化基线。
9. 点击 `开始仿真`，DataDrivenTestQT 定时发送实时数据，HwaSimIR 控制台应打印第 1 帧和每 100 帧的 Stage0 实时包摘要。
10. 点击 `停止仿真`，HwaSimIR 应停止处理实时场景更新。

## 7. 自动基线检查

在仓库根目录运行：

```powershell
powershell -ExecutionPolicy Bypass -File tools\stage0_check.ps1
```

严格模式会在检查失败时返回非零码：

```powershell
powershell -ExecutionPolicy Bypass -File tools\stage0_check.ps1 -Strict
```

该脚本检查：

- 关键工程文件、模型配置、材质表、大气表、温度表是否存在
- HwaSimIR UDP/TCP 端口是否仍为阶段 0 基线
- DataDrivenTestQT 默认端口、波段、分辨率、实时目标数、发送步长是否仍为阶段 0 基线
- VS 工程是否仍使用 `v140`
- Qt 工程是否包含 `network/widgets`

## 8. 阶段 0 诊断输出

本阶段只增加低频控制台诊断，不改变仿真状态机、网络协议、渲染逻辑或视频编码逻辑。

新增诊断包括：

- HwaSimIR 启动时打印 UDP 基线端点
- HwaSimIR 启动时打印 TCP 视频端点和 JPEG 输出格式
- 收到控制指令时打印 Stage0 控制摘要
- 收到初始化指令时打印传感器波段、分辨率、FOV、视频帧率和导弹数量上限
- 收到实时数据时只打印第 1 帧和每 100 帧摘要

## 9. 当前已冻结但未修复的问题

- HwaSimIR 当前窗口和 TCP 发送线程仍以 `800x800` 为主，尚未按 `trackerSensorWidth/Height` 输出。
- 当前红外模型仍是经验化简版，不是完整物理单位链路。
- 当前目标材质仍主要按平台类型硬编码，尚未使用 UV/材质 ID 贴图映射。
- 当前大气数据只有单一路径透过率表，尚缺 path radiance、sky radiance、多天气条件索引。
- 当前协议结构存在历史差异，后续阶段需要统一 HwaSimIR 与 DataDrivenTestQT 的 ICD 来源。

## 10. 自动构建与短时启动

阶段 0 固定使用以下工具链：

- VS2015 MSBuild：`C:\Program Files (x86)\MSBuild\14.0\Bin\MSBuild.exe`
- Qt 5.12.12 MinGW 7.3.0 64-bit qmake：`D:\Qt\Qt5.12.12\5.12.12\mingw73_64\bin\qmake.exe`
- MinGW make：`D:\Qt\Qt5.12.12\Tools\mingw730_64\bin\mingw32-make.exe`

默认构建命令：

```powershell
powershell -ExecutionPolicy Bypass -File tools\stage0_build.ps1
```

该命令默认：

- 构建 HwaSimIR：`Release|x64`
- 构建 DataDrivenTestQT：Qt 5.12.12 `release`
- 输出 HwaSimIR：`D:\HwaSimIR\ConsoleApplication1_LLA\Bin\ConsoleApplication1.exe`
- 输出 DataDrivenTestQT：`D:\HwaSimIR\build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe`

短时启动冒烟命令：

```powershell
powershell -ExecutionPolicy Bypass -File tools\stage0_smoke_run.ps1
```

该命令会按工作目录启动 HwaSimIR 和 DataDrivenTestQT，等待若干秒，记录 stdout/stderr 到 `logs/stage0`，默认清理进程。需要人工继续操作窗口时，可加 `-KeepRunning`。

2026-05-25 验证结果：

- `tools\stage0_check.ps1 -Strict`：通过。
- `tools\stage0_build.ps1`：HwaSimIR `Release|x64` 构建成功；DataDrivenTestQT Qt 5.12.12 MinGW release 构建成功。
- `tools\stage0_smoke_run.ps1 -Seconds 8`：`ConsoleApplication1.exe` 与 `DataDrivenTestQT.exe` 均成功启动并保持运行，脚本随后停止进程。
- 本次未自动点击 DataDrivenTestQT 的复位、初始化、开始、停止按钮，完整按钮流程仍需人工或后续 UI 自动化验证。

## 11. 阶段 1 配置检查

阶段 1 增加 IR 配置/类型模块后，可运行：

```powershell
powershell -ExecutionPolicy Bypass -File tools\stage1_check.ps1 -Strict
```

该命令会检查：

- `IR/IRTypes.*` 与 `IR/IRConfig.*` 是否存在并被 VS 工程纳入。
- `trackerSensorBand` 协议映射是否覆盖 SWIR/NIR/MWIR/LWIR/VIS。
- `SensorWave/default_*.json` 是否能提供谱范围、尺寸、ADC bit、Display bit、NETD。

验证 HwaSimIR 初始化时波段切换：

```powershell
powershell -ExecutionPolicy Bypass -File tools\stage1_band_switch_smoke.ps1
```

该命令只启动 HwaSimIR，并依次发送 `trackerSensorBand=0,1,2,3,4` 的初始化 UDP 包。通过日志确认 `Sensor profile (init-command)` 是否分别切换到 SWIR/NIR/MWIR/LWIR/VIS。
