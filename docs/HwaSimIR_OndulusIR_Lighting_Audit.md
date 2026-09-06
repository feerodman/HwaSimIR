# HwaSimIR 对 Ondulus IR 22 太阳、热加载与主动光源审计

日期：2026-09-06  
本机安装：`D:\Presagis\Suite22\Ondulus_IR_22_0`、`D:\Presagis\Suite22\Vega_Prime_22_0`  
证据边界：只读取本机 docs、公开 headers、samples、data、ACF/JSON；未反编译 DLL，未修改许可证或 Suite22 文件。

## 1. 结论摘要

Ondulus IR 22 将问题拆成三条相互关联但不混用的链：

1. Vega Prime `vpEnv + vpEnvSun` 用内部 ephemeris 根据日期、时间、经纬度和时区计算太阳方向/高度，驱动全局光照。
2. Ondulus Surface Thermal 用 ambient conditions、最大太阳辐照度、入射角、材料太阳吸收率、阴影、导热/对流/长波辐射计算材料表面温度。
3. Radiative Transfer 用材料温度/反射、波长相关 atmosphere transmittance、自然光或 `OndulusIRLightSource` 的光谱，在传感器谱段内形成 radiance；SensorResponse 再处理 MTF、QE、噪声等。

这个分层值得 HwaSimIR L1/L2 借鉴；不应把太阳可见反射、太阳热加载和相机 path radiance 合成一个经验亮度参数。

## 2. 十个必须回答的问题

### 2.1 太阳位置由什么计算

Vega Prime 的 `vpEnv` 管理 ephemeris 输入，`vpEnvSun` 是太阳光源/天空圆盘。`D:\Presagis\Suite22\Vega_Prime_22_0\include\vegaprime\vpEnvSun.h:26-33` 明确写明太阳光源和 sky disk 的位置由 internal ephemeris model 更新；更新频率由 `vpEnv::setEphemerisUpdateInterval()` 控制。`vpEnvSun` 影响 global illumination，但不直接改变 global visibility。

Ondulus 的 `AmbientConditionsSystem` 另有可设置的 `ReferenceLocation`、`ReferenceDate`、`TimeOfDay` 和 sun-elevation time series，用于 Surface Thermal。

### 2.2 需要哪些输入

Vega Prime API：

- `vpEnv::setDate(day, month, year)`：`vpEnv.h:276`
- `setTimeOfDay(float hours)`：`vpEnv.h:294`，小时小数
- `setReferencePosition(longitude, latitude)`：`vpEnv.h:370`
- `setEphemerisTimeZoneOffset(hours from GMT)`：`vpEnv.h:422-437`；0 表示 GMT/UTC，`-10000` 是 automatic token
- `setEphemerisUpdateInterval()`：控制重算间隔

公开 ephemeris 接口和 ACF 没有把 altitude 作为太阳位置必需输入；场景/observer altitude 仍用于地理坐标和视点，但不是这里的太阳天文输入。Ondulus thermal ambient 需要经纬度、日期、time-of-day；temperature CSV 还提供逐小时 SunAzimuth/SunElevation。UTC 使用 `timezone offset=0`；本地时间使用显式 offset 或 automatic。

### 2.3 是否有太阳光谱或 band irradiance 数据

本机 data 中没有一张供用户直接编辑的 natural-sun band irradiance LUT。默认 ambient thermal 模型在 `Ambient_Conditions_subsystem.html` 中使用 `R0=1395 W/m²`，clear-sky 经验值约 `0.75·R0·sin(µ)`，并由 `MaximumSolarRadiation` 和 attenuation factor 调整。

`Reflected_Radiance_subsystem.html` 则说明自然光跨谱段的比例基于 `T_sun=5778 K` 黑体与 `τ_atm(λ)` 的积分。这是模型公式，不是本机可见的 band irradiance CSV。

人工灯有真实可读光谱数据：`data\lights\*NormalizedIntensity.txt` 第一列 µm、第二列归一化强度；User1–4 在 `data\lights\UserLights.csv` 中给出 color、divergence(mrad)、wavelength(nm)、power(mW)，包括 3391 nm 示例。

### 2.4 atmosphere 如何影响 Sun→surface

有两种作用，不能混为一谈：

