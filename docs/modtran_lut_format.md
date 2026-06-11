# HwaSimIR MODTRAN LUT Format

This document defines the offline PcModWin5/MODTRAN5 lookup tables consumed by future HwaSimIR atmosphere integration. Runtime C++ code is not changed by this stage.

## Directory Layout

```text
HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/
  raw/templates/      Hand-created PcModWin5 modin and MODOUT2 examples.
  raw/samples/        Optional small retained samples from successful runs.
  raw/failed/         Failed case artifacts: modin, MODOUT1, MODOUT2, reason.
  generated/modin/    Dry-run generated modin files; can be regenerated.
  processed/          CSV tables intended for HwaSimIR runtime loading.
  processed_snapshots/ Archived processed outputs before larger run stages.
```

## Spectral Tables

### `path_lut_spectral.csv`

Path/up-looking spectral attenuation and path radiance.

```csv
case_id,band,mode,atmosphere_model,aerosol_model,humidity_profile,visibility_km,observer_alt_km,target_alt_km,range_km,wavenumber_cm,wavelength_um,tau_up,path_radiance,unit_radiance,source_file
```

- `wavenumber_cm`: MODTRAN spectral coordinate in `cm^-1`.
- `wavelength_um`: computed as `10000 / wavenumber_cm`.
- `tau_up`: transmittance from target to observer, dimensionless.
- `path_radiance`: native MODOUT2 radiance for `ThermalRadiance`; blank for pure transmittance rows.
- `unit_radiance`: currently `MODOUT2_native` until the exact PcModWin unit export is locked down from the installed model configuration.

### `solar_lut_spectral.csv`

Down-looking/direct solar spectral data at the target altitude.

```csv
case_id,band,mode,atmosphere_model,aerosol_model,humidity_profile,visibility_km,target_alt_km,solar_zenith_deg,wavenumber_cm,wavelength_um,tau_down,solar_irradiance,unit_irradiance,source_file
```

- `tau_down`: direct solar atmospheric transmittance, dimensionless.
- `solar_irradiance`: parsed from the MODOUT2 `SOLAR` column.
- `unit_irradiance`: currently `MODOUT2_native`.

### `sky_lut_spectral.csv`

Sky/path scattering spectral data.

```csv
case_id,band,mode,atmosphere_model,aerosol_model,humidity_profile,visibility_km,observer_alt_km,target_alt_km,range_km,solar_zenith_deg,view_zenith_deg,wavenumber_cm,wavelength_um,sky_radiance,path_scattering_radiance,unit_radiance,source_file
```

- `path_scattering_radiance`: parsed from `SOL_SCAT`.
- `sky_radiance`: parsed from `TOTAL_RAD` for the current scattering template.
- `view_zenith_deg`: reserved for later geometry expansion; dry-run templates use `0`.

## Band Table

`band_lut.csv` is the compact runtime table.

```csv
band,atmosphere_model,aerosol_model,humidity_profile,visibility_km,observer_alt_km,target_alt_km,range_km,solar_zenith_deg,tau_up_band,tau_down_band,path_radiance_band,sky_radiance_band,path_scattering_radiance_band,solar_irradiance_band,unit_radiance,unit_irradiance,source_case_ids
```

The first implementation should compute band values with a rectangular spectral response:

```text
L_band = integral(L_lambda d_lambda) / integral(d_lambda)
```

Later sensor-specific response curves `S(lambda)` can replace the rectangular response:

```text
L_band = integral(S(lambda) * L_lambda d_lambda) / integral(S(lambda) d_lambda)
```

`band_lut_visibility_smoke.csv` uses the same column schema for the 18-case visibility smoke only. It is diagnostic output, not a replacement for the Pilot72 `band_lut.csv`.

## MODOUT2 Native Units And SI Candidate LUT

The audited PcModWin5/MODTRAN retained samples indicate that MODOUT2 radiance and irradiance fields are native per-wavenumber (`cm^-1`) values. The production `band_lut.csv` intentionally remains a native table and keeps radiance/irradiance fields labeled as `MODOUT2_native`.

Dimensionless transmittance fields do not need unit conversion:

```text
tau_up_lambda   = tau_up_native
tau_down_lambda = tau_down_native
```

Radiance values parsed from `PTH_THRML`, `SOL_SCAT`, and `TOTAL_RAD` require a wavenumber-to-wavelength conversion before they may be labeled as SI per micron:

```text
L_lambda[W/(m^2 sr um)] = L_sigma[W/(cm^2 sr cm^-1)] * 1e8 / wavelength_um^2
```

Irradiance values parsed from `SOLAR` use the analogous conversion:

```text
E_lambda[W/(m^2 um)] = E_sigma[W/(cm^2 cm^-1)] * 1e8 / wavelength_um^2
```

`band_lut_si_candidate.csv` is a separate derived table generated from the production spectral CSV files after this conversion. It uses a rectangular sensor response in wavelength space:

```text
value_band_avg = integral(value_lambda d_lambda) / integral(d_lambda)
```

Do not overwrite `band_lut.csv` when generating `band_lut_si_candidate.csv`. The SI candidate table is for offline numerical review and readiness assessment before any runtime or shader integration.

## Manifest

`manifest.csv` records every generated or run case. `case_id` includes band, mode, observer altitude, target altitude, range, visibility, aerosol, humidity, and solar zenith when the mode uses solar geometry.

Dry-run generation is intentionally separate from MODTRAN execution:

```powershell
python tools/modtran/build_modtran_cases.py --config tools/modtran/modtran_grid_nir_mwir_priority.json --dry-run
```

