# HwaSim_IR 红外物理、大气、太阳照明与主动照明剩余工作核查报告与实施方案

> 项目：HwaSimIR  
> 核查对象：`HwaSim_IR` 当前 `main` 分支代码、配置、MODTRAN 数据、TargetLib 模型/材质、项目现有 `.md` 设计记录  
> 日期：2026-09-06  
> 目标：优先把“红外物理量正确、单位一致、能稳定生成可信红外图像”做实，再补太阳自然照明和平台主动照明；不以单纯调灰度/调 shader 参数替代物理链路。

---

## 1. 结论摘要

当前 HwaSim_IR 已经搭出了比较完整的红外仿真框架，但距离 Ondulus IR 一类物理型红外软件的效果差距，**不只是 MODTRAN 数据缺失或错误**，主要有以下几类原因。

| 项目 | 当前状态 | 结论 | 优先级 |
|---|---|---|---|
| MODTRAN LOS 透过率 `tau_up` | 已有 LUT 与 loader，但运行配置 `UseModtranTauForAtmosphere=0` | 当前生产图像没有真正使用 MODTRAN tau | P0 |
| MODTRAN path radiance | 有 raw 与 SI candidate，有 runtime 试验代码，但生产默认关闭 | 当前 raw 表单位与 `IRRadianceModelV2` 的 SI 辐亮度不一致，是此前相差 `1e6` 左右的主要原因 | P0 |
| MODTRAN sky/solar | 数据字段和代码骨架已存在，但生产关闭；sky 数据当前样本还有空值 | 不能直接打开开关，需要先重建/校核数据语义 | P0/P1 |
| MODTRAN LUT 查询 | 当前是离散最近邻，`humidityPercent` 查询参数实际不参与选表 | 不适合最终连续物理仿真，需要规则网格插值和明确湿度轴 | P1 |
| 太阳反射 | 当前是经验项：`reflectance * sunStrength * NdotL * textureLuma * weight` | 量纲不完整，没有真正使用太阳光谱辐照度；只能作为视觉近似 | P1 |
| 太阳热加载 | 当前没有完整“太阳吸收→热惯量→表面温度→MWIR/LWIR 发射”链 | 这是与 Ondulus IR 差距很大的部分 | P1/P2 |
| Panda3D 太阳光源 | 当前代码搜索不到正式 `DirectionalLight` 自然光节点 | 可见光阴影/光照尚未形成正式链路 | P2 |
| 主动照明器 | 协议字段已经存在，但 HwaSim_IR 中未发现 `illuminatorEn` 的正式消费逻辑 | 功能尚未真正实现 | P2 |
| 目标材质 | 已有材质 ID 纹理 + XML + MaterialDatabase 映射 | 架构方向正确，但 F35 等目标材质分类过粗、属性是宽带常数 | P1 |
| 材质运行加载 | 历史实机日志曾显示 `MaterialDatabase.csv` 未加载而回退默认材质 | 必须先确认当前部署工作目录/资源解析是否已修复 | P0 |
| SensorWave | 已读取波段范围、NETD、F/#、焦距、像元尺寸等白名单字段 | 很多 Ondulus profile 字段目前只用于日志/fallback，未进入完整探测器模型 | P2/P3 |
| MTF/噪声/AGC | 已有实现与测试脚本，但生产默认大多关闭 | 功能骨架存在，物理标定未完成 | P3 |
| 与李晨阳论文一致性 | 项目设计文档明确按论文提炼过全链路 | 当前是“论文架构的分阶段简化实现”，不是完整复现 | — |

**最重要的判断：现在不应该先继续调 shader 灰度，也不应该直接把 `UseModtranPathRuntime=true` 打开。**  
应先把 MODTRAN 的单位、数据语义、插值、太阳输入和材质参数统一到同一个物理量体系，然后再启用生产链路。

---

# 2. 当前 MODTRAN 数据到底有什么问题

## 2.1 当前程序为什么一直没有真正用上 MODTRAN

当前 `HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini` 中明确配置：

```ini
[Stage3]
EnableModtranTauDebug=0
UseModtranTauForAtmosphere=0

[Stage5ModtranRadiance]
EnableModtranRadianceDebug=false
UseModtranPathRuntime=false
UseModtranSkyRuntime=false
UseModtranSolarRuntime=false
ModtranPathRuntimeBand=MWIR
ModtranPathRuntimeMode=Off
ModtranPathUnitMode=Native
```

因此当前生产图像仍主要使用 legacy/经验大气链路。  
这不是“程序没找到 MODTRAN 文件”这么简单，而是**配置本来就禁止生产使用**。

项目自己的 `lut_readiness_report.md` 和 `final_a_line_readiness.md` 也曾明确写过：

- `tau_only_debug_ready = YES`
- `cpp_tau_only_loader_allowed = YES`
- `radiance_si_candidate_ready = NUMERIC_REVIEW_ONLY`
- `cpp_radiance_integration_allowed = NO`

也就是说，以前阶段就是有意只让 tau 进入 debug，path/sky/solar 暂不进正式图像。

---

## 2.2 之前 MODTRAN path radiance 与 legacy 差 5e5～1e7 的主要原因已经基本能解释

当前两份 LUT 里的同一组 MWIR 示例数据：

### `band_lut.csv` 原始 MODOUT2 量

```text
path_radiance_band = 2.562301659e-08
solar_irradiance_band = 1.503566713e-06
unit = MODOUT2_native
```

### `band_lut_si_candidate.csv` 转为候选 SI 后

```text
path_radiance_band_W_m2_sr_um = 0.1275239425
solar_irradiance_band_W_m2_um = 10.86727262
```

比例约为：

```text
path:  4.98e6
solar: 7.23e6
```

这正好落在此前看到的 `5e5～1e7` 量级差异范围。

MODTRAN 在以波数输出时，典型辐亮度单位是：

```text
W / (cm^2 sr cm^-1)
```

而 `IRRadianceModelV2::planckRadianceWm2SrUm()` 使用的是：

```text
W / (m^2 sr um)
```

从波数谱密度转换到波长谱密度，不能只乘一个固定 `1e4`，而要包含雅可比：

```text
ν~ = 10000 / λ_um

|dν~/dλ| = 10000 / λ_um^2
```

再从 `cm^-2` 转到 `m^-2`，总转换因子约为：

```text
1e8 / λ_um^2
```

在 MWIR 3～5 um 内，这个系数本身就在数百万量级。

### 结论

此前 `MODTRAN path 原始值比 legacy 小 5e5～1e7`，**高度符合“把 MODOUT2 原生波数单位直接与 SI 的 Planck 辐亮度比较”造成的单位错误**，不应首先判断为 MODTRAN 算错。

这也是当前不能直接启用 `ModtranPathUnitMode=Native` 的原因。

