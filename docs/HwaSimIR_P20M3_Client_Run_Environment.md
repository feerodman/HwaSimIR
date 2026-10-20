# P20M3 v6 客户Windows运行包与板卡连接说明

## 拷贝哪个目录

拷贝整个目录 **`D:\HwaSimIR\dist\P20M3_Client_v6_20261021`**，或完整交付压缩包 **`D:\HwaSimIR\dist\P20M3_Client_v6_20261021_READY.zip`**，解压到客户电脑任意可写目录，例如 `C:\HwaSimIR_Client`。不要只拷贝EXE。目录内部相对关系必须保留，不需要客户电脑存在D盘或HwaSimIR源码工程。

```text
P20M3_Client_v6_20261021/
  Start-Stim.cmd                 DataDrivenTestQT交互式启动
  Start-Receivers.cmd            一次启动两个接收器（显示窗口）
  Start-Receivers.ps1
  Start-Coarse.cmd               单独启动MWIR raw接收
  Start-Precise.cmd              单独启动SWIR H264接收
  Board-Start.txt                板端双路启动命令
  README.md
  manifest.json                  文件SHA-256
  DataDrivenTestQT/
    DataDrivenTestQT.exe
    NetworkConfig.ini
    P20M3_Board_Dual.json
    ZRDDS_PROTOCOL_QOS.xml
    zrddslicence.lic
    Qt5*.dll、ZRDDSCpp.dll、MinGW运行库等
    platforms/qwindows.dll、其他Qt插件目录、qt.conf
    Config/Atmosphere/parameters.txt、provenance.json、quality.json
  customer-receiver/
    HwaSimIRCustomerTxVideoReceiverDemo.exe
    ffmpeg.exe、ffprobe.exe、avcodec/avformat/avutil等FFmpeg DLL
    Qt5*.dll、ZRDDSCpp.dll、MinGW运行库等
    platforms/、其他Qt插件目录、qt.conf
    ZRDDS_PROTOCOL_QOS.xml、zrddslicence.lic
  Data/
    circleDataAll.csv
    circleDataAll2.csv
    circleDataAll3.csv
  Output/                       运行后自动生成；每次每路独立目录
```

原开发构建目录分别是 `D:\HwaSimIR\build-p20m3-stim\release` 和 `D:\HwaSimIR\build-p20m3-customer-receiver\release`。客户交付应使用上面的dist运行包，里面已经去掉开发绝对路径和.o文件，并带齐所需参数、三个原始CSV和运行库。

## 客户电脑运行环境

- Windows x64。建议Windows 10 1803及以后或Windows 11；当前发送器使用高分辨率waitable timer，过旧系统创建失败会明确停止。客户具体系统尚未现场验证。
- 不需要安装Qt SDK、Visual Studio、MinGW、Python、MODTRAN或Windows renderer。Qt 5.12.12与MinGW运行库随程序放置，FFmpeg随接收器放置；不需要配置全局PATH或QTDIR。
- 使用可写目录，需能创建Output和程序日志；不要从ZIP内部直接运行，也不要放在只读位置。首次建议本地磁盘目录，保持plugins和DLL完整。
- 两个程序都带现有ZRDDS许可证 `zrddslicence.lic`。文件原样复制，未修改许可内容；现有许可标签为Trial。客户机上的有效期/硬件适用性需要实际启动确认；若SDK报告许可失败，需要合法有效的客户运行许可，不能绕过检查。当前本机已验证加载成功。
- Windows网卡与板卡 `192.168.1.116` 网络可达，建议同一局域网，例如客户机 `192.168.1.188/24` 或另一个无冲突的 `192.168.1.x/24`。不要与板卡或其他主机重复IP。
- 正式链为DDS，QoS使用默认网卡自动发现和TCP业务传输；不要求客户机必须固定为188。防火墙需允许DataDrivenTestQT及接收器的局域网TCP/UDP通信，网络设备不能阻断DDS发现。不要仅放通旧UDP8888/8889就认为DDS已可用。
- 若同时有VPN、虚拟网卡或多网卡，先确认板卡所在物理网卡能够进行DDS发现。不要在同一网络同时运行两套相同Domain/Topic/platID/sensorID的发送或渲染实例。

## 1. 启动板端双路

从任意SSH终端登录root@192.168.1.116，执行：

```sh
python3 /userdata/HwaSimIR/candidates/P20_P20M3_SWIR_MWIR_Engineering_20261021_v6/p20_start_candidate_pair.py /userdata/HwaSimIR/candidates/P20_P20M3_SWIR_MWIR_Engineering_20261021_v6
```

