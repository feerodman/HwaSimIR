# HwaSimIR MODTRAN 数据生成计划：NIR/MWIR 优先版

适用场景：HwaSimIR Panda3D 实时红外/近红外成像仿真。  
重点波段：NIR 近红外、MWIR 中波红外。  
兼顾波段：VIS、SWIR、LWIR。  
典型环境：中纬度夏季。  
观测/目标海拔：3 km ~ 20 km。  
最大传感器-目标距离：50 km。  
目标：用 PcModWin5/MODTRAN5 离线生成大气查找表，HwaSimIR 运行时只读取预处理后的 LUT，不实时调用 MODTRAN。

---

## 1. 总体原则

不要把 VIS、NIR、SWIR、MWIR、LWIR 按同等精度、同等模式全部暴力扫表。  
本项目以 NIR 和 MWIR 为主，应采用“分波段、分物理机制、分优先级”的数据生成策略：

```text
一级优先：
  NIR  0.70–1.10 um
  MWIR 3.00–5.00 um

二级保底：
  VIS  0.40–0.70 um
  SWIR 1.10–2.50 um
  LWIR 8.00–14.00 um
```

最终 HwaSimIR 不应直接读取几万个 MODOUT2 文件，而应读取少量处理后的表：

```text
Config/Atmosphere/MODTRAN/processed/
  path_lut_spectral.csv
  solar_lut_spectral.csv
  sky_lut_spectral.csv
  band_lut.csv
  manifest.csv
  qc_report.md
```

---

## 2. 各波段物理重点

| 波段 | 范围 | 成像机制 | 必要 MODTRAN 数据 | 可后置数据 |
|---|---:|---|---|---|
| VIS | 0.40–0.70 um | 太阳反射、天空散射 | tau_up, tau_down, solar_irradiance, sky_radiance | thermal_radiance |
| NIR | 0.70–1.10 um | 太阳反射为主，弱热辐射 | tau_up, tau_down, solar_irradiance, sky_radiance/path scattering | thermal_radiance |
| SWIR | 1.10–2.50 um | 太阳反射为主，高温目标可见 | tau_up, tau_down, solar_irradiance, scattering | thermal_radiance 可选 |
| MWIR | 3.00–5.00 um | 热辐射 + 太阳反射 | tau_up, path_radiance, tau_down, solar_irradiance | sky_radiance 细化 |
| LWIR | 8.00–14.00 um | 热辐射为主 | tau_up, path_radiance | solar_irradiance, scattering |

---

## 3. PcModWin5 手工模板要求

在交给 Codex 批量自动化之前，必须先在 PcModWin5 GUI 中手工跑通少量 case。  
这样做的目的不是生成正式数据，而是得到可信的 `modin` 输入模板和 `MODOUT2` 输出样例，供 Codex 写解析器和参数替换脚本。

### 3.1 必须手工跑通的 6 个模板

建议统一条件：

```text
Atmosphere: Mid-Latitude Summer
Observer altitude: 10 km
Target altitude:   10 km
Range:             20 km
Visibility:        23 km
Aerosol:           Rural
Humidity:          default
```

手工运行：

```text
1. Transmittance / NIR  / 0.70–1.10 um
2. Direct Solar Irradiance / NIR / 0.70–1.10 um
3. Radiance with Scattering / NIR / 0.70–1.10 um

4. Transmittance / MWIR / 3.00–5.00 um
5. Thermal Radiance / MWIR / 3.00–5.00 um
6. Direct Solar Irradiance / MWIR / 3.00–5.00 um
```

可选补充模板：

```text
7. Transmittance / LWIR / 8.00–14.00 um
8. Thermal Radiance / LWIR / 8.00–14.00 um
9. Transmittance / VIS / 0.40–0.70 um
10. Transmittance / SWIR / 1.10–2.50 um
```

每次运行后保存：

```text
F:\Programs\PcModWin5\bin\modin
F:\Programs\PcModWin5\usr\MODOUT1
F:\Programs\PcModWin5\usr\MODOUT2
```

保存到项目目录：

