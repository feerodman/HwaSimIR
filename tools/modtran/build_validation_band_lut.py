#!/usr/bin/env python3
"""Build a minimal rectangular-response band_lut.csv for the six validation cases."""

from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path
from typing import Dict, Iterable, List, Sequence


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
    "solar_irradiance_band",
]


VALIDATION_CASES = [
    "MWIR_transmittance_obs10_tar10_rng20_vis23_aerRural_humdefault",
    "MWIR_thermal_obs10_tar10_rng20_vis23_aerRural_humdefault",
    "MWIR_solar_obs10_tar10_rng20_vis23_aerRural_humdefault_sza45",
    "NIR_transmittance_obs10_tar10_rng20_vis23_aerRural_humdefault",
    "NIR_solar_obs10_tar10_rng20_vis23_aerRural_humdefault_sza45",
    "NIR_scattering_obs10_tar10_rng20_vis23_aerRural_humdefault_sza45",
]


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
        for row in rows:
            writer.writerow(row)


def parse_float(value: str) -> float | None:
    if value is None or value == "":
        return None
    try:
        return float(value)
    except ValueError:
        return None


def average(values: Iterable[float | None]) -> str:
    clean = [value for value in values if value is not None]
    if not clean:
        return ""
    return format_float(sum(clean) / len(clean))


def format_float(value: float) -> str:
    return f"{value:.10g}"


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
        return format_float(points[0][1])

    width = points[-1][0] - points[0][0]
    if width <= 0:
        return average(value for _, value in points)

    area = 0.0
    for (wavelength_a, value_a), (wavelength_b, value_b) in zip(points, points[1:]):
        area += 0.5 * (value_a + value_b) * (wavelength_b - wavelength_a)
    return format_float(area / width)


def first_nonempty(rows: Sequence[Dict[str, str]], column: str, default: str = "") -> str:
    for row in rows:
        value = row.get(column, "")
        if value != "":
            return value
    return default


def rows_by_case(rows: Sequence[Dict[str, str]]) -> Dict[str, List[Dict[str, str]]]:
    result: Dict[str, List[Dict[str, str]]] = {}
    for row in rows:
        result.setdefault(row.get("case_id", ""), []).append(row)
    return result


def validate_expected_outputs(
    manifest_rows: Sequence[Dict[str, str]],
    path_rows: Sequence[Dict[str, str]],
    solar_rows: Sequence[Dict[str, str]],
    sky_rows: Sequence[Dict[str, str]],
) -> List[str]:
    path_by_case = rows_by_case(path_rows)
    solar_by_case = rows_by_case(solar_rows)
    sky_by_case = rows_by_case(sky_rows)
    errors: List[str] = []

    for row in manifest_rows:
        case_id = row.get("case_id", "")
        mode = row.get("mode", "")
        if mode in {"Transmittance", "ThermalRadiance"} and not path_by_case.get(case_id):
            errors.append(f"{case_id}: missing rows in path_lut_spectral.csv")
        elif mode == "DirectSolarIrradiance" and not solar_by_case.get(case_id):
            errors.append(f"{case_id}: missing rows in solar_lut_spectral.csv")
        elif mode == "RadianceWithScattering" and not sky_by_case.get(case_id):
            errors.append(f"{case_id}: missing rows in sky_lut_spectral.csv")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--processed-dir", required=True)
    parser.add_argument("--case-id", action="append", default=[], help="Validation case id to include. Defaults to the built-in six.")
    args = parser.parse_args()

    processed_dir = Path(args.processed_dir)
    case_ids = set(args.case_id or VALIDATION_CASES)

    path_rows = [row for row in read_csv(processed_dir / "path_lut_spectral.csv") if row.get("case_id") in case_ids]
    solar_rows = [row for row in read_csv(processed_dir / "solar_lut_spectral.csv") if row.get("case_id") in case_ids]
    sky_rows = [row for row in read_csv(processed_dir / "sky_lut_spectral.csv") if row.get("case_id") in case_ids]
    manifest_rows = [row for row in read_csv(processed_dir / "manifest.csv") if row.get("case_id") in case_ids]

    manifest_path = processed_dir / "manifest.csv"
    if manifest_path.exists():
        missing_manifest_cases = sorted(case_ids - {row.get("case_id", "") for row in manifest_rows})
        if missing_manifest_cases:
            print("Cannot build band_lut.csv; manifest.csv is missing validation cases:", file=sys.stderr)
            for case_id in missing_manifest_cases:
                print(f"  {case_id}", file=sys.stderr)
            return 1

    validation_errors = validate_expected_outputs(manifest_rows, path_rows, solar_rows, sky_rows)
    if validation_errors:
        print("Cannot build band_lut.csv; one or more validation cases has no parsed spectral rows:", file=sys.stderr)
        for error in validation_errors:
            print(f"  {error}", file=sys.stderr)
        return 1

    output_rows: List[Dict[str, object]] = []
    all_rows = [*manifest_rows, *path_rows, *solar_rows, *sky_rows]
    bands = sorted({row.get("band", "") for row in all_rows if row.get("band", "")})
    for band in bands:
        meta_rows = [row for row in manifest_rows if row.get("band", "") == band]
        if not meta_rows:
            meta_rows = [row for row in all_rows if row.get("band", "") == band]

        transmittance_rows = [
            row for row in path_rows if row.get("band", "") == band and row.get("mode", "") == "Transmittance"
        ]
        thermal_rows = [
            row for row in path_rows if row.get("band", "") == band and row.get("mode", "") == "ThermalRadiance"
        ]
        direct_solar_rows = [
            row for row in solar_rows if row.get("band", "") == band and row.get("mode", "") == "DirectSolarIrradiance"
        ]
        scattering_rows = [
            row for row in sky_rows if row.get("band", "") == band and row.get("mode", "") == "RadianceWithScattering"
        ]

        output_rows.append(
            {
                "band": band,
                "atmosphere_model": first_nonempty(meta_rows, "atmosphere_model"),
                "aerosol_model": first_nonempty(meta_rows, "aerosol_model"),
                "humidity_profile": first_nonempty(meta_rows, "humidity_profile"),
                "visibility_km": first_nonempty(meta_rows, "visibility_km"),
                "observer_alt_km": first_nonempty(meta_rows, "observer_alt_km"),
                "target_alt_km": first_nonempty(meta_rows, "target_alt_km"),
                "range_km": first_nonempty(meta_rows, "range_km"),
                "solar_zenith_deg": first_nonempty(meta_rows, "solar_zenith_deg"),
                "tau_up_band": rectangular_average(transmittance_rows, "tau_up"),
                "tau_down_band": rectangular_average(direct_solar_rows, "tau_down"),
                "path_radiance_band": rectangular_average(thermal_rows, "path_radiance"),
                "sky_radiance_band": rectangular_average(scattering_rows, "sky_radiance"),
                "solar_irradiance_band": rectangular_average(direct_solar_rows, "solar_irradiance"),
            }
        )

    if not output_rows:
        print("Cannot build band_lut.csv; no matching validation spectral rows were found.", file=sys.stderr)
        return 1
    write_csv(processed_dir / "band_lut.csv", output_rows)
    print(f"Wrote {len(output_rows)} rectangular-response band rows to {processed_dir / 'band_lut.csv'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
