#!/usr/bin/env python3
"""Build rectangular-response band_lut.csv and pilot QC from parsed MODTRAN spectra."""

from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple


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


MODE_TO_TABLE = {
    "Transmittance": "path",
    "ThermalRadiance": "path",
    "DirectSolarIrradiance": "solar",
    "RadianceWithScattering": "sky",
}


PILOT_RANGES = [1.0, 5.0, 20.0, 50.0]
PILOT_VISIBILITIES = [5.0, 23.0, 50.0]
TREND_EPS = 0.02
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
VISIBILITY_SENSITIVITY_EPS = 1e-5


def read_csv(path: Path) -> List[Dict[str, str]]:
    if not path.exists() or path.stat().st_size == 0:
        return []
    with path.open(newline="", encoding="utf-8-sig") as handle:
        return list(csv.DictReader(handle))


def write_csv(path: Path, rows: Sequence[Dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=BAND_COLUMNS, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def parse_float(value: object) -> float | None:
    if value is None or value == "":
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def format_float(value: float | None) -> str:
    if value is None:
        return ""
    return f"{value:.10g}"


def stable_float_text(value: object) -> str:
    parsed = parse_float(value)
    return format_float(parsed) if parsed is not None else str(value or "")


def rectangular_average(rows: Sequence[Dict[str, str]], column: str) -> str:
    points: List[Tuple[float, float]] = []
    for row in rows:
        wavelength = parse_float(row.get("wavelength_um", ""))
        value = parse_float(row.get(column, ""))
        if wavelength is not None and value is not None:
            points.append((wavelength, value))

    if not points:
        return ""
    points.sort(key=lambda item: item[0])
    if len(points) == 1:
        return format_float(points[0][1])

    width = points[-1][0] - points[0][0]
    if width <= 0:
        return format_float(sum(value for _, value in points) / len(points))

    area = 0.0
    for (wavelength_a, value_a), (wavelength_b, value_b) in zip(points, points[1:]):
        area += 0.5 * (value_a + value_b) * (wavelength_b - wavelength_a)
    return format_float(area / width)


def rows_by_case(rows: Sequence[Dict[str, str]]) -> Dict[str, List[Dict[str, str]]]:
    result: Dict[str, List[Dict[str, str]]] = defaultdict(list)
    for row in rows:
        result[row.get("case_id", "")].append(row)
    return dict(result)


def condition_key(row: Dict[str, str]) -> Tuple[str, str, str, str, str, str, str, str, str]:
    return (
        row.get("band", ""),
        row.get("atmosphere_model", ""),
        row.get("aerosol_model", ""),
        row.get("humidity_profile", ""),
        stable_float_text(row.get("visibility_km", "")),
        stable_float_text(row.get("observer_alt_km", "")),
        stable_float_text(row.get("target_alt_km", "")),
        stable_float_text(row.get("range_km", "")),
        stable_float_text(row.get("solar_zenith_deg", "")),
    )


def first_present(rows: Sequence[Dict[str, str]], column: str) -> str:
    for row in rows:
        value = row.get(column, "")
        if value != "":
            return value
    return ""


def min_max(values: Iterable[float]) -> str:
    clean = list(values)
    if not clean:
        return "n/a"
    return f"{min(clean):.10g} .. {max(clean):.10g}"


def numeric_values(rows: Sequence[Dict[str, str]], column: str) -> List[float]:
    values: List[float] = []
    for row in rows:
        parsed = parse_float(row.get(column, ""))
        if parsed is not None:
            values.append(parsed)
    return values


def build_band_rows(processed_dir: Path) -> tuple[List[Dict[str, object]], Dict[str, object]]:
    path_rows = read_csv(processed_dir / "path_lut_spectral.csv")
    solar_rows = read_csv(processed_dir / "solar_lut_spectral.csv")
    sky_rows = read_csv(processed_dir / "sky_lut_spectral.csv")
    all_manifest_rows = read_csv(processed_dir / "manifest.csv")

    path_by_case = rows_by_case(path_rows)
    solar_by_case = rows_by_case(solar_rows)
    sky_by_case = rows_by_case(sky_rows)
    parsed_case_ids = set(path_by_case) | set(solar_by_case) | set(sky_by_case)
    manifest_rows = [row for row in all_manifest_rows if row.get("case_id") in parsed_case_ids]

    groups: Dict[Tuple[str, str, str, str, str, str, str, str, str], Dict[str, object]] = {}
    for row in manifest_rows:
        key = condition_key(row)
        group = groups.setdefault(key, {"meta": [], "modes": defaultdict(list)})
        group["meta"].append(row)
        group["modes"][row.get("mode", "")].append(row)

    output_rows: List[Dict[str, object]] = []
    for key, group in sorted(
        groups.items(),
        key=lambda item: (
            item[0][0],
            parse_float(item[0][7]) or 0.0,
            parse_float(item[0][4]) or 0.0,
            item[0][8],
        ),
    ):
        band, atmosphere, aerosol, humidity, visibility, observer_alt, target_alt, range_km, solar_zenith = key
        modes: Dict[str, List[Dict[str, str]]] = group["modes"]  # type: ignore[assignment]
        meta_rows: List[Dict[str, str]] = group["meta"]  # type: ignore[assignment]

        trans_case_ids = [row["case_id"] for row in modes.get("Transmittance", [])]
        thermal_case_ids = [row["case_id"] for row in modes.get("ThermalRadiance", [])]
        solar_case_ids = [row["case_id"] for row in modes.get("DirectSolarIrradiance", [])]
        scattering_case_ids = [row["case_id"] for row in modes.get("RadianceWithScattering", [])]

        trans_rows = [row for case_id in trans_case_ids for row in path_by_case.get(case_id, [])]
        thermal_rows = [row for case_id in thermal_case_ids for row in path_by_case.get(case_id, [])]
        direct_solar_rows = [row for case_id in solar_case_ids for row in solar_by_case.get(case_id, [])]
        scattering_rows = [row for case_id in scattering_case_ids for row in sky_by_case.get(case_id, [])]

        source_case_ids = trans_case_ids + thermal_case_ids + solar_case_ids + scattering_case_ids
        radiance_unit = "MODOUT2_native" if thermal_rows or scattering_rows else ""
        irradiance_unit = "MODOUT2_native" if direct_solar_rows else ""
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
                "tau_up_band": rectangular_average(trans_rows, "tau_up"),
                "tau_down_band": rectangular_average(direct_solar_rows, "tau_down"),
                "path_radiance_band": rectangular_average(thermal_rows, "path_radiance"),
                "sky_radiance_band": rectangular_average(scattering_rows, "sky_radiance"),
                "path_scattering_radiance_band": rectangular_average(scattering_rows, "path_scattering_radiance"),
                "solar_irradiance_band": rectangular_average(direct_solar_rows, "solar_irradiance"),
                "unit_radiance": radiance_unit,
                "unit_irradiance": irradiance_unit,
                "source_case_ids": ";".join(source_case_ids),
            }
        )

    context = {
        "path_rows": path_rows,
        "solar_rows": solar_rows,
        "sky_rows": sky_rows,
        "manifest_rows": manifest_rows,
        "all_manifest_rows": all_manifest_rows,
        "parsed_case_ids": parsed_case_ids,
    }
    return output_rows, context


