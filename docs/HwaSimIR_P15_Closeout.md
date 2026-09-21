# HwaSimIR P15 工程收口

日期：2026-09-21  
基线提交：`633048417108dfc20958324b683b467d7a54cc40`  
状态：UI 常显与数字标签已交付；普通入口天气/尾焰反馈保持 `REOPENED`；真实标定保持 `NOT_VERIFIED_CALIBRATION`

## 1. 先给结论

| 验收项 | 状态 | 结论 |
| --- | --- | --- |
| 构建通过 | PASS | DataDrivenTestQT Release、UI 测试、Windows x64 renderer、AArch64 renderer 均构建通过 |
| 默认 UI 可见 | PASS | 普通启动、不展开文件预览即可看到现有目标类型下拉框、当前值和下拉按钮 |
| 实际交互 | PASS | 最终交付 EXE 上完成打开列表、选择民用 `0x55`、INIT 冻结、RESET 解锁、关闭和同目录重开 |
| 编号正确 | PASS | 关重部位只显示稳定数字；2 隐藏时显示 1、3，恢复/转动/重建后仍为原编号 |
| 媒体可解码 | PASS | P15 UI 录屏、普通窗口录屏和 800×800 真实 DDS MP4 均可播放；DDS MP4 2004/2004 帧完整 rawvideo 解码通过 |
| 生产天气反馈关闭 | **REOPENED** | 当前普通 Cloudy/6 km 入口仍无可辨识云；板端 `visibleCloudVolumes=0`，雨雪未由该默认普通场景请求 |
| 生产尾焰反馈关闭 | **REOPENED** | 当前普通入口内部为 `formalTauReady=1/coreVisible=1/haloVisible=1`，但小目标周围仍无可单独辨识尾焰，未定位新根因 |
| 真实标定 | `NOT_VERIFIED_CALIBRATION` | P15 未做目标真实红外标定 |

P15 没有扩大 MODTRAN 数据域，没有改首有效位置、协议、队列、热模型、天气或尾焰生产链，也没有重跑无关大矩阵。P14 报告和台账保持原样；P15 只新增只读核对和重新登记。

## 2. 最终常用程序与版本身份

明确交付启动路径：

`D:\HwaSimIR\deliverables\HwaSimIR_P15\DataDrivenTestQT\DataDrivenTestQT.exe`

| 项目 | 值 |
| --- | --- |
| 最终 EXE SHA-256 | `4d1238924beddd0626bf2ff4e0b1857c03618570d4b7668459d218cc249c6dde` |
| 工作目录 | `D:\HwaSimIR\deliverables\HwaSimIR_P15\DataDrivenTestQT` |
| 实际配置 | `D:\HwaSimIR\deliverables\HwaSimIR_P15\DataDrivenTestQT\NetworkConfig.ini` |
| 配置 SHA-256 | `ce7fb219d50beec28eb8ab4e7dbae5cf5b9e1148aa2a856c22c06f617b8a1a76` |
| 构建标识 | `P15-UI-NUMERIC | base 633048417108 | built Sep 21 2026 13:08:11` |
| 实际 HEAD | `633048417108dfc20958324b683b467d7a54cc40`（与参考提交一致） |
| 源工作区 | P15 本地修改，未 commit、未 push、未回退用户修改 |

P15 动工前先执行只读基线检查：实际分支为 `main`，HEAD 与参考提交完全一致，`git status --short` 为空。最终工作区差异只包含本报告第 8 节列出的 P15 源码、测试、工具、文档和交付物；P14 原始报告/台账的 tracked diff 仍为空。

常用 Release 构建目录的 EXE 与最终交付 EXE 哈希相同。P15 前的常用 Release EXE 已备份到 `logs/p15/backups/pre_p15_release_20260921`，旧哈希为 `b479cde1cd6cf1640a0296bd8b1b390329b9c6e9743cf83b1e7e4dcbf1b15e24`。桌面、公共桌面和两个开始菜单目录中没有找到指向 DataDrivenTestQT 的快捷方式，所以没有把某个测试目录臆定为用户快捷方式目标。

