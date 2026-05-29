#!/usr/bin/env python3
"""Diagnose production visibility-sensitivity failures from band LUT and raw samples."""

from __future__ import annotations

import argparse
import re
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

from diagnostics_common import (
    PRODUCTION_VISIBILITIES,
    VISIBILITY_SENSITIVITY_EPS,
    parse_float,
    project_path,
    read_csv,
    read_text_if_exists,
    sha256_short,
    upsert_marked_section,
    write_csv,
)


OUTPUT_COLUMNS = [
    "band",
    "observer_alt_km",
    "target_alt_km",
    "range_km",
    "visibility_km",
    "tau_up_band",
    "source_case_id",
    "requested_visibility",
    "modin_visibility_line_or_hash",
    "modin_sha256",
    "MODOUT1_actual_aerosol_visibility_lines",
    "modout1_sha256",
    "raw_sample_dir",
    "diagnosis",
]


VISIBILITY_LINE_RE = re.compile(r"METEOROLOGICAL_RANGE|AEROSOL|CARD 2|KM", re.IGNORECASE)


def stable_key(row: Dict[str, str]) -> Tuple[str, float, float, float]:
    return (
        row.get("band", ""),
        parse_float(row.get("observer_alt_km")) or 0.0,
        parse_float(row.get("target_alt_km")) or 0.0,
        parse_float(row.get("range_km")) or 0.0,
    )


def is_high_altitude_horizontal(observer_alt: float, target_alt: float) -> bool:
    return observer_alt == target_alt and observer_alt >= 10.0


def transmittance_case_id(row: Dict[str, str]) -> str:
    for case_id in row.get("source_case_ids", "").split(";"):
        if "_transmittance_" in case_id:
            return case_id
    return row.get("source_case_ids", "").split(";")[0] if row.get("source_case_ids") else ""


def extract_modin_visibility_line(text: str) -> str:
    lines = text.splitlines()
    if len(lines) >= 3:
        return f"line3:{lines[2].strip()}"
    return f"sha256:{sha256_short(text)}" if text else "MISSING_MODIN"


def extract_modout1_visibility_lines(text: str) -> str:
    if not text:
        return "MISSING_MODOUT1"
    selected = []
    for line in text.splitlines():
        if VISIBILITY_LINE_RE.search(line):
            selected.append(line.strip())
        if len(selected) >= 8:
            break
    return " | ".join(selected) if selected else "NO_AEROSOL_VISIBILITY_LINES_FOUND"


def normalize_modout1_visibility(text: str) -> str:
    return extract_modout1_visibility_lines(text)