---

## 2.3 当前 loader 还有一个结构性问题：SI candidate 的列名与 loader 要求不一致

当前 `IRModtranRadianceLut.cpp` 读取的是：

```text
path_radiance_band
sky_radiance_band
solar_irradiance_band
```

但 `band_lut_si_candidate.csv` 中列名是：

```text
path_radiance_band_W_m2_sr_um
sky_radiance_band_W_m2_sr_um
solar_irradiance_band_W_m2_um
```

因此：

- 直接加载 `band_lut.csv`：能读，但得到的是 `MODOUT2_native`，与 Planck SI 单位不统一；
- 直接加载 `band_lut_si_candidate.csv`：当前 loader 并不能按现有 required columns 正常读取 SI 字段。

### 正确做法

不要再通过运行时手工 `Scale=5e6` 去“调到差不多”。

应改为二选一：

**方案 A，推荐：**
生成正式 `band_lut_si.csv`，统一数据 schema，例如：

```csv
band,atmosphere_model,aerosol_model,humidity_profile,visibility_km,
observer_alt_km,target_alt_km,range_km,solar_zenith_deg,
tau_up_band,tau_down_band,
path_radiance_band_W_m2_sr_um,
sky_down_irradiance_band_W_m2_um,
solar_direct_irradiance_band_W_m2_um,
...
```

然后 C++ loader 只接受 SI 字段。

**方案 B：**
loader 同时支持 native 与 SI，但加载时立刻完成单位归一化，内部永远只保存 SI。

无论哪种，进入 `IRRadianceModelV2` 后都不再保留“Native 直接参与物理计算”的路径。

---

# 3. HwaSim_IR 真正需要 MODTRAN 提供哪些数据

对于你的 HwaSim_IR，建议不要再追求“把 MODTRAN 所有输出都塞进程序”，只需要服务于下面的物理方程。

目标表面到传感器的辐亮度：

```text
L_sensor(λ)
 = τ_target→sensor(λ) * L_surface(λ)
 + L_path(λ)
```

表面出射辐亮度：

```text
L_surface(λ)
 = ε(λ) * Bλ(T_surface)
 + L_solar_reflected(λ)
 + L_sky_reflected(λ)
 + L_active_reflected(λ)
 + L_hotspot/plume(...)
```

因此生产版最少需要下面 5 类 MODTRAN/大气数据。

---

## 3.1 `tau_up`：目标 → 传感器 LOS 透过率

必须有。

输入维度至少：

```text
sensor altitude
target altitude
slant range / geometry
atmosphere profile
aerosol type
visibility / aerosol loading
water vapor / humidity profile
band or wavelength
```

用途：

```text
L_sensor = tau_up * L_surface + L_path
```

当前代码已有这部分接口，主要问题是默认没启用和插值方式过于简单。

---

## 3.2 `L_path`：目标 → 传感器路径自身产生的辐亮度

必须有，尤其 MWIR/LWIR。

MODTRAN 可用的物理项应按波段和模式组合，例如：

```text
THRML_EM
THRML_SCT
SING_SCAT
MULT_SCAT
```

不能直接使用包含目标/地表项的 `TOTAL_RAD` 作为 path radiance，否则容易把目标表面辐射重复算一次。

推荐定义：

```text
L_path =
    atmospheric thermal emission
  + atmospheric thermal scatter
  + solar/lunar scattering into LOS     // 对 VIS/NIR/SWIR/MWIR 日间按需
```

具体组合应由 PcModWin5 实际输出字段确认。

---

## 3.3 太阳到目标表面的直接光谱辐照度

需要，用于太阳反射和太阳热加载。

建议程序直接保存：

```text
E_sun_direct_at_target(λ)
```

而不是只保存 `tau_down`。

因为如果 MODTRAN 已经给出了类似 `REF_SOL`：

```text
太阳顶层光谱辐照度 × Sun→target 透过率
```

那么它本身已经是目标处的太阳辐照度，再额外乘一次 `tau_down` 就会重复衰减。

生产 schema 可以同时保留：

```text
tau_sun_to_target                // 诊断
solar_direct_irradiance_at_target // 真正计算用
```

但公式中只选一种方式。

---

## 3.4 天空下行漫射辐照度

建议使用：

```text
E_sky_down(λ)   [W/m²/um]
```

比目前笼统的 `sky_radiance_band` 更适合 Lambert 表面反射：

```text
L_sky_reflected = rho_band / pi * E_sky_down_band
```

如果 MODTRAN/PcModWin5 当前只能方便导出天空方向辐亮度，则需要说明：

- 是单方向 sky radiance；
- 还是半球积分后的 downwelling irradiance/flux。

这两个量不能混用。

---

## 3.5 可选：大气/天空自身背景辐射

如果天空直接进入相机背景，应有：

```text
L_sky_sensor_view(direction, λ)
```

这与“照到目标表面的下行天空辐照度”不是同一个量。

首阶段可将二者拆开：

```text
sky_down_irradiance -> 表面反射
sky_view_radiance    -> 天空背景
```

---

# 4. MODTRAN LUT 建议重建范围

结合项目既定范围：

```text
目标/平台高度：3～20 km
距离：约 0～50 km，建议数据覆盖到 60 km 留边界
区域：中纬度夏季为主
波段：NIR/MWIR 为主，SWIR/LWIR 可同时准备
```

建议生产网格：

### 离散类别

```text
Atmosphere:
  Mid-Latitude Summer   // 当前主生产
  Mid-Latitude Winter   // 后续

Aerosol:
  Rural                 // 当前主生产
  Maritime              // 海上场景
  Desert                // 沙漠场景需要
```

### 连续网格

```text
observer_alt_km:
  3, 5, 8, 10, 15, 20

target_alt_km:
  3, 5, 8, 10, 15, 20

range_km:
  0.5, 1, 2, 5, 10, 20, 30, 40, 50, 60

visibility_km:
  2, 5, 10, 23, 50

solar_zenith_deg:
  0, 15, 30, 45, 60, 75, 85
```

湿度不要再保留成“接口里有一个 `humidityPercent`，但 LUT 实际固定 `humidity_profile=default`”的状态。

两种正确选择：

### 方案 1：首版固定 Mid-Latitude Summer 水汽廓线

```text
humidity_profile = default
```

那么 runtime 不再声称 humidity 是一个有效 MODTRAN 插值维度。

### 方案 2：真正做水汽轴

例如按：

```text
H2O scale = 0.5 / 0.75 / 1.0 / 1.25 / 1.5
```

或使用真实 radiosonde/NCEP profile。

推荐最终使用水汽柱或 profile scale，而不是地面一个 RH 百分数直接代表整个 20 km 大气柱。

---

# 5. 当前 `IRModtranRadianceLut` 算法问题

