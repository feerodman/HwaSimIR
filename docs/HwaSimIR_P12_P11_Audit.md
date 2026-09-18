# P11 交付复核与 P12 修复依据

复核日期：2026-09-17。范围：用户上传 ZIP 的结构化结果、抽样原始截图、逐帧计时、实际运行日志、LUT，以及 GitHub main 上显示端的启动与订阅源码。本复核没有在用户电脑或 RK3588 上重新启动程序；“源码确认的条件性故障”和“用户当前运行的实际根因”分别表述。

## 1. 证据身份与结论

上传文件：`HwaSimIR_P11_Delivery.zip`。

- 本次重算 SHA-256：`ce87733562d11ae9668e287567716dce1584bb7ceee4eab29be813107a1d179c`，与用户提供值相等。
- ZIP 条目数：4044。本次没有重跑完整 CRC 检查，不将用户的 CRC 结论写成本次独立检验。
- 最终板端 stage：`p11-20260917-000705`。
- ELF：`3a616afed1fc2d0984ce6d498e96000a8b9848bed670475b5b9fdd8297585862`。
- config manifest：`efa7aa0b4b068b53e8640a4bbbccfc300dc672cfe2c2ce07814024af4d2c5ac5`。

P11 已有真实的公式/CPU/WGL 数值修复及 RK3588 DDS/MPP/解码、重初始化、回滚证据。下一阶段不应推倒这些结果。需要补的是普通启动可用性、实际效果覆盖、缺失数据和元数据，以及与抓图诊断分离后的性能验收。

本文 ZIP 内路径均省略首级 `HwaSimIR_P11_Delivery/`。

## 2. A01：自动化通过不等于普通启动通过

### 已确认的路径差异

`bundle/tools/p11_rk3588_band_acceptance.ps1` 的接收端参数显式包含：

```text
--receive-transport=dds
--stream-role=direct
--plat-id=1001
--sensor-id=2
--dds-domain=150
--dds-topic=HwaSimIR.Video.1001.2.H264
--dds-codec=h264
--dds-width=800
--dds-height=800
--dds-fps=60
--dds-qos=<明确的 Windows QoS 绝对路径>
```

脚本还指定工作目录、测试环境变量和退出时刻；显示程序通过 `-WindowStyle Hidden` 启动。输入端使用生成的测试 INI，并明确指定 DDS、ID 和波段。因此这些证据证明指定路径收发和解码，不证明用户无参数启动时的自动发现，也不证明窗口实际完成图像绘制。

### 当前源码中可复现的条件性故障

文件：`HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay.cpp`，本次读取前 300 行。

1. 未指定配置文件时，只读取 EXE 同目录的 `NetworkConfig.ini`。
2. transport 默认是 `dds`，不是 TCP；stream role 默认是 `direct`。
3. 缺少有效 Identity 时，`platID=0`、`sensorID=0`。
4. 缺省视频 Topic 是 `HwaSimIR.Video.precise.H264`。
5. 普通无参数启动可以启用 `autoFromVideoStatus`；显式传入 topic、codec、width、height 或 fps 的自动化路径会关闭它。

文件：`HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/DdsVideoReceiverWorker.cpp`，`DdsVideoStatusListener`。

```cpp
if (m_config.platID >= 0 && sample.platID != m_config.platID) return;
if (m_config.sensorID >= 0 && sample.sensorID != m_config.sensorID) return;
```

若普通运行实际加载的是缺失/旧 INI，显示端以 0/0 等待，而仿真端广播 1001/2，则状态会被拒绝，无法依赖状态改订阅正确 Topic。这是明确的条件性故障机制；用户当次无图是否正由此触发，还要读取当次三端有效配置和日志。

运行手册又将板端正式 Topic 写成 `HwaSimIR.Video.precise.H264`，而正式矩阵 summary 记录 `HwaSimIR.Video.1001.2.H264`，存在文档不一致。但仅有 Topic 名称不同不足以认定无图根因，因为正常状态发现原本可能完成重绑定。

ZIP 的 `bundle/bin` 携带三端 EXE，但没有证明两端 Qt 的 EXE 同目录 `NetworkConfig.ini` 和依赖也作为普通启动包完整交付；发现的 NetworkConfig 是证据目录里的测试文件。这不等于用户机器上必然没有 INI。

