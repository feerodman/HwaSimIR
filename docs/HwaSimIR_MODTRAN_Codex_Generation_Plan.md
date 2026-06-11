# HwaSimIR MODTRAN 多条件大气数据自动生成计划（给 Codex 执行）

生成目标：用 PcModWin5/MODTRAN5 在本地 Windows 机器上批量生成 HwaSimIR 可用的大气查找表，覆盖 0.3–15 µm，并按 VIS、NIR、SWIR、MWIR、LWIR 常用波段预积分。

> 重要限制：不要让 HwaSimIR 实时调用 MODTRAN。MODTRAN/PcModWin5 用于离线生成表；HwaSimIR 运行时只读取 CSV/LUT 并插值。

---

## 1. Codex 是否可以自动做

可以，但建议分成两个层级：

1. **Codex 自动化脚本开发**：完全适合。让 Codex 在 HwaSimIR 仓库里编写 case manifest、MODTRAN 输入模板替换脚本、批量运行 PowerShell、TAPE7/MODOUT 解析器、CSV/LUT 生成器和检查脚本。
2. **MODTRAN 大批量真实运行**：只能在本机 Windows + 已授权 MODTRAN5/PcModWin5 环境里执行。不要用 Codex Cloud 直接跑，因为 cloud sandbox 没有你本机的 PcModWin5 安装、授权文件和 F 盘路径。

推荐执行方式：

```text
Codex App / Codex CLI / IDE Extension，本地 Windows 模式
工作目录：D:\HwaSimIR 或你的仓库根目录
MODTRAN/PcModWin 路径：F:\Programs\PcModWin5\
```

不推荐：

```text
Codex Cloud 直接运行 PcModWin5 GUI 或 MODTRAN5 大批量任务
```

原因：PcModWin5 是本地商业软件和 GUI/32-bit 程序，批量运行需要本机路径、授权、bin/usr 输出目录和可执行权限。

---

## 2. 最终产物

目标输出目录建议：

```text
HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/
  README.md
  case_grid/
    modtran_case_grid_pilot.yaml
    modtran_case_grid_production.yaml
    modtran_cases.csv
  templates/
    transmittance_template.modin
    thermal_radiance_template.modin
    direct_solar_irradiance_template.modin
    radiance_scattering_template.modin
  raw/
    <case_id>/
      modin
      MODOUT1
      MODOUT2
      tape7.scn
      run.log
      metadata.json
  spectral/
    modtran_spectral_index.csv
    <case_id>_spectral.csv
  band_lut/
    modtran_band_lut.csv
    modtran_band_lut_mwir_lwir.csv
    modtran_band_lut_allbands.csv
  qc/
    plots/
    warnings.csv
    failed_cases.csv
    summary.json
```

### 2.1 光谱级表结构

每个 case 一个光谱 CSV，避免单个超大文件难以打开：

```csv
case_id,mode,atmosphere_model,aerosol_model,visibility_km,water_vapor_scale,humidity_label,observer_alt_km,target_alt_km,range_km,view_zenith_deg,solar_zenith_deg,wavenumber_cm,wavelength_um,tau_up,tau_down,path_radiance,sky_radiance,solar_irradiance,total_radiance,source_file,units_note
```

字段说明：

- `tau_up`：目标/地物表面到传感器的上行透过率，主要来自 Transmittance 模式。
- `tau_down`：太阳到目标表面的下行透过率。若 MODTRAN 输出中不能直接取到，则先留空，并通过 `solar_irradiance` 参与 HwaSimIR 下行入射。
- `path_radiance`：目标到传感器路径中的大气程辐射，主要来自 Thermal Radiance 或 Radiance with Scattering。
- `sky_radiance`：天空视线背景辐亮度，来自 Radiance with Scattering 的专用 sky-view case；第一版可以为空或只生成典型角度表。
- `solar_irradiance`：到达目标高度的太阳直射谱辐照度，来自 Direct Solar Irradiance。

### 2.2 波段积分 LUT 表结构