当前查询结构里有：

```cpp
humidityPercent
```

但 LUT Entry 没有保存 humidityPercent，最近邻距离计算也没有使用 humidity。

同时当前选择方式是对：

```text
observer altitude
target altitude
range
visibility
solar zenith
```

做一个经验归一化距离，然后找单个最近邻。

这会造成：

- 输入连续变化时 LUT 值跳变；
- 太阳高度变化可能突然跳值；
- range 边界产生不连续；
- 不同物理维度的距离权重没有明确物理意义；
- humidity 看起来支持，实际上不支持。

### 建议

分类字段：

```text
band / atmosphere / aerosol / humidity-profile-id
```

先精确选组。

连续字段：

```text
observer_alt
target_alt
range
visibility
solar_zenith
```

使用规则网格上的多线性插值。

若一次做 5D 插值过重，可以：

- `observer_alt/target_alt` 先选最邻近高度组合；
- 对 `range + visibility + solar zenith` 做 3D 线性插值；
- 后续再扩到完整 5D。

运行时 30 Hz 更新大气，不需要 60 Hz 每像素查表，因此性能不是主要障碍。

---

# 6. PcModWin5 能不能让 Codex 获取数据

## 6.1 能做到什么

如果 Codex 运行在你的 Windows 本机，并且具备正常文件/命令行权限，它可以：

- 搜索 PcModWin5 安装目录；
- 阅读本机 help/docs/examples；
- 找到现有 `tp5/tape5/tp7/7sc/modout2/flx` 等输入输出；
- 判断是否有可命令行执行的 MODTRAN5 引擎；
- 克隆模板；
- 自动生成 case grid；
- 批量运行命令行 MODTRAN；
- 解析输出；
- 转 SI；
- 生成 LUT；
- 做 QC。

## 6.2 不一定能做什么

如果 PcModWin5 只有纯 GUI，没有可调用 CLI，而当前 Codex 环境没有 GUI Computer Use，那么 Codex 不应该通过猜坐标/模拟键盘去硬操作 GUI。

这种情况下正确做法是：

1. Codex 读取 PcModWin5 本地帮助文件；
2. 输出精确到菜单项/字段名的手动操作说明；
3. 你手动生成 1～3 个标准 case；
4. Codex 再根据这些 case 文件扩展为批量输入并运行底层 MODTRAN executable。

因此，**先让 Codex 在本机查清 PcModWin5 的实际安装结构和可执行方式**，比直接让我猜 GUI 菜单可靠得多。

---

# 7. PcModWin5 手工生成标准 case 时要确保的物理输出

即使暂时手工操作，也只需要先做一个 MWIR 标准 case 证明链路。

建议标准 case：

```text
Band:            3–5 um
Atmosphere:      Mid-Latitude Summer
Aerosol:         Rural
Visibility:      23 km
Sensor altitude: 10 km
Target altitude: 5 km
Slant range:     10 km
Solar zenith:    45 deg
```

分别确认/导出：

```text
1. LOS direct transmittance:
   TOT_TRANS / equivalent transmission spectrum

2. Atmospheric path radiance:
   THRML_EM
   THRML_SCT
   SING_SCAT / MULT_SCAT where appropriate

3. Direct solar irradiance at target:
   REF_SOL or equivalent Sun→target irradiance product

4. Downwelling sky diffuse/thermal flux:
   preferably FLX/DISORT downward flux
```

然后：

- 保留原始谱；
- 明确横轴是 `cm^-1` 还是 `um`；
- 明确每一列单位；
- 先在 Python/Excel 离线转 SI；
- 再按传感器波段/SRF 积分；
- 与黑体 Planck 数量级做交叉校验；
- 最后才生成 runtime LUT。

---

# 8. 为什么不建议“从网上找一份 MODTRAN 数据直接用”

生产大气 LUT 与下面条件强耦合：

```text
观测高度
目标高度
斜距/天顶角
太阳天顶角
气溶胶
水汽廓线
能见度
季节/标准大气
传感器波段/SRF
```

网上一条“3–5 um 大气透过曲线”最多适合作为趋势验证，不能替代项目生产 LUT。

同时 MODTRAN 本身及部分数据受许可约束。你已经有合法 PcModWin5，本地重新生成最合适，也最容易留下输入/输出 provenance。

公开数据可作为辅助：

- NOAA/NCEP/radiosonde：真实大气温湿廓线；
- 标准太阳光谱：用于校验太阳量级；
- 公共材料光谱：用于补充 emissivity/reflectance；
- 但大气 radiative-transfer 仍建议由你本机 MODTRAN 生成。

---

# 9. 当前太阳照明实现到底对不对

当前 `IRRadianceModelV2` 中太阳反射本质上是：

```text
reflectedRadiance
 = reflectance
 * solarStrength
 * NdotL
 * textureLuma
 * solarReflectanceWeight
```

项目文档也明确记录：该项**没有读取 MODTRAN solar_irradiance 表**。

这个式子可以让目标出现“迎光面亮、背光面暗”，但不能称为完整物理太阳辐射模型。

主要问题：

1. `solarStrength` 是归一化强度，不是 `W/m²/um`；
2. 没有真实太阳光谱；
3. 没有 `tau_sun_to_target`；
4. 没有 `rho/pi * E_sun` 的 Lambert 单位关系；
5. 用可见纹理 `textureLuma` 调制红外反射，可能制造虚假的 IR 纹理；
6. 没有太阳遮挡/阴影的物理 visibility；
7. 没有太阳吸收后的热加载和热惯量。

---

# 10. Ondulus IR 为什么太阳照明看起来明显更真实

公开资料和你仓库内的 Ondulus-style SensorWave 配置表明，Ondulus IR 的目标不是只增加一个 directional light，而是把：

```text
太阳/环境辐射
→ 材料吸收
→ 热加载
→ 导热/对流/冷却
→ 材料温度
→ 红外自发射
```

与：

```text
太阳/天空辐射
→ 材料反射
```

同时处理。

因此在 MWIR/LWIR 中，真实太阳影响通常包含两个效应：

### A. 瞬时反射

迎光方向可能有即时光学反射，NIR/SWIR 最明显，MWIR 可有一定贡献。

### B. 热加载

太阳把材料加热，经过材料热容/导热/对流后改变表面温度。

这会产生：

- 日照面与阴影面温差；
- 太阳移动后温度不会立刻跳变；
- 金属、涂层、玻璃等响应速度不同；
- 黄昏后仍保留热惯性；
- MWIR/LWIR 目标结构层次更丰富。

**HwaSim_IR 当前缺失的主要是 B，而不仅是 Panda3D 光源。**

---

# 11. 在 Panda3D 中实现自然太阳的正确方案

应把“可见光渲染”和“红外物理”分开。

## 11.1 太阳位置

