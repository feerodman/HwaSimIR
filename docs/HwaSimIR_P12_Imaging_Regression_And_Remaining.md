# HwaSimIR P12 成像回归与遗留项

## 判定规则

- `FIXED_WITH_FINAL_EVIDENCE`：最终候选在真实 RK3588→DDS→Windows 解码/GUI 链有当前证据。
- `REGRESSION`：P11 已有最终证据，P12 没有改写该物理实现，并以受影响的当前单元或端到端链回归。
- `UNIT_ONLY`：只有数值/组件门，不声称实际图像效果已单独证明。
- `HISTORICAL_FAILURE`：保留的旧失败，不进入 DDS-only PASS。
- `NOT_VERIFIED_CALIBRATION`：工程链可工作，但没有可追溯真实材料/器件测量。

## P11 成像问题核销

| 项目 | P12 状态 | 当前结论与证据 |
|---|---|---|
| SWIR 带积分、反射单位、tau→0、M1 门 | `REGRESSION` | P11 independent reference 194 checks、正式 MODTRAN/M1 契约继续有效；P12 没有把 Stage3 扩展成 path/sky/solar，也没有改写物理公式。最终 Stage3/Stage4 strict checks 复核。 |
| Windows RGBA32F / RK RGBA16F | `FIXED_WITH_FINAL_EVIDENCE` | P12 板端日志逐例确认 RGBA16F/half-float；SWIR/MWIR DDS 图、玻璃 8 例、天气 12 例和性能长测均走实际 RK 输出。Windows RGBA32F 合同未降级。 |
| `Failed to convert image`、裁切、方向、黑角 | `REGRESSION` | 16 个模型视图与天气/玻璃图未见全零或旧转换错误；保留 P10/P11 精确裁切与 AA 证据，没有靠关天气或裁分辨率掩盖。 |
| 低对比 AGC 与固定映射 | `REGRESSION` | P11 fixed/AGC/annotated 原始证据继续保留；P12 普通显示、天气和玻璃均实际解码。没有把 AGC 图当作辐射数值证明。 |
| 纹理路径、Linux 大小写、材质 ID | `FIXED_WITH_FINAL_EVIDENCE` | `logs/p12/p12b/assets/audit.json` 与 16 个 RK DDS 模型视图通过；原模型没有覆盖，使用可追溯派生/合成映射。AIM9X 既有非简单多边形三角化警告仍明确保留。 |
| 标注坐标与身份 | `FIXED_WITH_FINAL_EVIDENCE` | cache/prewarm 使用 `targetType+targetPlatID+targetID`；`f22_mwir_annotation_final` 实际普通 DDS 图通过。未把解码数冒充呈现数。 |
| 尾焰、局部热源、气动局部性、热惯性 | `REGRESSION` | P12 的 active/aero/thermal 单元回归通过，P11 最终 DDS 效果图保留。没有接入 damage 模型，没有 `engineState→BrightSpot` 或 `strikeFlag→rear ThermalHotspot`。 |
| 主动照明 | `REGRESSION` | P12 active illumination unit 通过；现有 P11 真实链图保留。没有为了肉眼明显而任意增益。 |
| 太阳角、能见度、高度/速度 | `REGRESSION` | P11 有效单因素图与 raw/fixed/AGC/annotation/frame identity 继续保留；P12 2 km 数据补齐和普通 1 km 性能输入通过覆盖预检。模型高度/速度图只用于位姿/投影，不冒称具体装备速度—热特征。 |
| 云、雨、雪 | `FIXED_WITH_FINAL_EVIDENCE` | P12 真实 LOS 云前/后、单 ID hide、rain/snow on/off 均有线性图和 received DDS 像素差；区分 Batch draw 与 Overlay 日志。 |
| 受控玻璃 | `FIXED_WITH_FINAL_EVIDENCE` | 板端实际 RGBA16F、SWIR/MWIR、明暗背景、transmission on/blocked 共 8/8 PASS；仍为单层直透工程模型。 |
| 真实目标/传感器标定 | `NOT_VERIFIED_CALIBRATION` | 没有真实样片光谱、温度、器件 SRF/QE、光学与曝光真值，不宣称真实装备或传感器精度。 |
| 旧 Windows UDP/TCP 矩阵 | `HISTORICAL_FAILURE` | 原失败、缺失和同源图片证据保留；P12 DDS-only 没有把它们改成 PASS，也没有重跑无关大矩阵。 |

## 四个 models 资产

`logs/p12/p12b/asset_views/audit.json` 的证据类别为 `real RK3588 H264 DDS receiver decode`，解释限定为 `synthetic normal diagnostic for geometry, pose and display only`。矩阵为：