保持终端开启，日志目录在 `/tmp/hwasimir-p20-pair-*`。这个命令同时启动coarse/precise，使用v6已修复尾焰和尾部热源的程序；不要再启动旧候选。客户Windows运行包不包含也不需要板端ELF。

## 2. 启动customer-receiver双路

客户电脑双击 **`Start-Receivers.cmd`**，会出现两个独立接收窗口；也可在包根目录的PowerShell执行：

```powershell
.\Start-Receivers.cmd
```

需要单独启动时，运行 `Start-Coarse.cmd` 或 `Start-Precise.cmd`。不要把双路脚本与单路脚本重复运行。

| 通道 | 波段 | 大小 | 编码 | Topic | Domain |
|---|---|---|---|---|---|
| coarse | MWIR 3–5μm | 320×320 | raw gray8 | TX/LDecode | 1 |
| precise | SWIR 1.1–2.5μm | 800×800 | H264 gray8 | TX/HD264 | 1 |

完整EXE参数可直接查看两个 `.cmd`。脚本自动切换到customer-receiver工作目录，使用随包QoS、许可和FFmpeg。每轮输出到 `Output/coarse_随机号`、`Output/precise_随机号`。首次等待输入不超时；raw收到数据后空闲4秒会排空退出，H264收到一次结束标记后排空退出。下一轮需重新启动接收器。

## 3. 启动DataDrivenTestQT

客户电脑双击 **`Start-Stim.cmd`**，或在包根目录PowerShell执行：

```powershell
.\Start-Stim.cmd
```

等价手动命令（工作目录必须是DataDrivenTestQT子目录）：

```powershell
Set-Location .\DataDrivenTestQT
.\DataDrivenTestQT.exe --network-config=NetworkConfig.ini --channels-json=P20M3_Board_Dual.json
```

在Qt双路界面确认两路勾选，依次 **RESET → INIT → 等待两路ACK/状态就绪 → START → STOP**。接收窗口要先启动。

- 控制Domain=150，客户视频Domain=1；不可混用。
- coarse为platID1001/sensorID1、MWIR320；precise为platID1001/sensorID2、SWIR800。
- 目标完整键由原输入解析，默认类型0x22；不要随意改成0x33。
- 默认CSV为 `Data/circleDataAll3.csv`。可在双路界面选择另两份CSV，三份均原样打包。
- 默认策略EngineeringContinuous，模型P20M3_GREY_GAS_1，参数通过SHA-256校验。预检只需要随包工程参数，不依赖开发机MODTRAN LUT或renderer目录。
- 默认UTC为2026-10-21 03:30。改变场景日期/时间时修改 `DataDrivenTestQT/NetworkConfig.ini` 的 `[Demo] UtcDate`、`UtcHour`，再启动Qt。
- 默认input=60 Hz、videoFps=60；CSV原10 ms仿真时间保留，属于减速回放，不能用墙钟节拍替代物理时间。

## 停止、再次运行与回滚

先Qt STOP，等待接收器排空写完视频；再到板端启动终端Ctrl+C，停止两路并恢复系统策略。下一轮重新启动两接收器；板端如已退出需重新启动。

如需回滚，先停止v6，在板端将上面的候选参数改为：

`/userdata/HwaSimIR/candidates/P20_P20M3_SWIR_MWIR_Engineering_20261021_v5`

v5保留修复前的初始化行为；原P19/P20入口也未覆盖。

## 验证边界与故障定位

本轮v6已完成Windows两波段与板卡Mali双路各90帧的短时成像验证、68项局部热源物理/状态检查。尾焰和尾部热源不再因首次engine-on快照停留环境温度而变黑。已有关闭→开启仍保留真实仿真时间的升温过程；engineState=false不强行点亮。

客户运行包另做本机异目录、仅系统PATH的启动/依赖/预检和板卡短闭环。它证明包不依赖开发目录，但不能替代客户电脑的实际许可、网络和软件互通测试。客户现场仍为NOT_RUN。

本轮未做双路60FPS／每帧≤80ms长测，旧v5性能不达标证据保留。工程近似不是严格MODTRAN全域认证，也未完成真实辐射标定。

若Qt拒绝INIT，查看参数hash、CSV和UTC；若没有ACK，先核对板端进程、控制Domain150及DDS发现；若有ACK但没有视频，核对接收Topic、视频Domain1、编码和分辨率。许可错误查看各程序目录的.ddslog；缺Qt插件时确认platforms/qwindows.dll没有漏拷。不要通过关闭预检或替换DLL版本掩盖错误。