主窗口新增清晰的“版本信息”入口，显示正在运行的绝对程序路径、构建标识、实际配置路径、工作目录和自校验 SHA-256；普通启动无需参数。最终交互记录中的首启 PID 为 10756，关闭后从同一交付目录重启 PID 为 30208，两次均正常退出。

## 3. 目标类型 UI 直接修复

没有创建第二个目标类型控件。既有 `targetTypeCombo` 和既有“演示强制显示”策略控件从折叠的“文件数据与身份”预览组移到控制按钮后的常显“测试目标”组；长传感器参数表仍在其后。文件位置/姿态只读预览继续默认折叠，但不再拥有或隐藏这两个控件。

协议码、信号绑定、默认 `0x22`、INIT 后冻结和 RESET 后解锁语义保持不变。最终交付 EXE 的普通零参数交互验证：

1. 不展开任何预览，目标类型标签、当前值和下拉按钮可见；
2. 实际打开列表并选择“民用厢式车/卡车测试载体 — 协议 0x55”；
3. 点击 INIT 后下拉框禁用，点击 RESET 后恢复；
4. 关闭后从同一目录重开，预览仍折叠且目标控件仍可见。

自动测试没有调用子控件 `show()`、没有展开预览、没有修改布局，也没有使用目标类型测试环境变量。100%/125%/150% 使用 Qt 缩放倍率分别运行；本机基础 DPR 为 2.0，因此实际记录 DPR 为 2.0/2.5/3.0，三组均 PASS，文字和下拉按钮无截断或覆盖。

证据入口：

- `logs/p15/ui/final_interaction/P15_final_DataDrivenTestQT_interaction.mp4`
- `logs/p15/ui/final_interaction/interaction_result.json`
- `logs/p15/ui/final_interaction/frame_0000_ordinary_open.png`
- `logs/p15/ui/final_interaction/frame_0008_dropdown_open.png`
- `logs/p15/ui/final_interaction/frame_0024_after_init_frozen.png`
- `logs/p15/ui/final_interaction/frame_0032_after_reset_unlocked.png`
- `logs/p15/ui/final_interaction/frame_0040_version_info.png`
- `logs/p15/ui/final_interaction/frame_0056_reopened_same_delivery.png`
- `logs/p15/ui/default/ordinary_default.json`
- `logs/p15/ui/scale_125/scale_125.json`
- `logs/p15/ui/scale_150/scale_150.json`

实际交互使用 Windows UI Automation 调用和原生下拉键消息，作用对象是最终交付 EXE。原生 computer-use 连接器当时没有返回可控原生应用，因此这是一份自动化交互证据，不冒充用户人工签收。

## 4. 关重部位数字标签

生产配置当前实际定义两个关重部位：`head`、`middle`。投影时按既有定义顺序固定赋值 1、2；新增的 `displayIndex` 仅用于显示，不序列化。视觉覆盖层只把关重部位文字改为纯数字，目标模型标签、十字、颜色、字号、避让和引线保持原行为。

原始 `name`、部位 ID、坐标、可见性、协议字段、记录身份、几何计算和目标关联均未修改。`AnnotationManager` 中原名称仍用于原始记录/诊断，屏幕关重部位入口不再同时叠加名称和数字。

稳定编号测试另外构造普通 CIVIL 三点对象：

- 2 不可见：屏幕请求为 1、3；
- 2 恢复且容器逆序：仍为 1、2、3；
- 对投影点绕图像中心执行明确的 90° 旋转并重建 Overlay：仍为 1、2、3；
- 非显示字段身份串前后完全一致；
- 4 种分辨率 × 4 种布局，共 16 例均无标签遗漏、溢出或重叠。

生产普通 DDS 新录像 `dds_t12s.png` 和 `dds_t24s.png` 也直接显示数字 1、2。对比入口：

- 修改前：`logs/p15/numeric_labels/before/labels_0_after.png`
- 修改后隐藏 2：`logs/p15/numeric_labels/after/p15_numeric_hidden_2.png`
- 修改后恢复：`logs/p15/numeric_labels/after/p15_numeric_1_2_3.png`
- 稳定性结果：`logs/p15/numeric_labels/after/p15_stable_numbering.txt`
- 非显示字段/布局测试：`logs/p15/numeric_labels/after/label_tests.csv`

## 5. P14 媒体只读核对与反馈重新登记

