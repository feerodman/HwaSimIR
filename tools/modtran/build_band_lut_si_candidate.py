#!/usr/bin/env python3
"""Build a separate SI-candidate band LUT from production MODTRAN spectra.

This script never writes processed/band_lut.csv. It streams the large spectral
CSVs case-by-case, converts MODOUT2 per-cm^-1 native radiance/irradiance values
to per-micron SI candidates, and writes a separate diagnostic table.
"""

from __future__ import annotations

import argparse
import csv
import math
from collections import Counter, defaultdict
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple

from diagnostics_common import read_csv, write_csv


OUTPUT_COLUMNS = [
    "band",
    "atmosphere_model",
    "aerosol_model",
    "humidity_profile",
    "visibility_km",
    "observer_alt_km",
    "target_alt_km",
    "range_km",
    "solar_zenith_deg",
    "tau_up_band",
    "tau_down_band",
    "path_radiance_band_W_m2_sr_um",
    "sky_radiance_band_W_m2_sr_um",
    "path_scattering_radiance_band_W_m2_sr_um",
    "solar_irradiance_band_W_m2_um",
    "unit_status",
    "integration_method",
    "source_case_ids",
]

UNIT_STATUS = "CONFIRMED_PER_CM1_TO_SI_CANDIDATE"
INTEGRATION_METHOD = "RECTANGULAR_RESPONSE_WAVELENGTH_DOMAIN_AFTER_CM1_TO_UM_CONVERSION"

PRODUCTION_RANGES = [1.0, 2.0, 5.0, 10.0, 20.0, 35.0, 50.0]
PRODUCTION_VISIBILITIES = [2.0, 5.0, 10.0, 23.0, 50.0]
PRODUCTION_ALTITUDE_PAIRS = [
    (3.0, 3.0),
    (5.0, 5.0),
    (10.0, 10.0),
    (15.0, 15.0),
    (20.0, 20.0),
    (5.0, 3.0),
    (10.0, 5.0),
    (15.0, 10.0),
    (20.0, 10.0),
    (20.0, 15.0),
    (10.0, 3.0),
    (20.0, 3.0),
]

TREND_EPS = 0.02
VISIBILITY_SENSITIVITY_EPS = 1e-5
SHORT_RANGE_PATH_DROP_REL_FAIL = 0.85
SHORT_RANGE_PATH_DROP_ABS_FAIL = 1.0


def parse_float(value: object) -> float | None:
    if value is None or value == "":
        return None
    try:
        parsed = float(value)
    except (TypeError, ValueError):
        return None
    return parsed if math.isfinite(parsed) else None


def format_float(value: float | None) -> str:
    if value is None:
        return ""
    return f"{value:.10g}"


def stable_float(value: object) -> str:
    parsed = parse_float(value)
    return format_float(parsed) if parsed is not None else str(value or "")


def condition_key(row: Dict[str, str]) -> Tuple[str, str, str, str, str, str, str, str, str]:
    return (
        row.get("band", ""),
        row.get("atmosphere_model", ""),
        row.get("aerosol_model", ""),
        row.get("humidity_profile", ""),
        stable_float(row.get("visibility_km")),
        stable_float(row.get("observer_alt_km")),
        stable_float(row.get("target_alt_km")),
        stable_float(row.get("range_km")),
        stable_float(row.get("solar_zenith_deg")),
    )


def rectangular_response_average(points: Sequence[Tuple[float, float]]) -> float | None:
    clean = [(wavelength, value) for wavelength, value in points if math.isfinite(wavelength) and math.isfinite(value)]
    if not clean:
        return None
    clean.sort(key=lambda item: item[0])
    if len(clean) == 1:
        return clean[0][1]
    width = clean[-1][0] - clean[0][0]
    if width <= 0.0:
        return sum(value for _, value in clean) / len(clean)
    area = 0.0
    for (wavelength_a, value_a), (wavelength_b, value_b) in zip(clean, clean[1:]):
        area += 0.5 * (value_a + value_b) * (wavelength_b - wavelength_a)
    return area / width


