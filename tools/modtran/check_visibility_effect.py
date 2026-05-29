#!/usr/bin/env python3
"""Audit whether PcModWin5 visibility settings are reflected in MODTRAN outputs."""

from __future__ import annotations

import argparse
import csv
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple


PILOT72_STATUS = "pilot72_succeeded"
SMOKE_STATUS = "visibility_smoke_succeeded"
VISIBILITIES = [5.0, 23.0, 50.0]
SMOKE_ALTITUDES = [(3.0, 3.0), (10.0, 10.0), (20.0, 3.0)]
SMOKE_SENSITIVITY_EPS = 1e-4


def read_csv(path: Path) -> List[Dict[str, str]]:
    if not path.exists() or path.stat().st_size == 0:
        return []
    with path.open(newline="", encoding="utf-8-sig") as handle:
        return list(csv.DictReader(handle))


def write_csv(path: Path, rows: Sequence[Dict[str, object]], columns: Sequence[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=columns, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def parse_float(value: object) -> float | None:
    if value is None or value == "":
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def float_key(value: object) -> str:
    parsed = parse_float(value)
    return f"{parsed:g}" if parsed is not None else str(value or "")


def read_text(path: Path) -> str:
    if not path.exists():
        return ""
    return path.read_text(encoding="utf-8", errors="replace")


def parse_modin_visibility(modin_text: str) -> str:
    lines = modin_text.splitlines()
    if len(lines) < 3:
        return "NEED_MANUAL_REVIEW"
    tokens = re.findall(r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[Ee][-+]?\d+)?", lines[2])
    if len(tokens) >= 7:
        return f"CARD2_token7={float(tokens[6]):g}; line={lines[2].strip()}"
    return f"NEED_MANUAL_REVIEW; line={lines[2].strip()}"


def parse_modout1_visibility(modout1_text: str) -> str:
    match = re.search(
        r"BOUNDARY\s+LAYER.*?RURAL\s+([0-9]+(?:\.[0-9]+)?)\s+KM\s+METEOROLOGICAL_RANGE_AT_SEA_LEVEL",
        modout1_text,
        flags=re.IGNORECASE,
    )
    if match:
        return f"RURAL {float(match.group(1)):g} KM METEOROLOGICAL_RANGE_AT_SEA_LEVEL"

    card2 = re.search(r"CARD\s+2\s+\*+\s*(.+)", modout1_text, flags=re.IGNORECASE)
    if card2:
        return f"NEED_MANUAL_REVIEW; CARD2={card2.group(1).strip()}"

    return "NEED_MANUAL_REVIEW"


def rows_by_key(rows: Sequence[Dict[str, str]], key_columns: Sequence[str]) -> Dict[Tuple[str, ...], List[Dict[str, str]]]:
    grouped: Dict[Tuple[str, ...], List[Dict[str, str]]] = defaultdict(list)
    for row in rows:
        grouped[tuple(float_key(row.get(column, "")) for column in key_columns)].append(row)
    return dict(grouped)


def band_lut_lookup(rows: Sequence[Dict[str, str]]) -> Dict[Tuple[str, str, str, str, str], Dict[str, str]]:
    lookup = {}
    for row in rows:
        key = (
            row.get("band", ""),
            float_key(row.get("observer_alt_km", "")),
            float_key(row.get("target_alt_km", "")),
            float_key(row.get("range_km", "")),
            float_key(row.get("visibility_km", "")),
        )
        lookup[key] = row
    return lookup


def upsert_marked_section(path: Path, marker: str, section: str) -> None:
    begin = f"<!-- BEGIN {marker} -->"
    end = f"<!-- END {marker} -->"
    text = read_text(path) if path.exists() else "# MODTRAN LUT QC Report\n"
    block = f"\n{begin}\n{section.rstrip()}\n{end}\n"
    if begin in text and end in text:
        before = text.split(begin, 1)[0]
        after = text.split(end, 1)[1]
        path.write_text(before.rstrip() + block + after.lstrip(), encoding="utf-8")
    else:
        path.write_text(text.rstrip() + "\n" + block, encoding="utf-8")


def same_nonempty(values: Iterable[str]) -> bool:
    clean = [value for value in values if value != ""]
    return bool(clean) and len(set(clean)) == 1


def build_pilot_audit(processed_dir: Path, raw_dir: Path) -> tuple[List[Dict[str, object]], List[str], str]:
    manifest = [row for row in read_csv(processed_dir / "manifest.csv") if row.get("status") == PILOT72_STATUS]
    band_rows = read_csv(processed_dir / "band_lut.csv")
    band_lookup = band_lut_lookup(band_rows)

    group_columns = ["band", "mode", "observer_alt_km", "target_alt_km", "range_km"]
    grouped = rows_by_key(manifest, group_columns)
    audit_rows: List[Dict[str, object]] = []
    statuses: List[str] = []

    for key, rows in sorted(grouped.items()):
        vis_rows = {float_key(row.get("visibility_km", "")): row for row in rows}
        if not all(f"{visibility:g}" in vis_rows for visibility in VISIBILITIES):
            continue

        modin_texts = {}
        tau_values = []
        group_modin_fields = []
        for visibility in VISIBILITIES:
            row = vis_rows[f"{visibility:g}"]
            case_id = row.get("case_id", "")
            sample_dir = raw_dir / case_id
            modin_text = read_text(sample_dir / "modin")
            modout1_text = read_text(sample_dir / "MODOUT1")
            modin_texts[f"{visibility:g}"] = modin_text
            modin_field = parse_modin_visibility(modin_text)
            group_modin_fields.append(modin_field)
            modout1_visibility = parse_modout1_visibility(modout1_text)
            band_key = (
                row.get("band", ""),
                float_key(row.get("observer_alt_km", "")),
                float_key(row.get("target_alt_km", "")),
                float_key(row.get("range_km", "")),
                f"{visibility:g}",
            )
            tau_up_band = band_lookup.get(band_key, {}).get("tau_up_band", "")
            if tau_up_band != "":
                tau_values.append(tau_up_band)
            audit_rows.append(
                {
                    "group": "|".join(key),
                    "case_id": case_id,
                    "requested_visibility_km": f"{visibility:g}",
                    "modin_visibility_field": modin_field,
                    "modout1_visibility": modout1_visibility,
                    "tau_up_band": tau_up_band,
                    "status": "NEED_MANUAL_REVIEW" if "NEED_MANUAL_REVIEW" in modout1_visibility else "OK",
                }
            )

        if len(set(modin_texts.values())) == 1:
            statuses.append(f"FAIL: modin files identical across visibilities for {'|'.join(key)}")
        elif same_nonempty(group_modin_fields):
            statuses.append(f"FAIL: suspected modin visibility field identical across visibilities for {'|'.join(key)}")

        if same_nonempty(tau_values):
            statuses.append(f"WARNING: tau_up_band identical across visibilities for {'|'.join(key)}")

    overall = "FAIL" if any(status.startswith("FAIL") for status in statuses) else "PASS"
    if not statuses:
        statuses.append("PASS: modin visibility fields differ across audited Pilot72 groups")
    return audit_rows, statuses, overall


def rectangular_average(rows: Sequence[Dict[str, str]], column: str) -> str:
    points = []
    for row in rows:
        wavelength = parse_float(row.get("wavelength_um", ""))
        value = parse_float(row.get(column, ""))
        if wavelength is not None and value is not None:
            points.append((wavelength, value))
    if not points:
        return ""
    points.sort(key=lambda item: item[0])
    if len(points) == 1:
        return f"{points[0][1]:.10g}"
    width = points[-1][0] - points[0][0]
    if width <= 0:
        return f"{sum(value for _, value in points) / len(points):.10g}"
    area = 0.0
    for (wave_a, value_a), (wave_b, value_b) in zip(points, points[1:]):
        area += 0.5 * (value_a + value_b) * (wave_b - wave_a)
    return f"{area / width:.10g}"


def rows_by_case(rows: Sequence[Dict[str, str]]) -> Dict[str, List[Dict[str, str]]]:
    grouped: Dict[str, List[Dict[str, str]]] = defaultdict(list)
    for row in rows:
        grouped[row.get("case_id", "")].append(row)
    return dict(grouped)


def build_smoke_band_lut(processed_dir: Path) -> List[Dict[str, object]]:
    manifest = [row for row in read_csv(processed_dir / "manifest.csv") if row.get("status") == SMOKE_STATUS]
    if not manifest:
        return []

    path_by_case = rows_by_case(read_csv(processed_dir / "path_lut_spectral.csv"))
    output = []
    seen = set()
    for row in manifest:
        key = (
            row.get("band", ""),
            row.get("atmosphere_model", ""),
            row.get("aerosol_model", ""),
            row.get("humidity_profile", ""),
            float_key(row.get("visibility_km", "")),
            float_key(row.get("observer_alt_km", "")),
            float_key(row.get("target_alt_km", "")),
            float_key(row.get("range_km", "")),
            float_key(row.get("solar_zenith_deg", "")),
        )
        if key in seen:
            continue
        seen.add(key)
        case_rows = path_by_case.get(row.get("case_id", ""), [])
        output.append(
            {
                "band": key[0],
                "atmosphere_model": key[1],
                "aerosol_model": key[2],
                "humidity_profile": key[3],
                "visibility_km": key[4],
                "observer_alt_km": key[5],
                "target_alt_km": key[6],
                "range_km": key[7],
                "solar_zenith_deg": key[8],
                "tau_up_band": rectangular_average(case_rows, "tau_up"),
                "source_case_ids": row.get("case_id", ""),
            }
        )
    return sorted(
        output,
        key=lambda item: (
            str(item["band"]),
            parse_float(item["observer_alt_km"]) or 0.0,
            parse_float(item["target_alt_km"]) or 0.0,
            parse_float(item["visibility_km"]) or 0.0,
        ),
    )


def smoke_values(rows: Sequence[Dict[str, object]], band: str, obs: float, target: float) -> List[tuple[float, float]]:
    values = []
    for row in rows:
        if row.get("band") != band:
            continue
        if parse_float(row.get("observer_alt_km")) != obs or parse_float(row.get("target_alt_km")) != target:
            continue
        visibility = parse_float(row.get("visibility_km"))
        tau = parse_float(row.get("tau_up_band"))
        if visibility is not None and tau is not None:
            values.append((visibility, tau))
    return sorted(values)


def evaluate_smoke(rows: Sequence[Dict[str, object]]) -> tuple[List[str], str]:
    lines: List[str] = []
    low_sensitive = False
    crossing_sensitive = False

    for band in ["MWIR", "NIR"]:
        for obs, target in SMOKE_ALTITUDES:
            values = smoke_values(rows, band, obs, target)
            if len(values) != 3:
                lines.append(f"FAIL: {band} obs={obs:g} target={target:g} missing smoke tau values")
                continue
            tau_span = max(tau for _, tau in values) - min(tau for _, tau in values)
            monotonic = all(values[index + 1][1] + SMOKE_SENSITIVITY_EPS >= values[index][1] for index in range(len(values) - 1))
            label = f"{band} obs={obs:g} target={target:g} range=50 vis 5->23->50 tau={', '.join(f'{v:g}:{t:.6g}' for v, t in values)}"
            if (obs, target) == (10.0, 10.0) and tau_span < SMOKE_SENSITIVITY_EPS:
                lines.append(f"INFO: {label}; high_altitude_low_sensitivity")
            elif not monotonic:
                lines.append(f"FAIL: {label}; tau_up_band is not generally increasing with visibility")
            elif tau_span < SMOKE_SENSITIVITY_EPS:
                lines.append(f"WARNING: {label}; visibility sensitivity is near zero")
            else:
                lines.append(f"PASS: {label}; visibility sensitivity detected")

            if (obs, target) == (3.0, 3.0) and tau_span >= SMOKE_SENSITIVITY_EPS:
                low_sensitive = True
            if (obs, target) == (20.0, 3.0) and tau_span >= SMOKE_SENSITIVITY_EPS:
                crossing_sensitive = True

    overall = "PASS"
    if not low_sensitive and not crossing_sensitive:
        lines.append("FAIL_VISIBILITY_NOT_EFFECTIVE: (3,3) and (20,3) both show no visibility sensitivity; review PcModWin5 aerosol templates in GUI.")
        overall = "FAIL_VISIBILITY_NOT_EFFECTIVE"
    elif any(line.startswith("FAIL:") for line in lines):
        overall = "FAIL"
    return lines, overall


def write_visibility_report(
    processed_dir: Path,
    audit_rows: Sequence[Dict[str, object]],
    audit_statuses: Sequence[str],
    audit_overall: str,
    smoke_rows: Sequence[Dict[str, object]],
    smoke_lines: Sequence[str],
    smoke_overall: str,
) -> None:
    lines = [
        "## Visibility Effect Audit",
        "",
        f"- pilot72_audit_status: {audit_overall}",
        f"- audited_rows: {len(audit_rows)}",
        "",
        "### Pilot72 Visibility Evidence",
        "",
        "| case_id | requested_visibility_km | modin_visibility_field | modout1_visibility | tau_up_band | status |",
        "| --- | ---: | --- | --- | ---: | --- |",
    ]
    for row in audit_rows:
        lines.append(
            f"| {row['case_id']} | {row['requested_visibility_km']} | {row['modin_visibility_field']} | {row['modout1_visibility']} | {row['tau_up_band']} | {row['status']} |"
        )

    lines.extend(["", "### Pilot72 Audit Findings", ""])
    for status in audit_statuses:
        lines.append(f"- {status}")

    if smoke_rows:
        lines.extend(
            [
                "",
                "### VisibilitySmoke18 Summary",
                "",
                f"- smoke_status: {smoke_overall}",
                f"- smoke_band_rows: {len(smoke_rows)}",
                "",
                "| band | observer_alt_km | target_alt_km | range_km | visibility_km | tau_up_band |",
                "| --- | ---: | ---: | ---: | ---: | ---: |",
            ]
        )
        for row in smoke_rows:
            lines.append(
                f"| {row['band']} | {row['observer_alt_km']} | {row['target_alt_km']} | {row['range_km']} | {row['visibility_km']} | {row['tau_up_band']} |"
            )
        lines.extend(["", "### VisibilitySmoke18 Checks", ""])
        for line in smoke_lines:
            lines.append(f"- {line}")

    upsert_marked_section(processed_dir / "qc_report.md", "VISIBILITY EFFECT AUDIT", "\n".join(lines))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--processed-dir", required=True)
    parser.add_argument("--raw-dir", required=True)
    args = parser.parse_args()

    processed_dir = Path(args.processed_dir)
    raw_dir = Path(args.raw_dir)

    audit_rows, audit_statuses, audit_overall = build_pilot_audit(processed_dir, raw_dir)
    smoke_rows = build_smoke_band_lut(processed_dir)
    smoke_lines: List[str] = []
    smoke_overall = "NOT_RUN"
    if smoke_rows:
        write_csv(
            processed_dir / "band_lut_visibility_smoke.csv",
            smoke_rows,
            [
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
                "source_case_ids",
            ],
        )
        smoke_lines, smoke_overall = evaluate_smoke(smoke_rows)

    write_visibility_report(processed_dir, audit_rows, audit_statuses, audit_overall, smoke_rows, smoke_lines, smoke_overall)

    print(f"Visibility Effect Audit: {audit_overall}")
    for status in audit_statuses:
        print(status)
    if smoke_rows:
        print(f"VisibilitySmoke18: {smoke_overall}")
        for line in smoke_lines:
            print(line)

    return 1 if audit_overall == "FAIL" or smoke_overall.startswith("FAIL") else 0


if __name__ == "__main__":
    raise SystemExit(main())
