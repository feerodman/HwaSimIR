# HwaSimIR P14 前置复核：首包、可见性、特效量纲与验收缺口

## 0. 复核身份与结论等级

- 本次读取的 GitHub `main` 提交：`61b875a4ca41cefcdce4f2824c354c2649b2d6f0`。
- 上传包：`HwaSimIR_P13_Delivery.zip`，281,246,521 bytes，430 个成员。
- 实际 SHA-256：`4a641502e5a4a735609cf2735dcbb48eae08c429be9e4ae62c82a38345758be6`，与用户提供值相同。
- 下述发现来自当前源码、包内原始 CSV、JSON/JSONL、日志与 PNG；未在本次对话中运行板端程序、MODTRAN 或三端 GUI，也不替代新的 GPU 像素验收。
- P13 板端吞吐、产品守恒、录像和原轨迹查表通过的成果应保留；其 PASS 不覆盖未经测试的外部占位包时序、任意大气范围或所有视觉效果。

## 1. 开头无目标：输入可见性标志已经能解释主要等待

证据输入：`inputs/DataDrivenTestQT/1.txt`。

- SHA-256：`f2c3db00d71012cd28336b5077e03ff8d4ed1eda2698fd900486ae9cdfdd7901`。
- 4318 个数据行，第 54 列为 `ViewValid`。
- 数据行 1–659 的 `ViewValid=0`；第 660 行开始为 1（包含表头时为文件第 661 行）。
- 第 660 行源时间为 6610 ms；相对首行 20 ms 为 6.590 s。
- P13 实际按 60 Hz 逐行发送，因此该状态转换约在发送后 10.983 s；不能把源时间与视频播放时间混为一谈。
- 两个最终原文件案例的 `producer_annotations.jsonl` 均在 `sourceSeq=663`、`ptsMs=11033` 首次出现非空 `targets`，此前 662 条为空。标注首次出现不等于 GPU 第一目标片元，二者需要分开验收。

当前 `DataDrivenTestQT/mainwindow.cpp` 直接读取：

```cpp
data.viewValid = fields[53].toInt() != 0;
// 发送时分别传入 weaponState / targetState 的相关 viewValid。
```

因此不能把开头约 11 s 等待直接判成 MODTRAN 缺远距数据或 GPU 冷启动故障。原始回放应保留此标志。演示需要提前看见时，只能通过明确、留痕的测试端可见性覆盖，不得改原文件或放宽生产程序的协议过滤。

特别注意：前 659 行平台/目标经纬高已有正常数值。`ViewValid=0` 不等于位置无效，不应被拿来延迟空间原点初始化。

## 2. 当前并不保证“第一包有效实时位置决定原点”

当前 `HwaSim_IR/HwaSim_IR/HwaSimIR.cpp` 的 INIT 预热路径包含：

```cpp
const BYHWICD::SpatialState& initCloudSpatial =
    m_initSceneData.platParamInit.spatial;
const bool initCloudSpatialValid =
    std::isfinite(initCloudSpatial.lat) &&
    std::isfinite(initCloudSpatial.lon) &&
    std::isfinite(initCloudSpatial.alt) &&
    initCloudSpatial.lat >= -90.0 && initCloudSpatial.lat <= 90.0 &&
    initCloudSpatial.lon >= -180.0 && initCloudSpatial.lon <= 180.0;
if (initCloudSpatialValid) {
    m_geoTrans.InitReferencePoint(initCloudSpatial.lat,
        initCloudSpatial.lon, initCloudSpatial.alt);
    m_isInitReferencePoint = true;
    // 同时初始化 cloud local frame。
}
```

实时更新中的原点初始化则包在 `if (!m_isInitReferencePoint)` 内。INIT 中满足上述检查的值会提前占用正式原点；全零经纬高也满足这组有限数/范围检查。原点选在别处本身未必让渲染错位，但这明确不满足用户本次指定的首包基准契约，也没有证明外部占位包不会污染场景。

P13 两个最终运行日志还实际记录：

```text
[InitReferencePrewarm] source=protocol_init_platform
sceneReferenceReady=1 cloudFrameReady=1
```