def is_pilot72(manifest_rows: Sequence[Dict[str, str]], band_rows: Sequence[Dict[str, object]]) -> bool:
    statuses = {row.get("status", "") for row in manifest_rows}
    stages = {row.get("stage", "") for row in manifest_rows}
    return (
        len(manifest_rows) == 72
        and len(band_rows) == 24
        and (any("pilot72" in value for value in statuses | stages) or {row.get("grid", "") for row in manifest_rows} == {"pilot72"})
    )


def is_production_nir_mwir(manifest_rows: Sequence[Dict[str, str]]) -> bool:
    grids = {row.get("grid", "") for row in manifest_rows}
    statuses = {row.get("status", "") for row in manifest_rows}
    stages = {row.get("stage", "") for row in manifest_rows}
    return (
        "production_nir_mwir" in grids
        or any("production_nir_mwir" in value for value in statuses | stages)
    )


def values_for_band_rows(
    band_rows: Sequence[Dict[str, object]],
    band: str,
    visibility: float | None = None,
    range_km: float | None = None,
    column: str = "tau_up_band",
) -> List[tuple[float, float, float]]:
    values: List[tuple[float, float, float]] = []
    for row in band_rows:
        if row.get("band") != band:
            continue
        row_range = parse_float(row.get("range_km"))
        row_visibility = parse_float(row.get("visibility_km"))
        value = parse_float(row.get(column))
        if row_range is None or row_visibility is None or value is None:
            continue
        if visibility is not None and row_visibility != visibility:
            continue
        if range_km is not None and row_range != range_km:
            continue
        values.append((row_range, row_visibility, value))
    return values


