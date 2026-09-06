#!/usr/bin/env python3
"""Build the formal HwaSimIR NIR/MWIR SI band LUT.

Accepted physical fields are MWIR COMBIN/PTH_THRML, NIR COMBIN/SOL_SCAT,
target direct SOL TR, and .flx DOWNWARD diffuse irradiance.  TOTAL_RAD and
top-of-atmosphere SOLAR are never used as substitutes.
"""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path
from typing import Iterable

from modtran_convert_to_si import parse_flux, parse_radiance, parse_solar


DEFAULT_CANDIDATE = Path("HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si_candidate.csv")
DEFAULT_M1_ROOT = Path("HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/raw/m1_nir_mwir_20260906")
DEFAULT_OUTPUT = Path("HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv")
SZA_VALUES = (20.0, 45.0, 70.0)

FIELDS = [
    "schema_version", "case_id", "band", "atmosphere_model", "aerosol_model",
    "humidity_profile", "visibility_km", "observer_alt_km", "target_alt_km",
    "range_km", "solar_zenith_deg", "tau_up",
    "path_thermal_W_m2_sr_um", "direct_solar_irradiance_at_target_W_m2_um",
    "downward_sky_diffuse_irradiance_W_m2_um",
    "los_path_scattering_radiance_W_m2_sr_um", "radiance_unit", "irradiance_unit",
    "tau_unit", "response_mode", "conversion_method", "modtran_source_fields",
    "source_case_ids", "source_files",
]


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as f:
        return list(csv.DictReader(f))


def number(row: dict[str, str], key: str) -> float:
    value = row.get(key, "")
    if value in {None, ""}:
        raise ValueError(f"Missing {key}: {row}")
    result = float(value)
    if not math.isfinite(result):
        raise ValueError(f"Non-finite {key}: {value}")
    return result


def fmt(value: float | str) -> str:
    return value if isinstance(value, str) else f"{value:.12g}"


def band_mean(rows: Iterable[dict[str, object]], column: str, low_um: float, high_um: float) -> float:
    samples = sorted(
        (float(r["wavelength_um"]), float(r[column]))
        for r in rows
        if r.get(column) not in {None, ""}
    )
    if len(samples) < 2:
        raise ValueError(f"Need at least two samples for {column} in [{low_um}, {high_um}] um")
    first_step = samples[1][0] - samples[0][0]
    last_step = samples[-1][0] - samples[-2][0]
    if samples[0][0] - low_um > first_step + 1.0e-12 or high_um - samples[-1][0] > last_step + 1.0e-12:
        raise ValueError(
            f"Spectrum [{samples[0][0]}, {samples[-1][0]}] um does not cover "
            f"[{low_um}, {high_um}] um within one endpoint grid step for {column}"
        )

    def interp_at(x: float) -> float:
        if x < samples[0][0]:
            x0, y0 = samples[0]
            x1, y1 = samples[1]
            return y0 + (y1 - y0) * (x - x0) / (x1 - x0)
        if x > samples[-1][0]:
            x0, y0 = samples[-2]
            x1, y1 = samples[-1]
            return y0 + (y1 - y0) * (x - x0) / (x1 - x0)
        for i, (sx, sy) in enumerate(samples):
            if abs(sx - x) < 1.0e-12:
                return sy
            if sx > x and i:
                x0, y0 = samples[i - 1]
                return y0 + (sy - y0) * (x - x0) / (sx - x0)
        raise ValueError(f"Band endpoint {x} not bracketed for {column}")

    clipped = [(x, y) for x, y in samples if low_um < x < high_um]
    clipped.insert(0, (low_um, interp_at(low_um)))
    clipped.append((high_um, interp_at(high_um)))
    integral = sum((x1 - x0) * (y0 + y1) * 0.5 for (x0, y0), (x1, y1) in zip(clipped, clipped[1:]))
    return integral / (high_um - low_um)