- Surface Thermal 的太阳热加载使用全局 `EnvAttenuationFactor β∈[0,1]`：`q_sr=(1-β)·Rs_max·sin(µ)·cos(θ)/sin(µ_max)`。这是简化的天气/大气衰减，不是逐波长 MODTRAN。
- Radiative Transfer 的 `AtmosphericTransmittanceSystem` 从文本加载 `τ(λ)`，按 `τ=exp(-γ(λ)R)` 影响表面到 sensor 的辐射，也用于自然/人工反射 radiance 的谱段积分。

本机 `default_Atmosphere.json` 的例子是 `EnvAttenuationFactor=0.075`；可见 transmittance 数据在 `data\transmittance`。

### 2.5 solar loading 是否改变 material temperature

**是。** `Heat_Transfer_subsystem.html` 明确将太阳吸收项并入热方程，输出 terrain/culture 的 surface temperature，后续由 Radiative Transfer 使用。`RawMaterial` 提供 `SolarAbsorptivity`。

### 2.6 是否考虑 heat capacity/conduction/convection/radiative cooling

**全部考虑。** 文档给出一维多层导热方程 `∂(k∂T/∂x)/∂x+Q=ρc∂T/∂t`；材料的 conductivity、density、specific heat 在 `RawMaterial.h:124-152`。`HeatTransferSystem` 同时考虑 free/forced convection；辐射热交换使用 `εσT⁴`，并含 atmosphere back radiation。模型说明时间项是 loosely coupled，并在特定时刻按稳态边值问题求解，因此它不是通用 3D 瞬态 CFD/FEA。

### 2.7 shadow 对太阳反射与热加载的作用

- 热加载：`ShadowSystem` 将表层太阳吸收率改成 `α_shadow=η_shadow·α`，其 API 输入 shadow intensity 定义为 0=no shadow、1=full black（`ShadowSystem.h:105-119`）；修改后的吸收率进入 Heat Transfer，直接改变表面温度。
- 太阳反射：默认 reflected-radiance 来自可见 PBR 图像/BRDF，文档说明 BRDF 包含 geometric attenuation/shadowing。Vega dynamic shadow map 因而会影响 PBR 直接照明与由其派生的反射外观；但公开文档没有声明它复用 Surface Thermal 的 `η_shadow` 标量。HwaSimIR 应保持两个 shadow consumer：一个遮蔽即时反射，一个遮蔽热源项，不能假定两者完全相同。

### 2.8 `OndulusIRLightSource` 用途、参数、单位、FOV/beam、band/wavelength

`vpOndulusIRLightSource`/`vpOndulusPBLightSource` 是附着到 scene node/vehicle 的人工主动光源：

- `Enable`
- RGBA `Color`，范围 [0,1]
- `BeamAngle`：degrees（`vpOndulusPBLightSource.h:134-144`）
- `Range`：meters（`:147-154`）
- `PowerLevel`：built-in type 通常 [0,1]
- `Type`：Natural, Incandescent, LED, Fluorescent, Halogen, Sodium, IR850nm, IR940nm, User1–User4（`Ondulus\PB\Constants.h:107-132`）

User1–4 不使用通用 beam angle/power setter；其 beam divergence、单波长和 mW power 来自 `UserLights.csv`。因此内建灯以归一化光谱和 beam/range 描述，UserX 以 divergence(mrad)、wavelength(nm)、power(mW) 描述。它没有一个直接“MWIR band”枚举；光源光谱/波长与 sensor 的 spectral response range 共同决定贡献。

官方 `CDB_summer.acf` 的真实例子：

- `vehicleHeadlight1/2`：Halogen、beam 30°、range 20 m、power level 1
- `doorLight*`：LED、beam 60°、range 8 m、power level 1

### 2.9 Suite22 是否有新 thermal reflection/light source 模型

**有。** 本地 22.0 release help 明确列出：

- `Thermal_Reflections.html`：22.0 新增 MWIR/LWIR screen-space thermal reflection，水面默认启用，可限制 water-only。
- `New_Light_Model.html`：新的 moon/ambient physical low-light model，替代仅面向人眼暗视觉的 inherited Vega Prime light colour 方法。
- `Enhanced_VP_Light_Point.html`：增强 light-point radiance，考虑 color consistency、beam width/direction、intensity 和 distance fading。
- `Enhanced_Reflected_Radiance_with_Attenuation.html`：增强 VIS/NIR/SWIR 在环境衰减下的 reflected radiance。