def check_range_tau_trend(band_rows: Sequence[Dict[str, object]], band: str, visibility: float) -> tuple[str, str]:
    values = sorted(values_for_band_rows(band_rows, band, visibility=visibility), key=lambda item: item[0])
    if len(values) != len(PILOT_RANGES):
        return "FAIL", f"{band} visibility={visibility:g}: missing tau_up values"
    bad = []
    for (range_a, _, value_a), (range_b, _, value_b) in zip(values, values[1:]):
        if value_b > value_a + TREND_EPS:
            bad.append(f"{range_a:g}->{range_b:g} km {value_a:.5g}->{value_b:.5g}")
    if bad:
        return "FAIL", f"{band} visibility={visibility:g}: tau_up increased: {'; '.join(bad)}"
    return "PASS", f"{band} visibility={visibility:g}: tau_up decreases with range within eps={TREND_EPS:g}"


def check_visibility_tau_trend(band_rows: Sequence[Dict[str, object]], band: str, range_km: float) -> tuple[str, str]:
    values = sorted(values_for_band_rows(band_rows, band, range_km=range_km), key=lambda item: item[1])
    if len(values) != len(PILOT_VISIBILITIES):
        return "FAIL", f"{band} range={range_km:g}: missing tau_up values"
    bad = []
    for (_, visibility_a, value_a), (_, visibility_b, value_b) in zip(values, values[1:]):
        if value_b + TREND_EPS < value_a:
            bad.append(f"{visibility_a:g}->{visibility_b:g} km {value_a:.5g}->{value_b:.5g}")
    if bad:
        return "FAIL", f"{band} range={range_km:g}: tau_up decreased with better visibility: {'; '.join(bad)}"
    return "PASS", f"{band} range={range_km:g}: tau_up increases with visibility within eps={TREND_EPS:g}"


def check_mwir_path_trend(band_rows: Sequence[Dict[str, object]], visibility: float) -> tuple[str, str]:
    values = sorted(
        values_for_band_rows(band_rows, "MWIR", visibility=visibility, column="path_radiance_band"),
        key=lambda item: item[0],
    )
    if len(values) != len(PILOT_RANGES):
        return "FAIL", f"MWIR visibility={visibility:g}: missing path_radiance values"
    bad = []
    for (range_a, _, value_a), (range_b, _, value_b) in zip(values, values[1:]):
        tolerance = max(abs(value_a) * 0.15, 1e-12)
        if value_b + tolerance < value_a:
            bad.append(f"{range_a:g}->{range_b:g} km {value_a:.5g}->{value_b:.5g}")
    if bad:
        return "FAIL", f"MWIR visibility={visibility:g}: path_radiance dropped unexpectedly: {'; '.join(bad)}"
    return "PASS", f"MWIR visibility={visibility:g}: path_radiance increases or saturates"