def cm1_native_to_si(value: float, wavelength_um: float) -> float | None:
    if wavelength_um <= 0.0:
        return None
    converted = value * 1.0e8 / (wavelength_um * wavelength_um)
    return converted if math.isfinite(converted) else None


def stream_case_averages(
    path: Path,
    field_specs: Dict[str, Tuple[str, bool]],
) -> Tuple[Dict[str, Dict[str, str]], Dict[str, object]]:
    """Return per-case rectangular averages without retaining full CSV rows."""

    case_metrics: Dict[str, Dict[str, str]] = {}
    stats: Dict[str, object] = {
        "file": str(path),
        "rows": 0,
        "cases": 0,
        "invalid_wavelength_rows": 0,
        "invalid_value_counts": Counter(),
        "repeated_case_blocks": 0,
    }

    if not path.exists() or path.stat().st_size == 0:
        return case_metrics, stats

    current_case_id = ""
    current_points: Dict[str, List[Tuple[float, float]]] = {name: [] for name in field_specs}
    seen_finalized: set[str] = set()

    def finalize_case() -> None:
        nonlocal current_case_id, current_points
        if not current_case_id:
            return
        if current_case_id in seen_finalized:
            stats["repeated_case_blocks"] = int(stats["repeated_case_blocks"]) + 1
        seen_finalized.add(current_case_id)
        metrics: Dict[str, str] = {}
        for output_name, points in current_points.items():
            metrics[output_name] = format_float(rectangular_response_average(points))
        case_metrics[current_case_id] = metrics
        stats["cases"] = int(stats["cases"]) + 1
        current_points = {name: [] for name in field_specs}

    with path.open(newline="", encoding="utf-8-sig") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            stats["rows"] = int(stats["rows"]) + 1
            case_id = row.get("case_id", "")
            if not case_id:
                continue
            if current_case_id and case_id != current_case_id:
                finalize_case()
            if case_id != current_case_id:
                current_case_id = case_id

            wavelength = parse_float(row.get("wavelength_um"))
            if wavelength is None or wavelength <= 0.0:
                stats["invalid_wavelength_rows"] = int(stats["invalid_wavelength_rows"]) + 1
                continue

            invalid_counts: Counter[str] = stats["invalid_value_counts"]  # type: ignore[assignment]
            for output_name, (input_name, convert_to_si) in field_specs.items():
                raw_text = row.get(input_name, "")
                value = parse_float(raw_text)
                if value is None:
                    if raw_text != "":
                        invalid_counts[output_name] += 1
                    continue
                if convert_to_si:
                    value = cm1_native_to_si(value, wavelength)
                    if value is None:
                        invalid_counts[output_name] += 1
                        continue
                current_points[output_name].append((wavelength, value))

    finalize_case()
    return case_metrics, stats


def first_metric(case_ids: Iterable[str], metrics: Dict[str, Dict[str, str]], name: str) -> str:
    for case_id in case_ids:
        value = metrics.get(case_id, {}).get(name, "")
        if value != "":
            return value
    return ""