```text
ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/raw/templates/
  NIR_transmittance_modin.txt
  NIR_transmittance_MODOUT2.txt
  NIR_solar_modin.txt
  NIR_solar_MODOUT2.txt
  NIR_scattering_modin.txt
  NIR_scattering_MODOUT2.txt
  MWIR_transmittance_modin.txt
  MWIR_transmittance_MODOUT2.txt
  MWIR_thermal_modin.txt
  MWIR_thermal_MODOUT2.txt
  MWIR_solar_modin.txt
  MWIR_solar_MODOUT2.txt
```

---

## 4. 光谱范围与波数设置

MODTRAN/PcModWin5 通常使用波数 cm^-1。换算公式：

```text
wavenumber_cm^-1 = 10000 / wavelength_um
wavelength_um = 10000 / wavenumber_cm^-1
```

| 波段 | 波长范围 um | 波数范围 cm^-1 | 建议 increment | 建议 FWHM |
|---|---:|---:|---:|---:|
| VIS | 0.40–0.70 | 25000–14285.7 | 10 或 20 | 同 increment |
| NIR | 0.70–1.10 | 14285.7–9090.9 | 5 或 10 | 同 increment |
| SWIR | 1.10–2.50 | 9090.9–4000 | 5 或 10 | 同 increment |
| MWIR | 3.00–5.00 | 3333.3–2000 | 5 或 10 | 同 increment |
| LWIR | 8.00–14.00 | 1250–714.3 | 2 或 5 | 同 increment |

注意：某些 PcModWin5 界面要求 Initial Frequency 小于 Final Frequency。  
因此按波数从小到大填写：

```text
NIR:  9090.9  -> 14285.7
MWIR: 2000.0  -> 3333.3
LWIR: 714.3   -> 1250.0
```

---

## 5. 数据生成优先级

### 5.1 一级优先：NIR

NIR 主要做太阳反射成像，不重点做热辐射。

必须生成：

```text
Transmittance
Direct Solar Irradiance
Radiance with Scattering
```

输出目标：

```text
tau_up
tau_down
solar_irradiance
sky_radiance
path_scattering_radiance
```

NIR 暂时不要求：

```text
Thermal Radiance
path_thermal_radiance
目标自身热辐射精细化
```

### 5.2 一级优先：MWIR

MWIR 同时包含热辐射和太阳反射，是 HwaSimIR 的核心波段之一。

必须生成：

```text
Transmittance
Thermal Radiance
Direct Solar Irradiance
```

建议生成：

```text
Radiance with Scattering
```

输出目标：

```text
tau_up
path_radiance
tau_down
solar_irradiance
sky_radiance
```

### 5.3 二级保底：VIS

VIS 可作为可见光/近可见光预览或反射成像对照。

生成：

```text
Transmittance
Direct Solar Irradiance
Radiance with Scattering
```

不生成：

```text
Thermal Radiance
```

### 5.4 二级保底：SWIR

SWIR 主要太阳反射，高温源可见但不是第一阶段核心。

生成：

```text
Transmittance
Direct Solar Irradiance
Radiance with Scattering
```

可选：

```text
Thermal Radiance，仅用于火焰/爆炸/高温尾喷口测试
```

### 5.5 二级保底：LWIR

LWIR 主要热成像，和 MWIR 类似但本项目优先级低于 MWIR。

生成：

```text
Transmittance
Thermal Radiance
```

可不生成或低优先级生成：

```text
Direct Solar Irradiance
Radiance with Scattering
```

---

## 6. 推荐参数网格

### 6.1 高度网格

不要做 3~20 km 的所有组合。先做稀疏高度对：

```text
same_altitude_pairs:
  (3, 3)
  (5, 5)
  (10, 10)
  (15, 15)
  (20, 20)

cross_altitude_pairs:
  (5, 3)
  (10, 5)
  (15, 10)
  (20, 10)
  (20, 15)
  (10, 3)
  (20, 3)
```

单位：km。

### 6.2 距离网格

```text
range_km:
  1
  2
  5
  10
  20
  35
  50
```

### 6.3 能见度网格

```text
visibility_km:
  2
  5
  10
  23
  50
```

