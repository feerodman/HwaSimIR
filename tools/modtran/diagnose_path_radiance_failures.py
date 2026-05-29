#!/usr/bin/env python3
"""Diagnose MWIR path radiance trend failures from band and spectral rows."""

from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

from diagnostics_common import format_float, min_max_mean, parse_float, read_csv, rectangular_average, upsert_marked_section, write_csv


OUTPUT_COLUMNS = [
    "observer_alt_km",
    "target_alt_km",
    "visibility_km",
    "range_km",
    "path_radiance_band",
    "tau_up_band",
    "source_case_id",
    "MODOUT2_parser_column_mapping",
    "path_radiance_spectral_min",
    "path_radiance_spectral_max",
    "path_radiance_spectral_mean",
    "path_radiance_spectral_rectangular_average",
    "diagnosis",
]


def thermal_case_id(row: Dict[str, str]) -> str:
    for case_id in row.get("source_case_ids", "").split(";"):
        if "_thermal_" in case_id:
            return case_id
    return ""


def key(row: Dict[str, str]) -> Tuple[float, float, float]:
    return (
        parse_float(row.get("observer_alt_km")) or 0.0,
        parse_float(row.get("target_alt_km")) or 0.0,
        parse_float(row.get("visibility_km")) or 0.0,
    )


