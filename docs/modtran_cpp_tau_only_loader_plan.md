# MODTRAN C++ Tau-Only Debug Loader Plan

本文只描述 B 线 `tau_up/tau_down` debug loader 设计，不修改 HwaSimIR C++、shader 或渲染输出。

## Scope

- 第一版只加载并查询 MODTRAN band LUT 中的 `tau_up_band`、`tau_down_band`。
- 第一版只输出日志或 debug 查询结果，不改变 shader 视觉效果。
- `path_radiance`、`sky_radiance`、`path_scattering_radiance`、`solar_irradiance` 暂时禁用，不进入渲染链路。
- 旧文件 `transmittance_0.3_15.txt` 继续保留，作为 fallback。

## Input Files

优先加载：

```text
Config/Atmosphere/MODTRAN/processed/band_lut.csv
```

可选加载：

```text
Config/Atmosphere/MODTRAN/processed/band_lut_si_candidate.csv
```

如果加载 `band_lut_si_candidate.csv`，第一版也只读取：

```text
band
atmosphere_model
aerosol_model
humidity_profile
visibility_km
observer_alt_km
target_alt_km
range_km
solar_zenith_deg
tau_up_band
tau_down_band
```

不要读取或使用 SI candidate radiance/irradiance 字段驱动 shader。

Fallback：

```text
Config/Atmosphere/transmittance/transmittance_0.3_15.txt
```

如果 MODTRAN LUT 不存在、表头不匹配、tau 字段为空，回退到旧 0.3-15 um 透过率表和现有参考路径长度逻辑。

## Query Key

第一版查询条件建议：

```text
band
observer_alt_km
target_alt_km
range_km
visibility_km
solar_zenith_deg
```

其中：

- `tau_up_band` 用于目标到观测器的上行大气透过率 debug。
- `tau_down_band` 用于太阳到目标高度的下行直射透过率 debug。
- `atmosphere_model`、`aerosol_model`、`humidity_profile` 先固定匹配 production 表中的 `Mid-Latitude Summer/Rural/default`，后续再开放环境配置。

## Interpolation Recommendation

不要直接线性插值 `tau`。建议转成 optical depth 后插值：

```text
tau_clamped = clamp(tau, 1e-6, 1.0)
od = -ln(tau_clamped)
```

对 `od` 按网格维度插值，然后转换回：

```text
tau = exp(-od)
```

第一版可先做保守实现：

- band 精确匹配。
- 高度对、range、visibility 使用 nearest-neighbor 或分段线性插值。
- range 插值优先使用 optical depth。
- visibility 插值也使用 optical depth，但要记录命中网格点和插值权重。
- 找不到足够邻点时回退到最近邻，并输出 debug warning。

## Debug Output

第一版只输出日志，不改变画面：

```text
MODTRAN Tau Debug:
  source=band_lut.csv
  band=MWIR
  observer_alt_km=...
  target_alt_km=...
  range_km=...
  visibility_km=...
  tau_up=...
  tau_down=...
  interpolation=nearest|optical_depth_linear
  fallback=none|transmittance_0.3_15
```

建议保留一个 runtime 开关，例如：

```text
EnableModtranTauDebug=0/1
```

默认关闭，避免影响现有视频链路。

## Integration Boundary

- 可以在后续 C++ 任务中新增 loader 类和 debug 日志。
- 不要直接改变 shader uniform、材质亮度、AGC 或最终灰度。
- 不要接入 path/sky/solar radiance。
- path/sky/solar 进入渲染前，必须先人工审查 `band_lut_si_candidate.csv` 的数量级，并单独做视觉回归。

## Stage 3 Alignment

该计划与 `HwaSimIR_InfraredSimulationFramework.md` 的阶段 3 方向匹配：阶段 3 要从单一 `transmittance_0.3_15.txt` 扩展到包含能见度、路径长度、目标高度、观测高度和太阳几何的 MODTRAN 数据库。本计划只开放 tau-only debug 查询，避开尚未通过 readiness 的 radiance 渲染链路。
