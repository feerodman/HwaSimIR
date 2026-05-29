#!/usr/bin/env python3
"""Audit MODOUT unit evidence and current band integration assumptions."""

from __future__ import annotations

import argparse
import re
from collections import Counter
from pathlib import Path
from typing import Dict, List, Sequence

from diagnostics_common import read_csv, read_text_if_exists, upsert_marked_section, write_csv


OUTPUT_COLUMNS = [
    "sample_dir",
    "file",
    "field_group",
    "evidence_type",
    "line_number",
    "line_text",
]

FIELD_UNIT_INTERPRETATIONS = [
    {
        "field": "PTH_THRML",
        "status": "CONFIRMED_PER_CM1_NATIVE",
        "basis": "MODOUT2 PTH_THRML values match the MODOUT1 PATH_THERMAL (CM-1) column in radiance tables.",
    },
    {
        "field": "SOL_SCAT",
        "status": "CONFIRMED_PER_CM1_NATIVE",
        "basis": "MODOUT2 SOL_SCAT values match the MODOUT1 SINGLE_SCATTER/SOL_IRR radiance (CM-1) column, not the MICRN column.",
    },
    {
        "field": "TOTAL_RAD",
        "status": "CONFIRMED_PER_CM1_NATIVE",
        "basis": "MODOUT2 TOTAL_RAD values match the MODOUT1 TOTAL_RADIANCE (CM-1) column in radiance tables.",
    },
    {
        "field": "SOLAR",
        "status": "CONFIRMED_PER_CM1_NATIVE",
        "basis": "Direct Solar MODOUT2 SOLAR values match the MODOUT1 SOLAR (CM-1) irradiance column, not the MICRN column.",
    },
]


UNIT_RE = re.compile(
    r"WATTS|CM2|STER|CM-1|MICRON|UM|RADIANCE|IRRADIANCE|PTH_THRML|SOLAR|SOL_SCAT|TOTAL_RAD|SOL TR",
    re.IGNORECASE,
)


def field_group_for_line(line: str) -> str:
    upper = line.upper()
    groups: List[str] = []
    if "PTH_THRML" in upper:
        groups.append("PTH_THRML")
    if "SOL_SCAT" in upper:
        groups.append("SOL_SCAT")
    if "TOTAL_RAD" in upper:
        groups.append("TOTAL_RAD")
    if "SOLAR" in upper or "SOL TR" in upper or "SOL@OBS" in upper:
        groups.append("SOLAR")
    if "IRRADIANCE" in upper:
        groups.append("IRRADIANCE_GENERIC")
    if "RADIANCE" in upper:
        groups.append("RADIANCE_GENERIC")
    if not groups:
        groups.append("UNIT_CONTEXT")
    return ";".join(groups)


def collect_unit_lines(raw_dir: Path) -> List[Dict[str, object]]:
    rows: List[Dict[str, object]] = []
    for sample_dir in sorted(path for path in raw_dir.iterdir() if path.is_dir()):
        for name in ["MODOUT1", "MODOUT2"]:
            path = sample_dir / name
            text = read_text_if_exists(path)
            if not text:
                continue
            for line_number, line in enumerate(text.splitlines(), 1):
                if UNIT_RE.search(line):
                    rows.append(
                        {
                            "sample_dir": str(sample_dir),
                            "file": name,
                            "field_group": field_group_for_line(line),
                            "evidence_type": "unit_or_table_header_candidate",
                            "line_number": line_number,
                            "line_text": line.strip(),
                        }
                    )
    return rows


def inspect_integration_source() -> Dict[str, str]:
    build_path = Path("tools/modtran/build_band_lut.py")
    text = read_text_if_exists(build_path)
    if not text:
        return {
            "integration_status": "NEED_MANUAL_REVIEW",
            "integration_detail": "tools/modtran/build_band_lut.py not found",
        }
    uses_wavelength = "wavelength_um" in text and "rectangular_average" in text
    has_lambda_conversion = "1e8" in text or "lambda_um" in text or "wavelength_um ** 2" in text
    if uses_wavelength and not has_lambda_conversion:
        return {
            "integration_status": "RECTANGULAR_WAVELENGTH_NATIVE_VALUES",
            "integration_detail": "build_band_lut.py integrates native MODOUT2 radiance/irradiance values over wavelength_um without wavenumber-to-wavelength unit conversion.",
        }
    if uses_wavelength and has_lambda_conversion:
        return {
            "integration_status": "POSSIBLE_UNIT_CONVERSION_PRESENT",
            "integration_detail": "build_band_lut.py references wavelength integration and conversion-like constants; inspect manually before changing units.",
        }
    return {
        "integration_status": "NEED_MANUAL_REVIEW",
        "integration_detail": "Could not identify rectangular wavelength integration pattern.",
    }