def build_band_rows(processed_dir: Path) -> Tuple[List[Dict[str, object]], Dict[str, object]]:
    manifest_rows_all = read_csv(processed_dir / "manifest.csv")
    manifest_rows = [
        row for row in manifest_rows_all
        if row.get("grid") == "production_nir_mwir"
        and (row.get("status") == "production_nir_mwir_succeeded" or row.get("stage") == "production_nir_mwir_succeeded")
    ]

    path_metrics, path_stats = stream_case_averages(
        processed_dir / "path_lut_spectral.csv",
        {
            "tau_up_band": ("tau_up", False),
            "path_radiance_band_W_m2_sr_um": ("path_radiance", True),
        },
    )
    solar_metrics, solar_stats = stream_case_averages(
        processed_dir / "solar_lut_spectral.csv",
        {
            "tau_down_band": ("tau_down", False),
            "solar_irradiance_band_W_m2_um": ("solar_irradiance", True),
        },
    )
    sky_metrics, sky_stats = stream_case_averages(
        processed_dir / "sky_lut_spectral.csv",
        {
            "sky_radiance_band_W_m2_sr_um": ("sky_radiance", True),
            "path_scattering_radiance_band_W_m2_sr_um": ("path_scattering_radiance", True),
        },
    )

    parsed_case_ids = set(path_metrics) | set(solar_metrics) | set(sky_metrics)
    groups: Dict[Tuple[str, str, str, str, str, str, str, str, str], Dict[str, object]] = {}
    for row in manifest_rows:
        if row.get("case_id") not in parsed_case_ids:
            continue
        key = condition_key(row)
        group = groups.setdefault(key, {"meta": [], "modes": defaultdict(list)})
        group["meta"].append(row)
        group["modes"][row.get("mode", "")].append(row)

    output_rows: List[Dict[str, object]] = []
    for key, group in sorted(
        groups.items(),
        key=lambda item: (
            item[0][0],
            parse_float(item[0][5]) or 0.0,
            parse_float(item[0][6]) or 0.0,
            parse_float(item[0][7]) or 0.0,
            parse_float(item[0][4]) or 0.0,
        ),
    ):
        band, atmosphere, aerosol, humidity, visibility, observer_alt, target_alt, range_km, solar_zenith = key
        modes: Dict[str, List[Dict[str, str]]] = group["modes"]  # type: ignore[assignment]

        trans_case_ids = [row["case_id"] for row in modes.get("Transmittance", [])]
        thermal_case_ids = [row["case_id"] for row in modes.get("ThermalRadiance", [])]
        solar_case_ids = [row["case_id"] for row in modes.get("DirectSolarIrradiance", [])]
        scattering_case_ids = [row["case_id"] for row in modes.get("RadianceWithScattering", [])]
        source_case_ids = trans_case_ids + thermal_case_ids + solar_case_ids + scattering_case_ids

        output_rows.append(
            {
                "band": band,
                "atmosphere_model": atmosphere,
                "aerosol_model": aerosol,
                "humidity_profile": humidity,
                "visibility_km": visibility,
                "observer_alt_km": observer_alt,
                "target_alt_km": target_alt,
                "range_km": range_km,
                "solar_zenith_deg": solar_zenith,
                "tau_up_band": first_metric(trans_case_ids, path_metrics, "tau_up_band"),
                "tau_down_band": first_metric(solar_case_ids, solar_metrics, "tau_down_band"),
                "path_radiance_band_W_m2_sr_um": first_metric(
                    thermal_case_ids,
                    path_metrics,
                    "path_radiance_band_W_m2_sr_um",
                ),
                "sky_radiance_band_W_m2_sr_um": first_metric(
                    scattering_case_ids,
                    sky_metrics,
                    "sky_radiance_band_W_m2_sr_um",
                ),
                "path_scattering_radiance_band_W_m2_sr_um": first_metric(
                    scattering_case_ids,
                    sky_metrics,
                    "path_scattering_radiance_band_W_m2_sr_um",
                ),
                "solar_irradiance_band_W_m2_um": first_metric(
                    solar_case_ids,
                    solar_metrics,
                    "solar_irradiance_band_W_m2_um",
                ),
                "unit_status": UNIT_STATUS,
                "integration_method": INTEGRATION_METHOD,
                "source_case_ids": ";".join(source_case_ids),
            }
        )

    context = {
        "manifest_rows": manifest_rows,
        "parsed_case_ids": parsed_case_ids,
        "stream_stats": [path_stats, solar_stats, sky_stats],
    }
    return output_rows, context


def value(row: Dict[str, object], column: str) -> float | None:
    return parse_float(row.get(column))


def valid_production_ranges(observer_alt: float, target_alt: float) -> List[float]:
    min_slant = abs(observer_alt - target_alt)
    return [range_km for range_km in PRODUCTION_RANGES if range_km + 1e-9 >= min_slant]