源码注释假设“发送端把第一行填入 INIT”，对 DataDrivenTestQT 的已知轨迹成立，不能推广到外部程序。

建议修复：临时预热上下文与正式空间参考分开；正式参考在本 INIT 世代消费的第一包有效实时平台位置提交。目标位置、viewValid、跟踪有效性、DDS SampleInfo 有效性和平台位置有效性必须拆分。没有独立位置有效位时，对全零占位的识别必须来自明确接口约定；经度 0、纬度 0、海拔 0、速度 0 本身都不是通用无效值。

## 3. 尾焰存在未覆盖的实际 shader 量纲路径

### 3.1 主 shader 修复不能代表实际 sprite shader 已修复

`HwaSimIR.cpp` 正式尾焰更新计算：

```cpp
const float shaderRadiance = formalPlumeRequested
    ? static_cast<float>(cache.formalTau *
                         static_cast<double>(sourceRadianceWm2SrUm))
    : gray;
node.set_shader_input("u_plume_gray", LVecBase2f(shaderRadiance, 0.0f));
```

但实际 sprite 节点在 `IR/IRGameSpriteBatch.h` 中：

```cpp
shader = Shader::make(... read("Config/GameVFX/sprite.vert"),
                         read("Config/GameVFX/sprite.frag"));
node.set_shader(shader, 100);
node.set_transparency(TransparencyAttrib::M_alpha);
```

外置 `HwaSim_IR/Bin/Config/GameVFX/sprite.frag` 仍按旧无量纲灰度处理：

```glsl
float source = clamp(u_plume_gray, 0.0, 1.0) * (smoke ? .65 : 1.0);
// 后续继续 clamp(source,0,1)，叠加旧 fog_gray / contrast / fog_density。
fragColor = vec4(vec3(source), alpha);
```

两个最终普通原文件日志同时出现：

```text
[Stage6 RawRadianceBuffer] requested=RGBA16F_SI
  domain=W_per_m2_sr_um
[GameSpriteResources] ... domain=common_linear
[DisplayEffective] ... field=WhiteHot value=1.000
```

这是一个具体的消费契约冲突：正式输入为 SI 辐亮度，priority=100 的外置特效 shader 却限制到 0–1 并使用经验雾。即使 `HwaSimIR.cpp` 内另一段尾焰 shader 已有 raw-SI 分支，也不能证明这条高优先级实际路径正确。

结论：确认存在必须修复的量纲/路由问题；最终黑色区域中该路径的贡献比例、alpha 与前景 path 的影响，还需隔离 draw 和 raw 像素测试。不得仅凭这段源码声称全部 GPU 根因已证明。

### 3.2 P13 78.20 指标没有验证“变亮”

包内 `tools/p13_plume_pixel_qc.py` 使用 `ImageChops.difference`，即绝对差；`heatToWorstSideControlMeanRatio=78.201720` 仅表示局部绝对变化大于控制区，不能证明辐射增加或图像增亮。

按原 QC 相同 ROI `[320,340,480,480]` 和包内 `images/plume_on.png`、`plume_off.png` 转 L 灰度重算：

| 量 | 实际复算 |
|---|---:|
| On 平均灰度 | 36.322232 / 255 |
| Off 平均灰度 | 37.494464 / 255 |
| On−Off 平均有符号差 | −1.172232 / 255 |
| 平均绝对差 | 2.083750 / 255 |
| 正差像素 | 4862 |
| 负差像素 | 14153 |
| 零差像素 | 3385 |

该统计来自解码 PNG，而非线性辐亮度。也不能只凭负差就认定任何介质变暗都错误：冷吸收烟、比背景暗的介质、黑热显示均可能变暗。P14 应以受控热源温度/背景、明确的发射吸收模型及单调映射为依据，验证带符号的 raw 和显示差。

另外，P13 的受控 on/off 是通用 nozzle fixture；普通 sprite 绑定只用日志检查 visibility/tau。它没有建立同一通用热源跨实际 sprite shader、CPU 参考、raw 像素的定量等价证据。

## 4. 大气覆盖：50 km 已有，但不是全域

当前 P13 新高空完整网格：

