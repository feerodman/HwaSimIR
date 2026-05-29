#!/usr/bin/env python3
"""Audit the completed six-case MODTRAN validation outputs before Pilot72."""

from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path
from typing import Dict, List, Sequence


REQUIRED_FILES = [
    "path_lut_spectral.csv",
    "solar_lut_spectral.csv",
    "sky_lut_spectral.csv",
    "band_lut.csv",
    "manifest.csv",
    "qc_report.md",
]

VALIDATION_STATUS = "validation_succeeded"
PILOT72_STATUS = "pilot72_succeeded"

VALIDATION_CASE_IDS = {
    "MWIR_transmittance_obs10_tar10_rng20_vis23_aerRural_humdefault",
    "MWIR_thermal_obs10_tar10_rng20_vis23_aerRural_humdefault",
    "MWIR_solar_obs10_tar10_rng20_vis23_aerRural_humdefault_sza45",
    "NIR_transmittance_obs10_tar10_rng20_vis23_aerRural_humdefault",
    "NIR_solar_obs10_tar10_rng20_vis23_aerRural_humdefault_sza45",
    "NIR_scattering_obs10_tar10_rng20_vis23_aerRural_humdefault_sza45",
}


def read_csv(path: Path) -> List[Dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as handle:
        return list(csv.DictReader(handle))


def parse_float(value: object) -> float | None:
    if value is None or value == "":
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def require(condition: bool, message: str, errors: List[str]) -> None:
    if not condition:
        errors.append(message)


def value_in_unit_interval(row: Dict[str, str], column: str) -> bool:
    value = parse_float(row.get(column, ""))
    return value is not None and 0.0 <= value <= 1.0


def nonempty(row: Dict[str, str], column: str) -> bool:
    return row.get(column, "") != ""


def check_required_files(processed_dir: Path, errors: List[str]) -> None:
    for name in REQUIRED_FILES:
        path = processed_dir / name
        require(path.exists(), f"missing required file: {path}", errors)
        if path.exists():
            require(path.stat().st_size > 0, f"required file is empty: {path}", errors)


def check_manifest(processed_dir: Path, errors: List[str]) -> List[Dict[str, str]]:
    manifest = read_csv(processed_dir / "manifest.csv")
    require(len(manifest) == 6, f"manifest.csv must contain exactly 6 validation rows, found {len(manifest)}", errors)
    bad_status = [row.get("case_id", "<missing>") for row in manifest if row.get("status") != VALIDATION_STATUS]
    require(not bad_status, f"manifest rows not marked {VALIDATION_STATUS}: {', '.join(bad_status)}", errors)
    return manifest


def check_band_lut(processed_dir: Path, errors: List[str]) -> List[Dict[str, str]]:
    rows = read_csv(processed_dir / "band_lut.csv")
    bands = {row.get("band", "") for row in rows}
    require(len(rows) == 2, f"band_lut.csv must contain exactly 2 validation rows, found {len(rows)}", errors)
    require(bands == {"NIR", "MWIR"}, f"band_lut.csv bands must be NIR and MWIR, found {sorted(bands)}", errors)

    by_band = {row.get("band", ""): row for row in rows}
    for band, row in by_band.items():
        require(value_in_unit_interval(row, "tau_up_band"), f"{band} tau_up_band is not in 0..1", errors)
        require(value_in_unit_interval(row, "tau_down_band"), f"{band} tau_down_band is not in 0..1", errors)

    nir = by_band.get("NIR", {})
    if nir:
        require(nonempty(nir, "tau_up_band"), "NIR row missing tau_up_band", errors)
        require(nonempty(nir, "tau_down_band"), "NIR row missing tau_down_band", errors)
        require(nonempty(nir, "solar_irradiance_band"), "NIR row missing solar_irradiance_band", errors)
        has_scattering = nonempty(nir, "sky_radiance_band") or nonempty(nir, "path_scattering_radiance_band")
        require(has_scattering, "NIR row missing sky/scattering band radiance integral", errors)

    mwir = by_band.get("MWIR", {})
    if mwir:
        require(nonempty(mwir, "tau_up_band"), "MWIR row missing tau_up_band", errors)
        require(nonempty(mwir, "path_radiance_band"), "MWIR row missing path_radiance_band", errors)
        require(nonempty(mwir, "solar_irradiance_band"), "MWIR row missing solar_irradiance_band", errors)

    return rows


def check_spectral_tables(processed_dir: Path, manifest: Sequence[Dict[str, str]], errors: List[str]) -> None:
    expected_cases = {row.get("case_id", "") for row in manifest}
    table_cases = set()
    row_counts = {}
    for name in ["path_lut_spectral.csv", "solar_lut_spectral.csv", "sky_lut_spectral.csv"]:
        rows = read_csv(processed_dir / name)
        row_counts[name] = len(rows)
        table_cases.update(row.get("case_id", "") for row in rows)
        require(len(rows) > 0, f"{name} contains no parsed rows", errors)

    missing_cases = sorted(expected_cases - table_cases)
    extra_cases = sorted(table_cases - expected_cases)
    require(not missing_cases, f"parsed spectral tables missing validation cases: {', '.join(missing_cases)}", errors)
    require(not extra_cases, f"parsed spectral tables contain non-validation cases: {', '.join(extra_cases)}", errors)


def check_qc_report(processed_dir: Path, manifest: Sequence[Dict[str, str]], errors: List[str]) -> None:
    text = (processed_dir / "qc_report.md").read_text(encoding="utf-8", errors="replace")
    for row in manifest:
        case_id = row.get("case_id", "")
        require(case_id in text, f"qc_report.md missing case entry for {case_id}", errors)
    require(text.count("## Case ") >= 6, "qc_report.md must include at least six case sections", errors)
    require("warnings_errors: none" in text, "qc_report.md does not record parser warnings/errors", errors)


def check_pilot72_superset(processed_dir: Path, errors: List[str]) -> bool:
    manifest = read_csv(processed_dir / "manifest.csv")
    if len(manifest) != 72:
        return False
    if any(row.get("status") != PILOT72_STATUS for row in manifest):
        return False

    manifest_cases = {row.get("case_id", "") for row in manifest}
    missing_manifest_cases = sorted(VALIDATION_CASE_IDS - manifest_cases)
    require(not missing_manifest_cases, f"Pilot72 manifest is missing validation baseline cases: {', '.join(missing_manifest_cases)}", errors)

    table_cases = set()
    for name in ["path_lut_spectral.csv", "solar_lut_spectral.csv", "sky_lut_spectral.csv"]:
        table_cases.update(row.get("case_id", "") for row in read_csv(processed_dir / name))
    missing_spectral_cases = sorted(VALIDATION_CASE_IDS - table_cases)
    require(not missing_spectral_cases, f"Pilot72 spectral tables are missing validation baseline cases: {', '.join(missing_spectral_cases)}", errors)

    band_rows = read_csv(processed_dir / "band_lut.csv")
    require(len(band_rows) == 24, f"Pilot72 band_lut.csv must contain 24 rows, found {len(band_rows)}", errors)
    for row in band_rows:
        band = row.get("band", "<missing>")
        require(value_in_unit_interval(row, "tau_up_band"), f"{band} tau_up_band is not in 0..1", errors)
        require(value_in_unit_interval(row, "tau_down_band"), f"{band} tau_down_band is not in 0..1", errors)

    qc_text = (processed_dir / "qc_report.md").read_text(encoding="utf-8", errors="replace")
    require("## Pilot72 Summary" in qc_text, "qc_report.md missing Pilot72 Summary", errors)
    require("overall_status: PASS" in qc_text, "Pilot72 Summary is not PASS", errors)
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--processed-dir", required=True)
    args = parser.parse_args()

    processed_dir = Path(args.processed_dir)
    errors: List[str] = []
    check_required_files(processed_dir, errors)
    if errors:
        for error in errors:
            print(f"FAIL: {error}", file=sys.stderr)
        return 1

    if check_pilot72_superset(processed_dir, errors):
        if errors:
            print("Pilot72 supersets the validation cases, but audit failed.", file=sys.stderr)
            for error in errors:
                print(f"FAIL: {error}", file=sys.stderr)
            return 1
        print("Validation output audit passed via Pilot72 superset.")
        print(f"processed_dir: {processed_dir}")
        print(f"manifest_rows: 72 status={PILOT72_STATUS}")
        print("validation_baseline_cases: present in manifest and spectral tables")
        return 0

    manifest = check_manifest(processed_dir, errors)
    band_rows = check_band_lut(processed_dir, errors)
    check_spectral_tables(processed_dir, manifest, errors)
    check_qc_report(processed_dir, manifest, errors)

    if errors:
        print("Validation output audit failed; do not run Pilot72.", file=sys.stderr)
        for error in errors:
            print(f"FAIL: {error}", file=sys.stderr)
        return 1

    print("Validation output audit passed.")
    print(f"processed_dir: {processed_dir}")
    print(f"manifest_rows: {len(manifest)} status={VALIDATION_STATUS}")
    print(f"band_lut_rows: {len(band_rows)} bands=NIR,MWIR")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