HwaSimIR 实时优先读取这个表：

```csv
profile_id,band,lambda_low_um,lambda_high_um,response_source,atmosphere_model,aerosol_model,visibility_km,water_vapor_scale,humidity_label,observer_alt_km,target_alt_km,range_km,view_zenith_deg,solar_zenith_deg,tau_up_band,tau_down_band,path_radiance_band,sky_radiance_band,solar_irradiance_band,notes
```

波段定义：

```text
VIS   0.40–0.70 µm
NIR   0.70–1.10 µm
SWIR  1.10–2.50 µm
MWIR  3.00–5.00 µm
LWIR  8.00–14.00 µm
FULL  0.30–15.00 µm，只用于校验，不建议实时直接用
```

积分公式：

```text
X_band = ∫ S(λ) X(λ) dλ / ∫ S(λ) dλ
```

第一版没有真实传感器谱响应时，`S(λ)` 使用矩形响应。后续可从 `HwaSim_IR/Bin/Config/SensorWave/default_*.json` 读取真实或默认响应范围。

---

## 3. 参数范围设计

用户场景：中纬度夏季；观测/目标海拔高度 3–20 km；最大距离约 50 km；需要 0.3–15 µm，覆盖 VIS/NIR/SWIR/MWIR/LWIR。

### 3.1 大气模型

第一版：

```text
Midlatitude Summer
```

可选对照：

```text
1976 U.S. Standard
Tropical
```

### 3.2 气溶胶模型

第一版建议：

```text
Rural
Desert
Maritime
```

后续扩展：

```text
Urban
Fog / Haze，视 PcModWin5 下拉框具体名称而定
```

### 3.3 能见度 grid

```text
2 km
5 km
10 km
23 km
50 km
```

说明：`23 km` 对应 PcModWin5 常见 `Rural - VIS = 23km` 默认清晰大气；`2/5 km` 用于低能见度和雾霾效果；`50 km` 用于高空清晰条件。

### 3.4 湿度 / 水汽 grid

PcModWin5/MODTRAN5 不一定能用一个简单 `relative_humidity_percent` 字段直接代表真实垂直湿度 profile。第一版建议用水汽缩放因子表示：

```text
water_vapor_scale = 0.5, 1.0, 1.5, 2.0
humidity_label = dry, standard, humid, very_humid
```

Codex 不要伪造“真实 RH%”。如果 PcModWin5 模板中没有明确水汽缩放字段，则第一阶段只使用默认水汽 profile，并在 metadata 中记录：

```json
"humidity_control": "default_water_vapor_column_only"
```

后续如果需要真实逐高度湿度 profile，应增加用户自定义大气 profile 输入，例如：

```csv
alt_km,pressure_mb,temperature_k,relative_humidity_percent
0,1013.25,288.15,60
1,898.7,281.65,55
...
```

### 3.5 高度 grid

为了控制组合数量，分两级。

Pilot：

```text
observer_alt_km = 10
 target_alt_km  = 10
range_km        = 5, 20, 50
```

Production Minimal：

```text
observer_alt_km = 3, 5, 10, 15, 20
 target_alt_km  = 3, 5, 10, 15, 20
range_km        = 1, 2, 5, 10, 20, 30, 50
```

跳过不合法组合：如果 `range_km < abs(observer_alt_km - target_alt_km)`，该斜距不可能，必须跳过。

### 3.6 太阳高度 / 太阳天顶角 grid

Direct Solar Irradiance 和 Radiance with Scattering 需要太阳几何。建议用太阳高度角：

```text
solar_elevation_deg = 5, 15, 30, 45, 60, 75
solar_zenith_deg = 90 - solar_elevation_deg
```

夜间条件：

```text
solar_elevation_deg <= 0
solar_irradiance = 0
```

夜间不要运行 Direct Solar Irradiance，可直接在表里置零。

### 3.7 观测角 / 视线天顶角

第一版使用由高度差和斜距推导的几何角，或者让 MODTRAN 的 `Observer Height, Final Height and Range` 自动计算。

