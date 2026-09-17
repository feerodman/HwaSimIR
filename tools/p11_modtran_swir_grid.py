#!/usr/bin/env python3
"""Generate the minimal complete five-axis SWIR LUT grid for a ground close-up."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path


WN_LOW = 4000.0
WN_HIGH = 9090.9091
STEP = 1.0
ALTITUDE_KM = 0.001
RANGES_KM = (0.1, 0.5, 1.0)
VISIBILITIES_KM = (6.0, 23.0)
SOLAR_ZENITH_DEG = (20.0, 45.0, 70.0)


def token(value: float) -> str:
    return f"{value:g}".replace(".", "p")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def los_lines(iemsct: int, range_km: float, visibility_km: float) -> list[str]:
    return [
        f"T F 2    2    {iemsct:d}    0    0    0    0    0    0    0    0    0    0   0.0000.00000",
        "fFF  2   0   360.000  0.000000  0.0000000F F F F F               0.000     0.000     0.000     0.000         0",
        f"    1    0    1    0    0    0{visibility_km:10.3f}     0.000     0.000     0.000     0.000",
        f"{ALTITUDE_KM:10.6f}{ALTITUDE_KM:10.6f}{0.0:10.6f}{range_km:10.5f}  0.000000  0.000000    0       0.000000  0.000000",
        f"{WN_LOW:10.4f}{WN_HIGH:10.4f}{STEP:10.4f}{STEP:10.4f} W                 0     0.000",
        "    0",
    ]


def scattering_lines(range_km: float, visibility_km: float, sza: float) -> list[str]:
    return los_lines(2, range_km, visibility_km)[:4] + [
        "    2    0  172    0",
        f"    90.000 {sza:9.3f}     0.000     0.000     0.000     0.000     0.000     0.000",
        f"{WN_LOW:10.4f}{WN_HIGH:10.4f}{STEP:10.4f}{STEP:10.4f} W                 0     0.000",
        "    0",
    ]


def solar_lines(visibility_km: float, sza: float) -> list[str]:
    return [
        "T F 2    2    3    0    0    0    0    0    0    0    0    0    0   0.0000.00000",
        "fFF  2   0   360.000  0.000000  0.0000000F F F F F               0.000     0.000     0.000     0.000         0",
        f"    1    0    1    0    0    0{visibility_km:10.3f}     0.000     0.000     0.000     0.000",
        f"{ALTITUDE_KM:10.3f}     0.000{sza:10.3f}  172          0.000    0     0.000",
        f"{WN_LOW:10.3f}{WN_HIGH:10.3f}{STEP:10.3f}{STEP:10.3f} W        W1         0     0.000",
        "    0",
    ]


def flux_lines(visibility_km: float, sza: float) -> list[str]:
    return [
        "T F 2    2    2    1    0    0    0    0    0    0    0    0    0   0.000   0.40",
        "tFF  4   0   330.000  1.000000     1.0000F T                     0.000     0.000     0.000     0.000         0",
        "01_2009",
        f"    1    0    1    0   18    0{visibility_km:10.3f}     0.000     0.000     0.000     0.000",
        "   0.000   0.000   0.000",
        f"{ALTITUDE_KM:10.3f}     0.000   180.000   0.00000     0.000     0.000    0          0.000     0.000",
        "    2    2    1    0",
        f"     0.000 {sza:9.3f}     0.000     0.000     0.000     0.000     0.000     0.000",
        f"{WN_LOW:10.3f}{WN_HIGH:10.3f}{STEP:10.3f}{2.0:10.3f}RN              T    0     0.000",
        "    0",
    ]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--output-root", type=Path, default=Path("logs/p11/modtran/swir_ground_grid"))
    args = ap.parse_args()
    root = args.output_root.resolve()
    root.mkdir(parents=True, exist_ok=True)
    specs: list[tuple[str, str, list[str], float, float | None, str]] = []
    for visibility in VISIBILITIES_KM:
        for range_km in RANGES_KM:
            suffix = f"obs{token(ALTITUDE_KM)}_tar{token(ALTITUDE_KM)}_rng{token(range_km)}_vis{token(visibility)}"
            specs.append((f"SWIR_trans_{suffix}", "Transmittance",
                          los_lines(0, range_km, visibility), visibility, None, "tau_los"))
            specs.append((f"SWIR_thermal_{suffix}", "ThermalRadiance",
                          los_lines(1, range_km, visibility), visibility, None, "path_thermal"))
            for sza in SOLAR_ZENITH_DEG:
                specs.append((f"SWIR_scattering_{suffix}_sza{token(sza)}", "RadianceWithScattering",
                              scattering_lines(range_km, visibility, sza), visibility, sza,
                              "los_path_solar_scattering"))
        for sza in SOLAR_ZENITH_DEG:
            suffix = f"tar{token(ALTITUDE_KM)}_vis{token(visibility)}_sza{token(sza)}"
            specs.append((f"SWIR_solar_{suffix}", "DirectSolarIrradiance",
                          solar_lines(visibility, sza), visibility, sza, "target_direct_solar"))
            specs.append((f"SWIR_flux_{suffix}", "SpectralFlux",
                          flux_lines(visibility, sza), visibility, sza, "target_downward_sky_flux"))

    rows: list[dict[str, object]] = []
    for case_id, mode, lines, visibility, sza, purpose in specs:
        case_dir = root / case_id
        case_dir.mkdir(parents=True, exist_ok=True)
        path = case_dir / f"{case_id}.tp5"
        path.write_text("\n".join(lines) + "\n", encoding="ascii")
        range_km = ""
        if "_rng" in case_id:
            range_km = next(str(r) for r in RANGES_KM if f"_rng{token(r)}_" in case_id)
        rows.append({
            "case_id": case_id, "mode": mode, "band": "SWIR",
            "atmosphere_model": "Mid-Latitude Summer", "aerosol_model": "Rural",
            "humidity_profile": "default", "visibility_km": f"{visibility:g}",
            "observer_alt_km": f"{ALTITUDE_KM:g}" if range_km else "", "target_alt_km": f"{ALTITUDE_KM:g}",
            "range_km": range_km, "solar_zenith_deg": "" if sza is None else f"{sza:g}",
            "wavelength_low_um": "1.1", "wavelength_high_um": "2.5",
            "wavenumber_low_cm1": f"{WN_LOW:.4f}", "wavenumber_high_cm1": f"{WN_HIGH:.4f}",
            "wavenumber_increment_cm1": f"{STEP:.4f}", "response_mode": "RectangularBand",
            "purpose": purpose, "input_file": str(path), "input_sha256": sha256(path),
        })
    with (root / "case_manifest.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader(); writer.writerows(rows)
    (root / "case_manifest.json").write_text(json.dumps(rows, indent=2) + "\n", encoding="utf-8")
    print(f"Generated {len(rows)} component runs for 18 complete SWIR LUT vertices under {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
