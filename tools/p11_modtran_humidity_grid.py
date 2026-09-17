#!/usr/bin/env python3
"""Generate isolated P11 real-MODTRAN SWIR/MWIR humidity cases.

The three humidity profiles intentionally describe a scaled Mid-Latitude
Summer water column whose *surface* RH is targeted to 30, 60, or 85 percent.
They do not claim that the full vertical profile has constant RH.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path


MLS_SURFACE_RH_PERCENT = 76.18
HUMIDITIES_PERCENT = (30.0, 60.0, 85.0)
ALTITUDES_KM = (0.001, 1.0)
RANGES_KM = (0.1, 0.5, 1.0)
VISIBILITIES_KM = (6.0, 23.0)
SOLAR_ZENITH_DEG = (20.0, 45.0, 70.0)
PILOT_ALTITUDE_KM = 0.001
PILOT_RANGE_KM = 0.5
PILOT_VISIBILITY_KM = 23.0
PILOT_SZA_DEG = 45.0

BANDS = {
    # Retain one real native sample beyond each response endpoint so exact
    # endpoint values are interpolated, never extrapolated.
    "SWIR": {"lo_um": 1.1, "hi_um": 2.5, "wn_lo": 4000.0, "wn_hi": 9090.9091},
    "MWIR": {"lo_um": 3.0, "hi_um": 5.0, "wn_lo": 1999.0, "wn_hi": 3334.0},
}
WN_STEP_CM1 = 1.0

CARD1A_BASE = "fFF  2   0   360.000  0.000000  0.0000000F F F F F               0.000     0.000     0.000     0.000         0"
CARD1A_FLUX_BASE = "tFF  4   0   330.000  1.000000     1.0000F T                     0.000     0.000     0.000     0.000         0"


def token(value: float) -> str:
    return f"{value:g}".replace(".", "p")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def humidity_profile(rh_percent: float) -> str:
    return f"scaled_mls_surface_rh{rh_percent:g}"


def card1a(rh_percent: float, *, flux: bool = False) -> str:
    """Patch the documented fixed-width CARD1A fields and verify round-trip."""
    scale = rh_percent / MLS_SURFACE_RH_PERCENT
    chars = list(CARD1A_FLUX_BASE if flux else CARD1A_BASE)
    if len(chars) != 110:
        raise AssertionError("CARD1A template is not exactly 110 columns")
    chars[20:30] = f"{scale:10.6f}"
    chars[47] = "T"  # H2OAER: update aerosol optical properties with RH.
    chars[90:100] = f"{rh_percent:10.3f}"  # AERRH: boundary-layer aerosol RH.
    line = "".join(chars)
    if len(line) != 110:
        raise AssertionError("CARD1A patch changed record width")
    if abs(float(line[20:30]) - scale) > 5.1e-7:
        raise AssertionError("H2OSTR did not round-trip from CARD1A columns 21--30")
    if line[47] != "T":
        raise AssertionError("H2OAER is not T in CARD1A column 48")
    if abs(float(line[90:100]) - rh_percent) > 5.1e-4:
        raise AssertionError("AERRH did not round-trip from CARD1A columns 91--100")
    return line


def spectral_card(band: str, *, flux: bool = False) -> str:
    cfg = BANDS[band]
    if flux:
        return (f"{cfg['wn_lo']:10.3f}{cfg['wn_hi']:10.3f}"
                f"{WN_STEP_CM1:10.3f}{2.0:10.3f}RN              T    0     0.000")
    return (f"{cfg['wn_lo']:10.4f}{cfg['wn_hi']:10.4f}"
            f"{WN_STEP_CM1:10.4f}{WN_STEP_CM1:10.4f} W                 0     0.000")


def los_lines(band: str, iemsct: int, altitude_km: float, range_km: float,
              visibility_km: float, rh_percent: float) -> list[str]:
    return [
        f"T F 2    2    {iemsct:d}    0    0    0    0    0    0    0    0    0    0   0.0000.00000",
        card1a(rh_percent),
        f"    1    0    1    0    0    0{visibility_km:10.3f}     0.000     0.000     0.000     0.000",
        f"{altitude_km:10.6f}{altitude_km:10.6f}{0.0:10.6f}{range_km:10.5f}  0.000000  0.000000    0       0.000000  0.000000",
        spectral_card(band),
        "    0",
    ]


def scattering_lines(band: str, altitude_km: float, range_km: float,
                     visibility_km: float, sza: float, rh_percent: float) -> list[str]:
    return los_lines(band, 2, altitude_km, range_km, visibility_km, rh_percent)[:4] + [
        "    2    0  172    0",
        f"    90.000 {sza:9.3f}     0.000     0.000     0.000     0.000     0.000     0.000",
        spectral_card(band),
        "    0",
    ]


def solar_lines(band: str, altitude_km: float, visibility_km: float,
                sza: float, rh_percent: float) -> list[str]:
    cfg = BANDS[band]
    return [
        "T F 2    2    3    0    0    0    0    0    0    0    0    0    0   0.0000.00000",
        card1a(rh_percent),
        f"    1    0    1    0    0    0{visibility_km:10.3f}     0.000     0.000     0.000     0.000",
        f"{altitude_km:10.3f}     0.000{sza:10.3f}  172          0.000    0     0.000",
        (f"{cfg['wn_lo']:10.3f}{cfg['wn_hi']:10.3f}{WN_STEP_CM1:10.3f}"
         f"{WN_STEP_CM1:10.3f} W        W1         0     0.000"),
        "    0",
    ]


def flux_lines(band: str, altitude_km: float, visibility_km: float,
               sza: float, rh_percent: float) -> list[str]:
    return [
        "T F 2    2    2    1    0    0    0    0    0    0    0    0    0   0.000   0.40",
        card1a(rh_percent, flux=True),
        "01_2009",
        f"    1    0    1    0   18    0{visibility_km:10.3f}     0.000     0.000     0.000     0.000",
        "   0.000   0.000   0.000",
        f"{altitude_km:10.3f}     0.000   180.000   0.00000     0.000     0.000    0          0.000     0.000",
        "    2    2    1    0",
        f"     0.000 {sza:9.3f}     0.000     0.000     0.000     0.000     0.000     0.000",
        spectral_card(band, flux=True),
        "    0",
    ]


def build_specs(mode: str) -> list[dict[str, object]]:
    specs: list[dict[str, object]] = []
    if mode == "pilot":
        geometry = ((PILOT_ALTITUDE_KM, PILOT_RANGE_KM,
                     PILOT_VISIBILITY_KM, PILOT_SZA_DEG),)
    else:
        geometry = tuple(
            (altitude, range_km, visibility, sza)
            for altitude in ALTITUDES_KM
            for range_km in RANGES_KM
            for visibility in VISIBILITIES_KM
            for sza in SOLAR_ZENITH_DEG
        )

    for band in BANDS:
        for rh in HUMIDITIES_PERCENT:
            profile = humidity_profile(rh)
            # Deduplicate components that do not depend on every final vertex
            # axis. This is 5 cases/profile/band for pilot and 84 for grid.
            trans_thermal_seen: set[tuple[float, float, float]] = set()
            solar_flux_seen: set[tuple[float, float, float]] = set()
            for altitude, range_km, visibility, sza in geometry:
                suffix = (f"rh{token(rh)}_obs{token(altitude)}_tar{token(altitude)}_"
                          f"rng{token(range_km)}_vis{token(visibility)}")
                los_key = (altitude, range_km, visibility)
                if los_key not in trans_thermal_seen:
                    trans_thermal_seen.add(los_key)
                    specs.extend([
                        {"case_id": f"{band}_humidity_trans_{suffix}", "mode": "Transmittance",
                         "lines": los_lines(band, 0, altitude, range_km, visibility, rh),
                         "purpose": "tau_los", "band": band, "rh": rh, "profile": profile,
                         "altitude": altitude, "range": range_km, "visibility": visibility, "sza": None},
                        {"case_id": f"{band}_humidity_thermal_{suffix}", "mode": "ThermalRadiance",
                         "lines": los_lines(band, 1, altitude, range_km, visibility, rh),
                         "purpose": "path_thermal", "band": band, "rh": rh, "profile": profile,
                         "altitude": altitude, "range": range_km, "visibility": visibility, "sza": None},
                    ])
                specs.append({
                    "case_id": f"{band}_humidity_scattering_{suffix}_sza{token(sza)}",
                    "mode": "RadianceWithScattering",
                    "lines": scattering_lines(band, altitude, range_km, visibility, sza, rh),
                    "purpose": "los_path_solar_scattering", "band": band, "rh": rh,
                    "profile": profile, "altitude": altitude, "range": range_km,
                    "visibility": visibility, "sza": sza,
                })
                solar_key = (altitude, visibility, sza)
                if solar_key not in solar_flux_seen:
                    solar_flux_seen.add(solar_key)
                    target_suffix = (f"rh{token(rh)}_tar{token(altitude)}_"
                                     f"vis{token(visibility)}_sza{token(sza)}")
                    specs.extend([
                        {"case_id": f"{band}_humidity_solar_{target_suffix}",
                         "mode": "DirectSolarIrradiance",
                         "lines": solar_lines(band, altitude, visibility, sza, rh),
                         "purpose": "target_direct_solar", "band": band, "rh": rh,
                         "profile": profile, "altitude": altitude, "range": None,
                         "visibility": visibility, "sza": sza},
                        {"case_id": f"{band}_humidity_flux_{target_suffix}",
                         "mode": "SpectralFlux",
                         "lines": flux_lines(band, altitude, visibility, sza, rh),
                         "purpose": "target_downward_sky_flux", "band": band, "rh": rh,
                         "profile": profile, "altitude": altitude, "range": None,
                         "visibility": visibility, "sza": sza},
                    ])
    return specs


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--mode", choices=("pilot", "grid"), default="pilot")
    ap.add_argument("--output-root", type=Path)
    args = ap.parse_args()
    default_root = Path(f"logs/p11/modtran/humidity_{args.mode}")
    root = (args.output_root or default_root).resolve()
    root.mkdir(parents=True, exist_ok=True)
    specs = build_specs(args.mode)
    rows: list[dict[str, object]] = []
    for spec in specs:
        case_id = str(spec["case_id"])
        case_dir = root / case_id
        case_dir.mkdir(parents=True, exist_ok=True)
        input_path = case_dir / f"{case_id}.tp5"
        lines = list(spec.pop("lines"))
        input_path.write_text("\n".join(lines) + "\n", encoding="ascii")
        band = str(spec["band"])
        cfg = BANDS[band]
        altitude = float(spec["altitude"])
        range_km = spec["range"]
        rh = float(spec["rh"])
        rows.append({
            "case_id": case_id, "mode": spec["mode"], "band": band,
            "atmosphere_model": "Mid-Latitude Summer", "aerosol_model": "Rural",
            "humidity_profile": spec["profile"],
            "humidity_semantics": "scaled MLS water column targeting stated surface RH; not constant RH aloft",
            "mls_reference_surface_rh_percent": f"{MLS_SURFACE_RH_PERCENT:.2f}",
            "target_surface_rh_percent": f"{rh:g}",
            "h2ostr_scale": f"{rh / MLS_SURFACE_RH_PERCENT:.9f}",
            "h2oaer": "T", "aerrh_percent": f"{rh:g}",
            "visibility_km": f"{float(spec['visibility']):g}",
            "observer_alt_km": f"{altitude:g}" if range_km is not None else "",
            "target_alt_km": f"{altitude:g}",
            "range_km": "" if range_km is None else f"{float(range_km):g}",
            "solar_zenith_deg": "" if spec["sza"] is None else f"{float(spec['sza']):g}",
            "wavelength_low_um": f"{cfg['lo_um']:g}",
            "wavelength_high_um": f"{cfg['hi_um']:g}",
            "wavenumber_low_cm1": f"{cfg['wn_lo']:.4f}",
            "wavenumber_high_cm1": f"{cfg['wn_hi']:.4f}",
            "wavenumber_increment_cm1": f"{WN_STEP_CM1:.4f}",
            "response_mode": "RectangularBand", "purpose": spec["purpose"],
            "input_file": str(input_path), "input_sha256": sha256(input_path),
        })
    with (root / "case_manifest.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (root / "case_manifest.json").write_text(
        json.dumps(rows, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    expected = 30 if args.mode == "pilot" else 504
    if len(rows) != expected:
        raise AssertionError(f"generated {len(rows)} cases, expected {expected}")
    provenance = {
        "mode": args.mode, "component_run_count": len(rows),
        "bands": list(BANDS), "humidity_profiles": [humidity_profile(x) for x in HUMIDITIES_PERCENT],
        "mls_reference_surface_rh_percent": MLS_SURFACE_RH_PERCENT,
        "formal_vertices_if_grid": 216 if args.mode == "grid" else 6,
        "card1a_fields": {"H2OSTR": "columns 21-30", "H2OAER": "column 48", "AERRH": "columns 91-100"},
        "generator": str(Path(__file__).resolve()), "generator_sha256": sha256(Path(__file__).resolve()),
    }
    (root / "generation_provenance.json").write_text(
        json.dumps(provenance, indent=2) + "\n", encoding="utf-8")
    print(f"Generated {len(rows)} isolated humidity component runs under {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
