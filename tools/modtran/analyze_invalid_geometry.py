#!/usr/bin/env python3
"""Audit production NIR/MWIR MODTRAN cases excluded by slant geometry."""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Sequence

from diagnostics_common import parse_float, read_csv, upsert_marked_section, write_csv


OUTPUT_COLUMNS = [
    "case_id",
    "band",
    "mode",
    "observer_alt_km",
    "target_alt_km",
    "range_km",
    "vertical_delta_km",
    "is_geometrically_impossible",
    "audit_status",
]


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, help="production_invalid_geometry_manifest.csv path.")
    parser.add_argument("--output", default="", help="Optional output CSV; defaults to invalid_geometry_audit.csv beside manifest.")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_arg_parser().parse_args(argv)
    manifest_path = Path(args.manifest)
    rows = read_csv(manifest_path)
    if not rows:
        print(f"ERROR: no invalid geometry rows found: {manifest_path}")
        return 2

    output_path = Path(args.output) if args.output else manifest_path.parent / "invalid_geometry_audit.csv"
    audit_rows = []
    impossible_count = 0
    review_count = 0
    for row in rows:
        observer_alt = parse_float(row.get("observer_alt_km"))
        target_alt = parse_float(row.get("target_alt_km"))
        range_km = parse_float(row.get("range_km"))
        vertical_delta = None
        impossible = False
        if observer_alt is not None and target_alt is not None and range_km is not None:
            vertical_delta = abs(observer_alt - target_alt)
            impossible = range_km + 1e-9 < vertical_delta
        if impossible:
            impossible_count += 1
            status = "EXPECTED_GEOMETRY_FILTER"
        else:
            review_count += 1
            status = "NEED_MANUAL_REVIEW"
        audit_rows.append(
            {
                "case_id": row.get("case_id", ""),
                "band": row.get("band", ""),
                "mode": row.get("mode", ""),
                "observer_alt_km": row.get("observer_alt_km", ""),
                "target_alt_km": row.get("target_alt_km", ""),
                "range_km": row.get("range_km", ""),
                "vertical_delta_km": "" if vertical_delta is None else f"{vertical_delta:.10g}",
                "is_geometrically_impossible": "true" if impossible else "false",
                "audit_status": status,
            }
        )

    write_csv(output_path, audit_rows, OUTPUT_COLUMNS)
    overall = "EXPECTED_GEOMETRY_FILTER" if impossible_count > len(rows) / 2 else "NEED_MANUAL_REVIEW"
    if review_count:
        overall = "NEED_MANUAL_REVIEW"

    qc_path = manifest_path.parent / "qc_report.md"
    lines = [
        "## Invalid Geometry Audit",
        "",
        f"- overall_status: {overall}",
        f"- invalid_rows: {len(rows)}",
        f"- expected_geometry_filter_rows: {impossible_count}",
        f"- need_manual_review_rows: {review_count}",
        f"- audit_csv: {output_path}",
        "",
        "| status | rows | explanation |",
        "| --- | ---: | --- |",
        f"| EXPECTED_GEOMETRY_FILTER | {impossible_count} | range_km is smaller than vertical altitude delta, so a straight slant path cannot connect the endpoints. |",
        f"| NEED_MANUAL_REVIEW | {review_count} | range_km is not smaller than vertical altitude delta but the row was excluded. |",
    ]
    if review_count:
        lines.extend(["", "### Manual Review Rows", "", "| case_id | observer_alt_km | target_alt_km | range_km | vertical_delta_km |", "| --- | ---: | ---: | ---: | ---: |"])
        for row in audit_rows:
            if row["audit_status"] == "NEED_MANUAL_REVIEW":
                lines.append(
                    f"| {row['case_id']} | {row['observer_alt_km']} | {row['target_alt_km']} | {row['range_km']} | {row['vertical_delta_km']} |"
                )
    upsert_marked_section(qc_path, "INVALID GEOMETRY AUDIT", "\n".join(lines))

    print(f"Wrote {len(audit_rows)} invalid geometry audit rows to {output_path}")
    print(f"overall_status={overall}")
    print(f"expected_geometry_filter_rows={impossible_count}")
    print(f"need_manual_review_rows={review_count}")
    return 0 if review_count == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())