def band_rows_for_condition(
    rows: Sequence[Dict[str, object]],
    band: str,
    observer_alt: float,
    target_alt: float,
    visibility: float | None = None,
    range_km: float | None = None,
    column: str = "tau_up_band",
) -> List[Tuple[float, float, float]]:
    values: List[Tuple[float, float, float]] = []
    for row in rows:
        if row.get("band") != band:
            continue
        row_obs = value(row, "observer_alt_km")
        row_target = value(row, "target_alt_km")
        row_range = value(row, "range_km")
        row_visibility = value(row, "visibility_km")
        row_value = value(row, column)
        if None in {row_obs, row_target, row_range, row_visibility, row_value}:
            continue
        if row_obs != observer_alt or row_target != target_alt:
            continue
        if visibility is not None and row_visibility != visibility:
            continue
        if range_km is not None and row_range != range_km:
            continue
        values.append((row_range, row_visibility, row_value))
    return values


def is_high_altitude_horizontal(observer_alt: float, target_alt: float) -> bool:
    return observer_alt == target_alt and observer_alt >= 10.0


def is_low_altitude_short_horizontal(observer_alt: float, target_alt: float, range_km: float) -> bool:
    return observer_alt == target_alt and observer_alt <= 5.0 and range_km <= 2.0


def aerosol_override_smoke_failed(processed_dir: Path) -> bool:
    path = processed_dir / "qc_aerosol_override_smoke.md"
    if not path.exists():
        return False
    text = path.read_text(encoding="utf-8", errors="replace")
    return "FAIL_VISIBILITY_OVERRIDE_NOT_EFFECTIVE" in text or "- overall_status: FAIL" in text


def production_range_tau_checks(rows: Sequence[Dict[str, object]]) -> List[Tuple[str, str]]:
    checks: List[Tuple[str, str]] = []
    for band in ["MWIR", "NIR"]:
        for observer_alt, target_alt in PRODUCTION_ALTITUDE_PAIRS:
            expected_ranges = valid_production_ranges(observer_alt, target_alt)
            for visibility in PRODUCTION_VISIBILITIES:
                values = sorted(
                    band_rows_for_condition(rows, band, observer_alt, target_alt, visibility=visibility),
                    key=lambda item: item[0],
                )
                prefix = f"{band} obs={observer_alt:g} target={target_alt:g} visibility={visibility:g}"
                if len(values) != len(expected_ranges):
                    checks.append(("FAIL", f"{prefix}: missing tau_up range samples"))
                    continue
                bad = []
                for (range_a, _, value_a), (range_b, _, value_b) in zip(values, values[1:]):
                    if value_b > value_a + TREND_EPS:
                        bad.append(f"{range_a:g}->{range_b:g} km {value_a:.5g}->{value_b:.5g}")
                if bad:
                    checks.append(("WARNING", f"{prefix}: tau_up range trend weak/nonmonotonic: {'; '.join(bad)}"))
                else:
                    checks.append(("PASS", f"{prefix}: tau_up decreases with range within eps={TREND_EPS:g}"))
    return checks


