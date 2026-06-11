# MODTRAN LUT A 线最终 Readiness

- overall_status: PASS_WITH_WARNINGS
- tau_only_debug_ready: YES
- radiance_si_candidate_ready: NUMERIC_REVIEW_ONLY
- cpp_tau_only_loader_allowed: YES
- cpp_radiance_integration_allowed: NO
- production_rerun_required: NO
- pcmodwin_template_rebuild_required: NO
- fatal_failures: 0
- warnings: 98
- aerosol_override_smoke_extreme_failure: NO
- unit_status: CONFIRMED_PER_CM1_TO_SI_CANDIDATE
- integration_method: RECTANGULAR_RESPONSE_WAVELENGTH_DOMAIN_AFTER_CM1_TO_UM_CONVERSION

## 结论

- tau_up/tau_down 是无量纲字段，可用于 debug 查询。
- SI 转换后的 path/sky/solar 字段只作为候选数值输出。
- 因为 AerosolOverrideSmoke 已确认低空和斜穿 extreme VIS 有响应，production 中 visibility 弱敏感降级为 warning。
- radiance/irradiance 在人工审查 SI 数量级前不得进入渲染链路。
- A 线收口不修改 HwaSimIR C++ 或 shader。

## 剩余 Warning

- production sparse grid 中仍存在 WARNING_VISIBILITY_LOW_SENSITIVITY。
- 高空水平路径仍保留 high_altitude_low_sensitivity。
- MWIR path radiance 仍保留 1->2 km nonmonotonic warning。