### 修复要求

先记录实际 EXE 路径、哈希、工作目录、所读 INI、DDS/QoS 文件、ID、Topic/状态 Topic、解码库；区分“未收到状态”“ID 被过滤”“没有视频样本”“不能解码”“GUI 未绘制”“上游物理条件被拒绝”。不得通过移除身份过滤或订阅任意流掩盖问题。

## 3. A02：延时长尾与诊断抓图强相关

依据：

- `artifacts/rk3588/dds_dual_band/{SWIR,MWIR}/frame_latency_ms.csv`
- 同轮 `board/input_audit/stage_render_*.csv`、`stage_output_*.csv`
- `bundle/tools/p11_rk3588_acceptance_analyze.py`

### 统计口径

分析器从输入受理 `beginNs` 到板端 output 阶段 `steadyNs` 相减，使用 RK3588 同一 steady clock。因此这不是 Windows 发送端到显示窗口的跨机端到端延时。

所报约 60 FPS 使用截去头尾后的输出区间；这两轮从索引 300 起统计，SWIR 结束索引 3542、MWIR 3540。必须保留这一语义，不称为包含全部启动帧的平均 FPS，更不称为 GUI 显示 FPS。

80 ms 是 P11 分析器报告阈值，并非其 overall PASS 的“全帧必须低于 80 ms”判据。

### 逐帧复核

| 指标 | SWIR | MWIR |
|---|---:|---:|
| 全部匹配样本 | 3602 | 3600 |
| >80 ms | 127（3.526%） | 90（2.500%） |
| 最大板端区间延时 | 292.916 ms | 276.723 ms |
| 首帧 doFrameMs | 224.605 | 218.699 |
| 第二帧 doFrameMs | 66.4519 | 57.4414 |

PFM 采集选择的帧恰为 sourceSeq 180、900、1800、2700、3540。这些帧均处于启动后的延时尖峰起点：

| sourceSeq | SWIR captureTaskMs | MWIR captureTaskMs |
|---:|---:|---:|
| 180 | 101.9100 | 93.3453 |
| 900 | 95.3911 | 95.3849 |
| 1800 | 86.0358 | 95.8566 |
| 2700 | 95.2811 | 96.2649 |
| 3540 | 94.8512 | 95.6865 |

邻近普通帧 179/181 的 captureTaskMs 约 0.64–0.66 ms。采集帧 doFrameMs 约 10–11 ms，而采集任务本身大幅变慢。该证据将“启动开销”和“诊断采集阻塞”列为优先定位对象，但尚不能区分采集任务内的 GPU 同步、CPU 复制、PFM 序列化和磁盘写入各占多少；不能直接断言全是硬盘慢或着色器编译。

SWIR >80 ms 连续区间：1–66、180–194、196–198、201–202、900–911、1800–1810、2700–2710、3540–3546。MWIR：1–45、180–187、900–906、908、1800–1809、2700–2707、3540–3550。

矩阵 2509/36046 = 6.961% 超过 80 ms；不能不经逐轮验证就断言矩阵的全部长尾也均为同一原因。

### P12 对策

分开做无 PFM/详细诊断的普通运行测量与打开诊断的采集开销测量。细拆抓取耗时；安全可行时将 CPU 序列化/写盘移入有界队列，不在无 GL context 的线程盲目访问纹理。不得丢弃已接受输入、改 Latest 或重复旧图保帧率。首次资源准备可在 READY 前完成，但 READY 用时、首个真实输入后的延时均须报告。

## 4. A03：天气案例缺少实际可见效果证据

代表性图片路径：

`artifacts/rk3588/dds_image_matrix/{SWIR,MWIR}/{cloud,rain,snow}/auto_clean.png`

抽查图像里主要是渐变背景与小目标，未看见明显的云团/雨雪粒子。单张图不能证明模块失效，但不能仅靠案例名称、envSky 或非黑像素就声称天气已经验收。

### 云的具体几何不匹配

`MWIR/cloud/case_request.json`：观察者和目标均在 500 m，距离 0.5 km。

同 case 的 AGC 实际 `board/hwa.log`：