后续需要天空背景时，增加 sky-view 角度：

```text
sky_view_elevation_deg = 0, 5, 15, 30, 60, 90
```

---

## 4. 光谱范围设置

PcModWin5 第 3 页使用波数 `cm^-1`。换算：

```text
wavenumber_cm^-1 = 10000 / wavelength_um
wavelength_um = 10000 / wavenumber_cm^-1
```

建议按波段分开跑，不要一次性跑 0.3–15 µm 全范围。原因是可见光到长波红外跨度太大，一次跑会慢且输出难管理。

| band | wavelength_um | initial_wavenumber_cm | final_wavenumber_cm | increment_cm | FWHM_cm |
|---|---:|---:|---:|---:|---:|
| VIS | 0.40–0.70 | 14285.714 | 25000.000 | 20 或 50 | 同 increment |
| NIR | 0.70–1.10 | 9090.909 | 14285.714 | 10 或 20 | 同 increment |
| SWIR | 1.10–2.50 | 4000.000 | 9090.909 | 10 | 同 increment |
| MWIR | 3.00–5.00 | 2000.000 | 3333.333 | 5 或 10 | 同 increment |
| LWIR | 8.00–14.00 | 714.286 | 1250.000 | 2 或 5 | 同 increment |
| FULL_QC | 0.30–15.00 | 666.667 | 33333.333 | 50 或 100 | 同 increment |

注意：PcModWin5 界面通常要求初始波数 < 最终波数。对应波长显示会从长波到短波，例如 MWIR：`2000 → 3333 cm^-1` 等于 `5 → 3 µm`。

---

## 5. MODTRAN 运行模式与物理量对应

每组几何/气象/光谱条件，按需要运行以下模式。

### 5.1 Transmittance

用途：生成 `tau_up`，必要时也可作为近似 `tau_down`。

PcModWin5：

```text
Mode of Execution = Transmittance
Path Type = Observer Height, Final Height and Range
```

解析目标列：

```text
TOTAL TRANS / COMBIN TRANS / TRANS，按实际 TAPE7 表头判断
```

### 5.2 Thermal Radiance

用途：生成 MWIR/LWIR 的 `path_radiance` 和大气热辐射项。

PcModWin5：

```text
Mode of Execution = Thermal Radiance
```

解析目标列：

```text
PATH THERMAL / PATH RAD / THRML SCT / TOTAL RAD，按实际 TAPE7 表头判断
```

如果只有总辐亮度，需要在 metadata 中说明列含义，不要把无法确认的列强行命名成 path_radiance。

### 5.3 Direct Solar Irradiance

用途：生成 `solar_irradiance`，用于 VIS/NIR/SWIR/MWIR 白天太阳直射反射项。

PcModWin5：

```text
Mode of Execution = Direct Solar Irradiance
Solar Zenith Angle = 90 - solar_elevation_deg
Target / ground altitude = target_alt_km
```

解析目标列：

```text
SOLAR IRRADIANCE / DIRECT SOLAR / TOA-SURFACE IRRADIANCE，按实际 TAPE7 表头判断
```

### 5.4 Radiance with Scattering

用途：生成 VIS/NIR/SWIR 的散射路径辐射和 sky_radiance；MWIR 只在 3–5 µm 白天、低能见度时选跑；LWIR 第一版可不跑。

PcModWin5：

```text
Mode of Execution = Radiance with Scattering
Multiple Scattering = VIS/NIR 建议开启；SWIR 可选；MWIR/LWIR 第一版可关闭或不跑
```

sky_radiance 建议单独建 sky-view case，不要混在目标视线 case 里。

---

## 6. 分阶段生成策略

### 阶段 A：人工生成 4 个模板

先用 PcModWin5 GUI 手工生成并保存 4 个模板输入文件：

```text
transmittance_template.modin
thermal_radiance_template.modin
direct_solar_irradiance_template.modin
radiance_scattering_template.modin
```

模板条件先统一用：