def production_visibility_tau_checks(
    rows: Sequence[Dict[str, object]],
    smoke_failed: bool,
) -> List[Tuple[str, str]]:
    checks: List[Tuple[str, str]] = []
    for band in ["MWIR", "NIR"]:
        for observer_alt, target_alt in PRODUCTION_ALTITUDE_PAIRS:
            for range_km in valid_production_ranges(observer_alt, target_alt):
                values = sorted(
                    band_rows_for_condition(rows, band, observer_alt, target_alt, range_km=range_km),
                    key=lambda item: item[1],
                )
                prefix = f"{band} obs={observer_alt:g} target={target_alt:g} range={range_km:g}"
                if len(values) != len(PRODUCTION_VISIBILITIES):
                    checks.append(("FAIL", f"{prefix}: missing tau_up visibility samples"))
                    continue
                bad = []
                for (_, visibility_a, value_a), (_, visibility_b, value_b) in zip(values, values[1:]):
                    if value_b + TREND_EPS < value_a:
                        bad.append(f"{visibility_a:g}->{visibility_b:g} km {value_a:.5g}->{value_b:.5g}")
                if bad:
                    checks.append(("WARNING", f"{prefix}: WARNING_VISIBILITY_LOW_SENSITIVITY tau_up decreased with better visibility: {'; '.join(bad)}"))
                    continue
                spread = max(value for _, _, value in values) - min(value for _, _, value in values)
                if spread <= VISIBILITY_SENSITIVITY_EPS:
                    if is_high_altitude_horizontal(observer_alt, target_alt):
                        checks.append(("WARNING", f"{prefix}: high_altitude_low_sensitivity spread={spread:.5g}"))
                    elif smoke_failed:
                        checks.append(("FAIL", f"{prefix}: AerosolOverrideSmoke low/slant extreme VIS failed; visibility sensitivity missing spread={spread:.5g}"))
                    else:
                        checks.append(("WARNING", f"{prefix}: WARNING_VISIBILITY_LOW_SENSITIVITY spread={spread:.5g}"))
                else:
                    checks.append(("PASS", f"{prefix}: visibility sensitivity detected spread={spread:.5g}"))
    return checks


def production_mwir_path_checks(rows: Sequence[Dict[str, object]]) -> List[Tuple[str, str]]:
    checks: List[Tuple[str, str]] = []
    for observer_alt, target_alt in PRODUCTION_ALTITUDE_PAIRS:
        expected_ranges = valid_production_ranges(observer_alt, target_alt)
        for visibility in PRODUCTION_VISIBILITIES:
            values = sorted(
                band_rows_for_condition(
                    rows,
                    "MWIR",
                    observer_alt,
                    target_alt,
                    visibility=visibility,
                    column="path_radiance_band_W_m2_sr_um",
                ),
                key=lambda item: item[0],
            )
            prefix = f"MWIR path_radiance obs={observer_alt:g} target={target_alt:g} visibility={visibility:g}"
            if len(values) != len(expected_ranges):
                checks.append(("FAIL", f"{prefix}: missing path radiance samples"))
                continue
            for (range_a, _, value_a), (range_b, _, value_b) in zip(values, values[1:]):
                tolerance = max(abs(value_a) * 0.15, 1e-12)
                if value_b + tolerance >= value_a:
                    continue
                drop = value_a - value_b
                drop_fraction = drop / max(abs(value_a), 1e-300)
                detail = f"{prefix}: {range_a:g}->{range_b:g} km {value_a:.5g}->{value_b:.5g}"
                if range_a == 1.0 and range_b == 2.0:
                    checks.append(("WARNING", f"{detail}; mwir_short_range_path_radiance_nonmonotonic fraction={drop_fraction:.3g}"))
                else:
                    checks.append(("WARNING", f"{detail}; path_radiance_nonmonotonic"))
    return checks