### 6.4 气溶胶模型

第一版：

```text
Rural
```

第二版：

```text
Rural
Desert
```

中纬度夏季陆地测试优先 Rural。  
干燥沙漠/半沙漠任务可增加 Desert。

### 6.5 湿度/水汽

不要直接扫百分比湿度，优先用 profile 级别：

```text
humidity_profile:
  default
  dry
  humid
```

如果 PcModWin5 模板里水汽列不方便自动化，第一版只做：

```text
default
```

待解析器和 LUT 稳定后再增加 dry/humid。

### 6.6 太阳天顶角

只用于 solar_lut 和 sky_lut，不用于 path_lut 全乘。

```text
solar_zenith_deg:
  20
  45
  70
```

第一版只做：

```text
45
```

---

## 7. Case 数量控制

### 7.1 Pilot 阶段

只跑 50~100 个 case。

建议 pilot：

```text
band:
  NIR, MWIR

altitude_pair:
  (10, 10)

range_km:
  1, 5, 20, 50

visibility_km:
  5, 23, 50

aerosol:
  Rural

humidity:
  default

modes:
  NIR:  Transmittance, Direct Solar Irradiance, Radiance with Scattering
  MWIR: Transmittance, Thermal Radiance, Direct Solar Irradiance
```

数量约：

```text
NIR:  4 range × 3 vis × 3 modes = 36
MWIR: 4 range × 3 vis × 3 modes = 36
Total ≈ 72
```

### 7.2 Production Sparse Grid

第一版 production 目标不要超过 3000 个 MODTRAN run。

建议：

```text
NIR path/solar/sky:
  altitude_pair: 12
  range:         7
  visibility:    5
  aerosol:       1
  humidity:      1
  modes:         3
  total ≈ 1260

MWIR path/solar:
  altitude_pair: 12
  range:         7
  visibility:    5
  aerosol:       1
  humidity:      1
  modes:         3
  total ≈ 1260

VIS/SWIR/LWIR 保底：
  每个波段只做 altitude_pair=(10,10), range=(1,5,20,50), visibility=(5,23,50)
  VIS:  4×3×3 modes = 36
  SWIR: 4×3×3 modes = 36
  LWIR: 4×3×2 modes = 24
```

总量约：

```text
1260 + 1260 + 36 + 36 + 24 = 2616
```

---

## 8. 输出文件策略

不要默认保存每个 case 的完整 `MODOUT1/MODOUT2`。  
每个 case 运行后立即解析，追加到统一 CSV 或 SQLite。

保留：

```text
raw/templates/     人工模板和样例输出
raw/samples/       每种 band/mode 保存少量样例
raw/failed/        失败 case 的 modin、MODOUT1、MODOUT2
generated/modin/   dry run 阶段生成的 modin，可清理
processed/         最终 HwaSimIR 使用的数据表
```

最终输出：

```text
processed/path_lut_spectral.csv
processed/solar_lut_spectral.csv
processed/sky_lut_spectral.csv
processed/band_lut.csv
processed/manifest.csv
processed/qc_report.md
```

---

## 9. 标准表结构

### 9.1 path_lut_spectral.csv

用于上行路径：

```csv
case_id,band,mode,atmosphere_model,aerosol_model,humidity_profile,visibility_km,observer_alt_km,target_alt_km,range_km,wavenumber_cm,wavelength_um,tau_up,path_radiance,unit_radiance,source_file
```

其中：

```text
Transmittance mode:
  tau_up 有值
  path_radiance 为空

Thermal Radiance mode:
  tau_up 可重复提取或为空
  path_radiance 有值
```

### 9.2 solar_lut_spectral.csv

用于太阳直射到目标表面：

```csv
case_id,band,mode,atmosphere_model,aerosol_model,humidity_profile,visibility_km,target_alt_km,solar_zenith_deg,wavenumber_cm,wavelength_um,tau_down,solar_irradiance,unit_irradiance,source_file
```

### 9.3 sky_lut_spectral.csv

用于天空散射/路径散射：