```text
Atmosphere = Midlatitude Summer
Aerosol = Rural, VIS = 23 km
Observer Height = 10 km
Target Height = 10 km
Range = 20 km
Band = MWIR, 3–5 µm, 2000–3333.333 cm^-1
Water Vapor = default
Ozone = default
CO2 = 默认 360 ppmv，或保留 PcModWin5 当前默认
Cloud/Rain = No Clouds or Rain
```

运行一次，确认生成：

```text
F:\Programs\PcModWin5\usr\MODOUT1
F:\Programs\PcModWin5\usr\MODOUT2
F:\Programs\PcModWin5\usr\tape7.scn
```

把对应 `F:\Programs\PcModWin5\bin\modin` 或 PcModWin5 保存出的输入文件复制到 HwaSimIR 的 templates 目录。

### 阶段 B：Codex 解析模板并生成 pilot case

Codex 任务：

1. 读取模板文件。
2. 识别或记录需要替换的字段位置：模式、大气模型、气溶胶、能见度、观测高度、目标高度、斜距、波数范围、步长、太阳天顶角、水汽缩放。
3. 生成 pilot case manifest。
4. 只生成输入文件，不批量运行。
5. 输出 `modtran_cases.csv`。

Pilot case 数量控制在 20 个以内。

### 阶段 C：Codex 本地批量运行 pilot

Codex 任务：

1. 用 PowerShell 逐个复制 case 输入到 PcModWin5/MODTRAN bin 目录。
2. 调用 MODTRAN5 32-bit 命令行可执行文件。
3. 等待输出完成。
4. 把 `MODOUT1/MODOUT2/tape7.scn/run.log` 复制到每个 case 的 raw 目录。
5. 失败时记录 failed_cases.csv，不中断整个批处理。

注意：如果找不到命令行 exe 或 PcModWin5 只能通过 GUI Run Model 运行，Codex 应停止并报告，不要尝试盲目点击 GUI。

### 阶段 D：解析 TAPE7 / MODOUT2

Codex 任务：

1. 自动识别 MODOUT2/TAPE7 表头。
2. 将波数转换为波长。
3. 根据 mode 映射候选列。
4. 生成每个 case 的 `<case_id>_spectral.csv`。
5. 生成 QC：波长范围、数值范围、NaN、负值、透过率是否在 0–1。

### 阶段 E：生成 HwaSimIR LUT

Codex 任务：

1. 读取所有 spectral case。
2. 按 VIS/NIR/SWIR/MWIR/LWIR 做矩形响应积分。
3. 合并同一 profile 下的 Transmittance、Thermal Radiance、Direct Solar Irradiance、Radiance with Scattering。
4. 输出 `modtran_band_lut.csv`。
5. 增加 `README.md` 说明每个字段、单位和插值方法。

### 阶段 F：接入 HwaSimIR，先不改 shader

Codex 任务：

1. 新增或扩展 `IRAtmosphereDatabase` 的 CSV loader。
2. 保留现有 `transmittance_0.3_15.txt` fallback。
3. 启动时优先读取 `Config/Atmosphere/MODTRAN/band_lut/modtran_band_lut.csv`。
4. 根据运行时 `band/range/visibility/observer_alt/target_alt` 做最近邻或线性插值。
5. 打印当前命中的 MODTRAN profile。
6. 增加 smoke test：MWIR/LWIR，range = 5/20/50 km 时 tau/path radiance 有变化。

---

## 7. 组合数量控制

Production Minimal 的理论组合数可能很大：

```text
atmosphere 1
× aerosol 3
× visibility 5
× water_vapor 4
× observer_alt 5
× target_alt 5
× range 7
× band 5
× mode 3~4
≈ 63,000+ runs before invalid geometry filtering
```

这对 MODTRAN5 GUI/32-bit 很重，不建议一次性跑完。

推荐先做最小生产表：

```text
atmosphere = Midlatitude Summer
 aerosol  = Rural, Desert
visibility = 5, 10, 23, 50
water_vapor_scale = 1.0
observer_alt_km = 5, 10, 15, 20
target_alt_km = 3, 5, 10, 15, 20
range_km = 5, 10, 20, 30, 50
band = MWIR, LWIR
mode = Transmittance, Thermal Radiance
```

