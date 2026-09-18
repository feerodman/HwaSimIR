# HwaSimIR P12 普通启动与无图修复

## 结论

普通三程序 DDS 链已经能显示真实、持续变化的新图像，覆盖：

- 全 Windows：DataDrivenTestQT + HwaSim_IR + HwaSim_IR_VideoDisplay；
- 混合运行：Windows 控制/显示 + RK3588 HwaSim_IR。

验收未强制 `--dds-topic`、plat/sensor identity、DDS domain、codec、尺寸、stream role、QoS 或隐藏窗口，也未使用 P11 测试环境变量。VideoDisplay 从自身 EXE 同目录读取 `NetworkConfig.ini`，严格过滤 `1001/2`，从 `HwaSimIR.VideoStatus` 自动发现 `HwaSimIR.Video.1001.2.H264`，实际解码并绘制到 Qt widget。

![普通混合 DDS 实际显示窗口](../logs/p12/p12a/ordinary-mixed-ui-20260917-204319/receiver_actual_widget.png)

## 准确启动位置与有效配置

| 程序 | EXE/ELF 与工作目录 | SHA-256 | 普通配置 |
|---|---|---|---|
| DataDrivenTestQT | `D:\HwaSimIR\build-DataDrivenTestQT-codex-mingw73_64-Release\release\DataDrivenTestQT.exe`；工作目录为 EXE 目录 | `1b8cdf2d88cf6bfa4d9de4cb0d8249ec3dca67eaba78db66ed47bd0c03d6ad16` | 同目录 `NetworkConfig.ini`，SHA `d00cb810e0ebfdeb4047cc024d2d6abc68e0f17c984d2cb5dc1994fb97e6002e`；1001/2、domain 150、MWIR、800x800、同步 60 FPS |
| Windows HwaSim_IR | `D:\HwaSimIR\HwaSim_IR\Bin\HwaSim_IR.exe`；工作目录 `D:\HwaSimIR\HwaSim_IR\Bin` | `53d213a65c572287d23fce2cf120661653c7903e48b332b6bf7678dd609355ea` | `Config\NetworkConfig_precise.ini` SHA `764ea203...b4137af`；runtime SHA `ddd59b2d...cae63e`；QoS SHA `174ea6aa...10a410` |
| RK3588 HwaSim_IR | `/userdata/HwaSimIR/HwaSim_IR`；工作目录 `/userdata/HwaSimIR`；入口 `./run_precise.sh` | `2d5791360307af746e103d0fdb394a003ef72f70c02f25edcfd3cec3ead2d583`，Build ID `25372bcde3528cace876c4d10c55b9bf91046ed7` | launcher 只提供 channel/config 路径；1001/2、domain 150、identity topic；活动 Config manifest `ca19fc48...e977e7` |
| VideoDisplay | `D:\HwaSimIR\HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe`；工作目录为 EXE 目录 | `a832e28feee08259c829f14b455f484489d19eb210d8b7dbb660c4ba4b40898e` | 同目录 `NetworkConfig.ini`，SHA `b53a17c0678db878008e21e46aacaa844a2f9a54c3adb561f3182d1da0b8388d`；自动状态发现开启 |

Windows 实际网卡地址为 `192.168.1.188/24`；板端活动接口为 `eth0=192.168.1.116/24`。QoS 可以枚举网卡，但应用流仍由身份和状态严格选择，没有订阅任意发布者。

Windows HwaSim_IR 实际导入 `avformat-62`、`avcodec-62`、`avutil-60` 和 `swscale-9`。板端 ELF 实际依赖 `librockchip_mpp.so.1`、`libZRDDSCpp.so`、FFmpeg 与 Panda3D；VideoDisplay 实际依赖 FFmpeg 解码库。

## 根因和修复

1. **Windows 普通 EXE 没有编码器功能。** 原普通入口接受并执行输入，但每帧 `h264_encode_failed`；Release 工程现在自动定位随附 FFmpeg，缺失时 fail-closed，并把产物写入真正被用户启动的 `HwaSim_IR\Bin`。
2. **RK 输出在 INIT 前按异步模式延迟 RAM attachment，普通 INIT 切同步后旧条件永远不能释放。** pending attachment 现在作为输出状态，在第一个真实业务帧精确释放一次，不消费假 sourceSeq，不改变 Latest/OrderedQueue。
3. **NIR RGB16F 被误套 SWIR/MWIR RGBA16F alpha 门。** 门槛现在按真实格式检查；SWIR/MWIR 仍要求 16-bit alpha，NIR 兼容路径要求 RGB16F。
4. **普通控制 sidecar 默认选了兼容 NIR。** 正式默认改为 MWIR；NIR 未被删除，而是独立完成兼容回归。
5. **显示端无图状态不可辨识。** UI/低频计数现在区分等待状态、identity 不匹配、未 INIT/START、已绑定但无样本、等待 IDR、解码错误、过期输入、上游错误和正常显示；旧图超过两秒会标记 stale。

