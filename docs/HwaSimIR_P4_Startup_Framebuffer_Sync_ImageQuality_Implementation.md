# HwaSimIR P4：正常启动、同步帧数与 NIR/MWIR 成像质量实施报告

## 1. 结论

P4A 与 P4B 已按顺序实施。正常 `./run_precise.sh` 不再扫描 5.72 GB Config，强制连接板卡本地 Xorg；同步模式建立了“有效 Realtime 输入—GPU 渲染—有效读回—H.264/DDS 发布”的一一对应关系，并修复了世界空间云分支触发的 `GL_INVALID_FRAMEBUFFER_OPERATION (0x506)`。P4B 将 MWIR 机体、局部热区、背景和云统一到 3–5 μm 矩形响应的波段平均辐亮度，恢复实际 GPU 正式分支中的局部尾部热区和亮斑，同时保持独立尾焰只合成一次。

最终中间辐射缓冲为板端实测可用的 RGB16F；只有 Stage6 最终显示映射后才量化为 H.264 8-bit。NIR/MWIR 目标与背景共享每个波段的公共显示范围，没有通过整目标升温、单目标增益、修改灯功率或缩小分辨率制造效果。

## 2. 修改文件

- `HwaSim_IR/HwaSim_IR/HwaSimIR.cpp/.h`
  - 后渲染捕获、无效/旧帧拒发、STOP 排空边界和同步回合守恒；
  - 世界空间云使用 raw-scene 原生深度附件；
  - RGB16F 原始辐射缓冲；
  - 3–5 μm GPU 波段平均、局部热区正式合成、统一背景辐射映射；
  - 启动里程碑、模型边界和关键物理链诊断。
- `IR/IRRadianceModelV2.cpp/.h`：公共 3–5 μm Planck 波段平均；正式 M1 CPU 标量不再把局部峰值当成整目标面积。
- `IR/IRWeatherEffects.cpp`：MWIR 云温度显示改用同一 3–5 μm 波段平均。
- `HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini`：NIR/MWIR 公共辐射显示范围和 MWIR 天空/地面/云工程环境参数。
- `HwaSim_IR/Bin/Config/IRHotspots/target_hotspots.json`：F35 局部尾部热区、机首/中部亮斑工程样例。
- `DataDrivenTestQT/main.cpp`、`mainwindow.*`：不改变协议的测试控制参数，包括发动机状态、打击标志和部位。
- `HwaSim_IR_VideoDisplay/...`：按指定 DDS sample 序号导出真实解码帧。
- `tools/rk3588_run_hwasimir_precise.sh`：快速正常启动、本地 Xorg 绑定和显式 `--verify-full`。
- `tools/rk3588_deploy_atomic.ps1`：完整 Config 原子部署，F35 热区列为必需资源，并抑制仅由系统时钟差产生的 tar 时间戳刷屏。
- `tools/p4_mwir_band_radiance_qc.ps1`：CPU/GPU 波段定义、局部热区和尾焰重复合成检查。
- `tools/p4_image_quality_qc.ps1`：对实际 PNG 做全图/ROI 灰度、对比度和黑白饱和统计。
- `tools/testdata/p4_f35_close_1km.csv`、`p4_f35_rear_close_1km.csv`：F35 正面和后视固定几何样例。

没有修改 CommonData、DDS IDL 或协议字节布局；未增加 `.prc` 文件，仓库当前也不存在需随版本部署的独立 PRC。Panda 运行参数仍由正式启动器和板端现有 Panda 环境提供。

## 3. P4A：正常启动

### 3.1 完整哈希只在部署时运行

原启动器每次执行 `sha256sum -c deployment_manifest.sha256`，会读取约 5.72 GB。新行为：

- 普通 `./run_precise.sh`：`[RunIntegrity] mode=quick ... fullConfigHashScan=0`；只验证 ELF/版本记录/必要配置、Xorg、Mali 设备和性能策略。
- 显式 `./run_precise.sh --verify-full`：才执行完整 Config manifest 校验。
- `rk3588_deploy_atomic.ps1`：上传 `Config.new` 后逐文件 SHA-256 校验，全部通过后原子切换。

最终普通启动实测：launcher 快速预检约 58 ms，性能策略和启动器合计约 0.21 s；不包含 Panda 资源加载和 DDS discovery 时间。旧完整扫描单独测得约 32 s，不再位于正常启动路径。