这些是 22.0 公布的模型边界；没有从二进制推测实现。

### 2.10 Sensor/MTF/QE/material/atmosphere 数据路径

- sensor/config：`D:\Presagis\Suite22\Ondulus_IR_22_0\data\configuration\*.json`
- MWIR 示例：`...\data\configuration\default_MWIR.json`，其中 `SpectralResponseRangeLow=3`、`High=5`
- MTF：`...\data\MTF\*.txt`，如 `MTF_f100_N8.txt`
- QE/IIT：`...\data\IIT\QE_*.txt`
- material：`...\data\materials\MaterialDatabase.csv`、`MaterialDatabase_example.csv`
- atmosphere config：`...\data\configuration\default_Atmosphere.json`
- spectral transmittance：`...\data\transmittance\*.txt`；格式是 reference path length(m)、wavelength(µm)、tau[0,1]
- artificial lights：`...\data\lights\*.txt`、`UserLights.csv`
- temperatures/sun elevation：`...\data\temperatures\Temperatures_*.csv`

`default_MWIR.json` 同时给出 horizontal/vertical MTF 路径、3–5 µm、640×480、FOV、f-number、detector pitch、integration time、QE/NEP/NETD 和 ShadowSystem 等实际字段。

## 3. Sample A/B 审计

### 3.1 配置级 A/B（已完成）

| sample | 日期/时间 | thermal input | shadow/light 特征 |
| --- | --- | --- | --- |
| `CDB_summer.acf` | 2015-06-01 09:15 | `Temperatures_Yemen_Summer.csv`；`RecalculateSunElevations=false` | Env layer shadow false；有 Halogen headlights 与 LED door lights。 |
| `CDB_winter.acf` | 2014-12-01 09:15 | `Temperatures_Yemen_Winter.csv`；false | 同位置/同时间，可作 season/material temperature A/B。 |
| `CDB_summer_LowLight.acf` | 2015-06-12 01:56:24；time multiplier 0 | Summer；`RecalculateSunElevations=true` | 可作低太阳高度/夜间 natural-light A/B。 |
| `CDB_summer_DynamicShadows.acf` | 2015-06-01 09:15 | Summer；false | 加载 `vpShadow`，`ShadowDynamicShadows` enable=true、intensity=0.81、4096² map、max distance 1000 m。 |

四者 reference position 都是 longitude 45.0453、latitude 12.7775，timezone 为 automatic (`-10000`)。对应真实文件均位于 `D:\Presagis\Suite22\Ondulus_IR_22_0\appdata\ondulus_ir_viewer`。

### 3.2 运行探针（未形成有效 A/B）

本轮按官方 `Run_ondulus_ir_viewer.cmd` 的 setup 启动了 `bin\ondulus_ir_viewer.exe`，控制台到达 Vega Prime 22 build `22.0.0.60658` banner，未出现许可证错误；但当前自动化会话没有暴露任何原生 app/window，无法可靠读取 sample menu、GUI 数值或抓图。随后已终止探针，没有修改 ACF，也没有声称 sample scene 已完整加载。

因此本报告没有伪造截图或运行数值。若在可控桌面复测，准确流程是：先运行 `Run_vpRTP.cmd`（CDB sample 要求），再运行 `Run_ondulus_ir_viewer.cmd`，从控制台选择上述 ACF；在 UI 中保持 camera/material 不变，分别切换 time/date、dynamic shadow、LightSource enable，并记录 Atmosphere panel 的 sun direction/elevation、thermal/radiance views 和日志。

建议最小运行矩阵：

1. Summer 09:15 vs LowLight 01:56，同位置同 camera。
2. Summer vs Winter，同 09:15。
3. Summer vs DynamicShadows，同 time/material。
4. 同一 Summer scene 内 `vehicleHeadlight1` enable off/on。
5. 对 asphalt/metal/glass/water 各锁定一个 ROI，记录 surface temperature 与 MWIR radiance。

## 4. 对 HwaSimIR 的可复制性