def case_token(value: float) -> str:
    return f"{value:g}"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--candidate", type=Path, default=DEFAULT_CANDIDATE)
    ap.add_argument("--m1-root", type=Path, default=DEFAULT_M1_ROOT)
    ap.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = ap.parse_args()

    candidate = read_csv(args.candidate)
    base_rows = [r for r in candidate if r.get("band") in {"NIR", "MWIR"}]
    if len(base_rows) != 670:
        raise ValueError(f"Expected 670 NIR/MWIR candidate rows, got {len(base_rows)}")

    solar_cache: dict[tuple[float, float, float], tuple[float, float, str, str]] = {}
    flux_cache: dict[tuple[float, float, float], tuple[float, str, str]] = {}
    manifest = read_csv(args.m1_root / "case_manifest.csv")
    for meta in manifest:
        mode = meta["mode"]
        target = number(meta, "target_alt_km")
        visibility = number(meta, "visibility_km")
        sza = number(meta, "solar_zenith_deg")
        key = (target, visibility, sza)
        case_dir = args.m1_root / meta["case_id"]
        if mode == "DirectSolarIrradiance":
            source = case_dir / "MODOUT2.txt"
            spectra = parse_solar(source)
            solar_cache[key] = (
                band_mean(spectra, "direct_solar_W_m2_um", 0.70, 1.10),
                band_mean(spectra, "tau_down", 0.70, 1.10),
                meta["case_id"], str(source.resolve()))
        elif mode == "SpectralFlux":
            source = case_dir / "spectral_flux.flx"
            spectra = parse_flux(source, target)
            flux_cache[key] = (
                band_mean(spectra, "downward_diffuse_W_m2_um", 0.70, 1.10),
                meta["case_id"], str(source.resolve()))

    output: list[dict[str, str]] = []
    for base in base_rows:
        band = base["band"]
        observer = number(base, "observer_alt_km")
        target = number(base, "target_alt_km")
        range_km = number(base, "range_km")
        visibility = number(base, "visibility_km")
        common = {
            "schema_version": "1", "band": band,
            "atmosphere_model": base["atmosphere_model"], "aerosol_model": base["aerosol_model"],
            "humidity_profile": "default", "visibility_km": fmt(visibility),
            "observer_alt_km": fmt(observer), "target_alt_km": fmt(target), "range_km": fmt(range_km),
            "tau_up": fmt(number(base, "tau_up_band")), "radiance_unit": "W/(m^2 sr um)",
            "irradiance_unit": "W/(m^2 um)", "tau_unit": "dimensionless",
            "response_mode": "RectangularBand",
            "conversion_method": "pointwise native*1e8/lambda_um^2 then wavelength-domain trapezoidal mean; endpoint linear extrapolation limited to one spectral grid step",
        }
        if band == "MWIR":
            row = dict(common)
            row.update({
                "case_id": f"MWIR_obs{case_token(observer)}_tar{case_token(target)}_rng{case_token(range_km)}_vis{case_token(visibility)}",
                "solar_zenith_deg": "", "path_thermal_W_m2_sr_um": fmt(number(base, "path_radiance_band_W_m2_sr_um")),
                "direct_solar_irradiance_at_target_W_m2_um": "", "downward_sky_diffuse_irradiance_W_m2_um": "",
                "los_path_scattering_radiance_W_m2_sr_um": "", "modtran_source_fields": "COMBIN TRANS;PTH_THRML",
                "source_case_ids": base["source_case_ids"], "source_files": str(args.candidate.resolve()),
            })
            output.append(row)
            continue

        for sza in SZA_VALUES:
            key = (target, visibility, sza)
            if key not in solar_cache or key not in flux_cache:
                raise ValueError(f"Missing NIR solar/flux case for {key}")
            direct, _, solar_id, solar_path = solar_cache[key]
            sky, flux_id, flux_path = flux_cache[key]
            if sza == 45.0:
                scatter = number(base, "path_scattering_radiance_band_W_m2_sr_um")
                scatter_id = next((x for x in base["source_case_ids"].split(";") if "_scattering_" in x), "candidate_sza45_scattering")
                scatter_path = str(args.candidate.resolve())
            else:
                scatter_id = (f"NIR_scattering_obs{case_token(observer)}_tar{case_token(target)}_rng{case_token(range_km)}_"
                              f"vis{case_token(visibility)}_aerRural_humdefault_sza{case_token(sza)}")
                scatter_source = args.m1_root / scatter_id / "MODOUT2.txt"
                scatter = band_mean(parse_radiance(scatter_source), "solar_scatter_W_m2_sr_um", 0.70, 1.10)
                scatter_path = str(scatter_source.resolve())
            trans_id = next((x for x in base["source_case_ids"].split(";") if "_transmittance_" in x), "candidate_tau")
            row = dict(common)
            row.update({
                "case_id": f"NIR_obs{case_token(observer)}_tar{case_token(target)}_rng{case_token(range_km)}_vis{case_token(visibility)}_sza{case_token(sza)}",
                "solar_zenith_deg": fmt(sza), "path_thermal_W_m2_sr_um": "",
                "direct_solar_irradiance_at_target_W_m2_um": fmt(direct),
                "downward_sky_diffuse_irradiance_W_m2_um": fmt(sky),
                "los_path_scattering_radiance_W_m2_sr_um": fmt(scatter),
                "modtran_source_fields": "COMBIN TRANS;SOL TR;.flx DOWNWARD;SOL_SCAT",
                "source_case_ids": ";".join([trans_id, scatter_id, solar_id, flux_id]),
                "source_files": ";".join([str(args.candidate.resolve()), scatter_path, solar_path, flux_path]),
            })
            output.append(row)

    output.sort(key=lambda r: (r["band"], float(r["observer_alt_km"]), float(r["target_alt_km"]),
                               float(r["range_km"]), float(r["visibility_km"]), float(r["solar_zenith_deg"] or 0.0)))
    if len(output) != 1340:
        raise ValueError(f"Expected 1340 formal LUT rows, got {len(output)}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(output)
    print(f"Wrote {len(output)} formal SI LUT rows -> {args.output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