### 3.2 板卡本地 Xorg

启动器从本地 Xorg 进程和授权文件解析显示端，显式覆盖 SSH 环境：

```text
[RunDisplay] inheritedDisplay=:0 effectiveLocalDisplay=:0
authorityPath=/var/run/lightdm/root/:0 xorgPid=666
```

即使 SSH 会话带 X11 转发值，也不继承转发显示。没有使用 `xhost +`，没有读取或打印授权内容。

### 3.3 部署和板卡时钟

完整部署时 tar 曾报告源文件时间戳晚于板卡时间。内容校验仍全部使用 SHA-256；本阶段没有设置系统时间、硬件时间，也没有联网校时。部署工具只抑制该类时间戳警告，真实解包/哈希错误仍会中止部署。

## 4. P4A：冻结与 GL 0x506 根因

### 4.1 0x506

受控 A/B 证明错误只在世界空间体积云深度路径出现。旧路径尝试把有效的 raw-scene 深度 renderbuffer 替换/复制到自动格式深度纹理；RK3588 g6p0 GLES 驱动分别产生不完整 FBO 的 `0x506`，或深度 copy 的 `0x500/0x502`。

修复后体积云 display region 与不透明场景位于同一 `m_stage6RawSceneBuffer`，排序在不透明区之后，直接使用该输出的原生深度测试，不创建第二张深度纹理、不做 CPU 深度读回，也不增加第二次目标渲染。板端证据 `logs/p4-final-runtime/p4a_shared_depth_test.log` 中：Mali-LODX/hardwareGpu=1，`GL_INVALID/0x506` 为 0。

### 4.2 “开始后冻结”

原 CaptureTask 在 `framework.do_frame()` 内部的 Panda task poll 阶段执行，早于本轮 `GraphicsEngine::render_frame`。因此输入序号可能与上一张 RAM 图配对；GPU/FBO 失败时，已有 RAM 图更会被错误标成新帧继续发布。

修复为：

1. 进入 `do_frame()` 前记录最终纹理 image sequence；
2. `do_frame()` 返回后确认本轮图像 sequence 前进；
3. 只在主线程后渲染位置执行 capture；
4. 无效、未前进或缺少 RAM 图时不发布 H.264，并把 VideoStatus 置为非运行；
5. `staleFramePublished` 独立计数。

DDS STOP 和 Realtime 位于不同 Topic，没有跨 Topic 的顺序保证。STOP 现在等待 FIFO 排空并等待至少两个帧周期（下限 50 ms）的 ingress quiet，再结束同步回合，避免最后 1–2 条已接受 Realtime 被 STOP 越过。

## 5. P4A：同步/异步验收

所有同步结果都以同一回合内 `acceptedRealtime == lastCapturedSourceSeq` 为准，不使用输出瞬时 FPS 代替帧守恒。

| 模式 | 输入设置 | 有效输入 | 成功采集/业务视频 | 差值 | 旧帧发布 |
|---|---:|---:|---:|---:|---:|
| Sync | 60 Hz × 10 s | 600 | 600 | 0 | 0 |
| Sync | 20 Hz × 6 s | 119 | 119 | 0 | 0 |
| Sync | 30 Hz × 6 s | 180 | 180 | 0 | 0 |
| Sync + pause | 30 Hz，12 s 中暂停 3 s | 270 | 270 | 0 | 0 |
| Sync final | 60 Hz × 60 s | 3600 | 3600 | 0 | 0 |
| Sync final long | 60 Hz × 600 s | 35999 | 35999 | 0 | 0 |

20 Hz 的实测窗口首尾只产生 119 条输入，但输出仍严格是 119，而不是补成 60/70 FPS。暂停期间不产生新业务帧，恢复后继续按 FIFO 顺序。连续 INIT 建立新 round，前一回合 STOP 排空后才切换；没有旧 round 图像混入。

异步测试单列：480 个唯一输入全部消费，允许按 60 Hz deadline 重复最后状态，产生 527 个输出，其中 48 个是明确记录的 repeated-state output。该结果没有计入同步通过。

最终 60 秒普通 DDS 链：

- DataDriven：`ControlTransport=dds source=production_default`；
- 板端：received/queued/consumed = 3600/3600/3600，覆盖、溢出、序号断裂、backpressure 均为 0；短暂 burst 的最大排队深度 8，结束为 0；
- H.264：MPP `rockchip_mpp_avc`、800×800、Annex-B、SPS/PPS、首帧 IDR；DDS write error/drop = 0；
- VideoDisplay：DDS H.264 sample 3600，FFmpeg 解码错误 0，source sequence 连续；观测窗口 display FPS 58.945–60.378。