def band_rows_for_condition(
    band_rows: Sequence[Dict[str, object]],
    band: str,
    observer_alt: float,
    target_alt: float,
    visibility: float | None = None,
    range_km: float | None = None,
    column: str = "tau_up_band",
) -> List[tuple[float, float, float]]:
    values: List[tuple[float, float, float]] = []
    for row in band_rows:
        if row.get("band") != band:
            continue
        row_obs = parse_float(row.get("observer_alt_km"))
        row_target = parse_float(row.get("target_alt_km"))
        row_range = parse_float(row.get("range_km"))
        row_visibility = parse_float(row.get("visibility_km"))
        value = parse_float(row.get(column))
        if None in {row_obs, row_target, row_range, row_visibility, value}:
            continue
        if row_obs != observer_alt or row_target != target_alt:
            continue
        if visibility is not None and row_visibility != visibility:
            continue
        if range_km is not None and row_range != range_km:
            continue
        values.append((row_range, row_visibility, value))
    return values


def is_high_altitude_horizontal(observer_alt: float, target_alt: float) -> bool:
    return observer_alt == target_alt and observer_alt >= 10.0


def valid_production_ranges(observer_alt: float, target_alt: float) -> List[float]:
    min_slant = abs(observer_alt - target_alt)
    return [range_km for range_km in PRODUCTION_RANGES if range_km + 1e-9 >= min_slant]


