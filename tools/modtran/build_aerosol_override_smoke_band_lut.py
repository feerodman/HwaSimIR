#!/usr/bin/env python3
"""Build band LUT and QC for the Aerosol/Visibility override smoke grid."""

from __future__ import annotations

import argparse
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

from diagnostics_common import format_float, parse_float, read_csv, rectangular_average, write_csv


BAND_COLUMNS = [
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
    "path_radiance_band",
    "sky_radiance_band",
    "path_scattering_radiance_band",
    "solar_irradiance_band",
    "unit_radiance",
    "unit_irradiance",
    "source_case_ids",
]

VISIBILITIES = [0.5, 2.0, 5.0, 23.0, 50.0]
LOW_ALT_STRONG_EPS = 1e-3
SLANT_VISIBLE_EPS = 1e-5
TREND_EPS = 0.02


def stable_float(value: object) -> str:
    parsed = parse_float(value)
    return format_float(parsed) if parsed is not None else str(value or "")


def rows_by_case(rows: Sequence[Dict[str, str]]) -> Dict[str, List[Dict[str, str]]]:
    result: Dict[str, List[Dict[str, str]]] = defaultdict(list)
    for row in rows:
        result[row.get("case_id", "")].append(row)
    return dict(result)


def average(rows: Sequence[Dict[str, str]], column: str) -> str:
    points = []
    for row in rows:
        wavelength = parse_float(row.get("wavelength_um"))
        value = parse_float(row.get(column))
        if wavelength is not None and value is not None:
            points.append((wavelength, value))
    return format_float(rectangular_average(points))


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


def build_band_rows(processed_dir: Path) -> List[Dict[str, object]]:
    manifest_rows = read_csv(processed_dir / "aerosol_override_smoke_manifest.csv")
    path_by_case = rows_by_case(read_csv(processed_dir / "aerosol_override_smoke_path_lut_spectral.csv"))
    output_rows: List[Dict[str, object]] = []

    for row in sorted(
        manifest_rows,
        key=lambda item: (
            item.get("band", ""),
            parse_float(item.get("observer_alt_km")) or 0.0,
            parse_float(item.get("target_alt_km")) or 0.0,
            parse_float(item.get("range_km")) or 0.0,
            parse_float(item.get("visibility_km")) or 0.0,
        ),
    ):
        case_id = row.get("case_id", "")
        spectral_rows = path_by_case.get(case_id, [])
        band, atmosphere, aerosol, humidity, visibility, observer_alt, target_alt, range_km, solar_zenith = condition_key(row)
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
                "tau_up_band": average(spectral_rows, "tau_up"),
                "tau_down_band": "",
                "path_radiance_band": "",
                "sky_radiance_band": "",
                "path_scattering_radiance_band": "",
                "solar_irradiance_band": "",
                "unit_radiance": "",
                "unit_irradiance": "",
                "source_case_ids": case_id,
            }
        )
    return output_rows


def classify_altitude_pair(observer_alt: float, target_alt: float) -> str:
    if observer_alt == target_alt and observer_alt in {0.1, 3.0}:
        return "low_horizontal"
    if observer_alt == target_alt and observer_alt == 10.0:
        return "high_horizontal"
    if observer_alt == 20.0 and target_alt == 3.0:
        return "slant"
    return "other"


def trend_status(values: Sequence[tuple[float, float]]) -> tuple[str, str]:
    ordered = sorted(values, key=lambda item: item[0])
    bad = []
    for (vis_a, tau_a), (vis_b, tau_b) in zip(ordered, ordered[1:]):
        if tau_b + TREND_EPS < tau_a:
            bad.append(f"{vis_a:g}->{vis_b:g} {tau_a:.6g}->{tau_b:.6g}")
    if bad:
        return "FAIL", "; ".join(bad)
    return "PASS", "tau_up increases with visibility within tolerance"