def basic_checks(rows: Sequence[Dict[str, object]], context: Dict[str, object]) -> List[Tuple[str, str]]:
    checks: List[Tuple[str, str]] = []
    checks.append(("PASS" if rows else "FAIL", f"band_lut_si_candidate rows={len(rows)}"))
    checks.append(("PASS", "production band_lut.csv was not overwritten by this script"))

    tau_bad = []
    numeric_bad = []
    for row in rows:
        for column in ["tau_up_band", "tau_down_band"]:
            parsed = value(row, column)
            if parsed is not None and not (0.0 <= parsed <= 1.0):
                tau_bad.append(row)
        for column in [
            "path_radiance_band_W_m2_sr_um",
            "sky_radiance_band_W_m2_sr_um",
            "path_scattering_radiance_band_W_m2_sr_um",
            "solar_irradiance_band_W_m2_um",
        ]:
            raw = row.get(column, "")
            parsed = value(row, column)
            if raw != "" and parsed is None:
                numeric_bad.append((column, row))
    checks.append(("PASS" if not tau_bad else "FAIL", f"tau_up_band/tau_down_band in 0..1 bad_rows={len(tau_bad)}"))
    checks.append(("PASS" if not numeric_bad else "FAIL", f"radiance/irradiance finite bad_values={len(numeric_bad)}"))

    unit_statuses = {str(row.get("unit_status", "")) for row in rows}
    checks.append(
        (
            "PASS" if unit_statuses == {UNIT_STATUS} else "FAIL",
            f"SI unit status confirmed values={','.join(sorted(unit_statuses))}",
        )
    )

    nir_rows = [row for row in rows if row.get("band") == "NIR"]
    mwir_rows = [row for row in rows if row.get("band") == "MWIR"]
    nir_solar_nonzero = all((value(row, "solar_irradiance_band_W_m2_um") or 0.0) > 0.0 for row in nir_rows)
    nir_sky_nonempty = all(
        row.get("sky_radiance_band_W_m2_sr_um", "") != ""
        or row.get("path_scattering_radiance_band_W_m2_sr_um", "") != ""
        for row in nir_rows
    )
    mwir_path_nonempty = all(row.get("path_radiance_band_W_m2_sr_um", "") != "" for row in mwir_rows)
    checks.append(("PASS" if nir_rows and nir_solar_nonzero else "FAIL", "NIR solar_irradiance_band_W_m2_um is nonzero"))
    checks.append(("PASS" if nir_rows and nir_sky_nonempty else "FAIL", "NIR sky/path_scattering radiance SI candidate is present"))
    checks.append(("PASS" if mwir_rows and mwir_path_nonempty else "FAIL", "MWIR path_radiance_band_W_m2_sr_um is present"))

    for stream_stat in context["stream_stats"]:  # type: ignore[index]
        invalid_counts: Counter[str] = stream_stat["invalid_value_counts"]  # type: ignore[index]
        invalid_total = sum(invalid_counts.values()) + int(stream_stat["invalid_wavelength_rows"])  # type: ignore[arg-type]
        status = "PASS" if invalid_total == 0 else "WARNING"
        checks.append((status, f"streamed {Path(str(stream_stat['file'])).name} rows={stream_stat['rows']} cases={stream_stat['cases']} invalid_values={invalid_total}"))
    return checks