def stream_path_stats(path_csv: Path, case_ids: set[str]) -> Dict[str, Dict[str, float]]:
    buckets: Dict[str, Dict[str, object]] = {
        case_id: {"values": [], "points": []} for case_id in case_ids
    }
    with path_csv.open(newline="", encoding="utf-8-sig") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            case_id = row.get("case_id", "")
            if case_id not in buckets:
                continue
            value = parse_float(row.get("path_radiance"))
            wavelength = parse_float(row.get("wavelength_um"))
            if value is None:
                continue
            buckets[case_id]["values"].append(value)  # type: ignore[index,union-attr]
            if wavelength is not None:
                buckets[case_id]["points"].append((wavelength, value))  # type: ignore[index,union-attr]

    stats: Dict[str, Dict[str, float]] = {}
    for case_id, bucket in buckets.items():
        values = bucket["values"]  # type: ignore[assignment]
        points = bucket["points"]  # type: ignore[assignment]
        min_value, max_value, mean_value = min_max_mean(values)
        rect_value = rectangular_average(points)
        stats[case_id] = {
            "min": min_value if min_value is not None else float("nan"),
            "max": max_value if max_value is not None else float("nan"),
            "mean": mean_value if mean_value is not None else float("nan"),
            "rect": rect_value if rect_value is not None else float("nan"),
        }
    return stats


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--processed-dir", required=True)
    parser.add_argument("--raw-dir", required=True, help="Accepted for CLI symmetry; production spectral CSV is authoritative.")
    parser.add_argument("--output", default="")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_arg_parser().parse_args(argv)
    processed_dir = Path(args.processed_dir)
    band_rows = [row for row in read_csv(processed_dir / "band_lut.csv") if row.get("band") == "MWIR" and row.get("path_radiance_band")]
    by_condition: Dict[Tuple[float, float, float], List[Dict[str, str]]] = defaultdict(list)
    for row in band_rows:
        by_condition[key(row)].append(row)

    failing_pairs: List[Tuple[Dict[str, str], Dict[str, str]]] = []
    for rows in by_condition.values():
        ordered = sorted(rows, key=lambda item: parse_float(item.get("range_km")) or 0.0)
        for previous, current in zip(ordered, ordered[1:]):
            prev_value = parse_float(previous.get("path_radiance_band"))
            curr_value = parse_float(current.get("path_radiance_band"))
            if prev_value is None or curr_value is None:
                continue
            tolerance = max(abs(prev_value) * 0.15, 1e-12)
            if curr_value + tolerance < prev_value:
                failing_pairs.append((previous, current))

    case_ids = {thermal_case_id(row) for pair in failing_pairs for row in pair if thermal_case_id(row)}
    stats = stream_path_stats(processed_dir / "path_lut_spectral.csv", case_ids) if case_ids else {}

    output_rows: List[Dict[str, object]] = []
    failure_summaries = []
    for previous, current in failing_pairs:
        previous_case = thermal_case_id(previous)
        current_case = thermal_case_id(current)
        prev_stats = stats.get(previous_case, {})
        curr_stats = stats.get(current_case, {})
        prev_rect = prev_stats.get("rect")
        curr_rect = curr_stats.get("rect")
        if prev_rect is not None and curr_rect is not None and curr_rect < prev_rect:
            diagnosis = "RAW_MODTRAN_TREND"
        else:
            diagnosis = "BAND_INTEGRATION_BUG"
        failure_summaries.append(
            (
                previous.get("observer_alt_km", ""),
                previous.get("target_alt_km", ""),
                previous.get("visibility_km", ""),
                previous.get("range_km", ""),
                current.get("range_km", ""),
                previous.get("path_radiance_band", ""),
                current.get("path_radiance_band", ""),
                diagnosis,
            )
        )
        for row, case_id in [(previous, previous_case), (current, current_case)]:
            row_stats = stats.get(case_id, {})
            output_rows.append(
                {
                    "observer_alt_km": row.get("observer_alt_km", ""),
                    "target_alt_km": row.get("target_alt_km", ""),
                    "visibility_km": row.get("visibility_km", ""),
                    "range_km": row.get("range_km", ""),
                    "path_radiance_band": row.get("path_radiance_band", ""),
                    "tau_up_band": row.get("tau_up_band", ""),
                    "source_case_id": case_id,
                    "MODOUT2_parser_column_mapping": "FREQ,TOT_TRANS,PTH_THRML",
                    "path_radiance_spectral_min": format_float(row_stats.get("min")),
                    "path_radiance_spectral_max": format_float(row_stats.get("max")),
                    "path_radiance_spectral_mean": format_float(row_stats.get("mean")),
                    "path_radiance_spectral_rectangular_average": format_float(row_stats.get("rect")),
                    "diagnosis": diagnosis,
                }
            )

    output_path = Path(args.output) if args.output else processed_dir / "path_radiance_failure_diagnosis.csv"
    write_csv(output_path, output_rows, OUTPUT_COLUMNS)

    raw_count = sum(1 for *_, diagnosis in failure_summaries if diagnosis == "RAW_MODTRAN_TREND")
    integration_count = sum(1 for *_, diagnosis in failure_summaries if diagnosis == "BAND_INTEGRATION_BUG")
    lines = [
        "## Production MWIR Path Radiance Failure Diagnosis",
        "",
        f"- failing_range_pairs: {len(failing_pairs)}",
        f"- output_csv: {output_path}",
        f"- raw_modtran_trend_pairs: {raw_count}",
        f"- band_integration_bug_pairs: {integration_count}",
        "",
        "| observer_alt_km | target_alt_km | visibility_km | range_pair_km | band_values | diagnosis |",
        "| ---: | ---: | ---: | --- | --- | --- |",
    ]
    for observer_alt, target_alt, visibility, range_a, range_b, value_a, value_b, diagnosis in failure_summaries:
        lines.append(f"| {observer_alt} | {target_alt} | {visibility} | {range_a}->{range_b} | {value_a}->{value_b} | {diagnosis} |")
    upsert_marked_section(processed_dir / "qc_report.md", "PRODUCTION MWIR PATH RADIANCE FAILURE DIAGNOSIS", "\n".join(lines))

    print(f"Wrote {len(output_rows)} path radiance diagnosis rows to {output_path}")
    print(f"failing_range_pairs={len(failing_pairs)}")
    print(f"RAW_MODTRAN_TREND={raw_count}")
    print(f"BAND_INTEGRATION_BUG={integration_count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

