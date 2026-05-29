#!/usr/bin/env python3
"""Evaluate HwaSimIR MODTRAN LUT readiness from existing processed outputs."""

from __future__ import annotations

import argparse
import csv
import math
import re
from pathlib import Path
from typing import Dict, List, Sequence


def parse_float(value: object) -> float | None:
    if value is None or value == "":
        return None
    try:
        parsed = float(value)
    except (TypeError, ValueError):
        return None
    return parsed if math.isfinite(parsed) else None


def read_csv_rows(path: Path) -> List[Dict[str, str]]:
    if not path.exists() or path.stat().st_size == 0:
        return []
    with path.open(newline="", encoding="utf-8-sig") as handle:
        return list(csv.DictReader(handle))


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace") if path.exists() else ""


def extract_status(text: str) -> str:
    match = re.search(r"overall_status:\s*([A-Z0-9_]+)", text)
    return match.group(1) if match else "UNKNOWN"


def tau_only_ready(processed_dir: Path) -> tuple[str, List[str]]:
    rows = read_csv_rows(processed_dir / "band_lut.csv")
    if not rows:
        return "NO", ["band_lut.csv missing or empty"]

    bad = 0
    for row in rows:
        for column in ["tau_up_band", "tau_down_band"]:
            value = parse_float(row.get(column))
            if value is not None and not (0.0 <= value <= 1.0):
                bad += 1
    if bad:
        return "NO", [f"tau values outside 0..1: {bad}"]
    return "YES", [f"band_lut.csv rows={len(rows)} tau range OK"]


def si_candidate_readiness(processed_dir: Path) -> tuple[str, List[str], str]:
    rows = read_csv_rows(processed_dir / "band_lut_si_candidate.csv")
    qc_text = read_text(processed_dir / "qc_band_lut_si_candidate.md")
    status = extract_status(qc_text)
    if not rows:
        return "NO", ["band_lut_si_candidate.csv missing or empty"], status
    if status == "FAIL":
        return "NO", ["qc_band_lut_si_candidate.md reports FAIL"], status

    unit_statuses = {row.get("unit_status", "") for row in rows}
    if unit_statuses != {"CONFIRMED_PER_CM1_TO_SI_CANDIDATE"}:
        return "NO", [f"unexpected unit_status values: {', '.join(sorted(unit_statuses))}"], status

    numeric_bad = 0
    for row in rows:
        for column in [
            "path_radiance_band_W_m2_sr_um",
            "sky_radiance_band_W_m2_sr_um",
            "path_scattering_radiance_band_W_m2_sr_um",
            "solar_irradiance_band_W_m2_um",
        ]:
            raw = row.get(column, "")
            if raw and parse_float(raw) is None:
                numeric_bad += 1
    if numeric_bad:
        return "NO", [f"non-finite SI candidate numeric values: {numeric_bad}"], status

    return "NUMERIC_REVIEW_ONLY", [f"band_lut_si_candidate.csv rows={len(rows)} qc_status={status}"], status


def collect_warnings(processed_dir: Path) -> List[str]:
    warnings: List[str] = []
    qc_text = read_text(processed_dir / "qc_band_lut_si_candidate.md")
    if "high_altitude_low_sensitivity" in qc_text:
        warnings.append("high_altitude_low_sensitivity")
    if "WARNING_VISIBILITY_LOW_SENSITIVITY" in qc_text:
        warnings.append("WARNING_VISIBILITY_LOW_SENSITIVITY")
    if "mwir_short_range_path_radiance_nonmonotonic" in qc_text:
        warnings.append("mwir_short_range_path_radiance_nonmonotonic_if_any")
    if "low_altitude_short_range_low_sensitivity" in qc_text:
        warnings.append("low_altitude_short_range_low_sensitivity")
    return warnings or ["none"]


def build_report(processed_dir: Path) -> str:
    tau_ready, tau_reasons = tau_only_ready(processed_dir)
    si_ready, si_reasons, si_qc_status = si_candidate_readiness(processed_dir)
    warnings = collect_warnings(processed_dir)
    smoke_status = extract_status(read_text(processed_dir / "qc_aerosol_override_smoke.md"))

    lines = [
        "# MODTRAN LUT Readiness Report",
        "",
        f"- tau_only_debug_ready: {tau_ready}",
        f"- cpp_tau_only_loader_allowed: {'YES' if tau_ready == 'YES' else 'NO'}",
        f"- radiance_si_candidate_ready: {si_ready}",
        "- cpp_radiance_integration_allowed: NO",
        "- production_rerun_required: NO",
        "- pcmodwin_template_rebuild_required: NO",
        f"- si_candidate_qc_status: {si_qc_status}",
        f"- aerosol_override_smoke_status: {smoke_status}",
        "",
        "## Evidence",
        "",
    ]
    for reason in tau_reasons:
        lines.append(f"- tau_only: {reason}")
    for reason in si_reasons:
        lines.append(f"- si_candidate: {reason}")

    lines.extend(["", "## Remaining Warnings", ""])
    for warning in warnings:
        lines.append(f"- {warning}")

    lines.extend(
        [
            "",
            "## Recommended C++ Scope",
            "",
            "- 可以做 tau-only debug loader。",
            "- 可以加载 band_lut.csv，或加载 band_lut_si_candidate.csv 但只读取 tau_up/tau_down。",
            "- tau_up/tau_down 可用于 debug 查询。",
            "- path_radiance、sky_radiance、solar_irradiance 只允许离线数值检查，暂不进入渲染链路。",
            "- 不要直接改变 shader 视觉效果。",
            "- path/sky/solar 接入前先人工审查 SI 数量级。",
            "",
            "## Boundaries",
            "",
            "- 本报告没有重新运行 MODTRAN。",
            "- 本报告没有覆盖 production band_lut.csv。",
            "- 本报告不批准 VIS/SWIR/LWIR production。",
            "- 本报告不修改 HwaSimIR C++ 或 shader。",
        ]
    )
    return "\n".join(lines) + "\n"


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--processed-dir", required=True)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_arg_parser().parse_args(argv)
    processed_dir = Path(args.processed_dir)
    output_path = processed_dir / "lut_readiness_report.md"
    output_path.write_text(build_report(processed_dir), encoding="utf-8-sig")
    print(f"Wrote LUT readiness report to {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