P14 文档中宣称的根目录 `D:\HwaSimIR\deliverables\HwaSimIR_P14_Delivery` 当前不存在。实际历史包位于 `D:\HwaSimIR\logs\p14\deliverables\HwaSimIR_P14_Delivery`。只读审计找到 15 个 MP4，15/15 文件大小和 SHA-256 与 P14 manifest 一致；P15 当前重新完整解码代表性的 8 个，8/8 通过，其余 7 个保留 P14 历史 15/15 解码记录。

所有 15 个 P14 媒体都标记为历史/受控证据，`fromUserOrdinaryEntry=false`：原轨迹视频是自动化受控回放，天气是合成民用地面 fixture，尾焰是受控 On/Off 通用热源，首有效位置是生成 fixture。它们不能直接关闭用户普通入口反馈。

当前仓库和板端的实际 `Config/GameVFX/sprite.frag` SHA-256 都是 `d20ed42cc01cfa4fa0e6f1a7b5fc1ef123380c97ee60250ffd196c08079063d0`，已经包含独立 SI 分支。P15 没有在未核对实际文件的情况下再次把旧 0–1 截断写成当前根因，也没有修改 shader。

完整记录：`logs/p15/p14_media_audit/p14_media_readonly_audit.json`。

## 6. 当前普通入口复现

P15 使用最终交付 DataDrivenTestQT、普通零参数 VideoDisplay、板端普通 `run_precise.sh`、原始未改 `1.txt` 和默认 MWIR 做了一次短观察。没有测试参数和环境覆盖；板端正常身份 1001/2、800×800、DDS-only。

本次有效轮收到并消费 2004 个实时包，生成并完成 2004 帧，`inputMinusCaptured=0`，STOP 排空完成。接收器实际 DDS MP4 是 H.264、800×800、2004 帧、33.416989 s；rawvideo 完整解码返回 0 且 stderr 为空，2004 个 packet 的 PTS/DTS 均无重复或回退。

板端日志同时显示：

- Cloudy、能见度 6000 m、cloud enabled、coverage 0.595 已进入运行态；但所审区间 `visibleCloudVolumes=0`；
- 该普通配置没有请求雨雪，`precipitationType=none`，所以 P15 不把这次观察说成普通雨雪验证；
- AIM120D 尾焰内部状态为 `formalTauReady=1`、`coreVisible=1`、`haloVisible=1`；
- 但 800×800 实际画面中的目标非常小且偏暗，主要靠目标标注和数字 1、2 才能识别，周围没有可单独辨识的尾焰；云体也不可辨识。

因此：

- “用户普通运行未见天气”在 Cloudy/6 km 普通入口上得到当前版本复现，整体仍为 `REOPENED`；雨雪普通入口尚未在本次默认场景覆盖。
- “用户普通运行仍见黑色尾焰”仍为 `REOPENED`。当前证据证明内部节点状态与用户可见结果存在落差，但不能仅凭这段小目标画面精确隔离根因，更不能用 P14 raw 正差或 MP4 可解码代替可见性验收。

证据入口：

- 完整传感器视频：`logs/p15/ordinary_feedback_probe/P15_ordinary_original_1txt_mwir_dds.mp4`
- 普通接收窗口录屏：`logs/p15/ordinary_feedback_probe/P15_ordinary_user_entry_feedback_probe.mp4`
- 帧身份：`logs/p15/ordinary_feedback_probe/dds_frame_index.jsonl`
- 录像完成状态：`logs/p15/ordinary_feedback_probe/dds_recording_status.json`
- 帧审阅和身份：`logs/p15/ordinary_feedback_probe/frame_review.json`
- 板端日志：`logs/p15/deployment/p15_ordinary_feedback_board.log`

P15 没有制作或修改新的独立图形样例，因为本轮没有实施粒子、透明层或天气生产修改。P14 受控 fixture 只作为历史工程证据，不被改写成 HwaSimIR 普通入口已修复。

## 7. 构建和部署