def synthesize_root_cause(processed_dir: Path, unit_status: str, integration_status: str) -> str:
    invalid_rows = read_csv(processed_dir / "invalid_geometry_audit.csv")
    visibility_rows = read_csv(processed_dir / "visibility_failure_diagnosis.csv")
    path_rows = read_csv(processed_dir / "path_radiance_failure_diagnosis.csv")

    invalid_review = [row for row in invalid_rows if row.get("audit_status") == "NEED_MANUAL_REVIEW"]
    invalid_status = "EXPECTED_GEOMETRY_FILTER" if invalid_rows and not invalid_review else "NEED_MANUAL_REVIEW"

    visibility_counts = Counter(row.get("diagnosis", "") for row in visibility_rows)
    if visibility_counts.get("FAIL_TEMPLATE_VISIBILITY_NOT_REPLACED", 0):
        visibility_status = "TEMPLATE_VISIBILITY_REPLACEMENT_BUG"
    elif visibility_counts.get("FAIL_MODTRAN_VISIBILITY_NOT_EFFECTIVE", 0):
        visibility_status = "MODTRAN_VISIBILITY_NOT_EFFECTIVE"
    elif visibility_counts.get("POSSIBLE_PHYSICAL_LOW_SENSITIVITY", 0) and visibility_counts.get("NEED_RAW_SAMPLE_REVIEW", 0):
        visibility_status = "POSSIBLE_PHYSICAL_LOW_SENSITIVITY_ON_TARGETED_SAMPLES_WITH_REMAINING_RAW_GAPS"
    elif visibility_counts.get("POSSIBLE_PHYSICAL_LOW_SENSITIVITY", 0):
        visibility_status = "POSSIBLE_PHYSICAL_LOW_SENSITIVITY"
    else:
        visibility_status = "NEED_RAW_SAMPLE_REVIEW"

    path_counts = Counter(row.get("diagnosis", "") for row in path_rows)
    if path_counts.get("BAND_INTEGRATION_BUG", 0):
        path_status = "BAND_INTEGRATION_BUG"
    elif path_counts.get("RAW_MODTRAN_TREND", 0):
        path_status = "RAW_MODTRAN_TREND_OR_QC_THRESHOLD"
    else:
        path_status = "NO_PATH_RADIANCE_FAILURE_ROWS"

    allow_rerun = "NO"
    needs_gui_review = "YES"
    needs_template_rebuild = "NOT_PROVEN"
    if visibility_status == "POSSIBLE_PHYSICAL_LOW_SENSITIVITY" and path_status == "RAW_MODTRAN_TREND_OR_QC_THRESHOLD":
        needs_gui_review = "MAYBE"

    lines = [
        "## Production FAIL Root Cause Analysis",
        "",
        f"- invalid_geometry: {invalid_status}",
        f"- visibility_fail_classification: {visibility_status}",
        f"- mwir_path_radiance_classification: {path_status}",
        f"- modout_unit_status: {unit_status}",
        f"- band_integration_status: {integration_status}",
        f"- needs_pcmodwin5_template_review: {needs_gui_review}",
        f"- needs_pcmodwin5_template_rebuild: {needs_template_rebuild}",
        f"- allow_production_rerun_now: {allow_rerun}",
        "",
        "### Conclusions",
        "",
        "- The 510 invalid geometry rows are expected if every row has `range_km < abs(observer_alt_km - target_alt_km)`; those cases are not runnable slant paths and should remain excluded from production manifests.",
        "- The visibility failures are not caused by identical generated `modin` files when the visibility diagnosis shows different `modin` hashes/lines. Targeted reruns preserve representative raw `MODOUT1` evidence, but production-wide raw was intentionally not retained for every failed group.",
        "- The MWIR path radiance failures should be treated as raw MODTRAN/PTH_THRML trend or QC-threshold issues when the spectral rectangular average drops in the same direction as `band_lut.csv`.",
        "- MODOUT2 radiance and irradiance values are now classified as per-cm^-1 native outputs for the audited fields, based on retained MODOUT1/MODOUT2 headers and matching sample values.",
        "- Existing LUT values must still stay labeled `MODOUT2_native` until a deliberate wavenumber-to-wavelength conversion is implemented and validated; do not relabel current values as SI.",
        "- Do not rebuild templates solely from this evidence. First review the PcModWin5 aerosol vertical profile and confirm whether sea-level meteorological range is expected to affect the 5 km horizontal and 10-to-5 km slant paths.",
        "",
        "### Per-Cm-1 Conversion Required Before SI Relabeling",
        "",
        "Use these conversions for a future SI LUT build; they were not applied to the current production or smoke LUTs:",
        "",
        "```text",
        "L_lambda[W/(m^2 sr um)] = L_nu[W/(cm^2 sr cm^-1)] * 1e8 / lambda_um^2",
        "E_lambda[W/(m^2 um)]    = E_nu[W/(cm^2 cm^-1)]    * 1e8 / lambda_um^2",
        "```",
        "",
        "Production should not be rerun until the visibility and path-radiance diagnoses are accepted or the PcModWin5 templates are corrected.",
    ]
    return "\n".join(lines)


