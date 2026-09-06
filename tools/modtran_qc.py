#!/usr/bin/env python3
"""QC the formal HwaSimIR M1 NIR/MWIR SI LUT and its local MODTRAN evidence."""

from __future__ import annotations

import argparse
import csv
import hashlib
import math
from pathlib import Path

from modtran_convert_to_si import native_cm1_to_si, si_to_native_cm1

ROOT = Path(__file__).resolve().parents[1]
FORMAL = ROOT / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv"
CANDIDATE = ROOT / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si_candidate.csv"
RAW = ROOT / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/raw/m1_nir_mwir_20260906"
REPORT = ROOT / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/m1_nir_mwir_qc.md"
TABLE = ROOT / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/m1_nir_mwir_qc.csv"
MWIR_GOLD_TAU = 0.666728313
MWIR_GOLD_PATH = 0.0360989067


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as f:
        return list(csv.DictReader(f))


def f(row: dict[str, str], key: str) -> float:
    return float(row[key])


def select(rows: list[dict[str, str]], band: str, obs: float, target: float,
           range_km: float, vis: float, sza: float | None = None) -> dict[str, str]:
    matches = [r for r in rows if r["band"] == band and
               abs(f(r, "observer_alt_km") - obs) < 1e-9 and
               abs(f(r, "target_alt_km") - target) < 1e-9 and
               abs(f(r, "range_km") - range_km) < 1e-9 and
               abs(f(r, "visibility_km") - vis) < 1e-9 and
               (sza is None or abs(f(r, "solar_zenith_deg") - sza) < 1e-9)]
    if len(matches) != 1:
        raise ValueError(f"Expected one {band} LUT row, got {len(matches)} for {obs,target,range_km,vis,sza}")
    return matches[0]


def interpolate_range(rows: list[dict[str, str]], band: str, obs: float, target: float,
                      range_km: float, vis: float, sza: float | None = None) -> dict[str, float]:
    candidates = [r for r in rows if r["band"] == band and
                  abs(f(r, "observer_alt_km") - obs) < 1e-9 and
                  abs(f(r, "target_alt_km") - target) < 1e-9 and
                  abs(f(r, "visibility_km") - vis) < 1e-9 and
                  (sza is None or abs(f(r, "solar_zenith_deg") - sza) < 1e-9)]
    below = max((r for r in candidates if f(r, "range_km") <= range_km), key=lambda r: f(r, "range_km"))
    above = min((r for r in candidates if f(r, "range_km") >= range_km), key=lambda r: f(r, "range_km"))
    lo, hi = f(below, "range_km"), f(above, "range_km")
    t = 0.0 if hi == lo else (range_km - lo) / (hi - lo)
    result: dict[str, float] = {}
    for key in ["path_thermal_W_m2_sr_um", "direct_solar_irradiance_at_target_W_m2_um",
                "downward_sky_diffuse_irradiance_W_m2_um", "los_path_scattering_radiance_W_m2_sr_um"]:
        if below[key] and above[key]:
            result[key] = f(below, key) + (f(above, key) - f(below, key)) * t
    od0, od1 = -math.log(f(below, "tau_up")), -math.log(f(above, "tau_up"))
    result["tau_up"] = math.exp(-(od0 + (od1 - od0) * t))
    return result


