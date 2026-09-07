#!/usr/bin/env python3
"""QC the L1 0.30--2.50 um shortwave solar-heating LUT."""

from __future__ import annotations

import argparse
import csv
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
    if len(rows) != 45:
        errors.append(f"expected 45 rows, got {len(rows)}")
    index = {(float(r["target_alt_km"]), float(r["visibility_km"]),
              float(r["solar_zenith_deg"])): r for r in rows}
    if len(index) != len(rows):
        errors.append("duplicate grid keys")
    for target in [3, 5, 10, 15, 20]:
        for visibility in [5, 23, 50]:
            direct = [float(index[target, visibility, s]["direct_shortwave_solar_irradiance_W_m2"])
                      for s in [20, 45, 70]]
            if not (direct[0] > direct[1] > direct[2] >= 0.0):
                errors.append(f"direct not decreasing with SZA at target={target}, vis={visibility}: {direct}")
    for target in [3, 5, 10, 15, 20]:
        for sza in [20, 45, 70]:
            direct = [float(index[target, v, sza]["direct_shortwave_solar_irradiance_W_m2"])
                      for v in [5, 23, 50]]
            if not (direct[0] <= direct[1] <= direct[2]):
                errors.append(f"direct not increasing with visibility at target={target}, sza={sza}: {direct}")
    standard = index[5.0, 23.0, 45.0]
    report = [
        "# L1 shortwave solar-heating MODTRAN QC", "",
        f"- rows: {len(rows)} (5 target altitudes x 3 visibilities x 3 SZA)",
        "- spectral range: 0.30-2.50 um", "- output unit: W/m^2 (band-integrated)",
        "- direct source: target-level MODOUT2 SOL TR; TOA SOLAR rejected",
        "- diffuse source: .flx DOWNWARD; .flx DIRECT retained as QC-only and never added to diffuse", "",
        "## Standard case target=5 km, visibility=23 km, SZA=45 deg", "",
        f"- direct={standard['direct_shortwave_solar_irradiance_W_m2']} W/m^2",
        f"- diffuse={standard['diffuse_shortwave_down_irradiance_W_m2']} W/m^2",
        f"- .flx DIRECT horizontal diagnostic={standard['flux_direct_horizontal_qc_W_m2']} W/m^2", "",
        "## Grid", "",
        "| target km | visibility km | SZA deg | direct W/m2 | diffuse W/m2 |",
        "| ---: | ---: | ---: | ---: | ---: |",
    ]
    for key in sorted(index):
        r = index[key]
        report.append(f"| {key[0]:g} | {key[1]:g} | {key[2]:g} | "
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