输入：

```text
platform latitude
platform longitude
platform altitude
UTC date/time
```

根据标准太阳位置算法计算：

```text
solar azimuth
solar elevation
Earth-Sun distance correction
```

项目实时数据已有平台经纬度/高度，当前也已有 `simulationHour`。

但是只有“小时”还不足以准确考虑季节太阳位置，因此建议在 `HwaSimIRRuntime.ini` 增加：

```ini
[NaturalLighting]
TimeSource=RealtimeData
FallbackDateUTC=2026-07-15
FallbackTimeUTC=12:00:00
UsePlatformGeodeticPosition=true
```

如果协议实时 `time` 已包含完整日期时间，则直接使用；若只是一日内秒数，就用 fallback date + realtime hour。

---

## 11.2 坐标系转换

太阳算法输出通常是当地 ENU：

```text
east
north
up
```

必须统一到 HwaSimIR/Panda3D world axes。

如果场景使用：

```text
X = East
Y = North
Z = Up
```

则可直接：

```text
sunDir.x = cos(elevation) * sin(azimuth)
sunDir.y = cos(elevation) * cos(azimuth)
sunDir.z = sin(elevation)
```

但这一步必须让 Codex 实际核对 HwaSimIR 当前 ECEF/ENU/Panda 坐标映射，不要凭假设硬写。

---

## 11.3 Panda3D 可见光表现

可增加：

```cpp
DirectionalLight
```

Panda3D 官方本来就把 DirectionalLight 定义为类似太阳的无限远平行光。

用途仅限：

- VIS/NIR/SWIR 的可视化辅助；
- shadow map；
- 太阳方向一致的场景明暗。

不要直接把 Panda3D light color 当成 MWIR/LWIR 辐亮度。

---

## 11.4 红外物理太阳项

推荐：

```text
E_sun_direct_band
 = MODTRAN_solar_direct_at_target
 * cloudDirectScale
 * sunVisibility
 * max(0, n·sunDir)

L_solar_reflected
 = rho_band / pi * E_sun_direct_band
```

天空漫射：

```text
L_sky_reflected
 = rho_band / pi
 * E_sky_down_band
 * skyVisibility
```

表面热加载：

```text
Q_abs_solar
 = alpha_solar * E_sun_broadband * max(0, n·sunDir) * visibility
```

用一个简化热 RC：

```text
C_eff * dT/dt
 = Q_abs_solar
 + Q_sky
 + Q_engine/aero
 - Q_convection
 - Q_radiation
 - Q_conduction
```

首版不需要每三角形求热传导，可按：

```text
target
  ├─ nose
  ├─ fuselage
  ├─ wing
  ├─ engine/nozzle
  └─ canopy
```

或按材质 ID 做 4～8 个热区，30 Hz 更新温度，然后 shader 每像素按 material ID 使用对应温度。

---

# 12. 让 Codex 本机核查 Ondulus IR 最值得看的内容

你的目录是：

```text
D:\Presagis\Suite22
```

公开的 Suite22 目录结构通常包含：

```text
Ondulus_IR_22_0
  docs
  include
  samples
  data
  VegaPrime
  bin
```

无需反编译 DLL。优先阅读：

```text
docs/*
include/*
samples/*
data/*
VegaPrime/*
```

重点搜索：

```text
OndulusIRLightSource
OndulusIRAtmosphere
Sun
Solar
DateTime
Latitude
Longitude
Geodetic
ThermalReflection
SolarAbsorptivity
Convection
Conduction
Shadow
Atmosphere
MODTRAN
SEGen
```

目标是弄清：

1. 太阳方位由 Vega Prime 提供还是 Ondulus 自己算；
2. 输入是否是经纬度+日期+UTC；
3. 是否有真实 solar irradiance / atmospheric attenuation；
4. solar loading 如何进入 material temperature；
5. `OndulusIRLightSource` 是可见光/红外主动源还是通用辐射源；
6. 热反射和光源模型的参数单位；
7. samples 中有没有动态阴影、夏季/冬季、低照度对比 case；
8. data 目录里是否有材料谱、太阳谱、大气表、MTF/QE 等数据。

---

# 13. 主动照明器在 Panda3D 中怎么实现

## 13.1 当前协议情况

已经有：

```cpp
trackerSensorParam:
  illuminatorX/Y/Z
  illuminatorPitch/Yaw/Roll
  illuminatorAngle       // mrad
  illuminatorSpotRad

WeaponState:
  illuminatorEn

emitter:
  emitterSpotRadius
  emitterSpotRad
```

当前 HwaSim_IR 代码检索没有发现正式消费 `weaponState.illuminatorEn` 的运行逻辑，因此主动照明目前基本还是协议占位。

用户已明确首版暂不考虑安装位置和安装角度，因此：

```text
origin    = sensor/platform optical center
direction = tracker/sensor boresight
```

即可。

---

## 13.2 不要把主动照明实现成“提高目标温度”

正常主动照明是光学反射源，不是加热器。

正确链路：

```text
illuminator
  ↓ source→target atmosphere
target material reflection
  ↓ target→sensor atmosphere
sensor
```

即：

```text
L_active_sensor
 = tau_target_sensor
 * [rho_band/pi * E_active_target]
```

其中：

```text
E_active_target
 = E_ref
 * (R_ref/R)^2
 * beam(theta)
 * tau_source_target
 * max(0, n·lightDir)
```

对于照明器和传感器近似共址时，等效大气衰减接近双程：

```text
tau_source_target * tau_target_sensor
```

而不是只算一次。

---

## 13.3 `illuminatorAngle`

协议写“张角（mrad）”，但“张角”可能指全角，也可能有人按半角理解。

建议运行配置明确：

```ini
[ActiveIlluminator]
AngleInterpretation=FullCone
```

如果是全角：

```text
halfAngleRad = illuminatorAngle * 1e-3 * 0.5
spotRadius ≈ R * tan(halfAngleRad)
```

---

## 13.4 `illuminatorSpotRad` 的单位必须先定清

当前变量名 `Rad` 不能证明它到底是：

- radiance；
- irradiance；
- 相对亮度；
- UI 归一化强度。

不要直接把数值塞入 `W/m²`。

建议增加 runtime 解释模式：

```ini
[ActiveIlluminator]
IntensityMode=LegacyNormalized
```

等协议/客户单位确认后改成：

```ini
IntensityMode=BandIrradianceAtReference
ReferenceRangeM=1000
```

此时：

```text
illuminatorSpotRad = E_ref [W/m²]
```

或者如果客户定义为源辐射强度，则再换对应物理公式。

---

## 13.5 波段选择

当前协议没有 illuminator band，按你的要求先放 ini：