def planck_mean(temp: float, lo: float = 3.0, hi: float = 5.0, n: int = 4001) -> float:
    h, c, kb = 6.62607015e-34, 299792458.0, 1.380649e-23
    dx = (hi - lo) / (n - 1)
    values = []
    for i in range(n):
        um = lo + i * dx
        m = um * 1e-6
        values.append((2*h*c*c)/(m**5 * math.expm1(h*c/(m*kb*temp))) * 1e-6)
    return dx * (0.5*values[0] + sum(values[1:-1]) + 0.5*values[-1]) / (hi-lo)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--formal-lut", type=Path, default=FORMAL)
    ap.add_argument("--candidate", type=Path, default=CANDIDATE)
    ap.add_argument("--raw-root", type=Path, default=RAW)
    ap.add_argument("--output", type=Path, default=REPORT)
    ap.add_argument("--table", type=Path, default=TABLE)
    args = ap.parse_args()
    rows, candidate, manifest = read_csv(args.formal_lut), read_csv(args.candidate), read_csv(args.raw_root / "case_manifest.csv")
    errors: list[str] = []
    if len(rows) != 1340: errors.append(f"formal row count {len(rows)} != 1340")
    if sum(r["band"] == "MWIR" for r in rows) != 335: errors.append("MWIR row count != 335")
    if sum(r["band"] == "NIR" for r in rows) != 1005: errors.append("NIR row count != 1005")
    if any(r["radiance_unit"] != "W/(m^2 sr um)" or r["irradiance_unit"] != "W/(m^2 um)" or
           r["tau_unit"] != "dimensionless" or r["response_mode"] != "RectangularBand" for r in rows):
        errors.append("unit/response schema mismatch")
    if any("TOTAL_RAD" in r["modtran_source_fields"] or "SOLAR" in r["modtran_source_fields"].split(";") for r in rows):
        errors.append("forbidden TOTAL_RAD or TOA SOLAR source")

    raw_missing = []
    input_hash_errors = []
    for meta in manifest:
        case_dir = args.raw_root / meta["case_id"]
        inp = Path(meta["input_file"])
        output = case_dir / ("spectral_flux.flx" if meta["mode"] == "SpectralFlux" else "MODOUT2.txt")
        if not output.exists() or output.stat().st_size == 0: raw_missing.append(str(output))
        if not inp.exists() or sha256(inp) != meta["input_sha256"]: input_hash_errors.append(str(inp))
    if raw_missing: errors.append(f"missing raw outputs={len(raw_missing)}")
    if input_hash_errors: errors.append(f"input hash mismatches={len(input_hash_errors)}")

    roundtrip = 0.0
    for wavelength in [0.70, 0.90, 1.10, 3.0, 4.0, 5.0]:
        native = 1.23456789e-8
        roundtrip = max(roundtrip, abs(si_to_native_cm1(native_cm1_to_si(native, wavelength), wavelength)-native)/native)
    if roundtrip >= 1e-14: errors.append(f"roundtrip error={roundtrip}")

    mwir = select(rows, "MWIR", 10, 5, 10, 23)
    tau_err = abs(f(mwir, "tau_up") - MWIR_GOLD_TAU)
    path_err = abs(f(mwir, "path_thermal_W_m2_sr_um") - MWIR_GOLD_PATH)
    if tau_err > 1e-9 or path_err > 1e-10: errors.append("MWIR golden mismatch")
    nir = select(rows, "NIR", 10, 5, 10, 23, 45)

    scenarios: list[dict[str, object]] = []
    for range_km in [1.0, 10.0, 30.0, 50.0]:
        value = interpolate_range(rows, "MWIR", 5, 5, range_km, 23)
        scenarios.append({"scenario": f"MWIR_range_{range_km:g}", "band": "MWIR", "range_km": range_km,
                          "visibility_km": 23, "solar_zenith_deg": "", **value})
    for vis in [5.0, 23.0, 50.0]:
        r = select(rows, "MWIR", 10, 5, 10, vis)
        scenarios.append({"scenario": f"MWIR_vis_{vis:g}", "band": "MWIR", "range_km": 10,
                          "visibility_km": vis, "solar_zenith_deg": "", "tau_up": f(r,"tau_up"),
                          "path_thermal_W_m2_sr_um": f(r,"path_thermal_W_m2_sr_um")})
    for sza in [20.0, 45.0, 70.0]:
        for range_km in [1.0, 10.0, 30.0, 50.0]:
            value = interpolate_range(rows, "NIR", 5, 5, range_km, 23, sza)
            scenarios.append({"scenario": f"NIR_sza_{sza:g}_range_{range_km:g}", "band": "NIR",
                              "range_km": range_km, "visibility_km": 23, "solar_zenith_deg": sza, **value})
    for vis in [5.0, 23.0, 50.0]:
        r = select(rows, "NIR", 10, 5, 10, vis, 45)
        scenarios.append({"scenario": f"NIR_vis_{vis:g}", "band": "NIR", "range_km": 10,
                          "visibility_km": vis, "solar_zenith_deg": 45, "tau_up": f(r,"tau_up"),
                          "direct_solar_irradiance_at_target_W_m2_um": f(r,"direct_solar_irradiance_at_target_W_m2_um"),
                          "downward_sky_diffuse_irradiance_W_m2_um": f(r,"downward_sky_diffuse_irradiance_W_m2_um"),
                          "los_path_scattering_radiance_W_m2_sr_um": f(r,"los_path_scattering_radiance_W_m2_sr_um")})
    fields = list(dict.fromkeys(k for row in scenarios for k in row))
    args.table.parent.mkdir(parents=True, exist_ok=True)
    with args.table.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields); writer.writeheader(); writer.writerows(scenarios)

    candidate_nir = next(r for r in candidate if r["band"] == "NIR" and r["observer_alt_km"] == "10" and
                         r["target_alt_km"] == "5" and r["range_km"] == "10" and r["visibility_km"] == "23")
    lines = ["# HwaSimIR M1 NIR/MWIR MODTRAN QC", "", f"- overall: {'FAIL' if errors else 'PASS'}",
             f"- formal LUT: `{args.formal_lut.resolve()}`", f"- rows: {len(rows)} (NIR=1005, MWIR=335)",
             f"- generated local cases: {len(manifest)}; missing outputs: {len(raw_missing)}; input hash errors: {len(input_hash_errors)}",
             f"- raw↔SI roundtrip max relative error: {roundtrip:.3e}",
             "- units: tau dimensionless; radiance W/(m^2 sr um); irradiance W/(m^2 um)",
             "- responseMode: RectangularBand", "- forbidden substitutions: TOTAL_RAD=not used; TOA SOLAR=not used", "",
             "## MWIR golden case", "", f"- tau={f(mwir,'tau_up'):.12g}; absolute error={tau_err:.3e}",
             f"- pathThermal={f(mwir,'path_thermal_W_m2_sr_um'):.12g}; absolute error={path_err:.3e} W/(m^2 sr um)", "",
             "## NIR standard case (10/5/10 km, visibility 23 km, SZA 45 deg)", "",
             f"- tau_up={f(nir,'tau_up'):.12g}",
             f"- direct target SOL TR={f(nir,'direct_solar_irradiance_at_target_W_m2_um'):.12g} W/(m^2 um)",
             f"- downward sky diffuse (.flx DOWNWARD)={f(nir,'downward_sky_diffuse_irradiance_W_m2_um'):.12g} W/(m^2 um)",
             f"- LOS path scattering (SOL_SCAT)={f(nir,'los_path_scattering_radiance_W_m2_sr_um'):.12g} W/(m^2 sr um)", "",
             "## Planck 3-5 um scale", "", "| T K | mean W/(m^2 sr um) |", "| ---: | ---: |"]
    for temp in [250,300,500,1000]: lines.append(f"| {temp} | {planck_mean(temp):.9g} |")
    lines += ["", "## Candidate comparison", "",
              f"- candidate tau={candidate_nir['tau_up_band']} (retained)",
              f"- candidate LOS SOL_SCAT={candidate_nir['path_scattering_radiance_band_W_m2_sr_um']} (retained for SZA45)",
              f"- candidate solar={candidate_nir['solar_irradiance_band_W_m2_um']} was TOA SOLAR and is rejected; formal target SOL TR={nir['direct_solar_irradiance_at_target_W_m2_um']}",
              "- candidate sky_radiance duplicated TOTAL_RAD and is rejected; formal sky diffuse comes only from .flx DOWNWARD.", "",
              "## Runtime A/B scenario table", "", f"- `{args.table.resolve()}`", "- Range=30 km is OD/linear interpolation between formal 20 and 35 km cells."]
    if errors: lines += ["", "## Failures", ""] + [f"- {e}" for e in errors]
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"{'PASS' if not errors else 'FAIL'}: {args.output.resolve()}")
    return 0 if not errors else 2


if __name__ == "__main__":
    raise SystemExit(main())