- SWIR/MWIR；Mid-Latitude Summer/Rural。
- 观察者高度 10.95–12.0 km、目标高度 9.7–10.05 km。
- 距离顶点 2.4/5/10/20/23/35/50 km。
- RH 顶点 30/60/85；能见度仅 6 km；SZA 顶点 20/45°。
- 原轨迹实际斜距 2.427569473–22.269183094 km，不含 50 km。

不必因同条件下距离达到 50 km 重做已有数据。需要补的是其它能见度、太阳角、高度/斜视工况，以及本轮卡车地面场景的合法查询单元。旧近距/低空数据可审计复用，不能把多个数据子集的轴取并集后声称完整覆盖。

`1.txt` 只能证明该轨迹覆盖；外部 DDS 不知道未来轨迹，应由板端按当前样本查询并报告缺轴。原文件哈希是测试血缘，不应成为外部发送者的业务依赖。保留数据 manifest 校验，不要求外部程序携带/知道该文件。

## 5. 目标类型 UI 已有基础，应该改造而不是重复添加

当前 `DataDrivenTestQT/mainwindow.cpp`：

```cpp
data.targetNumValid = 1 /*5*/;
data.targetState[0].targetType = m_targetType;
```

`setupUI()` 已创建“文件数据与身份 · 位置来自输入文件”分组内的“目标类型”十六进制 `QLineEdit`，初值来自 `Demo/TargetType`。`sendInitCommand()` 读取并校验，INIT 时冻结。

P14 应改成普通用户可见的“名称 + 十六进制码”下拉框，并保留原字段/配置语义；检查当前发布目录是否旧副本或控件不可见。选择应同时绑定有效目标和相关引用键，不改变 platID/sensorID、原文件及几何。普通生产默认不使用 `P6TestTargetType` 等环境覆盖。民用卡车/车辆作为特效测试载体。

## 6. 未关闭项与证据等级

- 外部 INIT 与首有效实时位置不一致、START 后无包/全零占位、有效后再无效：P13 未提供等价验收，且 INIT 预热逻辑存在契约冲突。
- 原文件早期 viewValid=0：输入预期，不是直接缺陷；UI 应说明等待原因；约 3 帧标注出现差异要区分刷新频率与目标实际首 draw。
- 黑色尾焰：sprite 实际 shader 与 SI 量纲冲突；需要当前版本定量闭环。
- 云/雨部分混合媒体在 P13 中被标为旧证据复用；P14 修改特效链后必须重测受影响媒体，不能借旧图关闭。
- 高空能见度只验证 6 km；其它高度/角度边界未证明全域。
- Qt paint 最大间隔仍存在 >80 ms；不是板端 accepted→writer 延时的同一指标。
- 完整 STOP 约 10 s：分项排空已有记录，不是“控制响应 10 s”；优化须保证录像完成。
- 真实材料/传感器标定保持 `NOT_VERIFIED_CALIBRATION`；民用工程视觉修复可以独立完成。

## 7. 供 Codex 使用的证据路径

以下路径相对 ZIP 顶层 `HwaSimIR_P13_Delivery/`：

- `docs/HwaSimIR_P13_Closeout.md`
- `docs/HwaSimIR_P13_Ordinary_Runbook.md`
- `docs/HwaSimIR_P13_OriginalInput_And_Atmosphere_Coverage.md`
- `docs/HwaSimIR_P13_Image_RootCause_And_Fix.md`
- `inputs/DataDrivenTestQT/1.txt`
- `evidence/cases/final13_original_1_SWIR_Clear/board.log`
- `evidence/cases/final13_original_1_MWIR_Snow/board.log`
- 两案例 `recording/*/producer_annotations.jsonl`、`frame_index.jsonl`
- `evidence/plume/plume_qc.json`
- `tools/p13_plume_pixel_qc.py`
- `images/plume_on.png`、`images/plume_off.png`

复算数值另见 `HwaSimIR_P14_P13_Review_Metrics.json`。

理论依据仅用于通用成像：MODTRAN 官方 spectral-output FAQ；Panda3D 1.10 TransparencyAttrib/Transparency and Blending 文档；PBRT 第 4 版 The Equation of Transfer。不要把本文的通用介质测试等同于具体装备物理标定。