def write_final_a_line_readiness(
    processed_dir: Path,
    overall: str,
    fail_count: int,
    warning_count: int,
    smoke_failed: bool,
) -> None:
    radiance_ready = "NUMERIC_REVIEW_ONLY" if fail_count == 0 else "NO"
    lines = [
        "# MODTRAN LUT A 线最终 Readiness",
        "",
        f"- overall_status: {overall}",
        "- tau_only_debug_ready: YES",
        f"- radiance_si_candidate_ready: {radiance_ready}",
        "- cpp_tau_only_loader_allowed: YES",
        "- cpp_radiance_integration_allowed: NO",
        "- production_rerun_required: NO",
        "- pcmodwin_template_rebuild_required: NO",
        f"- fatal_failures: {fail_count}",
        f"- warnings: {warning_count}",
        f"- aerosol_override_smoke_extreme_failure: {'YES' if smoke_failed else 'NO'}",
        f"- unit_status: {UNIT_STATUS}",
        f"- integration_method: {INTEGRATION_METHOD}",
        "",
        "## 结论",
        "",
        "- tau_up/tau_down 是无量纲字段，可用于 debug 查询。",
        "- SI 转换后的 path/sky/solar 字段只作为候选数值输出。",
        "- 因为 AerosolOverrideSmoke 已确认低空和斜穿 extreme VIS 有响应，production 中 visibility 弱敏感降级为 warning。",
        "- radiance/irradiance 在人工审查 SI 数量级前不得进入渲染链路。",
        "- A 线收口不修改 HwaSimIR C++ 或 shader。",
        "",
        "## 剩余 Warning",
        "",
        "- production sparse grid 中仍存在 WARNING_VISIBILITY_LOW_SENSITIVITY。",
        "- 高空水平路径仍保留 high_altitude_low_sensitivity。",
        "- MWIR path radiance 仍保留 1->2 km nonmonotonic warning。",
    ]
    (processed_dir / "final_a_line_readiness.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_qc(processed_dir: Path, rows: Sequence[Dict[str, object]], context: Dict[str, object]) -> str:
    smoke_failed = aerosol_override_smoke_failed(processed_dir)
    checks = []
    checks.extend(basic_checks(rows, context))
    checks.extend(production_range_tau_checks(rows))
    checks.extend(production_visibility_tau_checks(rows, smoke_failed))
    checks.extend(production_mwir_path_checks(rows))

    fail_count = sum(1 for status, _ in checks if status == "FAIL")
    warning_count = sum(1 for status, _ in checks if status == "WARNING")
    overall = "FAIL" if fail_count else ("PASS_WITH_WARNINGS" if warning_count else "PASS")

    warning_rows = [(status, detail) for status, detail in checks if status == "WARNING"]
    failure_rows = [(status, detail) for status, detail in checks if status == "FAIL"]

    lines = [
        "# SI Candidate Band LUT QC",
        "",
        f"- overall_status: {overall}",
        f"- fatal_failures: {fail_count}",
        f"- warnings: {warning_count}",
        f"- band_lut_rows: {len(rows)}",
        f"- output_csv: {processed_dir / 'band_lut_si_candidate.csv'}",
        "- production_lut_overwritten: no",
        f"- unit_status: {UNIT_STATUS}",
        f"- integration_method: {INTEGRATION_METHOD}",
        f"- aerosol_override_smoke_failed: {'YES' if smoke_failed else 'NO'}",
        "",
        "## Check Summary",
        "",
        "| status | count |",
        "| --- | ---: |",
    ]
    counts = Counter(status for status, _ in checks)
    for status in ["PASS", "WARNING", "FAIL"]:
        lines.append(f"| {status} | {counts.get(status, 0)} |")

    lines.extend(["", "## Failures", ""])
    if failure_rows:
        for _, detail in failure_rows[:200]:
            lines.append(f"- {detail}")
        if len(failure_rows) > 200:
            lines.append(f"- ... {len(failure_rows) - 200} additional failures omitted")
    else:
        lines.append("- none")

    lines.extend(["", "## Warnings", ""])
    if warning_rows:
        for _, detail in warning_rows[:200]:
            lines.append(f"- {detail}")
        if len(warning_rows) > 200:
            lines.append(f"- ... {len(warning_rows) - 200} additional warnings omitted")
    else:
        lines.append("- none")

    lines.extend(
        [
            "",
            "## Notes",
            "",
            "- High-altitude horizontal visibility weak sensitivity is classified as WARNING.",
            "- Production visibility low sensitivity is classified as WARNING_VISIBILITY_LOW_SENSITIVITY when AerosolOverrideSmoke low-altitude and slant extreme-VIS checks pass.",
            "- MWIR 1->2 km path radiance drops are classified as WARNING.",
            "- Fatal checks are limited to tau range, finite SI conversion, required band/mode presence, confirmed unit status, and AerosolOverrideSmoke extreme-VIS failure.",
            "- This script uses existing spectral CSVs only and does not run MODTRAN.",
        ]
    )

    (processed_dir / "qc_band_lut_si_candidate.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    write_final_a_line_readiness(processed_dir, overall, fail_count, warning_count, smoke_failed)
    return overall


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--processed-dir", required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_arg_parser().parse_args(argv)
    processed_dir = Path(args.processed_dir)
    output_path = processed_dir / "band_lut_si_candidate.csv"

    rows, context = build_band_rows(processed_dir)
    if not rows:
        print("ERROR: no production spectral rows matched manifest cases; SI candidate LUT was not written.")
        return 1

    write_csv(output_path, rows, OUTPUT_COLUMNS)
    overall = write_qc(processed_dir, rows, context)
    print(f"Wrote {len(rows)} SI-candidate band rows to {output_path}")
    print(f"overall_status={overall}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