def diagnose_group(group_rows: Sequence[Dict[str, object]]) -> str:
    modin_texts = [str(row.get("_modin_text", "")) for row in group_rows]
    modout1_texts = [str(row.get("_modout1_text", "")) for row in group_rows]
    modout1_available = all(text != "" for text in modout1_texts)
    if len(set(modin_texts)) == 1:
        return "FAIL_TEMPLATE_VISIBILITY_NOT_REPLACED"
    if modout1_available and len({normalize_modout1_visibility(text) for text in modout1_texts}) == 1:
        return "FAIL_MODTRAN_VISIBILITY_NOT_EFFECTIVE"
    if modout1_available:
        return "POSSIBLE_PHYSICAL_LOW_SENSITIVITY"
    return "NEED_RAW_SAMPLE_REVIEW"


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--processed-dir", required=True)
    parser.add_argument("--raw-dir", required=True)
    parser.add_argument("--output", default="")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_arg_parser().parse_args(argv)
    processed_dir = Path(args.processed_dir)
    raw_dir = Path(args.raw_dir)
    root = Path.cwd()
    band_rows = read_csv(processed_dir / "band_lut.csv")
    manifest_rows = read_csv(processed_dir / "manifest.csv")
    manifest_by_case = {row.get("case_id", ""): row for row in manifest_rows}

    grouped: Dict[Tuple[str, float, float, float], List[Dict[str, str]]] = defaultdict(list)
    for row in band_rows:
        tau = parse_float(row.get("tau_up_band"))
        visibility = parse_float(row.get("visibility_km"))
        if tau is None or visibility is None:
            continue
        grouped[stable_key(row)].append(row)

    output_rows: List[Dict[str, object]] = []
    failed_groups = []
    for key, rows in sorted(grouped.items()):
        band, observer_alt, target_alt, range_km = key
        if is_high_altitude_horizontal(observer_alt, target_alt):
            continue
        values = {parse_float(row.get("visibility_km")): parse_float(row.get("tau_up_band")) for row in rows}
        if any(visibility not in values for visibility in PRODUCTION_VISIBILITIES):
            continue
        spread = max(value for value in values.values() if value is not None) - min(value for value in values.values() if value is not None)
        if spread > VISIBILITY_SENSITIVITY_EPS:
            continue

        group_output_rows: List[Dict[str, object]] = []
        for row in sorted(rows, key=lambda item: parse_float(item.get("visibility_km")) or 0.0):
            visibility = parse_float(row.get("visibility_km"))
            if visibility not in PRODUCTION_VISIBILITIES:
                continue
            case_id = transmittance_case_id(row)
            sample_dir = raw_dir / case_id
            manifest = manifest_by_case.get(case_id, {})
            sample_modin = sample_dir / "modin"
            generated_modin = project_path(root, manifest.get("modin_file", "")) if manifest.get("modin_file") else Path()
            modin_text = read_text_if_exists(sample_modin) or read_text_if_exists(generated_modin)
            modout1_text = read_text_if_exists(sample_dir / "MODOUT1")
            group_output_rows.append(
                {
                    "band": band,
                    "observer_alt_km": f"{observer_alt:g}",
                    "target_alt_km": f"{target_alt:g}",
                    "range_km": f"{range_km:g}",
                    "visibility_km": "" if visibility is None else f"{visibility:g}",
                    "tau_up_band": row.get("tau_up_band", ""),
                    "source_case_id": case_id,
                    "requested_visibility": "" if visibility is None else f"{visibility:g}",
                    "modin_visibility_line_or_hash": extract_modin_visibility_line(modin_text),
                    "modin_sha256": sha256_short(modin_text) if modin_text else "MISSING_MODIN",
                    "MODOUT1_actual_aerosol_visibility_lines": extract_modout1_visibility_lines(modout1_text),
                    "modout1_sha256": sha256_short(modout1_text) if modout1_text else "MISSING_MODOUT1",
                    "raw_sample_dir": str(sample_dir) if sample_dir.exists() else "MISSING_RAW_SAMPLE_DIR",
                    "_modin_text": modin_text,
                    "_modout1_text": modout1_text,
                }
            )
        diagnosis = diagnose_group(group_output_rows)
        for output_row in group_output_rows:
            output_row["diagnosis"] = diagnosis
            output_row.pop("_modin_text", None)
            output_row.pop("_modout1_text", None)
            output_rows.append(output_row)
        failed_groups.append((band, observer_alt, target_alt, range_km, spread, diagnosis))

    output_path = Path(args.output) if args.output else processed_dir / "visibility_failure_diagnosis.csv"
    write_csv(output_path, output_rows, OUTPUT_COLUMNS)

    diagnosis_counts: Dict[str, int] = defaultdict(int)
    for *_, diagnosis in failed_groups:
        diagnosis_counts[diagnosis] += 1

    lines = [
        "## Production Visibility Failure Diagnosis",
        "",
        f"- failed_groups: {len(failed_groups)}",
        f"- output_csv: {output_path}",
        f"- visibility_sensitivity_eps: {VISIBILITY_SENSITIVITY_EPS:g}",
        "",
        "### Diagnosis Counts",
        "",
        "| diagnosis | groups |",
        "| --- | ---: |",
    ]
    for diagnosis, count in sorted(diagnosis_counts.items()):
        lines.append(f"| {diagnosis} | {count} |")
    lines.extend(["", "### Failed Groups", "", "| band | observer_alt_km | target_alt_km | range_km | spread | diagnosis |", "| --- | ---: | ---: | ---: | ---: | --- |"])
    for band, observer_alt, target_alt, range_km, spread, diagnosis in failed_groups:
        lines.append(f"| {band} | {observer_alt:g} | {target_alt:g} | {range_km:g} | {spread:.10g} | {diagnosis} |")
    upsert_marked_section(processed_dir / "qc_report.md", "PRODUCTION VISIBILITY FAILURE DIAGNOSIS", "\n".join(lines))

    print(f"Wrote {len(output_rows)} visibility diagnosis rows to {output_path}")
    print(f"failed_groups={len(failed_groups)}")
    for diagnosis, count in sorted(diagnosis_counts.items()):
        print(f"{diagnosis}={count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