这样先服务热红外主链路。VIS/NIR/SWIR 和太阳项第二批再生成。

建议批次：

1. `batch_001_mwir_lwir_clear`: MWIR/LWIR，Rural/Desert，VIS=23/50，water=1.0。
2. `batch_002_visibility`: MWIR/LWIR，VIS=5/10/23/50。
3. `batch_003_solar`: VIS/NIR/SWIR/MWIR，Direct Solar Irradiance，solar_elevation=5/15/30/45/60/75。
4. `batch_004_swir_scattering`: VIS/NIR/SWIR，Radiance with Scattering。
5. `batch_005_humidity`: 对核心高度/距离补 water_vapor_scale=0.5/1.5/2.0。

---

## 8. 单位规范

HwaSimIR 内部建议统一：

```text
wavelength_um: µm
wavenumber_cm: cm^-1
tau: dimensionless
radiance: W/(m^2 sr µm)
irradiance: W/(m^2 µm)
height/range: km in LUT metadata, m in runtime state; loader 内部转换清楚
```

如果 MODTRAN 输出是：

```text
radiance per cm^-1: W/(cm^2 sr cm^-1)
```

转换到波长域：

```text
L_lambda[W/(m^2 sr µm)] = L_nu[W/(cm^2 sr cm^-1)] * 1e8 / lambda_um^2
```

如果 MODTRAN 输出已经是 `W/(m^2 sr µm)` 或其他单位，Codex 不得重复转换。解析器必须把表头和单位写进 `units_note`。

---

## 9. 质量检查

Codex 必须生成以下 QC：

```text
tools/stage3_modtran_check.ps1
```

检查项：

1. 模板文件存在。
2. case manifest 可读。
3. 每个 raw case 有 `MODOUT1/MODOUT2/run.log/metadata.json`。
4. MODTRAN 日志包含 `MODTRAN completed successfully`。
5. 允许 `Warning: No tape8 file generated`，但必须记录。
6. spectral CSV 的 `wavelength_um` 覆盖对应 band。
7. `tau_up` 范围在 0–1，超出时报错。
8. radiance/irradiance 不应全空、不应全 0，夜间太阳除外。
9. band LUT 每个 profile 至少有 `tau_up_band`；MWIR/LWIR 应有 `path_radiance_band`；白天 VIS/NIR/SWIR/MWIR 应有 `solar_irradiance_band`。
10. 输出 `qc/summary.json`，包括成功、失败、warning、缺列统计。

---

## 10. HwaSimIR 插值策略

第一版使用简单策略：

1. band 精确匹配。
2. atmosphere/aerosol 使用最近或指定 profile。
3. visibility/range/observer_alt/target_alt 使用线性或双线性/多线性插值。
4. 找不到 profile 时 fallback 到最近邻，并输出 warning。
5. 绝对不允许因为缺表让程序崩溃；必须 fallback 到旧 `transmittance_0.3_15.txt` 或经验模型。

建议接口：

```cpp
struct IRAtmosphereLookupKey {
    IRBand band;
    double visibilityKm;
    double observerAltKm;
    double targetAltKm;
    double rangeKm;
    double solarZenithDeg;
    std::string atmosphereModel;
    std::string aerosolModel;
};

struct IRAtmosphereLookupResult {
    double tauUpBand;
    double tauDownBand;
    double pathRadianceBand;
    double skyRadianceBand;
    double solarIrradianceBand;
    bool fromModtranLut;
    std::string profileId;
};
```

---

## 11. 给 Codex 的执行 Prompt

下面整段可以直接贴给 Codex。

