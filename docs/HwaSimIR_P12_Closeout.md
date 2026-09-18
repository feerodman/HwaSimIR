# HwaSimIR P12 收口交付报告

## 最终结论

P12 已完成普通启动无图修复、四模型工程成像回归、真实视线内云/雨/雪证据、2 km MODTRAN 数据、板端受控玻璃、诊断抓图拆分、生命周期、最终部署和实际回滚。总体状态保持 `PARTIAL`，原因不是功能链失败，而是 SWIR 冷启动 80 ms 硬门仍有 3 帧超限，且真实标定没有外部真值。

详细状态见：

- `docs/HwaSimIR_P12_Execution_State.md`
- `docs/HwaSimIR_P12_ManualStartup_And_NoImage_Fix.md`
- `docs/HwaSimIR_P12_Imaging_Regression_And_Remaining.md`
- `docs/HwaSimIR_P12_Issue_Ledger.csv`

## 普通启动成果

普通 Windows+RK 三程序链使用 EXE 同目录 sidecar、严格 `1001/2` identity、domain 150、`HwaSimIR.VideoStatus` 自动发现和 `HwaSimIR.Video.1001.2.H264`。没有强制 topic/identity/QoS、隐藏窗口或 P11 环境变量。最小操作与根因见普通启动文档。

![普通启动实际显示窗口](../logs/p12/p12a/ordinary-mixed-ui-20260917-204319/receiver_actual_widget.png)

## P12B/P12C 图像与数据

| 交付项 | 结果 | 主证据 |
|---|---|---|
| 4 模型 × 2 波段 × 2 视角 | 16/16 PASS | `logs/p12/p12b/asset_views/index.html` |
| 云前/后、两个 cloudId、雨雪 | 12/12 PASS | `logs/p12/p12c/weather_cloud/index.html` |
| 板端受控玻璃 | 8/8 PASS | `logs/p12/p12c/controlled_glass/evidence/index.html` |
| MODTRAN 2 km | 160 工况、201 checks PASS、正式新增 96 行 | `logs/p12/p12c/modtran_2km/qc_results.json` |
| 手动 INIT 覆盖预检 | 超域明确拒绝；有效 1 km 正常显示 | `logs/p12/p12c/manual_init_coverage` |

图像均来自正式程序真实输出；标成 received 的图来自 DDS H.264 解码，没有复制本地 fixed/linear 图冒充。线性图、received 图和差分图分别命名。

## P12D 性能

测试为 800×800、DDS、RK MPP、真实 Windows 解码/Qt paint；普通运行关闭 PFM 详细诊断。板端、Windows 与跨机估计分开计时，没有直接相减不同机器的 steady clock。

| 案例 | 功能 | 性能硬门 | products / decoded | accepted→writer cold p99 / max / >80 | steady p99 / max / >80 | Qt paint |
|---|---|---|---:|---|---|---|
| SWIR 60 s Cloudy | PASS | **FAIL** | 3601 / 3601 | 80.823 / 84.159 ms / 3 | 34.266 / 36.048 ms / 0 | 3419 帧，59.965 FPS |
| MWIR 60 s Cloudy | PASS | PASS | 3600 / 3600 | 69.894 / 73.546 ms / 0 | 33.607 / 36.193 ms / 0 | 3420 帧，59.996 FPS |
| MWIR 300 s Snow | PASS | **FAIL** | 18000 / 18000 | 278.180 / 282.960 ms / 46 | 35.799 / 41.395 ms / 0 | 17809 帧，59.962 FPS；max gap 119.649 ms |

60 s 最终套件：`logs/p12/p12d/performance/ordinary-performance-20260918-043110`。300 s 长测：`logs/p12/p12d/performance/ordinary-performance-20260918-020639/MWIR_300s_Snow`。

STOP 分开统计：SWIR control response 0.131 ms、render drain 42.596 ms、DDS total drain 5000.109 ms、完整停止 10043.018 ms；MWIR 分别为 0.121、46.257、5000.168、10046.840 ms。录像连续，产品与解码守恒。

### PFM A/B

同输入短轮显示旧同步 PFM 在 sourceSeq 180/900 的总耗时为 91.801/85.464 ms。改成持有帧副本的容量 2 有界异步 CPU writer 后，render thread 为 52.973/51.059 ms，其中 GPU wait/readback 为 45.598/44.108 ms、CPU readback copy 约 6 ms、queue enqueue <0.1 ms。队列满会明确报告失败，不静默丢证据。

- 旧同步：`logs/p12/p12d/performance/p12d-perf-ab-20260918-012607/summary.json`
- 新异步：`logs/p12/p12d/performance/p12d-perf-ab-20260918-014219/summary.json`

普通路径没有改 Latest、丢输入、复制旧图或降分辨率。

## 生命周期

`logs/p12/p12d/lifecycle/ordinary-lifecycle-20260918-043854/lifecycle_summary.json` 为 PASS：

