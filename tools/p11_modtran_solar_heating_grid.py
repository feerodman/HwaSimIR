#!/usr/bin/env python3
"""Generate real 0.30--2.50 um P11 solar-heating cases for civil altitudes."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path

import p11_modtran_humidity_grid as humidity


WN_LOW = 3990.0
WN_HIGH = 33340.0
WN_STEP = 10.0
ALTITUDES_KM = (0.001, 1.0)
VISIBILITIES_KM = (6.0, 23.0)
SZAS_DEG = (20.0, 45.0, 70.0)
PROFILES: tuple[tuple[str, float | None], ...] = (
    ("default", None),
    ("scaled_mls_surface_rh30", 30.0),
    ("scaled_mls_surface_rh60", 60.0),
    ("scaled_mls_surface_rh85", 85.0),
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def token(value: float) -> str:
    return f"{value:g}".replace(".", "p")


def profile_card(profile: str, rh: float | None, *, flux: bool) -> str:
    if profile == "default":
        return humidity.CARD1A_FLUX_BASE if flux else humidity.CARD1A_BASE
    if rh is None:
        raise AssertionError("explicit humidity profile lacks RH")
    return humidity.card1a(rh, flux=flux)


def direct_lines(altitude: float, visibility: float, sza: float,
                 profile: str, rh: float | None) -> list[str]:
    return [
        "T F 2    2    3    0    0    0    0    0    0    0    0    0    0   0.0000.00000",
        profile_card(profile, rh, flux=False),
        f"    1    0    1    0    0    0{visibility:10.3f}     0.000     0.000     0.000     0.000",
        f"{altitude:10.3f}     0.000{sza:10.3f}  172          0.000    0     0.000",
        f"{WN_LOW:10.3f}{WN_HIGH:10.3f}{WN_STEP:10.3f}{WN_STEP:10.3f} W        W1         0     0.000",
        "    0",
    ]


def flux_lines(altitude: float, visibility: float, sza: float,
               profile: str, rh: float | None) -> list[str]:
    return [
        "T F 2    2    2    1    0    0    0    0    0    0    0    0    0   0.000   0.40",
        profile_card(profile, rh, flux=True),
        "01_2009",
        f"    1    0    1    0   18    0{visibility:10.3f}     0.000     0.000     0.000     0.000",
        "   0.000   0.000   0.000",
        f"{altitude:10.3f}     0.000   180.000   0.00000     0.000     0.000    0          0.000     0.000",
        "    2    2    1    0",
        f"     0.000 {sza:9.3f}     0.000     0.000     0.000     0.000     0.000     0.000",
        f"{WN_LOW:10.3f}{WN_HIGH:10.3f}{WN_STEP:10.3f}{20.0:10.3f}RN              T    0     0.000",
        "    0",
    ]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--output-root", type=Path,
                    default=Path("logs/p11/modtran/solar_heating_ground_grid"))
    args = ap.parse_args()
    root = args.output_root.resolve()
    root.mkdir(parents=True, exist_ok=True)
    rows = []
    for profile, rh in PROFILES:
        profile_token = "default" if profile == "default" else f"rh{token(float(rh))}"
        for altitude in ALTITUDES_KM:
            for visibility in VISIBILITIES_KM:
                for sza in SZAS_DEG:
                    suffix = f"{profile_token}_tar{token(altitude)}_vis{token(visibility)}_sza{token(sza)}"
                    cases = (
                        (f"SWHEAT_P11_direct_{suffix}", "DirectSolarIrradiance",
                         direct_lines(altitude, visibility, sza, profile, rh), "target_direct_solar"),
                        (f"SWHEAT_P11_flux_{suffix}", "SpectralFlux",
                         flux_lines(altitude, visibility, sza, profile, rh), "target_downward_diffuse"),
                    )
                    for case_id, mode, lines, purpose in cases:
                        case_dir = root / case_id
                        case_dir.mkdir(parents=True, exist_ok=True)
                        path = case_dir / f"{case_id}.tp5"
                        path.write_text("\n".join(lines) + "\n", encoding="ascii")
                        card = lines[1]
                        rows.append({
                            "case_id": case_id, "mode": mode, "band": "SOLAR_SHORTWAVE",
                            "atmosphere_model": "Mid-Latitude Summer", "aerosol_model": "Rural",
                            "humidity_profile": profile,
                            "humidity_semantics": ("unmodified MODTRAN MLS default" if rh is None else
                                                   "scaled MLS water column targeting stated surface RH; not constant RH aloft"),
                            "mls_reference_surface_rh_percent": "" if rh is None else "76.18",
                            "target_surface_rh_percent": "" if rh is None else f"{rh:g}",
                            "h2ostr_card_value": card[20:30].strip(),
                            "h2oaer_card_value": card[47], "aerrh_card_value": card[90:100].strip(),
                            "visibility_km": f"{visibility:g}", "observer_alt_km": "",
                            "target_alt_km": f"{altitude:g}", "range_km": "",
                            "solar_zenith_deg": f"{sza:g}", "wavelength_low_um": "0.30",
                            "wavelength_high_um": "2.50", "wavenumber_low_cm1": f"{WN_LOW:g}",
                            "wavenumber_high_cm1": f"{WN_HIGH:g}",
                            "wavenumber_increment_cm1": f"{WN_STEP:g}",
                            "fwhm_cm1": f"{20 if mode == 'SpectralFlux' else WN_STEP:g}",
                            "response_mode": "BroadbandIntegral", "purpose": purpose,
                            "input_file": str(path), "input_sha256": sha256(path),
                        })
    if len(rows) != 96:
        raise AssertionError(f"expected 96 component cases, generated {len(rows)}")
    with (root / "case_manifest.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)
    (root / "case_manifest.json").write_text(
        json.dumps(rows, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    (root / "generation_provenance.json").write_text(json.dumps({
        "component_runs": 96, "formal_vertices": 48,
        "spectral_range_um": [0.30, 2.50], "native_wavenumber_cm1": [WN_LOW, WN_HIGH],
        "note": "3990--33340 cm^-1 supplies real samples beyond both exact response endpoints",
        "profiles": [p for p, _ in PROFILES],
        "axes": {"target_alt_km": ALTITUDES_KM, "visibility_km": VISIBILITIES_KM,
                 "solar_zenith_deg": SZAS_DEG},
        "generator": str(Path(__file__).resolve()), "generator_sha256": sha256(Path(__file__).resolve()),
    }, indent=2) + "\n", encoding="utf-8")
    print(f"Generated 96 real-MODTRAN components for 48 broadband solar-heating vertices under {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