def build_qc(processed_dir: Path, band_rows: Sequence[Dict[str, object]]) -> str:
    grouped: Dict[Tuple[str, float, float, float], List[Tuple[float, float]]] = defaultdict(list)
    for row in band_rows:
        band = str(row.get("band", ""))
        observer_alt = parse_float(row.get("observer_alt_km"))
        target_alt = parse_float(row.get("target_alt_km"))
        range_km = parse_float(row.get("range_km"))
        visibility = parse_float(row.get("visibility_km"))
        tau = parse_float(row.get("tau_up_band"))
        if None in {observer_alt, target_alt, range_km, visibility, tau}:
            continue
        grouped[(band, observer_alt or 0.0, target_alt or 0.0, range_km or 0.0)].append((visibility or 0.0, tau or 0.0))

    checks: List[Tuple[str, str]] = []
    for (band, observer_alt, target_alt, range_km), values in sorted(grouped.items()):
        label = f"{band} obs={observer_alt:g} target={target_alt:g} range={range_km:g}"
        if len(values) != len(VISIBILITIES):
            checks.append(("FAIL", f"{label}: missing visibility samples count={len(values)}"))
            continue
        spread = max(tau for _, tau in values) - min(tau for _, tau in values)
        trend, detail = trend_status(values)
        if trend == "FAIL":
            checks.append(("FAIL", f"{label}: {detail}"))
            continue
        kind = classify_altitude_pair(observer_alt, target_alt)
        if kind == "low_horizontal":
            if spread <= LOW_ALT_STRONG_EPS:
                checks.append(("FAIL", f"{label}: FAIL_VISIBILITY_OVERRIDE_NOT_EFFECTIVE spread={spread:.10g}"))
            else:
                checks.append(("PASS", f"{label}: low-altitude visibility response spread={spread:.10g}"))
        elif kind == "high_horizontal":
            if spread <= SLANT_VISIBLE_EPS:
                checks.append(("WARNING", f"{label}: high_altitude_low_sensitivity spread={spread:.10g}"))
            else:
                checks.append(("PASS", f"{label}: high-altitude visibility response spread={spread:.10g}"))
        elif kind == "slant":
            if spread <= SLANT_VISIBLE_EPS:
                checks.append(("FAIL", f"{label}: FAIL_VISIBILITY_OVERRIDE_NOT_EFFECTIVE slant spread={spread:.10g}"))
            else:
                checks.append(("PASS", f"{label}: slant visibility response spread={spread:.10g}"))

    fail_count = sum(1 for status, _ in checks if status == "FAIL")
    warning_count = sum(1 for status, _ in checks if status == "WARNING")
    overall = "FAIL_VISIBILITY_OVERRIDE_NOT_EFFECTIVE" if fail_count else ("PASS_WITH_WARNINGS" if warning_count else "PASS")

    lines = [
        "# Aerosol Override Smoke QC",
        "",
        f"- overall_status: {overall}",
        f"- band_lut_rows: {len(band_rows)}",
        f"- low_altitude_strong_eps: {LOW_ALT_STRONG_EPS:g}",
        f"- slant_visible_eps: {SLANT_VISIBLE_EPS:g}",
        "- production_lut_overwritten: no",
        "",
        "## Visibility Sweep",
        "",
        "| status | check |",
        "| --- | --- |",
    ]
    for status, detail in checks:
        lines.append(f"| {status} | {detail} |")

    lines.extend(
        [
            "",
            "## Tau Matrix",
            "",
            "| band | observer_alt_km | target_alt_km | range_km | visibility_km | tau_up_band |",
            "| --- | ---: | ---: | ---: | ---: | ---: |",
        ]
    )
    for row in band_rows:
        lines.append(
            f"| {row.get('band','')} | {row.get('observer_alt_km','')} | {row.get('target_alt_km','')} | {row.get('range_km','')} | {row.get('visibility_km','')} | {row.get('tau_up_band','')} |"
        )

    qc_path = processed_dir / "qc_aerosol_override_smoke.md"
    qc_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return overall


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--processed-dir", required=True)
    args = parser.parse_args()
    processed_dir = Path(args.processed_dir)
    rows = build_band_rows(processed_dir)
    output_path = processed_dir / "band_lut_aerosol_override_smoke.csv"
    write_csv(output_path, rows, BAND_COLUMNS)
    overall = build_qc(processed_dir, rows)
    print(f"Wrote {len(rows)} aerosol override smoke band rows to {output_path}")
    print(f"overall_status={overall}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

