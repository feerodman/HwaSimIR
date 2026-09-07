#!/usr/bin/env python3
"""Build the formal 0.30--2.50 um L1 shortwave solar-heating LUT."""

from __future__ import annotations

import argparse
import csv
import hashlib
from pathlib import Path

from modtran_convert_to_si import parse_flux, parse_solar


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RAW = ROOT / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/raw/l1_solar_heating_20260907"
DEFAULT_OUTPUT = ROOT / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/solar_heating_lut_si.csv"
LOW_UM = 0.30
HIGH_UM = 2.50


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def integrate_band(rows: list[dict[str, object]], column: str,
                   low_um: float = LOW_UM, high_um: float = HIGH_UM) -> float:
    points = sorted((float(r["wavelength_um"]), float(r[column])) for r in rows)
    if len(points) < 2:
        raise ValueError("Spectrum has fewer than two points")
    low_step = points[1][0] - points[0][0]
    high_step = points[-1][0] - points[-2][0]
    if points[0][0] - low_um > low_step + 1.0e-12 or high_um - points[-1][0] > high_step + 1.0e-12:
        raise ValueError(f"Spectrum does not cover {low_um}-{high_um} um within one grid step")

    def interp(x: float) -> float:
        if x <= points[0][0]:
            pair = points[0], points[1]
        elif x >= points[-1][0]:
            pair = points[-2], points[-1]
        else:
            pair = None
        for (x0, y0), (x1, y1) in zip(points, points[1:]):
            if x0 <= x <= x1:
                pair = ((x0, y0), (x1, y1))
                break
        if pair is not None:
            (x0, y0), (x1, y1) = pair
            if abs(x1 - x0) < 1.0e-15:
                return y0
            t = (x - x0) / (x1 - x0)
            return y0 + (y1 - y0) * t
        raise ValueError(f"Cannot interpolate wavelength {x}")

    clipped = [(low_um, interp(low_um))]
    clipped.extend((x, y) for x, y in points if low_um < x < high_um)
    clipped.append((high_um, interp(high_um)))
    return sum(0.5 * (y0 + y1) * (x1 - x0)
               for (x0, y0), (x1, y1) in zip(clipped, clipped[1:]))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--raw-root", type=Path, default=DEFAULT_RAW)
    ap.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = ap.parse_args()
    raw_root = args.raw_root.resolve()
    manifest_path = raw_root / "case_manifest.csv"
    with manifest_path.open(newline="", encoding="utf-8-sig") as f:
        manifest = list(csv.DictReader(f))

    grouped: dict[tuple[str, str, str, str, str, str], dict[str, dict[str, str]]] = {}
    for row in manifest:
        key = (row["atmosphere_model"], row["aerosol_model"], row["humidity_profile"],
               row["target_alt_km"], row["visibility_km"], row["solar_zenith_deg"])
        grouped.setdefault(key, {})[row["mode"]] = row

    output: list[dict[str, object]] = []
    for key, modes in sorted(grouped.items(), key=lambda item: tuple(
            float(x) if i >= 3 else x for i, x in enumerate(item[0]))):
        atmosphere, aerosol, humidity, target, visibility, sza = key
        direct_meta = modes.get("DirectSolarIrradiance")
        flux_meta = modes.get("SpectralFlux")
        if direct_meta is None or flux_meta is None:
            raise ValueError(f"Incomplete direct/flux pair for {key}")
        direct_file = Path(direct_meta["input_file"]).parent / "MODOUT2.txt"
        flux_file = Path(flux_meta["input_file"]).parent / "spectral_flux.flx"
        if not direct_file.is_file() or not flux_file.is_file():
            raise FileNotFoundError(f"Missing output for {key}")
        direct_spectrum = parse_solar(direct_file)
        flux_spectrum = parse_flux(flux_file, float(target))
        direct = integrate_band(direct_spectrum, "direct_solar_W_m2_um")
        diffuse = integrate_band(flux_spectrum, "downward_diffuse_W_m2_um")
        flux_direct_qc = integrate_band(flux_spectrum, "direct_solar_W_m2_um")
        output.append({
            "schema_version": "1",
            "case_id": f"SWHEAT_tar{target}_vis{visibility}_sza{sza}",
            "band": "SOLAR_SHORTWAVE_0.30_2.50_UM",
            "atmosphere_model": atmosphere,
            "aerosol_model": aerosol,
            "humidity_profile": humidity,
            "target_alt_km": target,
            "visibility_km": visibility,
            "solar_zenith_deg": sza,
            "direct_shortwave_solar_irradiance_W_m2": f"{direct:.12g}",
            "diffuse_shortwave_down_irradiance_W_m2": f"{diffuse:.12g}",
            "flux_direct_horizontal_qc_W_m2": f"{flux_direct_qc:.12g}",
            "irradiance_unit": "W/m^2",
            "spectral_range_um": "0.30-2.50",
            "response_mode": "BroadbandIntegral",
            "direct_source_field": "MODOUT2 SOL TR",
            "diffuse_source_field": ".flx DOWNWARD",
            "flux_direct_policy": "QC_only_not_added_to_diffuse",
            "raw_solar_unit": "W/(cm^2 cm^-1)",
            "raw_flux_unit": "W/(cm^2 nm)",
            "conversion_method": "SOL_TR pointwise native*1e8/lambda_um^2; FLX DOWNWARD pointwise native*1e7; wavelength-domain trapezoidal integral 0.30-2.50 um",
            "source_case_ids": f"{direct_meta['case_id']};{flux_meta['case_id']}",
            "source_files": f"{direct_file};{flux_file}",
            "source_sha256": f"{sha256(direct_file)};{sha256(flux_file)}",
        })

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(output[0]))
        writer.writeheader()
        writer.writerows(output)
    print(f"Built {len(output)} solar-heating LUT rows -> {args.output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
