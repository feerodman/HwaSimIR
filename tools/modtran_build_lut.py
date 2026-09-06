#!/usr/bin/env python3
"""Build a one-row SI MWIR audit table from converted standard-case spectra."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path


def read(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as f:
        return list(csv.DictReader(f))


def trapz_mean(rows: list[dict[str, str]], column: str) -> tuple[float, float]:
    samples = sorted((float(r["wavelength_um"]), float(r[column])) for r in rows if r.get(column) not in {None, ""})
    if len(samples) < 2:
        raise ValueError(f"Need at least two samples for {column}")
    integral = sum((x1 - x0) * (y0 + y1) * 0.5 for (x0, y0), (x1, y1) in zip(samples, samples[1:]))
    return integral / (samples[-1][0] - samples[0][0]), integral


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--spectral-dir", required=True, type=Path)
    ap.add_argument("--output", required=True, type=Path)
    args = ap.parse_args()
    d = args.spectral_dir
    trans = read(d / "mwir_std_trans_si.csv")
    thermal = read(d / "mwir_std_thermal_si.csv")
    solar = read(d / "mwir_std_solar_direct_si.csv")
    scatter = read(d / "mwir_std_solar_scatter_si.csv")
    flux = read(d / "mwir_std_flux_si.csv")
    tau, _ = trapz_mean(trans, "tau_los")
    path, path_int = trapz_mean(thermal, "path_thermal_W_m2_sr_um")
    solar_scatter, scatter_int = trapz_mean(scatter, "solar_scatter_W_m2_sr_um")
    direct_solar, solar_int = trapz_mean(solar, "direct_solar_W_m2_um")
    tau_down, _ = trapz_mean(solar, "tau_down")
    down_flux, down_flux_int = trapz_mean(flux, "downward_diffuse_W_m2_um")
    row = {
        "case_id": "mwir_standard_20260906",
        "source_case_ids": "mwir_std_trans;mwir_std_thermal;mwir_std_solar_direct;mwir_std_solar_scatter;mwir_std_flux",
        "band": "MWIR",
        "atmosphere_model": "Mid-Latitude Summer",
        "aerosol_model": "Rural",
        "humidity_profile": "default",
        "visibility_km": 23,
        "observer_alt_km": 10,
        "target_alt_km": 5,
        "range_km": 10,
        "solar_zenith_deg": 45,
        "tau_los_mean": tau,
        "tau_down_mean": tau_down,
        "path_radiance_mean_W_m2_sr_um": path,
        "path_radiance_integral_W_m2_sr": path_int,
        "solar_scatter_mean_W_m2_sr_um": solar_scatter,
        "solar_scatter_integral_W_m2_sr": scatter_int,
        "direct_solar_irradiance_mean_W_m2_um": direct_solar,
        "direct_solar_irradiance_integral_W_m2": solar_int,
        "downward_sky_diffuse_flux_mean_W_m2_um": down_flux,
        "downward_sky_diffuse_flux_integral_W_m2": down_flux_int,
        "raw_unit": "radiance W/(cm^2 sr cm^-1); direct solar W/(cm^2 cm^-1); flux W/(cm^2 nm)",
        "si_unit": "W/(m^2 sr um); W/(m^2 um)",
        "conversion_method": "pointwise Jacobian 1e8/lambda_um^2, then trapezoidal wavelength integration",
        "source_files": ";".join(str((d / f).resolve()) for f in ["mwir_std_trans_si.csv", "mwir_std_thermal_si.csv", "mwir_std_solar_direct_si.csv", "mwir_std_solar_scatter_si.csv", "mwir_std_flux_si.csv"]),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(row))
        writer.writeheader()
        writer.writerow(row)
    print(args.output.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