```ini
[ActiveIlluminator]
Enabled=true
Band=FollowSensor
; 可选 VIS,NIR,SWIR,MWIR,LWIR,FollowSensor
CenterWavelengthUm=0.85
BandwidthUm=0.05
UseAtmosphericAttenuation=true
BeamProfile=Gaussian
AngleInterpretation=FullCone
IntensityMode=LegacyNormalized
ReferenceRangeM=1000
```

推荐增加 `CenterWavelengthUm/BandwidthUm`，即使首版不用，也方便以后处理激光/窄带主动照明。

规则：

```text
illuminatorEn=false
    -> active contribution = 0

Band 与 sensor band 不重叠
    -> active contribution = 0

Band 相容
    -> 进入 reflectedRadiance
```

---

## 13.6 Panda3D `Spotlight` 怎么用

Panda3D 官方 `Spotlight` 本身具有：

- 位置；
- 方向；
- FOV；
- PerspectiveLens；
- attenuation；
- shadow map。

所以可以创建一个 `Spotlight` 作为：

- VIS debug；
- 光锥可视化；
- shadow/occlusion 辅助。

但是正式红外亮度应通过你自己的 shader uniforms：

```text
u_illuminator_enabled
u_illuminator_pos
u_illuminator_dir
u_illuminator_half_angle
u_illuminator_Eref
u_illuminator_ref_range
u_illuminator_band
u_illuminator_tau_outbound
```

计算物理反射。

不要依赖 Panda3D built-in light RGB 颜色去代表 MWIR 光谱功率。

---

# 14. `emitterSpotRadius/emitterSpotRad` 不要和 illuminator 混在一起

这两个字段名字看起来更像：

```text
发射器/自身热点的图像半径与亮度
```

而 `illuminator*` 是对外照明器。

除非正式协议明确二者属于同一个装置，否则建议保持两个独立概念：

```text
illuminator -> 外部入射光 → 目标反射
emitter     -> 自身发光/热点图像
```

否则以后既有“主动照明”又有“发射器亮斑”时会互相污染。

---

# 15. 当前 TargetLib 模型本身有什么问题

## 15.1 几何资源本身不是最大的短板

当前 F35 目录已经有：

```text
F35C.obj
F35C.mtl
f35c.jpg
f35c_mat.tif
f35c_mat.tif.xml
```

而 `IRSceneMaterialMapper` 也已经支持：

```text
material ID texture
→ XML composite material
→ MaterialDatabase.csv
→ emissivity / reflectance / solar absorptivity / roughness
→ shader
```

所以模型具备实现太阳方向效果和主动照明方向效果的基本条件。

---

## 15.2 F35 的可见材质很粗

`F35C.mtl` 只有一个：

```text
01___Default
```

并使用：

```text
map_Kd f35c.jpg
```

这对于普通可见光贴图够用，但不是红外材料模型。

红外不应该简单把可见 RGB 纹理灰度当作 MWIR/LWIR 的 emissivity/reflectance map。

---

## 15.3 F35 红外材质 ID 目前也很粗

`f35c_mat.tif.xml` 当前只有 4 类：

```text
1   METAL-ALUMINUM
85  METAL-PAINTED -> BM_PAINT
169 WINDOW-GLASS
255 METAL-IRON
```

对于战斗机红外图像，建议至少区分：

```text
机体涂层
雷达罩
座舱盖
机鼻
机翼前缘
机翼主体
进气道
发动机舱附近
尾喷管
尾喷口周边
内部热源影响区域
```

不一定每类都需要新几何；可以只扩 material-ID texture。

---

## 15.4 当前材质参数是“宽带常数”，不是各波段光谱属性

`MaterialDatabase.csv` 有：

```text
Solar Absorptivity
Thermal Emissivity
Specific Heat
Conductivity
Density
Transmissivity
Roughness
```

这些对建立简化热模型很有价值。

但当前 mapper 的反射率计算：

```text
reflectance = 1 - solarAbsorptivity - transmissivity
```

这是宽带太阳能量关系，不等价于：

```text
rho_MWIR
rho_LWIR
```

对于金属、玻璃、涂层尤其可能偏差很大。

最终应逐步增加：

```text
epsilon_VIS/NIR/SWIR/MWIR/LWIR
rho_VIS/NIR/SWIR/MWIR/LWIR
alpha_solar
```

或者直接提供光谱曲线再按传感器 SRF 积分。

---

## 15.5 当前模型数据已经有做“热惯量”的基础，但程序还没充分使用

MaterialDatabase 已经有：

```text
specific heat
conductivity
density
```

部分 XML 还有：

```text
thickness
```

所以完全可以做一个较轻量的动态温度模型。

当前 `IRSceneMaterialMapper` 送 shader 的主要还是：

```text
emissivity
reflectance
solar absorptivity
roughness
```

没有把：

```text
density
specific heat
conductivity
layer thickness
```

真正用于动态太阳热加载。

这是模型数据到 Ondulus-style thermal behavior 之间的关键缺口。

---

# 16. 一个很重要的部署核查：MaterialDatabase 是否真的加载成功

仓库中：

```text
materials/MaterialDatabase.csv
```

位于 repo 根目录。

而历史 RK3588 日志明确出现过：

```text
材质库=未加载，使用默认材质
路径=materials/MaterialDatabase.csv
```

这意味着如果 HwaSim_IR 从：

```text
HwaSim_IR/Bin
```

或部署包其他目录直接运行，且没有统一资源根目录解析，`materials/...` 可能找不到。

### 必须先验收

程序启动必须打印：

```text
MaterialDatabase=OK
count=...
absoluteResolvedPath=...
```

每个 target 必须打印：

```text
materialIdTex=OK
materialMap=OK
entries=N
default=...
```

否则你看到的“模型红外效果差”可能根本不是材料物理差，而是整个模型都在用默认材质。

---

# 17. SensorWave 链路还有哪些缺口

当前 `default_MWIR.json` 里实际上已经有类似 Ondulus IR profile 的参数：

```text
ADCBitNumber=14
DetectorPitch=0.02 mm
DetectorTemperature
FOV
FocalLength=100 mm
LensFnumber=1
IntegrationTime
NETD=0.05 K
QuantumEfficiency≈0.9
SpectralResponseRange=3–5 um
MTF file
GainControlSystem
```

项目 `sensorwave_config_usage.md` 明确说明：

目前 HwaSim_IR 只白名单读取一部分，其中：

```text
NETD
DetectorPitch
FocalLength
F-number
ADC bits
```

很多仍只是 profile/log/fallback，尚未形成完整传感器物理模型。

因此 HwaSimIR 与 Ondulus 图像差距还来自：

- 没有完整光学通量 → 探测器电子数；
- QE 没进入完整 detector response；
- integration time 没参与 photon/electron count；
- dark current/read noise/full well 等未完整计算；
- MTF 目前是近似 Gaussian；
- AGC/Noise/MTF 生产默认关闭；
- 没有真正按 sensor SRF 曲线做波段积分，只主要依赖 range low/high 和代表波长。