- 同一板端 PID 50 完成 SWIR→MWIR→SWIR；
- 三轮各 601 个成功 realtime write，均收到 INIT ack 并 STOP；
- 晚启动接收端最多解码 601 帧，重启后接收端最多 1202 帧；
- 两端 `decodeErrors=0`、`statusIdentityRejected=0`；
- 输入空档恢复、STOP 后再 START 两次、三轮 drain 均通过；
- 预热在 READY 前做两个 discard-only pass，不消费输入、不发布产品、不推进热状态。

## 部署与回滚

最终板端 ELF：

- SHA-256 `2d5791360307af746e103d0fdb394a003ef72f70c02f25edcfd3cec3ead2d583`
- Build ID `25372bcde3528cace876c4d10c55b9bf91046ed7`
- Config manifest `ca19fc482985ff6b0ea75eb2614f4892fae2d5d14687e64edacb033539e977e7`

最终部署见 `logs/p12/p12d/deployment/p12-elf-20260918-042702` 与 `p12-20260918-042901`。

实际回滚 `logs/p12/p12d/rollback/p12rb-20260918-045424/rollback_summary.json` 为 PASS：P11 回滚和 P12 恢复各收到/解码 481 个新帧并保存真实 widget；P12 恢复前后 ELF、Build ID、launcher、性能工具、完整配置、LUT、Targets 与 TargetLib 子集一致。板端保留 P11 原快照和本轮两个可恢复快照。

失败的第一次脚本运行 `p12rb-20260918-045030` 原样保留，不能作为 PASS；它的应急恢复成功，修正 PATH 构造后才重新完成实测。

## 有效配置与数据清单

| 数据 | 用途 | 最终身份 |
|---|---|---|
| `Config/HwaSimIRRuntime.ini` | 正式运行开关、映射、天气、性能 | `ddd59b2d...cae63e` |
| `Config/NetworkConfig.ini` | 普通兼容网络入口 | `948dce57...627f4` |
| `Config/NetworkConfig_precise.ini` | 板端普通 precise launcher | `764ea203...137af` |
| `Config/DDS/ZRDDS_PROTOCOL_QOS.xml` | DDS QoS | `174ea6aa...0a410` |
| `MODTRAN/processed/band_lut_si.csv` | 正式相机带查询，含 P12 2 km | `6f22d25d...5fd99` |
| `MODTRAN/processed/solar_heating_lut_si.csv` | 宽谱太阳加热 | 保持 P11 正式表 |
| `TargetLib/Targets.json` | 正式 targetType 映射 | `16425775...ad02` |
| `TargetLib/p12` | P12 受控玻璃/派生映射 | 工程 fixture，非真实标定 |
| `ordinary_demo_1km.txt` | 日常安全普通启动输入 | 1 km 受控几何，不声明物理真值 |

全量 P12 2 km 原始 MODTRAN 输入/输出、组件产物、单位/Jacobian、积分和 QC 位于 `logs/p12/p12c/modtran_2km`。运行时只依赖正式压缩表，不依赖 MODTRAN 引擎在线执行。

## 交付包

`HwaSimIR_P12_Delivery.zip` 包含：

- 三个 Windows 普通运行目录和 RK3588 ELF/launcher；
- 普通运行所需 Config/TargetLib/Weather/Effects/Panda3D/FFmpeg/Qt 依赖；
- P12 源码增量、测试脚本、最终文档；
- 独立图片和离线 HTML；
- P12 2 km 原始证据与精选性能/生命周期/部署/回滚证据；
- `delivery_file_manifest.json`、`final_status.json` 和 ZIP SHA-256 sidecar。

DDS 许可证、MODTRAN 可执行程序/许可证、账号口令、`.pdb/.iobj/.ipdb/.o/.ddslog`、旧 MP4 录像和无关大体积历史矩阵不装包。部署时必须由合法运行环境单独提供 DDS 许可证。

最终包为 `D:\HwaSimIR\deliverables\HwaSimIR_P12\HwaSimIR_P12_Delivery.zip`，454,790,715 字节，1957 个 ZIP 成员，SHA-256：

`f112b2fd445915b5b654149e169cef0149685b87f405bf39b46763de094b16a8`

同目录 `HwaSimIR_P12_Delivery.zip.sha256`、`delivery_receipt.json` 和 `delivery_file_manifest.json` 已交付。ZIP 自身文档按非递归规则指向外部 sidecar，不把 ZIP 自身哈希写回 ZIP 内造成循环身份。

## 未关闭和非声明

- SWIR 冷启动 3 帧超过 80 ms：性能项 FAIL。
- 真实材料/目标/器件标定：`NOT_VERIFIED_CALIBRATION`。
- 人手逐键操作：未由当前自动化面执行。
- 不含 damage 模型；不做 `engineState→BrightSpot`、`strikeFlag→rear ThermalHotspot`；不接 Stage5 path/sky/solar；不把通用模型参数称为具体装备真实材料、温度、尾焰或速度—热特征。
- 没有 commit 或 push。