```text
cloudBaseAltitudeM=10500
altitudeM=9400-10600
radiusZM=180-500
sensorFovDeg=9.167325
EnableFrustumCull/相应运行开关开启
```

这说明低空、同高、窄视场案例沿用了高空云体配置，不能证明云位于实际视线内。优先修测试场景几何与投影覆盖元数据，而不是重写云体算法或全屏加亮。

### 雨雪的诊断歧义

同轮可见：

```text
[PrecipitationBatch] particles=128 draws=1 vertices=512 ...
[Stage7 Precipitation] enabled=1 type=rain/snow ...
[Stage7 PrecipitationOverlay] mode=Batch active=0 ...
```

在 Batch 模式下 Overlay 的 active=0 不能单独解释为真实粒子批次没有绘制。需核对实际批次位置、深度、alpha、投影覆盖和后处理前后的像素贡献。

### “20 类/60 变体”不等于单因素对照完整

代表矩阵选用 `exhaust_plume_on`、`active_in_band_on`、`solar_az_180_el_45`、`speed_static_15mps` 等单个案例，并非每个 comparison group 均有基线/关闭/另一角度/另一速度。三个 fixed/AGC/annotated 运行是显示变体，不能替代同条件的效果 on/off。应保留既有传输和身份 PASS，同时另列实际效果、物理一致性和缺失对照状态。

## 5. A04：2 km 是视线路径距离，且需完整多维数据契约

LUT：`bundle/data/HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv`。

本次直接检查共 1628 行：NIR 1005、SWIR 144、MWIR 479。

| 子集 | 条目 | 观察/目标高度坐标（km） | 距离坐标（km） | 备注 |
|---|---:|---|---|---|
| SWIR 新完整列条目 | 144 | 0.001、1 | 0.1、0.5、1 | 具太阳角、直射、天空与路径散射字段 |
| MWIR 新完整列条目 | 144 | 0.001、1 | 0.1、0.5、1 | 与新受控低空场景对应 |
| MWIR 旧条目 | 335 | 含 3、5、10、15、20 | 含 1、2、5、10、20、35、50 | 缺新完整链要求的部分太阳角/直射/天空/路径散射字段 |

这里列出的是坐标取值集合，不宣称任意组合都存在或插值都合法；最终须调用真实查询器审计。

因此“整个 MWIR 文件没有 2 km”不准确；准确说法是 P11 请求所需的新完整链 2 km 工况缺失，旧 2 km 不自动满足相同高度、湿度、太阳角及字段契约。

普通手动输入若仍为旧高空/远距离工况，即使 DDS 修好了，也可能碰到正式物理查询 fail-closed。必须先输出缺失维度和有效覆盖，不得夹到 1 km、拿 NIR 冒充、填零冒充或任意外推。

P12 只补实际所需的完整工况，使用真实 MODTRAN 输出和现有可用脚本；已通过的 LUT 与宽谱太阳加热表不整体重做。

## 6. P11 问题应分四类处理

| 类别 | 判断 | P12 工作 |
|---|---|---|
| 公式/单位/近零 tau/profile/浮点附件 | 有针对性通过证据 | 作为回归基线，不推翻 |
| 普通启动、GUI 显示、效果单因素与天气可见性 | 自动化代表矩阵未充分覆盖 | 复现、修复、增加对应证据 |
| 云几何/ID 元数据、2 km 数据、板端受控玻璃 | 明确缺口 | 实现元数据、补真实数据、补小规模 DDS 样片测试 |
| 真实目标与器件标定、缺失的原始 before 图 | 不能由合成图补齐真实性 | 标定另列；重建基线明确写“重建”，不冒称原始截图 |

阶段总体建议：P12 普通启动与成像回归收口。目标目录里的模型用于通用几何、材质绑定、位姿与显示回归；真实辐射校验仍用已知参数的普通样件/民用模型，不把通用合成材料变成具体装备的真实特征声明。

## 7. 仓库证据位置

- 根因报告：`docs/HwaSimIR_P11_IR_Physics_RootCause_And_Fix.md`
- 运行手册：`docs/HwaSimIR_P11_Runbook.md`
- 接收端配置：`HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay.cpp`
- 状态身份过滤：`HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/DdsVideoReceiverWorker.cpp`
- 显式测试参数：`tools/p11_rk3588_band_acceptance.ps1`
- FPS/延时统计：`tools/p11_rk3588_acceptance_analyze.py`
- 仓库： https://github.com/feerodman/HwaSimIR

