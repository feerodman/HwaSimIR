# 阶段 3 MODTRAN 工具清单

日期：2026-05-29

本清单用于冻结当前阶段 3 MODTRAN 工具形态，不删除数据，也不继续扩展功能。
脚本按当前收口需要分为 `keep`、`optional`、`historical` 三类，方便后续进入阶段 4
前减少重复入口和误跑生产任务的风险。

## Keep

这些文件属于当前阶段 3 基线，日常检查、构建或运行时 smoke 中应保留清晰入口。

- `tools/stage3_check.ps1`：阶段 3 大气与环境模型检查。
- `tools/stage3_modtran_lut_check.ps1`：MODTRAN LUT 目录、模板、compact LUT 和 tau 范围严格检查。
- `tools/stage3_modtran_tau_loader_check.ps1`：C++ tau-only loader 与阶段边界检查。
- `tools/stage3_modtran_tau_debug_smoke.ps1`：`EnableModtranTauDebug` 开关的运行时日志 smoke。
- `tools/stage3_modtran_tau_active_smoke.ps1`：`UseModtranTauForAtmosphere` 受控实验 smoke。
- `tools/stage3_modtran_tau_delta_report.py`：从运行时日志生成 old/new tau 差异报告。
- `tools/stage3_modtran_tau_ab_smoke.ps1`：NIR/MWIR active off/on 最小 A/B smoke。
- `tools/modtran/build_modtran_cases.py`：可复现 MODTRAN case manifest 生成器，包括 NIR/MWIR production manifest 支持。
- `tools/modtran/build_band_lut.py`：native compact `band_lut.csv` 生成器。
- `tools/modtran/build_band_lut_si_candidate.py`：仅供离线数值审查的 SI candidate LUT 生成器。
- `tools/modtran/evaluate_lut_readiness.py`：A 线 readiness 分级报告。
- `tools/modtran/parse_modout2.py`：runner 和诊断共用的 MODOUT2 解析器。
- `tools/modtran/diagnostics_common.py`：MODTRAN 诊断脚本共享工具。
- `tools/modtran/modtran_grid_nir_mwir_priority.json`：阶段 3 NIR/MWIR grid 配置。
- `tools/run_modtran_cases.ps1` 与 `tools/modtran/run_modtran_cases.ps1`：单线程 MODTRAN runner 入口。未经明确批准，不应再次运行 production。

## Optional

这些文件用于数据质量回看或证据复核，不需要每次阶段 3 构建或 smoke 都运行。

- `tools/modtran/check_validation_outputs.py`：6-case validation 输出检查。
- `tools/modtran/check_visibility_effect.py`：Pilot72 visibility 生效性审计。
- `tools/modtran/analyze_invalid_geometry.py`：invalid geometry 解释。
- `tools/modtran/diagnose_visibility_failures.py`：production visibility 趋势诊断。
- `tools/modtran/diagnose_path_radiance_failures.py`：MWIR path radiance 趋势诊断。
- `tools/modtran/audit_modout_units.py`：MODOUT1/MODOUT2 单位证据审计。
- `tools/modtran/search_pcmodwin_units_docs.ps1`：本机 PcModWin5 文档关键词搜索。
- `tools/modtran/snapshot_processed.py`：processed 目录快照工具。
- `tools/modtran/find_modtran_entry.ps1`：PcModWin5/MODTRAN 可执行入口探测，不盲目运行候选文件。

## Historical

这些工具记录早期验证或专项诊断流程，保留用于追溯；除非要复现对应历史诊断，不建议新增调用面。

- `tools/modtran/build_validation_band_lut.py`：6-case validation compact LUT。
- `tools/modtran/build_aerosol_override_smoke_band_lut.py`：aerosol override smoke 专用 band LUT。
- `tools/modtran/build_targeted_diagnosis_band_lut.py`：targeted diagnosis rerun LUT。
- `tools/modtran/run_targeted_diagnosis_cases.ps1`：有限 targeted rerun 诊断工具。

## Git Ignore 确认

仓库当前已忽略以下不应提交的阶段 3 数据：

- `ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/generated/`
- `ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/raw/samples/`
- `ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/raw/failed/`
- `ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/raw/archive/`
- `ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/processed_snapshots/`
- `ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/processed/*_lut_spectral.csv`
- `ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/processed/*_path_lut_spectral.csv`
- `ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/processed/modout_units_audit.csv`
- `ConsoleApplication1_LLA/Bin/Config/Atmosphere/MODTRAN/processed/modout_units_candidate_lines.csv`
- `logs/`

compact LUT、manifest、QC 报告、少量手工模板、脚本和文档仍保持可见，除非它们被其他规则忽略。本轮只做清单和 ignore 状态确认，没有删除任何数据。