def production_range_tau_checks(band_rows: Sequence[Dict[str, object]]) -> List[tuple[str, str]]:
    checks: List[tuple[str, str]] = []
    for band in ["MWIR", "NIR"]:
        for observer_alt, target_alt in PRODUCTION_ALTITUDE_PAIRS:
            expected_ranges = valid_production_ranges(observer_alt, target_alt)
            for visibility in PRODUCTION_VISIBILITIES:
                values = sorted(
                    band_rows_for_condition(band_rows, band, observer_alt, target_alt, visibility=visibility),
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
                    checks.append(("FAIL", f"{prefix}: tau_up increased with range: {'; '.join(bad)}"))
                else:
                    checks.append(("PASS", f"{prefix}: tau_up decreases with range within eps={TREND_EPS:g}"))
    return checks


def production_visibility_tau_checks(band_rows: Sequence[Dict[str, object]]) -> List[tuple[str, str]]:
    checks: List[tuple[str, str]] = []
    for band in ["MWIR", "NIR"]:
        for observer_alt, target_alt in PRODUCTION_ALTITUDE_PAIRS:
            for range_km in valid_production_ranges(observer_alt, target_alt):
                values = sorted(
                    band_rows_for_condition(band_rows, band, observer_alt, target_alt, range_km=range_km),
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
                    checks.append(("FAIL", f"{prefix}: tau_up decreased with better visibility: {'; '.join(bad)}"))
                    continue
                spread = max(value for _, _, value in values) - min(value for _, _, value in values)
                if spread <= VISIBILITY_SENSITIVITY_EPS:
                    if is_high_altitude_horizontal(observer_alt, target_alt):
                        checks.append(("WARNING", f"{prefix}: high_altitude_low_sensitivity spread={spread:.5g}"))
                    else:
                        checks.append(("FAIL", f"{prefix}: visibility sensitivity missing spread={spread:.5g}"))
                else:
                    checks.append(("PASS", f"{prefix}: visibility sensitivity detected spread={spread:.5g}"))
    return checks


def production_mwir_path_checks(band_rows: Sequence[Dict[str, object]]) -> List[tuple[str, str]]:
    checks: List[tuple[str, str]] = []
    for observer_alt, target_alt in PRODUCTION_ALTITUDE_PAIRS:
        expected_ranges = valid_production_ranges(observer_alt, target_alt)
        for visibility in PRODUCTION_VISIBILITIES:
            values = sorted(
                band_rows_for_condition(
                    band_rows,
                    "MWIR",
                    observer_alt,
                    target_alt,
                    visibility=visibility,
                    column="path_radiance_band",
                ),
                key=lambda item: item[0],
            )
            prefix = f"MWIR path_radiance obs={observer_alt:g} target={target_alt:g} visibility={visibility:g}"
            if len(values) != len(expected_ranges):
                checks.append(("FAIL", f"{prefix}: missing path radiance samples"))
                continue
            bad = []
            for (range_a, _, value_a), (range_b, _, value_b) in zip(values, values[1:]):
                tolerance = max(abs(value_a) * 0.15, 1e-12)
                if value_b + tolerance < value_a:
                    bad.append(f"{range_a:g}->{range_b:g} km {value_a:.5g}->{value_b:.5g}")
            if bad:
                checks.append(("FAIL", f"{prefix}: path_radiance dropped unexpectedly: {'; '.join(bad)}"))
            else:
                checks.append(("PASS", f"{prefix}: path_radiance increases or saturates"))
    return checks


def case_counts_by_band_mode(
    manifest_rows: Sequence[Dict[str, str]],
    spectral_rows: Sequence[Dict[str, str]],
) -> List[str]:
    rows_by_id = rows_by_case(spectral_rows)
    counts: Dict[Tuple[str, str], Dict[str, object]] = {}
    for row in manifest_rows:
        key = (row.get("band", ""), row.get("mode", ""))
        bucket = counts.setdefault(key, {"cases": 0, "rows": 0, "wavelengths": []})
        bucket["cases"] = int(bucket["cases"]) + 1
        case_rows = rows_by_id.get(row.get("case_id", ""), [])
        bucket["rows"] = int(bucket["rows"]) + len(case_rows)
        bucket["wavelengths"].extend(numeric_values(case_rows, "wavelength_um"))  # type: ignore[union-attr]

    lines = []
    for (band, mode), values in sorted(counts.items()):
        lines.append(
            f"| {band} | {mode} | {values['cases']} | {values['rows']} | {min_max(values['wavelengths'])} |"
        )
    return lines


def upsert_marked_section(path: Path, marker: str, section: str) -> None:
    begin = f"<!-- BEGIN {marker} -->"
    end = f"<!-- END {marker} -->"
    text = path.read_text(encoding="utf-8", errors="replace") if path.exists() else "# MODTRAN LUT QC Report\n"
    block = f"\n{begin}\n{section.rstrip()}\n{end}\n"
    if begin in text and end in text:
        before = text.split(begin, 1)[0]
        after = text.split(end, 1)[1]
        path.write_text(before.rstrip() + block + after.lstrip(), encoding="utf-8")
    else:
        path.write_text(text.rstrip() + "\n" + block, encoding="utf-8")


def write_pilot72_summary(
    processed_dir: Path,
    band_rows: Sequence[Dict[str, object]],
    context: Dict[str, object],
) -> None:
    manifest_rows: List[Dict[str, str]] = context["manifest_rows"]  # type: ignore[assignment]
    path_rows: List[Dict[str, str]] = context["path_rows"]  # type: ignore[assignment]
    solar_rows: List[Dict[str, str]] = context["solar_rows"]  # type: ignore[assignment]
    sky_rows: List[Dict[str, str]] = context["sky_rows"]  # type: ignore[assignment]
    spectral_rows = path_rows + solar_rows + sky_rows

    failures = read_csv(processed_dir / "failed_cases.csv")
    checks: List[tuple[str, str]] = []
    for band in ["MWIR", "NIR"]:
        for visibility in PILOT_VISIBILITIES:
            checks.append(check_range_tau_trend(band_rows, band, visibility))
        for range_km in PILOT_RANGES:
            checks.append(check_visibility_tau_trend(band_rows, band, range_km))
    for visibility in PILOT_VISIBILITIES:
        checks.append(check_mwir_path_trend(band_rows, visibility))

    nir_rows = [row for row in band_rows if row.get("band") == "NIR"]
    nir_solar_nonzero = all((parse_float(row.get("solar_irradiance_band")) or 0.0) > 0 for row in nir_rows)
    nir_sky_nonempty = all(
        row.get("sky_radiance_band", "") != "" or row.get("path_scattering_radiance_band", "") != ""
        for row in nir_rows
    )
    checks.append(("PASS" if nir_solar_nonzero else "FAIL", "NIR solar_irradiance_band is nonzero for all pilot rows"))
    checks.append(("PASS" if nir_sky_nonempty else "FAIL", "NIR sky/scattering band radiance is present for all pilot rows"))

    overall = "PASS" if all(status == "PASS" for status, _ in checks) and not failures else "FAIL"

    lines = [
        "## Pilot72 Summary",
        "",
        f"- overall_status: {overall}",
        f"- manifest_cases: {len(manifest_rows)}",
        f"- band_lut_rows: {len(band_rows)}",
        f"- units: MODOUT2_native for radiance/irradiance fields until PcModWin5 export units are confirmed",
        f"- failed_cases: {len(failures)}",
        "",
        "### Band/Mode Coverage",
        "",
        "| band | mode | case_count | spectral_row_count | wavelength_range_um |",
        "| --- | --- | ---: | ---: | --- |",
        *case_counts_by_band_mode(manifest_rows, spectral_rows),
        "",
        "### Tau Up By Band/Range/Visibility",
        "",
        "| band | range_km | visibility_km | tau_up_band |",
        "| --- | ---: | ---: | ---: |",
    ]

    for row in sorted(
        band_rows,
        key=lambda item: (str(item.get("band")), parse_float(item.get("range_km")) or 0.0, parse_float(item.get("visibility_km")) or 0.0),
    ):
        lines.append(
            f"| {row.get('band','')} | {row.get('range_km','')} | {row.get('visibility_km','')} | {row.get('tau_up_band','')} |"
        )

    lines.extend(
        [
            "",
            "### MWIR Path Radiance Trend",
            "",
            "| range_km | visibility_km | path_radiance_band |",
            "| ---: | ---: | ---: |",
        ]
    )
    for row in sorted(
        [row for row in band_rows if row.get("band") == "MWIR"],
        key=lambda item: (parse_float(item.get("visibility_km")) or 0.0, parse_float(item.get("range_km")) or 0.0),
    ):
        lines.append(f"| {row.get('range_km','')} | {row.get('visibility_km','')} | {row.get('path_radiance_band','')} |")

    lines.extend(
        [
            "",
            "### Trend Checks",
            "",
            "| status | check |",
            "| --- | --- |",
        ]
    )
    for status, detail in checks:
        lines.append(f"| {status} | {detail} |")

    lines.extend(["", "### Failed Cases", ""])
    if failures:
        for failure in failures:
            lines.append(f"- {failure.get('case_id','')}: {failure.get('reason','')}")
    else:
        lines.append("- none")

    if overall != "PASS":
        lines.extend(["", "Trend QC failed; do not proceed to production sparse grid."])
    else:
        lines.extend(["", "Pilot72 trend QC passed for this limited grid; production remains intentionally blocked until explicitly requested."])

    upsert_marked_section(processed_dir / "qc_report.md", "PILOT72 SUMMARY", "\n".join(lines))


def write_production_nir_mwir_summary(
    processed_dir: Path,
    band_rows: Sequence[Dict[str, object]],
    context: Dict[str, object],
) -> None:
    manifest_rows: List[Dict[str, str]] = context["manifest_rows"]  # type: ignore[assignment]
    all_manifest_rows: List[Dict[str, str]] = context["all_manifest_rows"]  # type: ignore[assignment]
    path_rows: List[Dict[str, str]] = context["path_rows"]  # type: ignore[assignment]
    solar_rows: List[Dict[str, str]] = context["solar_rows"]  # type: ignore[assignment]
    sky_rows: List[Dict[str, str]] = context["sky_rows"]  # type: ignore[assignment]
    spectral_rows = path_rows + solar_rows + sky_rows

    production_manifest = [
        row for row in all_manifest_rows
        if row.get("grid") == "production_nir_mwir"
        or "production_nir_mwir" in row.get("status", "")
        or "production_nir_mwir" in row.get("stage", "")
    ]
    succeeded = [
        row for row in production_manifest
        if row.get("status") == "production_nir_mwir_succeeded"
        or row.get("stage") == "production_nir_mwir_succeeded"
    ]
    failed = [
        row for row in production_manifest
        if "failed" in row.get("status", "")
        or "failed" in row.get("stage", "")
    ]
    failures_csv = read_csv(processed_dir / "failed_cases.csv")
    expected_case_count = len(production_manifest)
    expected_band_row_count = len({condition_key(row) for row in production_manifest})

    checks: List[tuple[str, str]] = []
    checks.append(
        (
            "PASS" if expected_case_count > 0 else "FAIL",
            f"production manifest runnable case count actual={expected_case_count}",
        )
    )
    checks.append(
        (
            "PASS" if len(succeeded) == expected_case_count else "FAIL",
            f"production succeeded case count expected={expected_case_count} actual={len(succeeded)}",
        )
    )
    checks.append(("PASS" if len(band_rows) == expected_band_row_count else "FAIL", f"production band_lut rows expected={expected_band_row_count} actual={len(band_rows)}"))
    checks.append(("PASS" if not failed and not failures_csv else "FAIL", f"failed cases = {len(failed) + len(failures_csv)}"))

    tau_bad = [
        row for row in band_rows
        if (
            (parse_float(row.get("tau_up_band")) is not None and not (0.0 <= (parse_float(row.get("tau_up_band")) or 0.0) <= 1.0))
            or (parse_float(row.get("tau_down_band")) is not None and not (0.0 <= (parse_float(row.get("tau_down_band")) or 0.0) <= 1.0))
        )
    ]
    checks.append(("PASS" if not tau_bad else "FAIL", f"tau_up_band/tau_down_band in 0..1 bad_rows={len(tau_bad)}"))

    checks.extend(production_range_tau_checks(band_rows))
    checks.extend(production_visibility_tau_checks(band_rows))
    checks.extend(production_mwir_path_checks(band_rows))

    nir_rows = [row for row in band_rows if row.get("band") == "NIR"]
    nir_solar_nonzero = all((parse_float(row.get("solar_irradiance_band")) or 0.0) > 0 for row in nir_rows)
    nir_sky_nonempty = all(
        row.get("sky_radiance_band", "") != "" or row.get("path_scattering_radiance_band", "") != ""
        for row in nir_rows
    )
    checks.append(("PASS" if nir_solar_nonzero else "FAIL", "NIR solar_irradiance_band is nonzero for all production rows"))
    checks.append(("PASS" if nir_sky_nonempty else "FAIL", "NIR sky/path_scattering radiance is present for all production rows"))

    fail_count = sum(1 for status, _ in checks if status == "FAIL")
    warning_count = sum(1 for status, _ in checks if status == "WARNING")
    overall = "FAIL" if fail_count else ("PASS_WITH_WARNINGS" if warning_count else "PASS")

    lines = [
        "## ProductionNirMwir Summary",
        "",
        f"- overall_status: {overall}",
        f"- manifest_cases: {len(production_manifest)}",
        f"- succeeded_cases: {len(succeeded)}",
        f"- band_lut_rows: {len(band_rows)}",
        f"- units: MODOUT2_native for radiance/irradiance fields until PcModWin5 export units are confirmed",
        f"- failed_cases: {len(failed) + len(failures_csv)}",
        f"- warning_checks: {warning_count}",
        "",
        "### Band/Mode Coverage",
        "",
        "| band | mode | case_count | spectral_row_count | wavelength_range_um |",
        "| --- | --- | ---: | ---: | --- |",
        *case_counts_by_band_mode(manifest_rows, spectral_rows),
        "",
        "### Tau Up By Band/Altitude/Range/Visibility",
        "",
        "| band | observer_alt_km | target_alt_km | range_km | visibility_km | tau_up_band |",
        "| --- | ---: | ---: | ---: | ---: | ---: |",
    ]

    for row in sorted(
        band_rows,
        key=lambda item: (
            str(item.get("band")),
            parse_float(item.get("observer_alt_km")) or 0.0,
            parse_float(item.get("target_alt_km")) or 0.0,
            parse_float(item.get("range_km")) or 0.0,
            parse_float(item.get("visibility_km")) or 0.0,
        ),
    ):
        lines.append(
            f"| {row.get('band','')} | {row.get('observer_alt_km','')} | {row.get('target_alt_km','')} | {row.get('range_km','')} | {row.get('visibility_km','')} | {row.get('tau_up_band','')} |"
        )

    lines.extend(
        [
            "",
            "### MWIR Path Radiance By Range",
            "",
            "| observer_alt_km | target_alt_km | range_km | visibility_km | path_radiance_band |",
            "| ---: | ---: | ---: | ---: | ---: |",
        ]
    )
    for row in sorted(
        [row for row in band_rows if row.get("band") == "MWIR"],
        key=lambda item: (
            parse_float(item.get("observer_alt_km")) or 0.0,
            parse_float(item.get("target_alt_km")) or 0.0,
            parse_float(item.get("visibility_km")) or 0.0,
            parse_float(item.get("range_km")) or 0.0,
        ),
    ):
        lines.append(
            f"| {row.get('observer_alt_km','')} | {row.get('target_alt_km','')} | {row.get('range_km','')} | {row.get('visibility_km','')} | {row.get('path_radiance_band','')} |"
        )

    lines.extend(["", "### Production Checks", "", "| status | check |", "| --- | --- |"])
    for status, detail in checks:
        lines.append(f"| {status} | {detail} |")

    lines.extend(["", "### Failed Cases", ""])
    if failed or failures_csv:
        for failure in failed:
            lines.append(f"- {failure.get('case_id','')}: {failure.get('status','') or failure.get('stage','')}")
        for failure in failures_csv:
            lines.append(f"- {failure.get('case_id','')}: {failure.get('reason','')}")
    else:
        lines.append("- none")

    if overall == "FAIL":
        lines.extend(["", "Production QC failed. Do not connect this LUT to runtime code."])
    else:
        lines.extend(["", "Production NIR/MWIR LUT passed automated range and trend checks for this sparse grid. Runtime C++ integration remains intentionally out of scope."])

    upsert_marked_section(processed_dir / "qc_report.md", "PRODUCTION NIR MWIR SUMMARY", "\n".join(lines))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--processed-dir", required=True)
    args = parser.parse_args()

    processed_dir = Path(args.processed_dir)
    rows, context = build_band_rows(processed_dir)
    if not rows:
        print("ERROR: no parsed spectral rows matched manifest cases; band_lut.csv was not written.")
        return 1
    write_csv(processed_dir / "band_lut.csv", rows)

    manifest_rows: List[Dict[str, str]] = context["manifest_rows"]  # type: ignore[assignment]
    if is_pilot72(manifest_rows, rows):
        write_pilot72_summary(processed_dir, rows, context)
    all_manifest_rows: List[Dict[str, str]] = context["all_manifest_rows"]  # type: ignore[assignment]
    if is_production_nir_mwir(all_manifest_rows):
        write_production_nir_mwir_summary(processed_dir, rows, context)

    print(f"Wrote {len(rows)} rectangular-response band rows to {processed_dir / 'band_lut.csv'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
