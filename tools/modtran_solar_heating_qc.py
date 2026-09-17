#!/usr/bin/env python3
"""QC the legacy and P11 0.30--2.50 um shortwave solar-heating LUT."""

from __future__ import annotations

import argparse
import csv
import math
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_LUT = ROOT / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/solar_heating_lut_si.csv"
DEFAULT_REPORT = ROOT / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/l1_solar_heating_qc.md"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--lut", type=Path, default=DEFAULT_LUT)
    ap.add_argument("--report", type=Path, default=DEFAULT_REPORT)
    args = ap.parse_args()
    with args.lut.open(newline="", encoding="utf-8-sig") as f:
        rows = list(csv.DictReader(f))
    errors: list[str] = []
    if len(rows) != 93:
        errors.append(f"expected 93 rows (45 legacy + 48 P11), got {len(rows)}")
    required_text = {
        "band": "SOLAR_SHORTWAVE_0.30_2.50_UM",
        "spectral_range_um": "0.30-2.50",
        "irradiance_unit": "W/m^2",
    }
    for row in rows:
        for field, expected in required_text.items():
            if row.get(field) != expected:
                errors.append(f"{row.get('case_id', 'unknown')}: {field}={row.get(field)!r}")
        for field in ("direct_shortwave_solar_irradiance_W_m2",
                      "diffuse_shortwave_down_irradiance_W_m2"):
            try:
                value = float(row[field])
                if not math.isfinite(value) or value < 0.0:
                    raise ValueError
            except (KeyError, ValueError):
                errors.append(f"{row.get('case_id', 'unknown')}: invalid {field}")
    index = {(r["humidity_profile"], float(r["target_alt_km"]),
              float(r["visibility_km"]), float(r["solar_zenith_deg"])): r for r in rows}
    if len(index) != len(rows):
        errors.append("duplicate grid keys")
    profile_counts = Counter(r["humidity_profile"] for r in rows)
    expected_counts = Counter({
        "default": 57,
        "scaled_mls_surface_rh30": 12,
        "scaled_mls_surface_rh60": 12,
        "scaled_mls_surface_rh85": 12,
    })
    if profile_counts != expected_counts:
        errors.append(f"humidity profile counts differ: {dict(profile_counts)}")

    legacy_keys = {
        ("default", float(target), float(visibility), float(sza))
        for target in [3, 5, 10, 15, 20]
        for visibility in [5, 23, 50]
        for sza in [20, 45, 70]
    }
    profiles = ["default", "scaled_mls_surface_rh30",
                "scaled_mls_surface_rh60", "scaled_mls_surface_rh85"]
    p11_keys = {
        (profile, float(target), float(visibility), float(sza))
        for profile in profiles
        for target in [0.001, 1]
        for visibility in [6, 23]
        for sza in [20, 45, 70]
    }
    actual_keys = set(index)
    if actual_keys != legacy_keys | p11_keys:
        errors.append(
            f"grid keys differ: missing={sorted((legacy_keys | p11_keys) - actual_keys)}, "
            f"extra={sorted(actual_keys - (legacy_keys | p11_keys))}")

    for target in [3, 5, 10, 15, 20]:
        for visibility in [5, 23, 50]:
            direct = [float(index["default", target, visibility, s]["direct_shortwave_solar_irradiance_W_m2"])
                      for s in [20, 45, 70]]
            if not (direct[0] > direct[1] > direct[2] >= 0.0):
                errors.append(f"direct not decreasing with SZA at target={target}, vis={visibility}: {direct}")
    for target in [3, 5, 10, 15, 20]:
        for sza in [20, 45, 70]:
            direct = [float(index["default", target, v, sza]["direct_shortwave_solar_irradiance_W_m2"])
                      for v in [5, 23, 50]]
            if not (direct[0] <= direct[1] <= direct[2]):
                errors.append(f"direct not increasing with visibility at target={target}, sza={sza}: {direct}")
    for profile in profiles:
        for target in [0.001, 1.0]:
            for visibility in [6.0, 23.0]:
                direct = [float(index[profile, target, visibility, s]
                                ["direct_shortwave_solar_irradiance_W_m2"])
                          for s in [20, 45, 70]]
                if not (direct[0] > direct[1] > direct[2] >= 0.0):
                    errors.append(
                        f"P11 direct not decreasing with SZA at profile={profile}, "
                        f"target={target}, vis={visibility}: {direct}")
    for target in [0.001, 1.0]:
        for visibility in [6.0, 23.0]:
            for sza in [20.0, 45.0, 70.0]:
                direct = [float(index[f"scaled_mls_surface_rh{rh}", target, visibility, sza]
                                ["direct_shortwave_solar_irradiance_W_m2"])
                          for rh in [30, 60, 85]]
                if not (direct[0] > direct[1] > direct[2] >= 0.0):
                    errors.append(
                        f"P11 direct humidity response not decreasing at target={target}, "
                        f"vis={visibility}, SZA={sza}: {direct}")
    standard = index["default", 5.0, 23.0, 45.0]
    report = [
        "# L1 shortwave solar-heating MODTRAN QC", "",
        f"- rows: {len(rows)} (45 preserved legacy + 48 P11 civil-altitude)",
        f"- humidity-profile counts: {dict(sorted(profile_counts.items()))}",
        "- spectral range: 0.30-2.50 um", "- output unit: W/m^2 (band-integrated)",
        "- direct source: target-level MODOUT2 SOL TR; TOA SOLAR rejected",
        "- diffuse source: .flx DOWNWARD; .flx DIRECT retained as QC-only and never added to diffuse", "",
        "## Standard case target=5 km, visibility=23 km, SZA=45 deg", "",
        f"- direct={standard['direct_shortwave_solar_irradiance_W_m2']} W/m^2",
        f"- diffuse={standard['diffuse_shortwave_down_irradiance_W_m2']} W/m^2",
        f"- .flx DIRECT horizontal diagnostic={standard['flux_direct_horizontal_qc_W_m2']} W/m^2", "",
        "## Grid", "",
        "| humidity profile | target km | visibility km | SZA deg | direct W/m2 | diffuse W/m2 |",
        "| --- | ---: | ---: | ---: | ---: | ---: |",
    ]
    for key in sorted(index):
        r = index[key]
        report.append(f"| {key[0]} | {key[1]:g} | {key[2]:g} | {key[3]:g} | "
                      f"{float(r['direct_shortwave_solar_irradiance_W_m2']):.6f} | "
                      f"{float(r['diffuse_shortwave_down_irradiance_W_m2']):.6f} |")
    report += ["", "## Result", "", "PASS" if not errors else "FAIL"]
    report += [f"- {error}" for error in errors]
    args.report.write_text("\n".join(report) + "\n", encoding="utf-8")
    if errors:
        raise SystemExit("; ".join(errors))
    print(f"PASS: {args.report.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