| 模块 | 结论 | 建议 |
| --- | --- | --- |
| SolarPosition | 可直接借鉴 | 独立 `SolarPosition`，输入 UTC/date/lat/lon，输出 ENU direction、azimuth/elevation；按时间/位置变化更新，不进 shader 算天文。 |
| NaturalLight | 可直接借鉴架构，数值需自建 | 将 direct solar、sky diffuse、surface BRDF 反射分开；使用本轮 MODTRAN SI irradiance/flux，不复制 Ondulus 的经验常数作为最终物理值。 |
| MaterialThermalState | 可直接借鉴分层 | 单独状态更新：太阳吸收、阴影、ρc、k、free/forced convection、εσT⁴；L1 可先一维/lumped，L2 再多层。 |
| Atmosphere | 只能概念借鉴 | Ondulus 默认只有 `τ(λ)` 加简化 β；HwaSimIR 已有 MODTRAN，应保留 tau/path/solar/sky 的 SI 多分量链，不能退化成一个 attenuation color。 |
| ActiveLightSource | 可直接借鉴接口 | type/spectrum or wavelength、power W/mW、beam/divergence、range、transform、enable；结果按 sensor response 积分，并经过 LOS atmosphere/shadow。 |
| SensorResponse | 可直接借鉴数据分层 | sensor config、MTF、QE、noise、gain 与 scene radiance 分离；3–5 µm response 不应只由渲染 band 名决定。 |

### 不需要复制

- 不需要复制 Ondulus 的专有 UI、ACF 格式或 DLL task graph。
- 不需要复制 `R0=1395 W/m²` 与 `0.75` clear-sky 经验式作为 HwaSimIR 的 MODTRAN 真值。
- 不需要把 screen-space reflection 当作 material thermal solver；它属于图像反射表现层。

## 5. 对 HwaSimIR L1/L2 的建议落点

1. L1：`SolarPosition` + direct/diffuse SI irradiance + shadow mask + material thermal state（低频更新）+ active-light source schema。
2. L1：立即区分两个 shadow 输出：`shadowDirectOptical` 与 `shadowSolarThermal`，即使初始复用同一 mask 也保持接口分开。
3. L1：人工灯至少包含 `spectrum_id/wavelength_um`、radiant power、beam/divergence、range、pose、enable；不要只传 RGB。
4. L2：多层材料、thermal inertia、conduction、convection/radiative cooling、自然光/人工光 BRDF、二次反射。
5. SensorResponse 末端统一消费 SI spectral/band radiance，并通过 MTF/QE/noise；禁止让 light power 直接变成 pixel gray。

## 6. 主要本地证据

- `D:\Presagis\Suite22\Vega_Prime_22_0\include\vegaprime\vpEnv.h`
- `D:\Presagis\Suite22\Vega_Prime_22_0\include\vegaprime\vpEnvSun.h`
- `D:\Presagis\Suite22\Ondulus_IR_22_0\include\vpOndulusPBLightSource.h`
- `D:\Presagis\Suite22\Ondulus_IR_22_0\include\Ondulus\PB\PBLightSource.h`
- `D:\Presagis\Suite22\Ondulus_IR_22_0\include\Ondulus\PB\Constants.h`
- `D:\Presagis\Suite22\Ondulus_IR_22_0\include\Ondulus\Atmosphere\AmbientConditionsSystem.h`
- `D:\Presagis\Suite22\Ondulus_IR_22_0\include\Ondulus\IRSurfaceThermal\HeatTransferSystem.h`
- `D:\Presagis\Suite22\Ondulus_IR_22_0\include\Ondulus\IRSurfaceThermal\ShadowSystem.h`
- `D:\Presagis\Suite22\Ondulus_IR_22_0\include\Ondulus\Material\RawMaterial.h`
- `D:\Presagis\Suite22\Ondulus_IR_22_0\docs\help\Ambient_Conditions_subsystem.html`
- `...\Atmospheric_Transmittance_subsystem.html`、`Heat_Transfer_subsystem.html`、`Shadow_subsystem.html`、`Reflected_Radiance_subsystem.html`
- `...\Thermal_Reflections.html`、`New_Light_Model.html`、`Enhanced_VP_Light_Point.html`
- `D:\Presagis\Suite22\Ondulus_IR_22_0\appdata\ondulus_ir_viewer\CDB_*.acf`