| 产物 | 结果 | 身份 |
| --- | --- | --- |
| DataDrivenTestQT Release | PASS | `4d123892...9c6dde` |
| P15 UI 测试程序 | PASS | `6daef42d...34b6d6` |
| Windows HwaSim_IR x64 Release | PASS | `34c25cda...50bb29c` |
| AArch64 HwaSim_IR | PASS | `008a514e...8273a`，Build ID `9cca0333...884f3` |
| RK3588 普通启动 | PASS | DeploymentVersion、RunPreflight、Mali GPU、800×800 |

第一次 Windows renderer 构建在受限沙箱内因访问 `Microsoft.Cpp.x64.user.props` 被拒，失败日志保留且不计 PASS；随后在已批准环境执行相同 Release 命令通过，并在收口前再次增量构建通过。

板端 `/userdata/HwaSimIR/HwaSim_IR` 已部署为上述 P15 AArch64 哈希；运行配置和外置 sprite shader 未变。旧 P14 ELF 与版本文件保存在 `/userdata/HwaSimIR/.p15_backup_20260921_1323`。普通启动和普通入口复现结束后，P15 启动的板端 renderer 已停止，没有提前终止录像。

## 8. 修改文件清单

生产源码：

- `DataDrivenTestQT/mainwindow.cpp`
- `DataDrivenTestQT/mainwindow.h`
- `HwaSim_IR/HwaSim_IR/Annotation/AnnotationTypes.h`
- `HwaSim_IR/HwaSim_IR/Annotation/AnnotationProjector.cpp`
- `HwaSim_IR/HwaSim_IR/Annotation/AnnotationOverlay.cpp`
- `HwaSim_IR/HwaSim_IR/Annotation/AnnotationOverlay.h`
- `HwaSim_IR/HwaSim_IR/Annotation/AnnotationLabelLayout.h`

测试和证据工具：

- `DataDrivenTestQT/tests/p15_ui_default_visibility_test.cpp`
- `DataDrivenTestQT/tests/p15_ui_default_visibility_test.pro`
- `tools/p10_label_test.cpp`
- `tools/p15_final_ui_evidence.ps1`
- `tools/p15_inventory_program_identity.ps1`
- `tools/p15_audit_p14_media.ps1`
- `tools/p15_ordinary_effect_feedback_probe.ps1`

P14 文档、P14 台账、原始模型、原始 `1.txt`、大气 LUT/coverage、生产 shader 和运行配置未修改。原 `1.txt` 源文件与交付副本 SHA-256 均为 `f2c3db00d71012cd28336b5077e03ff8d4ed1eda2698fd900486ae9cdfdd7901`。

## 9. 回滚

Windows DataDrivenTestQT 可恢复备份：

- `logs/p15/backups/pre_p15_release_20260921/DataDrivenTestQT.exe`
- `logs/p15/backups/pre_p15_release_20260921/NetworkConfig.ini`

回滚时先关闭对应 DataDrivenTestQT 进程，核对目标为常用 Release 或明确交付目录，再用备份替换同名文件；不要删除其它构建目录。源码回滚应只反向应用本节列出的 P15 diff，不得使用 `git reset --hard` 或覆盖用户其它修改。

板端回滚时先正常停止当前 renderer，核对备份目录 `/userdata/HwaSimIR/.p15_backup_20260921_1323`，在同一文件系统原子替换 ELF 和 `Config/deployment_version.env`，再运行正常启动预检。P15 没有自动执行回滚，以免改变已完成的最终部署。

## 10. 仍未关闭

1. `P15-ORDINARY-WEATHER`：普通 Cloudy/6 km 的云体不可辨识，板端本轮 `visibleCloudVolumes=0`；普通雨雪未由默认场景触发。
2. `P15-ORDINARY-PLUME`：内部 tau/node 状态有效，但普通窗口和完整 DDS 图中没有可单独辨识尾焰；需以后在当前普通入口下继续定位投影尺寸、遮挡、混合和显示映射，不能靠增益或 `tau=1` 掩盖。
3. `P14-C-CAL`：保持 `NOT_VERIFIED_CALIBRATION`。
4. `P14-C-DOMAIN`：仍是 P14 声明的离散覆盖，不扩写成全域。
5. `P14-D-QT-PAINT`：历史 Windows GUI paint 长尾仍单独 OPEN，P15 没有重跑性能矩阵。
6. P14 历史交付绝对路径与当前实际位置不一致，已在 P15 台账公开记录。