最终 10 分钟普通 DDS 长测中，DDS Protocol 接收回调看到了发送端的 36000 条样本；START 与 Realtime 属于不同 Topic，其中第一条 Realtime 在业务 START 生效前到达，因此按既有状态机不是有效业务输入。业务层 accepted/queued/consumed/captured/H.264/DDS/VideoDisplay 均为 35999，数量守恒；覆盖、溢出、序号断裂、旧帧、DDS write/drop 和 H.264 decode error 均为 0。短暂 burst 最大队列深度 8，结束为 0。VideoDisplay 的 297 个约 2 s 统计窗口平均 59.992 FPS，窗口最小/最大 57.933/62.148（短时调度抖动，未形成积压）；发送端 299 个窗口平均 60.001 Hz，最小/最大 59.622/60.432。测试结束后热区约 41.6–42.5 °C，HwaSim_IR RSS 约 350 MiB，没有热降频或队列持续增长证据。

## 6. P4B：CPU/GPU 正式物理分量对齐

| 分量 | CPU 含义 | GPU 正式像素实现 | 大气/合成位置 |
|---|---|---|---|
| MWIR body | `epsilon * Bbar_3-5(Tmaterial)` | 每材料发射率、每像素方向 solarDelta 后计算同一波段平均 | 乘 inbound tau |
| rear hotspot | 局部峰值/参考，不作为整目标面积 | `stage5_rear_mask` 内替换被覆盖机体辐射 | 乘 inbound tau |
| brightspot | strikeFlag/strikePart 控制的局部特殊辐射 | `stage5_bright_mask` 内替换被覆盖机体辐射 | 乘 inbound tau |
| plume | 独立尾焰几何节点 | 独立透明合成 | 不再并入机体一次 |
| active | L2 反射项 | cone、入射角和 band overlap 后的 sensor 项 | source/target 双程 tau 已在 L2 处理 |
| path | MODTRAN SI path thermal/scatter | 正式 M1 sensor 项 | 不再重复天气经验衰减 |

局部热区最终公式采用面积替换，而不是同一面积叠加两个完整表面：

```text
Lsurface = (1-mrear) * Lbody + mrear * Lrear
Lsurface = (1-mbright) * Lsurface + mbright * Lbright
Lsensor  = tau * Lsurface + Lpath + LactiveSensor
```

F35 的 rear/head/mid 区域不重叠。发动机仍只驱动后部 ThermalHotspot/独立 plume；没有恢复整机 `LegacyEngineBodyHeating`。

## 7. 3–5 μm 波段平均

矩形 `responseMode=RectangularBand` 下，CPU 与 GPU 都采用 3.0、3.5、4.0、4.5、5.0 μm 的复合 Simpson 积分，并除以 2 μm 带宽：

```text
Bbar_3-5(T) = [B3 + 4B3.5 + 2B4 + 4B4.5 + B5] / 12
```

独立 QC：

| T/K | 3–5 μm 平均 W/(m²·sr·μm) | 4 μm 单点 | 平均/单点 |
|---:|---:|---:|---:|
| 250 | 0.108486933 | 0.065630553 | 1.652994 |
| 300 | 0.932586978 | 0.721985926 | 1.291697 |
| 500 | 83.778744709 | 87.436511469 | 0.958167 |
| 1000 | 3252.813378591 | 3277.674976098 | 0.992415 |

这说明常温目标继续混用 4 μm 单点会产生明显系统误差。自动检查见 `logs/p4_mwir_band_radiance_qc/mwir_3_5_band_mean.csv`。

## 8. 统一辐射和显示尺度

### 8.1 中间缓冲与最终量化

旧 raw scene 为 unsigned 8-bit RGB，Stage5 辐亮度在 MTF/AGC/极性前已被截断和量化。现在 raw scene 请求并在真实 Mali 上确认：

```text
[Stage6 RawRadianceBuffer] requested=RGB16F actualFloat=1
actualRgbBits=(16,16,16) finalTransport=H264_8bit
```

仅最终传感器显示和 H.264 使用 8-bit。正式 M1 shader 在进入公共显示映射前不做上限 1.0 clamp。