这应作为大气和光照之后的 P3 收口，不建议现在先动。

---

# 18. 与《基于三维场景的红外成像仿真系统及实现_李晨阳》架构对照

项目已有 `HwaSimIR_InfraredSimulationFramework.md` 明确记录：设计阶段阅读并提炼过该论文的全链路，包括：

```text
三维场景
大气要素
可见性
温度/入射/出射辐射场
混合像元
成像系统
```

因此可以说：

**HwaSimIR 的红外重构总体架构是按照该论文的方法框架规划的。**

但当前代码不是论文方法的完整复现。

---

## 18.1 已经基本对应的部分

```text
三维场景几何
材质分类/材质 ID
目标热辐射 Planck 模型
大气 tau/path 接口骨架
目标热点/尾喷/羽流
天气环境状态
传感器后处理 Stage6
MTF/噪声/AGC 框架
```

---

## 18.2 目前仍明显简化的部分

### 大气

论文式链路要求：

```text
tau_up
tau_down
path radiance
sky radiation
atmosphere thermal/scattering
```

当前生产仍主要 legacy，大部分 MODTRAN 项关闭。

### 可见性

当前太阳项主要使用：

```text
NdotL
```

还不是完整：

```text
direct sun visibility
sky hemisphere visibility
facet-to-facet visibility
```

### 温度场

目前主要是：

```text
预设材质温度
发动机局部热源
气动加热简化模型
```

尚缺：

```text
太阳吸收
热容
导热
对流
辐射冷却
历史状态/热惯量
```

### 光谱

当前 `IRRadianceModelV2` 很多地方采用：

```text
band center wavelength
```

而不是：

```text
∫ S_sensor(λ) * L(λ) dλ
```

### 混合像元

当前主要依靠 rasterization/纹理采样形成像元结果，没有专门的亚像元辐射面积混合模型。

### 传感器绝对物理

尚未完整从：

```text
aperture radiance
→ optical throughput
→ detector irradiance
→ electrons
→ ADC
→ display
```

做绝对标定。

---

# 19. 推荐的最终红外计算链路

建议把最终物理定义收敛成下面一条，不再在多个 Stage 中混用“灰度”和“物理辐亮度”。

```text
1. Material/thermal state
   T_surface
   epsilon_band
   rho_band

2. Environment
   sun position
   weather
   atmosphere query

3. Incident radiation
   direct solar irradiance
   sky diffuse irradiance
   active illuminator irradiance

4. Surface outgoing radiance
   thermal self emission
   solar reflection
   sky reflection
   active reflection
   engine/hotspot/plume

5. Atmosphere target→sensor
   tau_up
   path radiance

6. Sensor aperture radiance

7. Sensor spectral response integration

8. Optics/detector
   F-number / QE / integration / NETD/noise / MTF

9. AGC / polarity / display quantization

10. TCP/H264 output
```

核心约束：

```text
Stage 1～7 尽量保持物理单位
最后显示阶段才归一化成灰度
```

---

# 20. 建议的实施顺序

## P0：先把“数据真的被加载且单位一致”做实

只做核查与最小修复：

1. 统一资源根目录解析；
2. 启动时打印所有物理数据绝对路径；
3. MaterialDatabase 必须加载成功；
4. MODTRAN tau/path LUT 必须明确 loaded/active；
5. 做 SI 单位转换单元测试；
6. 禁止 raw native path 进入 `IRRadianceModelV2`；
7. 用一个 300 K 黑体 + 固定 MWIR case 做数值闭环。

**P0 完成前不要继续调视觉参数。**

---

## P1：重建 MODTRAN SI 数据 + 太阳物理反射

1. PcModWin5 重建 MWIR 3–5 um 标准 cases；
2. 保存 raw spectrum；
3. 统一转换 SI；
4. 生成正式 SI LUT；
5. 引入插值；
6. 先启用 `tau_up + path`；
7. 再接 `solar direct + sky diffuse`；
8. 去掉当前经验 `solarStrength * textureLuma` 作为正式 MWIR 物理项。

先只做 MWIR，验证正确后再扩 NIR/SWIR/LWIR。

---

## P2：自然太阳 + 主动照明

### Natural Light

```text
lat/lon/date/time
→ solar position
→ DirectionalLight for visible/debug
→ physical solar irradiance for IR
→ shadow visibility
→ solar thermal loading
```

### Active Illuminator

```text
illuminatorEn
illuminatorAngle
illuminatorSpotRad
runtime band
→ beam irradiance
→ two-way atmosphere
→ material reflection
→ sensor
```

---

## P3：模型/传感器高保真

1. F35/F22/AIM 材质 ID 精细化；
2. 补各波段 emissivity/reflectance；
3. 4～8 区域动态热模型；
4. 用真实 SRF 做带内积分；
5. QE/F-number/integration time/detector noise；
6. MTF 按 profile 文件；
7. AGC 标定。

---

# 21. 验收用的最小物理基准

建议以后每次改红外算法都固定保留下面的数值 smoke。

## A. Blackbody

```text
T=250/300/500/1000 K
epsilon=1
tau=1
path=0
```

输出必须与 Planck/带积分离线结果一致。

## B. Atmosphere

```text
same target
range=1/10/30/50 km
```

应满足：

- tau 总体随距离下降；
- path 总体随路径增加，但具体波段允许非严格单调；
- 不得出现单位级突跳。

## C. Solar

同一材料、同一温度：

```text
sun elevation=10/30/60/90 deg
```

NIR/SWIR 反射应明显变化；MWIR 有合理弱反射；LWIR 直接太阳反射应很弱。

## D. Solar thermal inertia

同一材料：

```text
sun on 60 s
sun off 60 s
```

温度不能瞬时跳变/恢复。

## E. Active illuminator

固定目标：

```text
illuminatorEn off/on
range 1/2/5 km
angle 1/2/5 mrad
```

验证：

- off 完全无贡献；
- band 不匹配无贡献；
- spot size 随距离和 angle 增长；
- 强度随距离衰减；
- 有大气时体现双程衰减。

---

# 22. 给 Codex 的 Prompt A：PcModWin5 + MODTRAN 数据专项核查/重建

