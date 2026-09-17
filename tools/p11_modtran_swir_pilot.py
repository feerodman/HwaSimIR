#!/usr/bin/env python3
"""Generate a small, auditable MODTRAN5 SWIR pilot without touching production LUTs.

The five MODTRAN runs represent one physical condition (MLS, Rural, 23 km
visibility, observer/target/range 10/5/10 km, solar zenith 45 deg).  Separate
runs are required because MODTRAN exposes LOS transmission/path radiance,
target-plane direct solar, and spectral flux through different IEMSCT/IMULT
products.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path


WN_LOW = 4000.0
WN_HIGH = 9090.9091
STEP = 1.0


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def los_lines(iemsct: int) -> list[str]:
    return [
        f"T F 2    2    {iemsct:d}    0    0    0    0    0    0    0    0    0    0   0.0000.00000",
        "fFF  2   0   360.000  0.000000  0.0000000F F F F F               0.000     0.000     0.000     0.000         0",
        "    1    0    1    0    0    0    23.000     0.000     0.000     0.000     0.000",
        " 10.000000  5.000000  0.000000  10.00000  0.000000  0.000000    0       0.000000  0.000000",
        f"{WN_LOW:10.4f}{WN_HIGH:10.4f}{STEP:10.4f}{STEP:10.4f} W                 0     0.000",
        "    0",
    ]


def scattering_lines() -> list[str]:
    return los_lines(2)[:4] + [
        "    2    0  172    0",
        "    90.000    45.000     0.000     0.000     0.000     0.000     0.000     0.000",
        f"{WN_LOW:10.4f}{WN_HIGH:10.4f}{STEP:10.4f}{STEP:10.4f} W                 0     0.000",
        "    0",
    ]


def solar_lines() -> list[str]:
    return [
        "T F 2    2    3    0    0    0    0    0    0    0    0    0    0   0.0000.00000",
        "fFF  2   0   360.000  0.000000  0.0000000F F F F F               0.000     0.000     0.000     0.000         0",
        "    1    0    1    0    0    0    23.000     0.000     0.000     0.000     0.000",
        "     5.000     0.000    45.000  172          0.000    0     0.000",
        f"{WN_LOW:10.3f}{WN_HIGH:10.3f}{STEP:10.3f}{STEP:10.3f} W        W1         0     0.000",
        "    0",
    ]


def flux_lines() -> list[str]:
    # Matches the locally installed official DisortScatter.ltn card layout.
    return [
        "T F 2    2    2    1    0    0    0    0    0    0    0    0    0   0.000   0.40",
        "tFF  4   0   330.000  1.000000     1.0000F T                     0.000     0.000     0.000     0.000         0",
        "01_2009",
        "    1    0    1    0   18    0    23.000     0.000     0.000     0.000     0.000",
        "   0.000   0.000   0.000",
        "     5.000     0.000   180.000   0.00000     0.000     0.000    0          0.000     0.000",
        "    2    2    1    0",
        "     0.000    45.000     0.000     0.000     0.000     0.000     0.000     0.000",
        f"{WN_LOW:10.3f}{WN_HIGH:10.3f}{STEP:10.3f}{2.0:10.3f}RN              T    0     0.000",
        "    0",
    ]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--output-root", type=Path,
                    default=Path("logs/p11/modtran/pilot"))
    args = ap.parse_args()
    root = args.output_root.resolve()
    root.mkdir(parents=True, exist_ok=True)
    cases = [
        ("SWIR_pilot_trans", "Transmittance", los_lines(0), "tau_los"),
        ("SWIR_pilot_thermal", "ThermalRadiance", los_lines(1), "path_thermal"),
        ("SWIR_pilot_scattering", "RadianceWithScattering", scattering_lines(), "path_solar_scattering"),
        ("SWIR_pilot_solar", "DirectSolarIrradiance", solar_lines(), "target_direct_solar"),
        ("SWIR_pilot_flux", "SpectralFlux", flux_lines(), "target_downward_sky_flux"),
    ]
    rows: list[dict[str, object]] = []
    for case_id, mode, lines, purpose in cases:
        case_dir = root / case_id
        case_dir.mkdir(parents=True, exist_ok=True)
        path = case_dir / f"{case_id}.tp5"
        path.write_text("\n".join(lines) + "\n", encoding="ascii")
        rows.append({
            "scenario_id": "SWIR_MLS_Rural_vis23_obs10_tar5_rng10_sza45",
            "case_id": case_id,
            "mode": mode,
            "band": "SWIR",
            "wavelength_low_um": "1.1",
            "wavelength_high_um": "2.5",
            "wavenumber_low_cm1": f"{WN_LOW:.4f}",
            "wavenumber_high_cm1": f"{WN_HIGH:.4f}",
            "wavenumber_increment_cm1": f"{STEP:.4f}",
            "response_mode": "RectangularBandReference",
            "atmosphere_model": "Mid-Latitude Summer",
            "aerosol_model": "Rural",
            "humidity_profile": "default",
            "visibility_km": "23",
            "observer_alt_km": "10" if mode not in {"DirectSolarIrradiance", "SpectralFlux"} else "",
            "target_alt_km": "5",
            "range_km": "10" if mode not in {"DirectSolarIrradiance", "SpectralFlux"} else "",
            "solar_zenith_deg": "45" if mode in {"RadianceWithScattering", "DirectSolarIrradiance", "SpectralFlux"} else "",
            "purpose": purpose,
            "input_file": str(path),
            "input_sha256": sha256(path),
        })
    fields = list(rows[0])
    with (root / "case_manifest.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)
    (root / "case_manifest.json").write_text(
        json.dumps(rows, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Generated {len(rows)} component runs for one SWIR pilot scenario under {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