## Topic 与状态契约

| 项目 | 值 |
|---|---|
| DDS domain | 150 |
| Identity | platID 1001、sensorID 2 |
| Control/init/realtime | `HwaSimIR.Control`、`HwaSimIR.Init`、`HwaSimIR.Realtime` |
| Status | `HwaSimIR.VideoStatus` |
| 自动发现 video | `HwaSimIR.Video.1001.2.H264` |
| Metadata / annotation | `HwaSimIR.VideoMeta.1001.2`、`HwaSimIR.Annotation.1001.2` |
| Codec / image | H.264 Annex-B、800x800、请求 60 FPS |

`HwaSimIR.Video.precise.H264` 只保留为显式 legacy-channel 兼容值。普通自动发现不使用它。缺失/不匹配 identity 会明确显示并拒绝，不会退化成 0/0 通配。

## 最少普通操作

### Windows + RK3588

1. 板端执行 `cd /userdata/HwaSimIR && ./run_precise.sh`。
2. 从 EXE 所在目录双击 DataDrivenTestQT。
3. 从 EXE 所在目录双击 VideoDisplay；它可以在 START 前或后启动，并通过状态与下一 IDR 恢复。
4. 在 DataDrivenTestQT 依次点击 RESET、INIT、START；结束时点击 STOP 等待排空与录像结束。

### 全 Windows

1. 从 EXE 所在目录启动 DataDrivenTestQT。
2. 以 `D:\HwaSimIR\HwaSim_IR\Bin` 为工作目录、无参数启动 `HwaSim_IR.exe`。
3. 从 EXE 所在目录无参数启动 VideoDisplay。
4. 点击 RESET、INIT、START，结束时点击 STOP。

无需 DecodeGateway 或第四个程序。P12 自动验收只给控制端有限运行时长；没有用 topic/identity/QoS 强制参数代替普通配置。当前桌面自动化面不能点击这些原生按钮，因此“人手逐键点击”本身仍标为未执行；三个真实窗口可见且响应、普通入口和真实图像已实测。

## 当前证据

| 证据 | 结果 |
|---|---|
| `logs/p12/p12a/ordinary-mixed-syncgate-20260917-204034` | 1500 accepted/executed/received/decoded，0 DDS/decode error，连续 MP4 |
| `logs/p12/p12a/ordinary-mixed-ui-20260917-204319` | 实际 Qt widget，185/185/185，约 58 FPS |
| `logs/p12/p12a/mixed-late-restart-20260917-204516` | 晚启动与重启均自动恢复；等待 IDR 后解码 1831 帧，0 error |
| `logs/p12/p12a/mixed-input-pause-20260917-204738` | 真实 5 秒输入中断后恢复 60 FPS，没有新增 PAUSE opcode |
| `logs/p12/p12a/ordinary-mwir-final-20260917-210454` | MWIR 600/600/600，连续 MP4 |
| `logs/p12/p12a/nir-compat-rk-20260917-210308` | NIR RGB16F 720/720/720，实际 widget |
| `logs/p12/p12d/lifecycle/ordinary-lifecycle-20260918-043854` | 最终 ELF 的晚启动、接收端重启、输入空档、同进程 SWIR→MWIR→SWIR、STOP→START 全部 PASS |

## 部署和回滚

- 最终部署证据：`logs/p12/p12d/deployment/p12-elf-20260918-042702` 与 `p12-20260918-042901`。
- 实际 P11→P12 回滚证据：`logs/p12/p12d/rollback/p12rb-20260918-045424/rollback_summary.json`。
- P11 回滚后收到 481 个新帧，实际 widget SHA `e3a48e...8ee8f`；P12 恢复后再次收到 481 个新帧，widget SHA `111c0e...d4594`。
- 恢复后 ELF、Build ID、launcher、性能工具、完整 Config manifest、runtime/network、LUT、Targets 和 TargetLib 子集均与回滚前 P12 相同。
- 第一次回滚脚本运行因 Windows PATH 环境构造错误失败，现场 `logs/p12/p12d/rollback/p12rb-20260918-045030` 保留；紧急恢复成功，修正后才产生上述 PASS。

原失败运行没有删除或重写为 PASS。