### 8.2 公共显示范围

第一版固定、可审计范围：

- NIR：0–350 W/(m²·sr·μm)；
- MWIR：0–2.5 W/(m²·sr·μm)。

MWIR 2.5 的工程依据是约 330 K 黑体在 3–5 μm 的矩形波段平均约 2.524 W/(m²·sr·μm)，不是针对 F35 的单目标增益。天空、地面、云和目标使用同一范围。当前 MWIR 环境输入为明确配置的工程代理：天空 255 K/epsilon 0.98、地面 288 K/epsilon 0.95、云 emissivity 0.98；这些不是实测数据。

正式 M1 目标已经包含 MODTRAN tau/path，因此跳过旧的 Stage7 fog/contrast 二次衰减；云的几何遮挡和独立体积合成仍保留。天空/地面 CPU 更新也不再预先做一次 fog mix，避免重复天气衰减。

## 9. F35 完整样例与数据来源

实际资源：

- 几何/UV/顶点法线：`Config/TargetLib/models/f35/F35C.obj`；板端实测模型局部边界 min `(-6.5322,-8.549,0.008)`、max `(6.5322,8.549,3.786)`。
- 可见纹理：`f35c.jpg`；正式 NIR 反射率没有直接使用 visible texture luma。
- 材质编号纹理：`f35c_mat.tif`，作为数据纹理采样；对应 `f35c_mat.tif.xml/.attr`。
- 波段光学：`Config/Materials/MaterialBandOptics.csv` 当前没有本地可追溯的实测 NIR/MWIR reflectance，明确走 solar absorptivity/thermal emissivity fallback，不虚构 band_database 数值。
- NIR：每像素顶点法线、太阳方向、材料编号和 band reflectance 参与正式辐射；图中机背/机腹和机翼差异来自几何与材料链，不是 RGB 灯颜色。
- MWIR：每材料发射率、6 向太阳热状态、局部 rear/head/mid mask；F35 热区位置和尺寸从模型几何边界推导，JSON 已写明 `geometry-derived engineering regions, not measured thermal data`。

当前 F35 OBJ/材质资源能形成完整可运行样例，但没有新增或声称拥有实测 NIR 光谱纹理、实测空间温度图或真实 SRF。材质编号图的覆盖准确性仍需要权威模型数据复核。

## 10. 本机 Ondulus 与论文原文核查

实际打开的本机 Ondulus 图片：

- `D:/HwaSimIR/ondulus ir 红外图片示例/近红外-f35.png`
- `近红外-f35-噪声.png`
- `近红外-f35-片状云.png`
- `中波-f35.png`

它们只用于比较 NIR 法线/反射层次、MWIR 局部热结构和云层表现，不是本项目输出，也没有被用作数值真值。

实际读取并渲染 `基于三维场景的红外成像仿真系统及实现_李晨阳.pdf` 第 82–87 PDF 页：

- PDF 82 页（论文页 68）：0.7–1.1 μm 近红外以太阳反射为主，3–5 μm 中波同时包含反射和自身热辐射；
- PDF 83 页（论文页 69），图 5.5/表 5.8：论文特定场景给出 MWIR 太阳反射与热辐射比例，仅作为该研究场景结果，不能当成本项目 F35 实测；
- PDF 84 页（论文页 70）：同一波段图像采用公共线性归一化；
- PDF 85 页：MWIR 随方位/时刻呈现方向性变化；
- PDF 87 页：能见度变化图。

## 11. 实际 GPU 分量图、最终图与 DDS 图

以下均为真实 RK3588 Mali 渲染、MPP H.264 编码、DDS 发送、Windows FFmpeg 解码后保存的 800×800 PNG，不是合成示意图：

- 修复前/旧公共尺度对照：[`after_mwir_geometry_frame120.png`](../logs/p4b_visual_mwir_close/after_mwir_geometry_frame120.png)
- GPU 纯 BrightSpot 分量：[`gpu_brightspot_fixed_head1_frame120.png`](../logs/p4b_visual_mwir_close/gpu_brightspot_fixed_head1_frame120.png)
- GPU rear hotspot 后视分量：[`gpu_rear_hotspot_rearview_frame120.png`](../logs/p4b_visual_mwir_close/gpu_rear_hotspot_rearview_frame120.png)
- MWIR 后视最终图（独立尾部热区）：[`after_mwir_final_rear_engine_frame120.png`](../logs/p4b_visual_mwir_close/after_mwir_final_rear_engine_frame120.png)
- NIR 日间最终图：[`after_nir_daylight_current_frame120.png`](../logs/p4b_visual_mwir_close/after_nir_daylight_current_frame120.png)
- 最终普通 DDS 接收图（局部面积替换版）：[`dds_mwir_area_replace_frame120.png`](../logs/p4-final-runtime/dds_mwir_area_replace_frame120.png)