Pilot execution is capped at 100 cases and requires a real command-line MODTRAN executable:

```powershell
powershell -ExecutionPolicy Bypass -File tools/modtran/find_modtran_entry.ps1 -PcModWinRoot "F:\Programs\PcModWin5"
powershell -ExecutionPolicy Bypass -File tools/run_modtran_cases.ps1 -SingleCase -CaseLimit 1 -PcModWinRoot "F:\Programs\PcModWin5" -ModtranExe "<confirmed path>"
powershell -ExecutionPolicy Bypass -File tools/run_modtran_cases.ps1 -ValidationSix -CaseLimit 6 -PcModWinRoot "F:\Programs\PcModWin5" -ModtranExe "<confirmed path>" -NoDeleteRaw
python tools/modtran/check_validation_outputs.py --processed-dir HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed
powershell -ExecutionPolicy Bypass -File tools/run_modtran_cases.ps1 -Pilot72 -CaseLimit 72 -PcModWinRoot "F:\Programs\PcModWin5" -ModtranExe "<confirmed path>" -NoDeleteRaw
python tools/modtran/build_band_lut.py --processed-dir HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed
python tools/modtran/check_visibility_effect.py --processed-dir HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed --raw-dir HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\raw\samples
powershell -ExecutionPolicy Bypass -File tools/run_modtran_cases.ps1 -VisibilitySmoke18 -CaseLimit 18 -PcModWinRoot "F:\Programs\PcModWin5" -ModtranExe "<confirmed path>" -NoDeleteRaw
```

This safety stage allows one real MWIR transmittance case first, then six validation cases, then an explicit 72-case NIR/MWIR pilot. The visibility smoke is capped at 18 transmittance cases and only diagnoses whether requested visibility is visible in `modin`, `MODOUT1`, and band-integrated tau. It does not run the 2616-case production grid. If validation audit, executable confirmation, MODTRAN execution, parsing, or trend QC fails, the workflow stops and does not fabricate processed rows.

## Production NIR/MWIR Sparse Grid

Production NIR/MWIR is generated only with the explicit `--production-nir-mwir` and `-ProductionNirMwir` switches. It does not include VIS, SWIR, or LWIR and does not modify HwaSimIR C++ runtime code.

The requested sparse grid contains 2520 theoretical NIR/MWIR cases before geometry validation. PcModWin5/MODTRAN rejects slant range cases where `range_km < abs(observer_alt_km - target_alt_km)`, so the generator writes those 510 impossible cases to:

```text
HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/generated/production_invalid_geometry_manifest.csv
```

The runnable production manifest therefore contains 2010 cases:

```text
NIR Transmittance              335
NIR DirectSolarIrradiance      335
NIR RadianceWithScattering     335
MWIR Transmittance             335
MWIR ThermalRadiance           335
MWIR DirectSolarIrradiance     335
```

Before production, snapshot the current pilot/smoke processed outputs:

```powershell
python tools/modtran/snapshot_processed.py --processed-dir HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed
```

Production is intentionally split into single-threaded batches because PcModWin5/MODTRAN uses fixed output filenames:

```powershell
powershell -ExecutionPolicy Bypass -File tools\run_modtran_cases.ps1 -ProductionNirMwir -BatchName NIR_Transmittance -PcModWinRoot "F:\Programs\PcModWin5" -ModtranExe "F:\Programs\PcModWin5\bin\Mod5.2.1.0.exe"
powershell -ExecutionPolicy Bypass -File tools\run_modtran_cases.ps1 -ProductionNirMwir -BatchName MWIR_Transmittance -PcModWinRoot "F:\Programs\PcModWin5" -ModtranExe "F:\Programs\PcModWin5\bin\Mod5.2.1.0.exe"
powershell -ExecutionPolicy Bypass -File tools\run_modtran_cases.ps1 -ProductionNirMwir -BatchName MWIR_ThermalRadiance -PcModWinRoot "F:\Programs\PcModWin5" -ModtranExe "F:\Programs\PcModWin5\bin\Mod5.2.1.0.exe"
powershell -ExecutionPolicy Bypass -File tools\run_modtran_cases.ps1 -ProductionNirMwir -BatchName Solar_NIR_MWIR -PcModWinRoot "F:\Programs\PcModWin5" -ModtranExe "F:\Programs\PcModWin5\bin\Mod5.2.1.0.exe"
powershell -ExecutionPolicy Bypass -File tools\run_modtran_cases.ps1 -ProductionNirMwir -BatchName NIR_RadianceWithScattering -PcModWinRoot "F:\Programs\PcModWin5" -ModtranExe "F:\Programs\PcModWin5\bin\Mod5.2.1.0.exe"
```

After all batches, rebuild the rectangular-response band table:

```powershell
python tools/modtran/build_band_lut.py --processed-dir HwaSim_IR\Bin\Config\Atmosphere\MODTRAN\processed
powershell -ExecutionPolicy Bypass -File tools\stage3_modtran_lut_check.ps1 -Strict
```

Production QC may legitimately report `overall_status: FAIL` even when parsing and table integrity pass. In the 2026-05-28 production run, all 2010 runnable cases completed and `band_lut.csv` contains 670 rows, but `qc_report.md` flags missing visibility sensitivity for several low-altitude or slant geometry groups and MWIR path radiance drops from 1 to 2 km in low horizontal paths. These are physics/template QA findings and should block C++ integration until reviewed.

Radiance and irradiance units remain `MODOUT2_native`. Do not label them as SI units until PcModWin5/MODTRAN export units are confirmed.