所有源码条件需在 Codex 实际工作区再次确认，避免把读取时的 main 与随后本地改动混同。

## 附录：本次复核重点源文件身份

以下为上传 ZIP 中原始字节的 SHA-256，不是重写后的文件身份。

- `artifacts/rk3588/dds_dual_band/MWIR/board/input_audit/stage_render_aafa079e8b42451238277d3b6107248f.csv`
  - 172629 bytes；SHA-256 `ccb386a9a4dbfefe5cdbaa4b2490c47e651291522a8ea9cc0b63c0a86d316a24`
- `artifacts/rk3588/dds_dual_band/MWIR/frame_latency_ms.csv`
  - 179030 bytes；SHA-256 `4a500ddec7eb7b7f334834d43b4d55337d8e7396b461ef1e45443541767ffb44`
- `artifacts/rk3588/dds_dual_band/SWIR/board/input_audit/stage_render_d4f93cb99d298200051d036b897419dd.csv`
  - 172913 bytes；SHA-256 `ba40ba8b191e908723a8cbc47d5b4de2ad8680e30dd890759b8048dd853350bc`
- `artifacts/rk3588/dds_dual_band/SWIR/frame_latency_ms.csv`
  - 179156 bytes；SHA-256 `cfd073f3dd6ffc2578c77582af728116eccd7373874c2e8675a7811f19e353b8`
- `artifacts/rk3588/dds_image_matrix/dds_image_matrix_summary.json`
  - 23880 bytes；SHA-256 `032a05f9c1a6251e4547f1854b79987e489f08bc7b61486587f022a1c5fb7ed7`
- `artifacts/rk3588/dds_image_matrix/MWIR/cloud/auto_clean.png`
  - 5091 bytes；SHA-256 `227b00e7ef860a387518e4704156d33b200b9fbc55a9669329c60373816743fc`
- `artifacts/rk3588/dds_image_matrix/MWIR/cloud/case_request.json`
  - 6129 bytes；SHA-256 `35710ccbb89ed398aaae7fa86c5a73b947ca40cea27340ce073b1f50edb38098`
- `artifacts/rk3588/dds_image_matrix/MWIR/cloud/variants/agc/acceptance/MWIR/board/hwa.log`
  - 669171 bytes；SHA-256 `1d137164043709d90fb9f80692956907db29da17832f66447b925a501cd3720a`
- `artifacts/rk3588/dds_image_matrix/MWIR/rain/auto_clean.png`
  - 5625 bytes；SHA-256 `0152493e4c8877c266964c272344010b51413f2aa31907daa02e24dfed1eb275`
- `artifacts/rk3588/dds_image_matrix/MWIR/rain/variants/agc/acceptance/MWIR/board/hwa.log`
  - 667192 bytes；SHA-256 `e56b431cb1f6ec8f1a03554d7074bf0517d43800a5d7b10507c12b3e6d67f725`
- `artifacts/rk3588/dds_image_matrix/MWIR/snow/auto_clean.png`
  - 5625 bytes；SHA-256 `6f59fd776fa7501290f5396ccfb5e55d2c900f93f049ed455b522c5994677745`
- `artifacts/rk3588/dds_image_matrix/MWIR/snow/variants/agc/acceptance/MWIR/board/hwa.log`
  - 666462 bytes；SHA-256 `ea64770b6ba890ff8e4d4fce86cb634a16f747b69354887f71071ce119615c44`
- `bundle/data/HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv`
  - 1643966 bytes；SHA-256 `feb5f30ee6d4991e019185a3d2e31ae1440041ed0d21964099e0fd1c5433c57e`
- `bundle/tools/p11_rk3588_acceptance_analyze.py`
  - 50305 bytes；SHA-256 `7005cd1dfbaf741ff7af36afec333aed636d2d57935bdab6138570ff8b6e953f`
- `bundle/tools/p11_rk3588_band_acceptance.ps1`
  - 31933 bytes；SHA-256 `68f01f9f5a319e19f3b40980a15e09d9f2bdb9a27837d293bab84e9aa5e4f10c`
