#!/usr/bin/env python3
"""Build a separate band LUT for targeted MODTRAN diagnosis reruns."""

from __future__ import annotations

import argparse
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

from diagnostics_common import format_float, parse_float, read_csv, rectangular_average, upsert_marked_section, write_csv


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


def build(processed_dir: Path) -> List[Dict[str, object]]:
    manifest_rows = read_csv(processed_dir / "targeted_diagnosis_manifest.csv")
    path_by_case = rows_by_case(read_csv(processed_dir / "targeted_diagnosis_path_lut_spectral.csv"))
    solar_by_case = rows_by_case(read_csv(processed_dir / "targeted_diagnosis_solar_lut_spectral.csv"))
    sky_by_case = rows_by_case(read_csv(processed_dir / "targeted_diagnosis_sky_lut_spectral.csv"))

    groups: Dict[Tuple[str, str, str, str, str, str, str, str, str], Dict[str, List[str]]] = {}
    for row in manifest_rows:
        key = condition_key(row)
        mode_map = groups.setdefault(key, defaultdict(list))
        mode_map[row.get("mode", "")].append(row.get("case_id", ""))

    output_rows: List[Dict[str, object]] = []
    for key, mode_map in sorted(groups.items()):
        band, atmosphere, aerosol, humidity, visibility, observer_alt, target_alt, range_km, solar_zenith = key
        trans_ids = mode_map.get("Transmittance", [])
        thermal_ids = mode_map.get("ThermalRadiance", [])
        solar_ids = mode_map.get("DirectSolarIrradiance", [])
        sky_ids = mode_map.get("RadianceWithScattering", [])
        trans_rows = [row for case_id in trans_ids for row in path_by_case.get(case_id, [])]
        thermal_rows = [row for case_id in thermal_ids for row in path_by_case.get(case_id, [])]
        solar_rows = [row for case_id in solar_ids for row in solar_by_case.get(case_id, [])]
        sky_rows = [row for case_id in sky_ids for row in sky_by_case.get(case_id, [])]
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
                "tau_up_band": average(trans_rows, "tau_up"),
                "tau_down_band": average(solar_rows, "tau_down"),
                "path_radiance_band": average(thermal_rows, "path_radiance"),
                "sky_radiance_band": average(sky_rows, "sky_radiance"),
                "path_scattering_radiance_band": average(sky_rows, "path_scattering_radiance"),
                "solar_irradiance_band": average(solar_rows, "solar_irradiance"),
                "unit_radiance": "MODOUT2_native" if thermal_rows or sky_rows else "",
                "unit_irradiance": "MODOUT2_native" if solar_rows else "",
                "source_case_ids": ";".join(trans_ids + thermal_ids + solar_ids + sky_ids),
            }
        )
    return output_rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--processed-dir", required=True)
    args = parser.parse_args()
    processed_dir = Path(args.processed_dir)
    rows = build(processed_dir)
    output = processed_dir / "targeted_diagnosis_band_lut.csv"
    write_csv(output, rows, BAND_COLUMNS)
    upsert_marked_section(
        processed_dir / "qc_report.md",
        "TARGETED DIAGNOSIS BAND LUT",
        "\n".join(
            [
                "## Targeted Diagnosis Band LUT",
                "",
                f"- rows: {len(rows)}",
                f"- output_csv: {output}",
                "- note: separate targeted rerun output; production band_lut.csv was not overwritten.",
            ]
        ),
    )
    print(f"Wrote {len(rows)} targeted diagnosis band rows to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