```text
你在 HwaSimIR 仓库中工作。目标是为 HwaSimIR 建立 MODTRAN5/PcModWin5 离线大气表自动生成工具链，不要一次性重构红外渲染主链路。当前本机 PcModWin5/MODTRAN5 安装路径大概是 F:\Programs\PcModWin5\，HwaSimIR 仓库在 D:\HwaSimIR。请优先使用本地 Windows PowerShell/Python，不要依赖互联网，不要提交 MODTRAN exe、授权文件或原始安装数据。

背景：HwaSimIR 当前阶段 3 大气模型只有单条件谱透过率和经验能见度缩放；后续需要按不同距离、湿度/水汽、能见度、气溶胶、观测高度、目标高度生成 tau_up/tau_down/path_radiance/sky_radiance/solar_irradiance 表。场景为中纬度夏季，观测/目标海拔 3–20 km，距离最大 50 km，光谱范围 0.3–15 µm，常用波段 VIS/NIR/SWIR/MWIR/LWIR。

请按以下步骤执行：

1. 先创建目录：
   HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/
   tools/modtran/

2. 编写文档：
   HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/README.md
   说明数据表字段、单位、生成流程、PcModWin5 路径、不要实时调用 MODTRAN 的原则。

3. 创建 case grid YAML：
   tools/modtran/modtran_case_grid_pilot.yaml
   tools/modtran/modtran_case_grid_production_minimal.yaml
   Pilot 先只覆盖：
   - atmosphere = Midlatitude Summer
   - aerosol = Rural
   - visibility = 23 km
   - water_vapor_scale = 1.0 或 default
   - observer_alt_km = 10
   - target_alt_km = 10
   - range_km = 5, 20, 50
   - band = MWIR, LWIR
   - mode = Transmittance, Thermal Radiance
   Production Minimal 先覆盖 MWIR/LWIR 主链路，不要全量爆炸。

4. 编写 generate_modtran_cases.py：
   - 读取 YAML。
   - 根据 band 自动计算 wavenumber 初始/终止/步长。
   - 生成唯一 case_id。
   - 跳过 range_km < abs(observer_alt_km - target_alt_km) 的不合法斜距。
   - 输出 modtran_cases.csv 和 raw/<case_id>/metadata.json。
   - 暂时如果没有模板，不要失败；生成 TODO 提示和空输入草稿。

5. 模板处理：
   - 预留 templates/transmittance_template.modin、thermal_radiance_template.modin、direct_solar_irradiance_template.modin、radiance_scattering_template.modin。
   - 如果模板存在，请分析其中可替换字段，写一个 apply_modtran_template.py，把 observer height、target height、range、bandpass、mode、atmosphere、aerosol、visibility、solar zenith 写入 case 的 modin。
   - 如果无法可靠修改模板，停止并写明需要用户手工提供 PcModWin5 保存出的模板，不要乱写 MODTRAN card。

6. 编写 run_modtran_cases.ps1：
   - 参数：-CasesCsv, -PcModWinRoot, -MaxCases, -DryRun。
   - 默认 PcModWinRoot = F:\Programs\PcModWin5。
   - DryRun 只打印将要执行的 case。
   - 非 DryRun 时，将 case 的 modin 复制到 PcModWin5 bin 目录，并调用可用的 MODTRAN5 命令行 exe。
   - 自动查找可能的 exe 名称，但找不到时必须停止并报告。
   - 每个 case 运行后复制 MODOUT1、MODOUT2、tape7.scn、new.fl7、run.log 到 raw/<case_id>/。
   - 失败写 qc/failed_cases.csv，不能丢失日志。
   - 不要自动点击 PcModWin5 GUI。

7. 编写 parse_tape7.py：
   - 输入 raw/<case_id>/MODOUT2 或 TAPE7。
   - 自动识别表头。
   - 输出 spectral/<case_id>_spectral.csv。
   - 波数转波长。
   - mode=Transmittance 时优先找 TOTAL TRANS / COMBIN TRANS / TRANS 作为 tau_up。
   - mode=Thermal Radiance 时识别 path radiance/thermal radiance/total radiance 候选列，但无法确认时保留原列并在 units_note 中说明，不要假装正确。
   - mode=Direct Solar Irradiance 时识别 solar irradiance 候选列。
   - 输出 spectral/modtran_spectral_index.csv。

8. 编写 build_hwasimir_tables.py：
   - 读取 spectral CSV。
   - 按 VIS 0.4–0.7、NIR 0.7–1.1、SWIR 1.1–2.5、MWIR 3–5、LWIR 8–14 做矩形响应积分。
   - 合并同 profile 的 tau_up、path_radiance、solar_irradiance 等。
   - 输出 band_lut/modtran_band_lut.csv。
   - 保留 source_case_ids，方便追溯。

9. 编写 tools/stage3_modtran_check.ps1：
   - 检查目录、YAML、case CSV、raw 输出、spectral 输出、band LUT。
   - 检查 tau 范围 0–1。
   - 检查 MWIR/LWIR 有 path_radiance。
   - 允许 No tape8 file generated warning，但必须记录。

10. 只在用户明确确认后再接入 C++：
    - 新增/扩展 IRAtmosphereDatabase 的 MODTRAN LUT loader。
    - 先只读取 modtran_band_lut.csv 并打印命中的 profile，不改变 shader。
    - 保留旧 transmittance_0.3_15.txt fallback。

验收标准：
- PowerShell DryRun 可以生成 pilot case 列表。
- 如果用户提供了至少一个现有 MODOUT2，parse_tape7.py 能解析成 spectral CSV。
- build_hwasimir_tables.py 能生成 band LUT。
- stage3_modtran_check.ps1 能输出 summary.json。
- 不提交 MODTRAN 程序、授权、安装目录大文件。
- 所有新增脚本有用法说明和失败提示。
```