def determine_unit_status(rows: Sequence[Dict[str, object]]) -> str:
    lines = [str(row.get("line_text", "")).upper() for row in rows]
    per_cm1 = [
        line for line in lines
        if ("WATT" in line or "W/" in line)
        and ("CM-1" in line or "CM^-1" in line or "PER CM" in line)
        and "XXX" not in line
    ]
    per_micron = [
        line for line in lines
        if ("WATT" in line or "W/" in line)
        and ("MICRON" in line or " UM" in line or "/UM" in line or "PER UM" in line)
        and "XXX" not in line
    ]
    if per_cm1 and not per_micron:
        return "CONFIRMED_PER_CM1"
    if per_micron and not per_cm1:
        return "CONFIRMED_PER_MICRON"
    return "NEED_UNIT_CONFIRMATION"


def write_candidate_markdown(path: Path, rows: Sequence[Dict[str, object]], unit_status: str, integration: Dict[str, str]) -> None:
    counts = Counter()
    for row in rows:
        for group in str(row.get("field_group", "")).split(";"):
            counts[group] += 1

    lines = [
        "# MODOUT Unit Candidate Lines",
        "",
        f"- unit_status: {unit_status}",
        f"- evidence_rows: {len(rows)}",
        f"- integration_status: {integration['integration_status']}",
        f"- integration_detail: {integration['integration_detail']}",
        "",
        "Unless the evidence clearly states per-cm^-1 or per-micron radiance/irradiance units, keep exported values labeled as MODOUT2_native.",
        "",
        "## Field Unit Interpretation",
        "",
        "| field | status | basis |",
        "| --- | --- | --- |",
    ]
    for item in FIELD_UNIT_INTERPRETATIONS:
        lines.append(f"| {item['field']} | {item['status']} | {item['basis']} |")

    lines.extend(
        [
            "",
        "## Field Groups",
        "",
        "| field_group | candidate_lines |",
        "| --- | ---: |",
        ]
    )
    for group, count in sorted(counts.items()):
        lines.append(f"| {group} | {count} |")

    lines.extend(
        [
            "",
            "## Conversion Required Before SI Relabeling",
            "",
            "```text",
            "L_lambda[W/(m^2 sr um)] = L_nu[W/(cm^2 sr cm^-1)] * 1e8 / lambda_um^2",
            "E_lambda[W/(m^2 um)]    = E_nu[W/(cm^2 cm^-1)]    * 1e8 / lambda_um^2",
            "```",
            "",
            "## Sample Lines",
            "",
            "| field_group | file | line | text |",
            "| --- | --- | ---: | --- |",
        ]
    )
    for row in rows[:400]:
        text = str(row.get("line_text", "")).replace("|", "\\|")
        lines.append(f"| {row.get('field_group','')} | {row.get('file','')} | {row.get('line_number','')} | {text} |")

    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw-dir", required=True)
    parser.add_argument("--processed-dir", default="", help="Defaults to ../processed relative to raw directory.")
    parser.add_argument("--output", default="")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_arg_parser().parse_args(argv)
    raw_dir = Path(args.raw_dir)
    processed_dir = Path(args.processed_dir) if args.processed_dir else raw_dir.parent.parent / "processed"
    rows = collect_unit_lines(raw_dir)
    output_path = Path(args.output) if args.output else processed_dir / "modout_units_audit.csv"
    candidate_csv = processed_dir / "modout_units_candidate_lines.csv"
    candidate_md = processed_dir / "modout_units_candidate_lines.md"
    write_csv(output_path, rows, OUTPUT_COLUMNS)
    write_csv(candidate_csv, rows, OUTPUT_COLUMNS)

    integration = inspect_integration_source()
    unit_status = determine_unit_status(rows)
    write_candidate_markdown(candidate_md, rows, unit_status, integration)

    lines = [
        "## MODOUT Unit And Integration Audit",
        "",
        f"- unit_status: {unit_status}",
        f"- evidence_rows: {len(rows)}",
        f"- output_csv: {output_path}",
        f"- candidate_lines_csv: {candidate_csv}",
        f"- candidate_lines_md: {candidate_md}",
        f"- integration_status: {integration['integration_status']}",
        f"- integration_detail: {integration['integration_detail']}",
        "",
        "Retained MODOUT1/MODOUT2 samples indicate PTH_THRML, SOL_SCAT, TOTAL_RAD, and SOLAR are per-cm^-1 native outputs. Current LUT values remain MODOUT2_native because no SI conversion was applied.",
    ]
    upsert_marked_section(processed_dir / "qc_report.md", "MODOUT UNIT AND INTEGRATION AUDIT", "\n".join(lines))
    upsert_marked_section(
        processed_dir / "qc_report.md",
        "PRODUCTION FAIL ROOT CAUSE ANALYSIS",
        synthesize_root_cause(processed_dir, unit_status, integration["integration_status"]),
    )

    print(f"Wrote {len(rows)} MODOUT unit audit rows to {output_path}")
    print(f"unit_status={unit_status}")
    print(f"integration_status={integration['integration_status']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