```text
你现在在本机 Windows 工程 D:\HwaSimIR 工作。先不要修改 HwaSim_IR 的生产视觉参数，也不要直接打开 UseModtranPathRuntime。

目标：
查清 HwaSimIR 当前 MODTRAN 数据的单位、来源、PcModWin5 生成方式，建立一套可审计的 MWIR 3–5 um SI 数据链，并输出核查报告。优先正确性，不要靠人工 scale 把数值调得像 legacy。

一、先审计仓库
重点检查：
- HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/raw
- HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed
- IRModtranTauLut.*
- IRModtranRadianceLut.*
- IRRadianceModelV2.*
- HwaSimIR.cpp/.h
- HwaSimIRRuntime.ini
- tools 目录所有 modtran 脚本
- docs/HwaSimIR_IR_Architecture_Refactor_Plan.md
- docs/HwaSimIR_InfraredSimulationFramework.md

明确回答：
1. band_lut.csv 每个 radiance/irradiance 列的真实原始单位；
2. band_lut_si_candidate.csv 的 SI 转换公式；
3. 为什么 raw path 与 legacy/Planck 差 1e6 量级；
4. 当前 C++ loader 能不能实际读取 SI candidate；
5. humidityPercent 是否真的参与 LUT 查询；
6. 当前 runtime 最终实际使用的是 legacy 还是 MODTRAN。

二、检查本机 PcModWin5
搜索本机 Ontar/PcModWin5/MODTRAN5 安装目录、注册表、开始菜单 shortcut、help/docs/examples。
不要修改/破解许可证，不要反编译。
查清：
- PcModWin5 的真实安装路径；
- 底层 MODTRAN5 executable；
- 是否支持命令行批量；
- 输入文件扩展名和模板；
- tp7/7sc/modout2/flx 等输出的字段和单位；
- 本地文档中 TOT_TRANS、THRML_EM、THRML_SCT、SING_SCAT、MULT_SCAT、REF_SOL、flux 的定义。

如果可 CLI 批量运行，则直接做下面标准 case；
如果只能 GUI，不要用盲目坐标点击，读取本地帮助后输出准确的手工菜单/字段操作说明，并尽量找到 GUI 最终保存的 case 文件，后续用它批量复制。

三、先只重建 MWIR 标准 case
基准：
- 3–5 um
- Mid-Latitude Summer
- Rural
- visibility=23 km
- observer=10 km
- target=5 km
- range=10 km
- solar zenith=45 deg

输出并保留 raw spectrum：
- LOS direct transmittance
- path thermal emission/scattering
- solar-scattered path if applicable
- direct solar irradiance at target
- downward sky diffuse/thermal flux

严禁直接使用 TOTAL_RAD 当 path radiance。

四、单位
程序物理内部统一：
- radiance: W/(m^2 sr um)
- irradiance: W/(m^2 um)
- tau: dimensionless

如果 MODTRAN 输出为 W/(cm^2 sr cm^-1)，必须按谱变量雅可比完成 cm^-1 -> um 转换，并写自动单元测试，不允许只乘固定经验 Scale。

五、生成工具
优先补/整理：
- tools/modtran_generate_cases.py
- tools/modtran_convert_to_si.py
- tools/modtran_build_lut.py
- tools/modtran_qc.py

每条 LUT 保留：
- 输入 case id
- atmosphere/aerosol/profile
- geometry
- solar zenith
- raw unit
- SI unit
- source file
- conversion method

六、C++ 最小修正方案
先输出设计，不立即把生产开关打开。
方案必须满足：
- loader 内部只输出 SI；
- 不允许 MODOUT2_native 直接进入 IRRadianceModelV2；
- 支持正式 SI 列名；
- 修掉“humidity 参数存在但实际不参与”的假支持；
- 把 nearest-neighbor 改为规则网格插值或明确的分阶段插值；
- out-of-range 必须日志可解释并 fallback；
- 30 Hz atmosphere update 足够，不要每像素查 LUT。

七、QC
至少做：
- 250/300/500/1000 K Planck 数量级对比；
- range 1/10/30/50 km tau/path 趋势；
- visibility 2/5/10/23/50 km；
- solar zenith 0/30/45/60/75；
- raw↔SI 转换回算误差；
- 与现有 band_lut_si_candidate 对比。

输出：
1. docs/HwaSimIR_MODTRAN_Physics_Audit.md
2. 一份标准 MWIR case 的原始输入/输出路径
3. SI 转换后的数值表
4. 当前问题按 P0/P1/P2 排序
5. 下一阶段可直接实施的文件清单

本轮先以“证明数据和单位正确”为结束条件，不为了好看修改 shader，不开启生产 MODTRAN runtime。
```

---

# 23. 给 Codex 的 Prompt B：本机 Ondulus IR 太阳/光源专项核查

```text
只做本机资料、sample、header、配置和可运行示例核查，不反编译 DLL，不修改许可证。

目标：
核清 D:\Presagis\Suite22 中 Ondulus IR 22 的太阳照明、太阳热加载、自然环境和 LightSource 实现方式，为 HwaSimIR L1/L2 提供可复制的设计依据。

一、定位目录
在 D:\Presagis\Suite22 下自动查找 Ondulus_IR_22_0 或等效目录，重点：
- docs
- include
- samples
- data
- VegaPrime
- bin

二、全文检索
关键词：
OndulusIRLightSource
OndulusIRAtmosphere
Sun
Solar
LightSource
Date
Time
Latitude
Longitude
Geodetic
Atmosphere
ThermalReflection
Radiance
Irradiance
SolarAbsorptivity
Conduction
Convection
Cooling
Shadow
MODTRAN
SEGen

三、必须回答
1. Ondulus/VegaPrime 中太阳位置由什么模块/类计算；
2. 需要哪些输入：lat/lon/alt/date/time/timezone/UTC；
3. 是否有太阳光谱或 band irradiance 数据；
4. atmosphere 如何影响 Sun→surface；
5. solar loading 是否改变 material temperature；
6. temperature update 是否考虑 heat capacity/conduction/convection/radiative cooling；
7. shadow 对“太阳反射”和“太阳热加载”分别怎样作用；
8. OndulusIRLightSource 的用途、参数、单位、FOV/beam、band/wavelength 支持；
9. Suite22 是否有 thermal reflection/light source 新模型；
10. Sensor/MTF/QE/material/atmosphere data 的实际文件路径。

四、sample A/B
如果现有 license 和 sample 可正常运行，优先使用官方/本地 sample，不改软件。
选：
- CDB_summer
- CDB_winter
- CDB_summer_DynamicShadows
- CDB_summer_LowLight
或最接近的 sample。

做小规模 A/B：
- 同位置不同时刻
- 同时刻不同太阳高度
- sunlight/shadow
- 不同 material
- LightSource off/on

记录：
- 配置文件差异
- 实际类/属性名
- 截图
- 可读日志/数值
- 不需要追求性能。

五、和 HwaSimIR 对照
按下面模块输出“可直接借鉴 / 只能概念借鉴 / 不需要复制”：
- SolarPosition
- NaturalLight
- MaterialThermalState
- Atmosphere
- ActiveLightSource
- SensorResponse

输出：
docs/HwaSimIR_OndulusIR_Lighting_Audit.md

报告必须引用本机真实文件路径、类名、字段名和 sample，不允许只依据网上介绍猜实现。
```

