# P14 普通 DDS 运行、部署与复核手册

## 固定身份与文件

- 控制与视频：DDS-only。
- 平台/传感器身份：1001/2。
- 输出：800×800，不降分辨率。
- 原始输入：`DataDrivenTestQT/1.txt`，SHA-256 `f2c3db00d71012cd28336b5077e03ff8d4ed1eda2698fd900486ae9cdfdd7901`。
- 最终板端 ELF：SHA-256 `80267b01054d9fe8e8ef2c3fd8f3e726003794dafc6cc1086958e9713f18841c`，Build ID `0856b18abf672f4c00904e653ffeb795f5c93e71`。
- 运行配置 SHA-256：`3a56f9f6af7034beae7fe12f8c4e32d9a55df9e377d98e3d343fc579fe9df07f`。
- 配置 manifest SHA-256：`9b344f42964dbf4e05d7502ba815bea6d4c6e37276bebd3aa63a8f702ec600c9`。
- LUT/coverage manifest：`48432459...7cd7` / `f5d27c30...5d7b`。

## 普通原 1.txt 启动

1. 在板端 `/userdata/HwaSimIR` 使用已部署启动器启动 `HwaSim_IR --channel precise`。启动日志必须同时出现：DeploymentVersion PASS、RunPreflight PASS、Mali GPU、DDS protocol ready、P14 AtmosphereIdentity PASS。
2. 在 Windows 启动 `HwaSim_IR_VideoDisplay.exe`。接收端通过 `VideoStatus` 自动发现 `HwaSimIR.Video.1001.2.H264`，不要手工改成旧 F1/F2 或 TCP/UDP 视频链路。
3. 启动 `DataDrivenTestQT.exe`，加载原始 `1.txt`。保持“演示强制显示”未勾选；选择波段 SWIR 或 MWIR；目标类型可选择“民用厢式车/卡车测试载体 — 协议 0x55”。
4. 点击 INIT。INIT 后目标类型和演示显示控件冻结；等待有效实时位置仍是预期状态。
5. 点击 START。前 659 行继续发送和消费，但目标按源 `ViewValid=0` 隐藏；第 660 行开始显示。不要用强制显示把普通验收改成演示语义。
6. 等待 4318/4318 行和帧完成，再点击 STOP。分别等待控制响应、渲染排空、DDS 约 5 秒完整排空和 Windows 录像 muxer finalized；不要提前终止任一进程。
7. 复核 `recording_status.json`、`frame_index.jsonl`、`event_timeline.json` 和 `media_qc.json`：输入/写出帧必须相等，sourceSeq/frameSeq 连续，无复用、无 gap、decode error 为 0。

已验收的普通运行目录：

- `logs/p14/runs/p14_final4_original_1_swir_clear/`
- `logs/p14/runs/p14_final4_original_1_mwir_clear/`

## UI 目标类型

目标类型使用 `QComboBox`，显示名称并绑定真实整数协议码；默认仍选择一个有效目标。INIT 时把当前选择写入 `targetState[0].targetType`、weapon/target 引用键以及对应 INIT 容量字段，然后冻结，RESET 后解锁。正式 UI 点击证据：`logs/p14/ui/target_type_click_selected.png`、`target_type_click_init_frozen.png`、`target_type_click.json`、`target_type_click.log`。

## 天气混合运行

地面天气使用独立 `DataDrivenTestQT/p14_ground_truck_weather_30s.txt`（1800 行，SHA-256 `c2e8b06909e6c5149680b477004cda5f216475116df3fe411c126ae292d5183a`），它不读取原 `1.txt`，不得改名冒充原轨迹。四类环境：晴天 23 km/RH30、云与能见度 12 km/RH60、雨 6 km/RH85、雪 6 km/RH85。两波段最终用例均为 1800/1800。

原始高空轨迹和民用地面天气必须分开报告。世界云使用相机/世界网格流送中心，不粘到目标；稳定 ID、对象池、LOD、Batch 雨雪保留。

## 诊断采集边界

- 普通运行：`P5MaterialView=0`、正式 M1、正常固定映射/AGC、无 PFM 诊断负载。
- PFM/raw 采集：单独运行并明确 `diagnostic_linear`；不得把其时延算入普通性能，也不得用诊断图替代正常材质媒体。
- 尾焰因果：相同 ELF/配置/轨迹下执行通用热源 On/Off，比较同一 sourceSeq 的 SI PFM，使用 `On - Off` 带符号差。

## 部署闭包

部署时必须一起校验而非只替换 ELF：

- `HwaSim_IR` AArch64 ELF；
- `Config/HwaSimIRRuntime.ini` 与配置 manifest；
- `Config/GameVFX/sprite.vert|frag` 和 sprite atlas；
- `Config/Weather/precipitation.vert|frag`、weather profile/texture 配置及所需纹理；
- `Config/Atmosphere/MODTRAN/processed/band_lut_si.csv` 与 `p14_coverage_manifest.json`；
- `Config/TargetLib/p11/civil_van/`、`Config/IRPlume/engine_plume_profiles.json` 和相关材料/标注配置；
- Windows DataDrivenTestQT/VideoDisplay EXE 与各自网络配置。

部署后从板端直接计算哈希，并运行 quick preflight。板卡不能连接互联网，交付包已经包含本轮闭包，不依赖在线下载。

## 回滚

使用同一文件系统的原子 rename 在“当前 ELF、候选旧 ELF、最终 ELF”之间切换。每次切换后至少验证版本身份、RunPreflight、GPU、DDS 和 P14 AtmosphereIdentity。不要使用会在空间不足时产生完整重复副本的 copy 方案。最终通过收据位于 `logs/p14/rollback_final2/p14rb-20260921-final2/rollback_receipt.json`。

