# MODTRAN LUT Readiness Report

- tau_only_debug_ready: YES
- cpp_tau_only_loader_allowed: YES
- radiance_si_candidate_ready: NUMERIC_REVIEW_ONLY
- cpp_radiance_integration_allowed: NO
- production_rerun_required: NO
- pcmodwin_template_rebuild_required: NO
- si_candidate_qc_status: PASS_WITH_WARNINGS
- aerosol_override_smoke_status: PASS_WITH_WARNINGS

## Evidence

- tau_only: band_lut.csv rows=670 tau range OK
- si_candidate: band_lut_si_candidate.csv rows=670 qc_status=PASS_WITH_WARNINGS

## Remaining Warnings

- high_altitude_low_sensitivity
- WARNING_VISIBILITY_LOW_SENSITIVITY
- mwir_short_range_path_radiance_nonmonotonic_if_any

## Recommended C++ Scope

- 可以做 tau-only debug loader。
- 可以加载 band_lut.csv，或加载 band_lut_si_candidate.csv 但只读取 tau_up/tau_down。
- tau_up/tau_down 可用于 debug 查询。
- path_radiance、sky_radiance、solar_irradiance 只允许离线数值检查，暂不进入渲染链路。
- 不要直接改变 shader 视觉效果。
- path/sky/solar 接入前先人工审查 SI 数量级。

## Boundaries

- 本报告没有重新运行 MODTRAN。
- 本报告没有覆盖 production band_lut.csv。
- 本报告不批准 VIS/SWIR/LWIR production。
- 本报告不修改 HwaSimIR C++ 或 shader。