局部面积替换版最终 DDS 图的全图白饱和比例为 0.000623438，目标 ROI 白饱和比例为 0.004862533；不是整图白化。NIR 图全图和 ROI 白饱和比例均为 0。完整统计在 `logs/p4-final-runtime/p4_final_image_metrics.csv`。

## 12. 构建、部署与版本

- Windows `HwaSim_IR` Release x64：PASS。
- Windows `HwaSim_IR_VideoDisplay` Release x64（FFmpeg H.264）：PASS。
- DataDrivenTestQT Release：本轮参数扩展后的程序已用于 DDS 实机测试。
- aarch64 Release：在 `192.168.203.128` 实际 CMake 源树重新编译并链接，未用 VM 冒充 RK3588 运行。
- RK3588：`192.168.1.116`，Mali-LODX、hardwareGpu=1、MPP PASS。

最终原子部署身份：

```text
Git commit: 6b4b133167c1bc31874916cf328078c4d52ad55f (working tree dirty)
AArch64 ELF SHA-256: 8c99307361b9ac685203ce8fc09ee590305d052ddeb0c787b235cd1a7e9ca19b
Build ID: 72fd2a09685dfc889ef55f8f11d9d1dc85c9c86a
Runtime INI SHA-256: a01798395b5a84c9e3a70cf43eadcbda55dbf67ddc65ff7ff7fd33079d37e1d29
Config manifest SHA-256: 54c9a7323e01686972525b9c27c7ff574aebe7c2002714dd658d27c959c6149e
```

部署包括完整 `HwaSim_IR/Bin/Config`、ELF、`run_precise.sh` 和性能策略。先上传 `Config.new`，检查必需文件与清单，才切换正式目录；失败不会留下半部署。部署时因空间不足曾安全停机，随后只删除 3 组明确命名的旧 Config 压缩备份，保留当前 Config 和较新回滚包。

## 13. 正常用户启动方式

板端：

```sh
cd /userdata/HwaSimIR
./run_precise.sh
```

正常启动不需要手工设置 DISPLAY、XAUTHORITY、CPU/GPU/DMC 频率或执行 Config 哈希。只有部署审计时显式使用：

```sh
./run_precise.sh --verify-full
```

Windows 端直接运行 `DataDrivenTestQT.exe` 和 `HwaSim_IR_VideoDisplay.exe`，缺省分别为 DDS 控制和 DDS 视频；测试工具的 sensor/rate/scenario 参数不改变默认 transport。

最终还执行了两个 Windows 程序均不带任何参数的“普通双击等价”smoke：两个 GUI 都正常响应；DataDriven 打印 `ControlTransport=dds source=production_default`，VideoDisplay 打印 `VideoInput Transport=dds`，DDS runtime 各初始化一次，没有静默回退 UDP/TCP。

## 14. 最终生产开关

保持 P3 已通过的生产状态：M1 NIR/MWIR runtime、NaturalSolar optical/thermal、ActiveIlluminator 软件支持均开启；主动照明的实际出光仍由实时 `WeaponState.illuminatorEn` 控制。DDS protocol/video、OrderedQueue、TCP legacy payload off、Stage6Diagnostics=false 保持不变。

## 15. 已知限制

- SRF 仍为明确标记的 `RectangularBand`，未实现真实高保真 SRF/QE/electron detector。
- 没有全地形/建筑物深度阴影；当前光学/热/主动 visibility 保留现有目标级能力。
- F35 band reflectance 和局部热区是有来源标记的工程 fallback/几何样例，不是材料实测或 Ondulus 导出数据。
- NIR 天空/地面背景仍包含工程环境代理；MWIR 已统一到配置温度/发射率和公共辐射窗口。
- 最终 H.264 仍是 8-bit；RGB16F 保存的是量化前的渲染余量，尚未新增浮点科学数据输出协议。
- 本阶段未扩展 VIS/SWIR/LWIR，没有新增通信字段，也没有实现完整探测器电子模型。