```csv
case_id,band,mode,atmosphere_model,aerosol_model,humidity_profile,visibility_km,observer_alt_km,target_alt_km,range_km,solar_zenith_deg,view_zenith_deg,wavenumber_cm,wavelength_um,sky_radiance,path_scattering_radiance,unit_radiance,source_file
```

### 9.4 band_lut.csv

用于 HwaSimIR 实时快速查表：

```csv
band,atmosphere_model,aerosol_model,humidity_profile,visibility_km,observer_alt_km,target_alt_km,range_km,solar_zenith_deg,tau_up_band,tau_down_band,path_radiance_band,sky_radiance_band,solar_irradiance_band
```

---

## 10. QC 检查

Codex 必须生成 `qc_report.md`，至少检查：

```text
1. tau 是否在 0~1。
2. 同一高度/能见度下，range 增大时 tau_up 大体下降。
3. 同一距离/高度下，visibility 增大时 tau_up 大体上升。
4. MWIR/LWIR path_radiance 通常随距离增大而增大或趋于饱和。
5. NIR solar_irradiance 白天应明显非零。
6. NIR thermal_radiance 不作为主结果。
7. wavelength_um 与 wavenumber_cm 换算一致。
8. 每种 band/mode 至少解析到有效数据行。
9. 输出列单位明确。
10. 失败 case 单独记录，不混入 processed 表。
```

---

## 11. 给 Codex 的执行 Prompt

下面内容可直接交给 Codex 执行。