---

## 12. 首次手工操作清单

在把 prompt 交给 Codex 前，建议你先做这几件事：

1. 在 PcModWin5 里用 GUI 分别跑通：

```text
Transmittance / MWIR / obs=10 km / target=10 km / range=20 km
Thermal Radiance / MWIR / 同条件
Direct Solar Irradiance / MWIR / 同条件
Radiance with Scattering / VIS 或 SWIR / 同条件
```

2. 每次运行后，把 `F:\Programs\PcModWin5\bin\modin` 复制出来作为模板。

3. 把 `F:\Programs\PcModWin5\usr\MODOUT2` 也复制一份给 Codex 用来开发解析器。

4. 先让 Codex 做 dry run 和解析器，不要一上来跑 63000 个 case。

5. 阶段 A：读取人工保存的 modin 模板和 MODOUT2 样例，开发解析器。
   阶段 B：实现 dry run，只生成 case_manifest.csv 和 modin 文件，不调用 MODTRAN。
   阶段 C：运行不超过 50 个 pilot case，验证解析结果和物理趋势。
   阶段 D：经用户确认后，运行 production sparse grid，默认目标不超过 3000 个 MODTRAN run。
   阶段 E：不要默认保存每个 case 的完整 MODOUT1/MODOUT2，只保存 failed cases 和 sample cases；最终输出 path_lut、solar_lut、sky_lut、band_lut、manifest、qc_report。


---

## 13. 建议命名规范

case_id：

```text
<mode>_<band>_<atm>_<aer>_vis<visibility>_wv<water>_obs<obs>_tar<tar>_rng<range>_sunz<sunz>
```

示例：

```text
trans_mwir_midlat_sum_rural_vis23_wv1p0_obs10_tar10_rng20_sunz45
thermal_lwir_midlat_sum_desert_vis10_wv1p0_obs15_tar5_rng30
solar_swir_midlat_sum_rural_vis23_wv1p0_tar10_sunz30
```

---

## 14. 后续接入 HwaSimIR 的优先级

1. **先接 MWIR/LWIR 的 tau_up + path_radiance**：最能改善热红外远距离效果。
2. 再接 **VIS/NIR/SWIR/MWIR 的 solar_irradiance**：改善白天太阳反射项。
3. 再接 **sky_radiance / scattering**：改善天空、雾霾、低能见度背景。
4. 最后做 **湿度 profile 和更多气溶胶模型**。