---

# 24. 给 Codex 的 Prompt C：在 A/B 核查完成后的 HwaSim_IR 实施 Prompt

```text
基于已完成的：
- docs/HwaSimIR_MODTRAN_Physics_Audit.md
- docs/HwaSimIR_OndulusIR_Lighting_Audit.md
- docs/HwaSimIR_InfraredSimulationFramework.md
- docs/HwaSimIR_RuntimeChannel_H264_Weather_Lighting_Implementation_Plan.md

实施 HwaSimIR 红外大气 + 自然太阳 + 主动照明剩余主体工作。

原则：
- 先物理正确，再视觉调参；
- 不改变现有通信协议；
- illuminator band 暂放 HwaSimIRRuntime.ini；
- illuminatorX/Y/Z、Pitch/Yaw/Roll 首版保留不使用；
- WeaponState.illuminatorEn 是运行开关；
- 不用主动照明修改目标温度；
- MWIR 先闭环，再扩其它波段；
- 保持 Windows/RK3588、Visible/Headless 一致；
- 不破坏 60 Hz 帧链，物理状态可 30 Hz 更新并复用。

阶段 1：P0 修复
1. 统一资源根目录解析，MaterialDatabase/MODTRAN/SensorWave/TargetLib 启动必须打印 resolved absolute path；
2. MaterialDatabase 加载失败视为明确物理 fallback，不允许静默；
3. MODTRAN loader 内部统一 SI：
   radiance W/(m^2 sr um)
   irradiance W/(m^2 um)
4. native raw 不允许直接进入 IRRadianceModelV2；
5. 修掉 humidity 假支持；
6. 加 LUT 插值和 QC test；
7. 先只启用 MWIR tau_up + path，做 legacy A/B。

阶段 2：自然太阳
新增轻量模块，名称可按现有风格调整：
- IRSolarPosition
- IRNaturalIllumination
- IRMaterialThermalState

输入：
- realtime lat/lon/alt
- date/time
- weather
- MODTRAN solar/sky
- material

计算：
E_sun_direct
E_sky_down
L_solar_reflected = rho/pi * E_sun * NdotL * visibility
L_sky_reflected = rho/pi * E_sky * skyVisibility

增加 DirectionalLight 仅用于 VIS/debug/shadow；MWIR/LWIR 正式结果使用自有物理 shader/input。

加入简化热惯量：
Ceff*dT/dt = Qsolar + Qother - Qconv - Qrad - Qcond
首版按材质ID/目标区域更新，不做全三角网格热求解。

阶段 3：主动照明
读取现有：
- trackerSensorParam.illuminatorAngle
- trackerSensorParam.illuminatorSpotRad
- WeaponState.illuminatorEn

首版：
origin=sensor optical center
direction=sensor/tracker boresight

新增 ini：
[ActiveIlluminator]
Band=FollowSensor
CenterWavelengthUm=...
BandwidthUm=...
AngleInterpretation=FullCone
IntensityMode=LegacyNormalized
ReferenceRangeM=1000
UseAtmosphericAttenuation=true
BeamProfile=Gaussian

计算：
E_target = Eref*(Rref/R)^2*beam*TauOut*max(0,NdotL)
L_active = rho_band/pi*E_target
最终还要乘 target→sensor tau，因此共址场景体现双程大气。

Panda3D Spotlight 仅用于 debug/可见光锥/阴影辅助，不以 RGB light 代替 IR 物理。

阶段 4：模型材质
不换模型几何作为首要任务。
先：
- 检查每个 target 的 material id texture/XML 实际加载；
- F35 增加更合理的机体涂层、座舱盖、雷达罩、前缘、发动机区、尾喷区 material IDs；
- 材质接口支持 band emissivity/reflectance；
- visible texture luma 不再作为 MWIR/LWIR 正式 reflectance。

阶段 5：验收
至少输出：
- MODTRAN off/on 数值 A/B
- sun 10/30/60/90 deg
- sun shadow
- solar heating on/off 120 s
- illuminator off/on
- illuminator range/angle/band A/B
- Windows Visible
- Windows Headless
- RK3588 Headless
- 性能：物理更新 30 Hz，视频 60 Hz，不产生队列持续增长

修改完补：
docs/HwaSimIR_IR_Atmosphere_Natural_Active_Lighting_Implementation.md

不要为了通过截图使用任意灰度补偿；所有新增 scale 必须有物理意义、单位、默认值和日志来源。
```

---

# 25. 最终判断

如果目标是“先得到正确、可信的红外图像”，建议你下一步**先执行 Prompt A 和 Prompt B 的核查，不要直接执行 Prompt C 全量修改**。

原因是目前最关键的两件事都已经不是猜测：

1. 现有 MODTRAN raw radiance 与 SI Planck 链路单位不一致；
2. HwaSimIR 当前太阳反射是经验项，Ondulus 类效果真正依赖的太阳热加载尚未形成。

先把 PcModWin5 的真实输出和 Ondulus 本机 API/sample 弄清，后续 HwaSimIR 的修改范围反而会更小、更确定。

---

# 26. 本次核查涉及的主要仓库文件

```text
HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini
HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut.csv
HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si_candidate.csv
HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/lut_readiness_report.md
HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/final_a_line_readiness.md

HwaSim_IR/HwaSim_IR/IR/IRModtranRadianceLut.h
HwaSim_IR/HwaSim_IR/IR/IRModtranRadianceLut.cpp
HwaSim_IR/HwaSim_IR/IR/IRRadianceModelV2.h
HwaSim_IR/HwaSim_IR/IR/IRRadianceModelV2.cpp
HwaSim_IR/HwaSim_IR/IR/IRSceneMaterialMapper.cpp

materials/MaterialDatabase.csv
HwaSim_IR/Bin/Config/TargetLib/models/f35/F35C.mtl
HwaSim_IR/Bin/Config/TargetLib/models/f35/f35c_mat.tif.xml
HwaSim_IR/Bin/Config/SensorWave/default_MWIR.json

docs/HwaSimIR_InfraredSimulationFramework.md
docs/HwaSimIR_IR_Architecture_Refactor_Plan.md
docs/HwaSimIR_RuntimeChannel_H264_Weather_Lighting_Implementation_Plan.md
docs/sensorwave_config_usage.md
```

外部技术依据主要为：

```text
MODTRAN official FAQ / spectral output definitions
Spectral Sciences MODTRAN overview
Panda3D DirectionalLight / Spotlight / GLSL shader input documentation
Ondulus IR public product descriptions仅用于能力佐证；
具体实现仍要求 Codex 以本机 Suite22 docs/include/samples 为准。
```
