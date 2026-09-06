#!/usr/bin/env python3
"""Generate auditable MODTRAN5 reference and M1 NIR runtime-grid cases.

The M1 grid deliberately reuses the already-audited NIR/MWIR transmittance and
MWIR thermal spectra.  It generates only the missing NIR solar-zenith cases:
LOS solar scattering, target direct solar (SOL TR), and downward diffuse flux.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path


DEFAULT_ROOT = Path("HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/raw/audit_mwir_standard_20260906")
M1_ROOT = Path("HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/raw/m1_nir_mwir_20260906")
ALTITUDE_PAIRS = [
    (3.0, 3.0), (5.0, 5.0), (10.0, 10.0), (15.0, 15.0), (20.0, 20.0),
    (5.0, 3.0), (10.0, 5.0), (15.0, 10.0), (20.0, 10.0),
    (20.0, 15.0), (10.0, 3.0), (20.0, 3.0),
]
RANGES_KM = [1.0, 2.0, 5.0, 10.0, 20.0, 35.0, 50.0]
VISIBILITIES_KM = [2.0, 5.0, 10.0, 23.0, 50.0]
NIR_SOLAR_ZENITH_DEG = [20.0, 45.0, 70.0]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def base_lines(iemsct: int, h1: float, h2: float, range_km: float, visibility_km: float = 23.0) -> list[str]:
    return [
        f"T F 2    2    {iemsct:d}    0    0    0    0    0    0    0    0    0    0   0.0000.00000",
        "fFF  2   0   360.000  0.000000  0.0000000F F F F F               0.000     0.000     0.000     0.000         0",
        f"    1    0    1    0    0    0{visibility_km:10.3f}     0.000     0.000     0.000     0.000",
        f"{h1:10.6f}{h2:10.6f}{0.0:10.6f}{range_km:10.5f}  0.000000  0.000000    0       0.000000  0.000000",
        " 2000.000003333.33330   1.00000   1.00000 W                 0     0.000",
        "    0",
    ]


def solar_lines(target_alt_km: float, sza_deg: float, visibility_km: float = 23.0) -> list[str]:
    return [
        "T F 2    2    3    0    0    0    0    0    0    0    0    0    0   0.0000.00000",
        "fFF  2   0   360.000  0.000000  0.0000000F F F F F               0.000     0.000     0.000     0.000         0",
        f"    1    0    1    0    0    0{visibility_km:10.3f}     0.000     0.000     0.000     0.000",
        f"{target_alt_km:10.3f}     0.000{sza_deg:10.3f}  172          0.000    0     0.000",
        "  2000.000  3333.330     1.000     1.000 W        W1         0     0.000",
        "    0",
    ]


def scattering_lines(h1: float, h2: float, range_km: float, sza_deg: float,
                     visibility_km: float = 23.0, band: str = "MWIR",
                     increment_cm1: float = 1.0) -> list[str]:
    lines = base_lines(2, h1, h2, range_km, visibility_km)[:4]
    wn_low, wn_high = ((2000.0, 3333.3333) if band == "MWIR" else (9090.9091, 14285.7143))
    lines += [
        "    2    0  172    0",
        f"    90.000 {sza_deg:9.3f}     0.000     0.000     0.000     0.000     0.000     0.000",
        f"{wn_low:10.4f}{wn_high:10.4f}{increment_cm1:10.4f}{increment_cm1:10.4f} W                 0     0.000",
        "    0",
    ]
    return lines


def flux_lines(target_alt_km: float, sza_deg: float, visibility_km: float = 23.0,
               band: str = "MWIR", increment_cm1: float = 1.0) -> list[str]:
    # Card layout follows the locally installed official DisortScatter.ltn.
    # IMULT=1 and the Card-4 FLAGS spectral-flux flag request the .flx table.
    return [
        "T F 2    2    2    1    0    0    0    0    0    0    0    0    0   0.000   0.40",
        "tFF  4   0   330.000  1.000000     1.0000F T                     0.000     0.000     0.000     0.000         0",
        "01_2009",
        f"    1    0    1    0   18    0{visibility_km:10.3f}     0.000     0.000     0.000     0.000",
        "   0.000   0.000   0.000",
        f"{target_alt_km:10.3f}     0.000   180.000   0.00000     0.000     0.000    0          0.000     0.000",
        "    2    2    1    0",
        f"     0.000 {sza_deg:9.3f}     0.000     0.000     0.000     0.000     0.000     0.000",
        ((f"{2000.0:10.3f}{3333.3333:10.3f}{increment_cm1:10.3f}{max(2.0, increment_cm1):10.3f}RN              T    0     0.000")
         if band == "MWIR" else
         (f"{9090.9091:10.3f}{14285.7143:10.3f}{increment_cm1:10.3f}{max(2.0, increment_cm1):10.3f}RN              T    0     0.000")),
        "    0",
    ]


def solar_lines_for_band(target_alt_km: float, sza_deg: float, visibility_km: float,
                         band: str, increment_cm1: float) -> list[str]:
    if band == "MWIR" and increment_cm1 == 1.0:
        return solar_lines(target_alt_km, sza_deg, visibility_km)
    wn_low, wn_high = ((2000.0, 3333.3333) if band == "MWIR" else (9090.9091, 14285.7143))
    return [
        "T F 2    2    3    0    0    0    0    0    0    0    0    0    0   0.0000.00000",
        "fFF  2   0   360.000  0.000000  0.0000000F F F F F               0.000     0.000     0.000     0.000         0",
        f"    1    0    1    0    0    0{visibility_km:10.3f}     0.000     0.000     0.000     0.000",
        f"{target_alt_km:10.3f}     0.000{sza_deg:10.3f}  172          0.000    0     0.000",
        f"{wn_low:10.3f}{wn_high:10.3f}{increment_cm1:10.3f}{increment_cm1:10.3f} W        W1         0     0.000",
        "    0",
    ]


def write_case(case_root: Path, case_id: str, mode: str, lines: list[str],
               observer_alt_km: float | None, target_alt_km: float,
               range_km: float | None, visibility_km: float,
               solar_zenith_deg: float) -> dict[str, object]:
    case_dir = case_root / case_id
    case_dir.mkdir(parents=True, exist_ok=True)
    input_path = case_dir / f"{case_id}.tp5"
    input_path.write_text("\n".join(lines) + "\n", encoding="ascii")
    return {
        "case_id": case_id,
        "mode": mode,
        "band": "NIR",
        "atmosphere_model": "Mid-Latitude Summer",
        "aerosol_model": "Rural",
        "humidity_profile": "default",
        "visibility_km": f"{visibility_km:g}",
        "observer_alt_km": "" if observer_alt_km is None else f"{observer_alt_km:g}",
        "target_alt_km": f"{target_alt_km:g}",
        "range_km": "" if range_km is None else f"{range_km:g}",
        "solar_zenith_deg": f"{solar_zenith_deg:g}",
        "wavelength_low_um": "0.70",
        "wavelength_high_um": "1.10",
        "wavenumber_increment_cm1": "10",
        "fwhm_cm1": "10",
        "response_mode": "RectangularBand",
        "input_file": str(input_path.resolve()),
        "input_sha256": sha256(input_path),
    }


def generate_m1_nir_grid(root: Path) -> Path:
    root.mkdir(parents=True, exist_ok=True)
    rows: list[dict[str, object]] = []
    increment = 10.0
    for observer, target in ALTITUDE_PAIRS:
        for range_km in RANGES_KM:
            if range_km + 1.0e-9 < abs(observer - target):
                continue
            for visibility in VISIBILITIES_KM:
                # The existing audited production table already owns SZA=45.
                # Generate the two missing planes only, avoiding duplicate raw data.
                for sza in (20.0, 70.0):
                    case_id = (f"NIR_scattering_obs{observer:g}_tar{target:g}_rng{range_km:g}_"
                               f"vis{visibility:g}_aerRural_humdefault_sza{sza:g}")
                    rows.append(write_case(
                        root, case_id, "RadianceWithScattering",
                        scattering_lines(observer, target, range_km, sza, visibility, "NIR", increment),
                        observer, target, range_km, visibility, sza))
    target_altitudes = sorted({target for _, target in ALTITUDE_PAIRS})
    for target in target_altitudes:
        for visibility in VISIBILITIES_KM:
            for sza in NIR_SOLAR_ZENITH_DEG:
                solar_id = f"NIR_solar_tar{target:g}_vis{visibility:g}_aerRural_humdefault_sza{sza:g}"
                rows.append(write_case(
                    root, solar_id, "DirectSolarIrradiance",
                    solar_lines_for_band(target, sza, visibility, "NIR", increment),
                    None, target, None, visibility, sza))
                flux_id = f"NIR_flux_tar{target:g}_vis{visibility:g}_aerRural_humdefault_sza{sza:g}"
                rows.append(write_case(
                    root, flux_id, "SpectralFlux",
                    flux_lines(target, sza, visibility, "NIR", increment),
                    None, target, None, visibility, sza))
    manifest = root / "case_manifest.csv"
    with manifest.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (root / "case_manifest.json").write_text(json.dumps(rows, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return manifest


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--output-root", type=Path, default=DEFAULT_ROOT)
    ap.add_argument("--include-qc-grid", action="store_true")
    ap.add_argument("--m1-nir-runtime-grid", action="store_true")
    args = ap.parse_args()
    if args.m1_nir_runtime_grid:
        root = args.output_root if args.output_root != DEFAULT_ROOT else M1_ROOT
        manifest = generate_m1_nir_grid(root)
        print(f"Generated M1 NIR cases under {root.resolve()}")
        print(manifest.resolve())
        return 0
    root = args.output_root
    root.mkdir(parents=True, exist_ok=True)

    cases = [
        ("mwir_std_trans", "Transmittance", base_lines(0, 10.0, 5.0, 10.0), "LOS direct transmittance"),
        ("mwir_std_thermal", "ThermalRadiance", base_lines(1, 10.0, 5.0, 10.0), "LOS path thermal emission"),
        ("mwir_std_solar_direct", "DirectSolarIrradiance", solar_lines(5.0, 45.0), "direct solar irradiance at target altitude"),
        ("mwir_std_solar_scatter", "RadianceWithScattering", scattering_lines(10.0, 5.0, 10.0, 45.0), "LOS solar-scattered radiance components"),
        ("mwir_std_flux", "SpectralFlux", flux_lines(5.0, 45.0), "downward diffuse/thermal and direct solar spectral flux at target altitude"),
    ]
    rows = []
    for case_id, mode, lines, purpose in cases:
        case_dir = root / case_id
        case_dir.mkdir(parents=True, exist_ok=True)
        input_path = case_dir / f"{case_id}.tp5"
        input_path.write_text("\n".join(lines) + "\n", encoding="ascii")
        rows.append({
            "case_id": case_id,
            "mode": mode,
            "band": "MWIR",
            "atmosphere_model": "Mid-Latitude Summer",
            "aerosol_model": "Rural",
            "humidity_profile": "default",
            "visibility_km": "23",
            "observer_alt_km": "10" if mode not in {"DirectSolarIrradiance", "SpectralFlux"} else "",
            "target_alt_km": "5",
            "range_km": "10" if mode not in {"DirectSolarIrradiance", "SpectralFlux"} else "",
            "solar_zenith_deg": "45" if mode in {"DirectSolarIrradiance", "RadianceWithScattering", "SpectralFlux"} else "",
            "raw_unit": "W/(cm^2 sr cm^-1) or W/(cm^2 cm^-1); tau dimensionless",
            "si_unit": "W/(m^2 sr um) or W/(m^2 um); tau dimensionless",
            "conversion_method": "Jacobian 1e8/lambda_um^2; no empirical scale",
            "purpose": purpose,
            "input_file": str(input_path.resolve()),
            "input_sha256": sha256(input_path),
        })
    manifest = root / "case_manifest.csv"
    with manifest.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (root / "case_manifest.json").write_text(json.dumps(rows, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    if args.include_qc_grid:
        qc_specs: list[tuple[str, str, list[str], float, float, float, float, float | None]] = []
        for range_km in [1.0, 10.0, 30.0, 50.0]:
            qc_specs.append((f"qc_range_{range_km:g}_trans", "Transmittance", base_lines(0, 5.0, 5.0, range_km), 23.0, 5.0, 5.0, range_km, None))
            qc_specs.append((f"qc_range_{range_km:g}_thermal", "ThermalRadiance", base_lines(1, 5.0, 5.0, range_km), 23.0, 5.0, 5.0, range_km, None))
        for visibility in [2.0, 5.0, 10.0, 23.0, 50.0]:
            qc_specs.append((f"qc_vis_{visibility:g}_trans", "Transmittance", base_lines(0, 1.0, 1.0, 10.0, visibility), visibility, 1.0, 1.0, 10.0, None))
            qc_specs.append((f"qc_vis_{visibility:g}_thermal", "ThermalRadiance", base_lines(1, 1.0, 1.0, 10.0, visibility), visibility, 1.0, 1.0, 10.0, None))
        for sza in [0.0, 30.0, 45.0, 60.0, 75.0]:
            qc_specs.append((f"qc_sza_{sza:g}_solar", "DirectSolarIrradiance", solar_lines(5.0, sza), 23.0, 0.0, 5.0, 0.0, sza))
        qc_rows = []
        for case_id, mode, lines, visibility, observer, target, range_km, sza in qc_specs:
            case_dir = root / "qc_grid" / case_id
            case_dir.mkdir(parents=True, exist_ok=True)
            input_path = case_dir / f"{case_id}.tp5"
            input_path.write_text("\n".join(lines) + "\n", encoding="ascii")
            qc_rows.append({"case_id": case_id, "mode": mode, "visibility_km": visibility, "observer_alt_km": observer, "target_alt_km": target, "range_km": range_km, "solar_zenith_deg": "" if sza is None else sza, "input_file": str(input_path.resolve()), "input_sha256": sha256(input_path)})
        with (root / "qc_grid_manifest.csv").open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=list(qc_rows[0]))
            writer.writeheader()
            writer.writerows(qc_rows)
        print(f"Generated {len(qc_rows)} QC cases")
    print(f"Generated {len(rows)} cases under {root.resolve()}")
    print(manifest.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