```text
你正在维护 D:\HwaSimIR 项目。目标是为 HwaSimIR 生成 PcModWin5/MODTRAN5 离线大气查找表。项目以 NIR 和 MWIR 为主，VIS/SWIR/LWIR 也需要保底支持。

重要约束：
1. 不要直接运行全量 MODTRAN case。
2. 不要一开始生成几万文件。
3. 先基于人工保存的 modin 模板和 MODOUT2 样例开发解析器。
4. 先实现 dry run，只生成 manifest 和 modin，不调用 MODTRAN。
5. pilot 阶段最多运行 100 个 case。
6. production sparse grid 默认不超过 3000 个 MODTRAN run。
7. 最终 HwaSimIR 读取 processed 汇总表，不读取每个 case 的 MODOUT2 原始文件。
8. 当前重点波段为 NIR 0.70–1.10 um 和 MWIR 3.00–5.00 um。
9. VIS/SWIR/LWIR 需要保底表，但不要和 NIR/MWIR 同等密度扫表。
10. 所有脚本必须支持 Windows PowerShell，路径中包含 F:\Programs\PcModWin5。

请完成以下任务：

阶段 A：目录和配置
- 创建目录：
  ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/raw/templates
  ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/raw/samples
  ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/raw/failed
  ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/generated/modin
  ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/processed
- 创建配置文件：
  tools/modtran/modtran_grid_nir_mwir_priority.json
- 配置中包含：
  atmosphere_model = Mid-Latitude Summer
  altitude_pairs = [(3,3),(5,5),(10,10),(15,15),(20,20),(5,3),(10,5),(15,10),(20,10),(20,15),(10,3),(20,3)]
  range_km = [1,2,5,10,20,35,50]
  visibility_km = [2,5,10,23,50]
  aerosol_model = ["Rural"]
  humidity_profile = ["default"]
  solar_zenith_deg = [20,45,70]
  priority_bands:
    NIR:  0.70-1.10 um, modes=[Transmittance, DirectSolarIrradiance, RadianceWithScattering]
    MWIR: 3.00-5.00 um, modes=[Transmittance, ThermalRadiance, DirectSolarIrradiance]
  support_bands:
    VIS:  0.40-0.70 um, modes=[Transmittance, DirectSolarIrradiance, RadianceWithScattering], sparse_only=true
    SWIR: 1.10-2.50 um, modes=[Transmittance, DirectSolarIrradiance, RadianceWithScattering], sparse_only=true
    LWIR: 8.00-14.00 um, modes=[Transmittance, ThermalRadiance], sparse_only=true

阶段 B：解析器
- 在 tools/modtran/ 下创建 Python 解析脚本 parse_modout2.py。
- 解析人工保存的 MODOUT2 样例。
- 自动识别 wavenumber 列，计算 wavelength_um = 10000 / wavenumber_cm。
- 对 Transmittance 提取 total transmittance 作为 tau。
- 对 Thermal Radiance 提取 path radiance 或 total/path thermal radiance，列名必须写入 qc_report。
- 对 Direct Solar Irradiance 提取 solar irradiance。
- 对 Radiance with Scattering 提取 sky/path scattering radiance。
- 如果列名不确定，不要猜，输出错误并提示用户贴出表头。

阶段 C：dry run
- 创建 build_modtran_cases.py。
- 支持 --dry-run。
- dry run 只生成 case_manifest.csv，不调用 MODTRAN。
- case_id 中包含 band、mode、obs、tar、range、vis、aerosol、humidity。
- 统计 case 数量，并在超过 3000 时直接失败，提示需要缩小 grid。
- 生成每个 case 的 modin 到 generated/modin，但不要运行。

阶段 D：pilot
- 创建 run_modtran_cases.ps1。
- 支持 -Pilot，只运行不超过 100 个 case。
- 每个 case：
  复制对应 modin 到 F:\Programs\PcModWin5\bin\modin
  调用 MODTRAN5/PcModWin5 能使用的命令行执行方式
  读取 F:\Programs\PcModWin5\usr\MODOUT2
  解析后追加到 processed CSV
  失败时把 modin、MODOUT1、MODOUT2 复制到 raw/failed
- 如果无法确认命令行调用方式，停止并输出说明，不要伪造结果。

阶段 E：processed 表和 QC
- 输出：
  processed/path_lut_spectral.csv
  processed/solar_lut_spectral.csv
  processed/sky_lut_spectral.csv
  processed/band_lut.csv
  processed/manifest.csv
  processed/qc_report.md
- band_lut 用矩形响应积分；后续真实 S(lambda) 可替换。
- qc_report.md 必须包含：
  每个 band/mode 的行数
  tau 范围
  wavelength 范围
  距离增加时 tau_up 趋势
  能见度增加时 tau_up 趋势
  失败 case 列表
  无法识别的列名列表

阶段 F：HwaSimIR 接入准备
- 不修改 C++ 主逻辑。
- 只新增 docs/modtran_lut_format.md，说明 processed CSV 列意义、单位、插值方式。
- 只新增 tools/stage3_modtran_lut_check.ps1，用来检查 processed 表是否存在、列是否完整、数值范围是否合理。
- 不要删除现有 transmittance_0.3_15.txt；让新 LUT 作为可选增强数据源。

验收命令：
powershell -ExecutionPolicy Bypass -File tools\stage3_modtran_lut_check.ps1 -Strict
python tools/modtran/parse_modout2.py --input <sample_MODOUT2> --band MWIR --mode Transmittance --output <test_csv>
python tools/modtran/build_modtran_cases.py --config tools/modtran/modtran_grid_nir_mwir_priority.json --dry-run
```

---

## 12. 推荐先做的人工文件

在运行 Codex 前，请先人工准备这 6 组文件：

```text
raw/templates/NIR_transmittance_modin.txt
raw/templates/NIR_transmittance_MODOUT2.txt
raw/templates/NIR_solar_modin.txt
raw/templates/NIR_solar_MODOUT2.txt
raw/templates/NIR_scattering_modin.txt
raw/templates/NIR_scattering_MODOUT2.txt

raw/templates/MWIR_transmittance_modin.txt
raw/templates/MWIR_transmittance_MODOUT2.txt
raw/templates/MWIR_thermal_modin.txt
raw/templates/MWIR_thermal_MODOUT2.txt
raw/templates/MWIR_solar_modin.txt
raw/templates/MWIR_solar_MODOUT2.txt
```

最小化启动也可以只准备：

```text
MWIR_transmittance_modin.txt
MWIR_transmittance_MODOUT2.txt
MWIR_thermal_modin.txt
MWIR_thermal_MODOUT2.txt
NIR_transmittance_modin.txt
NIR_transmittance_MODOUT2.txt
```

但这样 NIR solar/scattering 和 MWIR solar 解析器要后续补齐。