| 资产 | SWIR near/side | MWIR near/side | 结果 |
|---|---|---|---|
| aim120 | 2 个独立 received 图 | 2 个独立 received 图 | PASS |
| aim9x | 2 个独立 received 图 | 2 个独立 received 图 | PASS |
| f22 | 2 个独立 received 图 | 2 个独立 received 图 | PASS |
| f35 | 2 个独立 received 图 | 2 个独立 received 图 | PASS |

共 16/16 案例；每例 `ddsErrors=0`、`decodeErrors=0`、`statusIdentityRejected=0`。索引和全部独立图片位于：

- `logs/p12/p12b/asset_views/index.html`
- `logs/p12/p12b/asset_views/images/`
- `logs/p12/p12b/asset_views/contact_sheet.png`

![四模型双波段/双视角 received DDS 拼图](../logs/p12/p12b/asset_views/contact_sheet.png)

这些名称只定位用户已有资产。P12 所用温度、材料和法线显示是通用/合成回归参数，不代表 aim120、aim9x、f22、f35 的真实红外特征。

## 天气与云几何

P11 的 500 m 观察案例与 9400–10600 m 云高不相交，不能证明云显示。P12 fixture 把相机放在 2600 m，实际运行返回两个稳定对象：

- `198EE8338358C182`，LOS 实际进入/离开约 5109–6051 m；
- `D73EEA5ECB3EA0B8`，LOS 实际进入/离开约 3140–3887 m。

目标样距 1000 m 时云在目标后，6500 m 时云在相机与目标之间。单独隐藏前者/后者分别改变 136785/124421 个线性像素；MWIR rain on/off 改变 12607 像素，snow on/off 改变 59793 像素。SWIR/MWIR 的 1000 m 与 6500 m received 图均有超过 105000 个变化像素。

- 结构化结果：`logs/p12/p12c/weather_cloud/audit.json`
- 离线索引：`logs/p12/p12c/weather_cloud/index.html`

![云雨雪 received DDS 拼图](../logs/p12/p12c/weather_cloud/contact_sheet_received_dds.png)

这证明真实绘制对象、几何覆盖和传输像素贡献，不证明实测云光学标定。雨雪的 `particles/draws` 与 `Overlay active` 是不同路径，未再用 Overlay=0 否定 Batch draw。

## 2 km MODTRAN 数据

P12 使用本机已许可 PcModWin5/MODTRAN5，按完整环境/组件契约生成 160 个真实工况；`qc_results.json` 的 201 个检查全部通过。正式发布：

- 发布前 `band_lut_si.csv`：1628 行；
- 新增正式 2 km 行：96 行；
- 发布后：1724 行；
- 发布后 SHA-256：`6f22d25d32bc00e753b454560af630d09c2e1a130d53835fa505e27d97f5fd99`；
- 原文件字节为新文件的精确前缀，发布为原子追加；
- 输入卡、MODOUT、组件运行、列/单位/Jacobian、带响应积分和 QC 保存在 `logs/p12/p12c/modtran_2km/`。

运行时只使用正式 `band_lut_si.csv` 和既有 `solar_heating_lut_si.csv`；不把巨大光谱审计 CSV 当成日常依赖。没有夹到 1 km、改名 NIR、外推或填零。

## 受控玻璃

P12 使用普通受控玻璃样件、已知前后背景做：

- SWIR/MWIR；
- bright/dark background；
- transmission on/blocked。

8/8 个板端案例都实际记录 RGBA16F，走 H.264 DDS 并由 Windows 解码；六组对比均通过。证据：

- `logs/p12/p12c/controlled_glass/evidence/audit.json`
- `logs/p12/p12c/controlled_glass/evidence/index.html`

![板端受控玻璃 received DDS 拼图](../logs/p12/p12c/controlled_glass/evidence/contact_sheet_received_dds.png)

单层直透仍是工程近似；没有扩展折射、Fresnel 或内部多次反射，也没有声称真实样片标定。

## 手动 INIT 与正式域

旧 `1.txt` 的 observer altitude 为 11000.04785 m，超出本轮正式普通 SWIR/MWIR 轨迹覆盖。`logs/p12/p12c/manual_init_coverage` 证明：

- 超域 SWIR/MWIR 在 INIT 前 fail-closed，并在控制 UI 显示具体错误；
- 有效 MWIR 1 km 输入能正常 INIT、START 并显示；
- 不静默 clamp、不回退成黑图。

日常使用建议选择随交付包提供的 `ordinary_demo_1km.txt`，或先扩充正式 LUT 后再使用其它轨迹。

## 仍未关闭

1. `NOT_VERIFIED_CALIBRATION`：真实目标/材料/传感器标定未提供。
2. `PERFORMANCE_FAIL`：SWIR 60 s 冷启动还有 3 帧超过原始 80 ms 硬门。
3. 人手逐键 RESET/INIT/START/STOP 没有由当前自动化面实际点击；普通入口、窗口和图像链已经实测。

这些项不会通过修改验收口径、删除 blocker 或把历史测试改名为 PASS 来关闭。
