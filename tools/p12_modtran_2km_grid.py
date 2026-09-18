#!/usr/bin/env python3
"""Generate only the missing real-MODTRAN 2 km LOS component cards for P12."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import sys
from pathlib import Path


TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
import p11_modtran_humidity_grid as humidity  # noqa: E402


RANGE_KM = 2.0
ALTITUDES_KM = (0.001, 1.0)
VISIBILITIES_KM = (6.0, 23.0)
SOLAR_ZENITH_DEG = (20.0, 45.0, 70.0)
DEFAULT_CARD1A = "fFF  2   0   360.000  0.000000  0.0000000F F F F F               0.000     0.000     0.000     0.000         0"
PROFILES: tuple[tuple[str, float | None], ...] = (
    ("default", None),
    (humidity.humidity_profile(30.0), 30.0),
    (humidity.humidity_profile(60.0), 60.0),
    (humidity.humidity_profile(85.0), 85.0),
)


def token(value: float) -> str:
    return f"{value:g}".replace(".", "p")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def card1a(rh_percent: float | None) -> str:
    return DEFAULT_CARD1A if rh_percent is None else humidity.card1a(rh_percent)


def los_lines(band: str, iemsct: int, altitude: float, visibility: float,
              rh_percent: float | None) -> list[str]:
    return [
        f"T F 2    2    {iemsct:d}    0    0    0    0    0    0    0    0    0    0   0.0000.00000",
        card1a(rh_percent),
        f"    1    0    1    0    0    0{visibility:10.3f}     0.000     0.000     0.000     0.000",
        f"{altitude:10.6f}{altitude:10.6f}{0.0:10.6f}{RANGE_KM:10.5f}  0.000000  0.000000    0       0.000000  0.000000",
        humidity.spectral_card(band),
        "    0",
    ]


def scattering_lines(band: str, altitude: float, visibility: float, sza: float,
                     rh_percent: float | None) -> list[str]:
    return los_lines(band, 2, altitude, visibility, rh_percent)[:4] + [
        "    2    0  172    0",
        f"    90.000 {sza:9.3f}     0.000     0.000     0.000     0.000     0.000     0.000",
        humidity.spectral_card(band),
        "    0",
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-root", type=Path,
                        default=Path("logs/p12/p12c/modtran_2km"))
    args = parser.parse_args()
    root = args.output_root.resolve()
    root.mkdir(parents=True, exist_ok=True)
    rows: list[dict[str, str]] = []

    for band, (profile, rh), altitude, visibility in (
        (band, profile, altitude, visibility)
        for band in humidity.BANDS
        for profile in PROFILES
        for altitude in ALTITUDES_KM
        for visibility in VISIBILITIES_KM
    ):
        rh_token = "default" if rh is None else f"rh{token(rh)}"
        suffix = (f"{rh_token}_obs{token(altitude)}_tar{token(altitude)}_"
                  f"rng{token(RANGE_KM)}_vis{token(visibility)}")
        specs = [
            (f"P12_{band}_trans_{suffix}", "Transmittance",
             los_lines(band, 0, altitude, visibility, rh), "tau_los", None),
            (f"P12_{band}_thermal_{suffix}", "ThermalRadiance",
             los_lines(band, 1, altitude, visibility, rh), "path_thermal", None),
        ]
        specs.extend(
            (f"P12_{band}_scattering_{suffix}_sza{token(sza)}", "RadianceWithScattering",
             scattering_lines(band, altitude, visibility, sza, rh),
             "los_path_solar_scattering", sza)
            for sza in SOLAR_ZENITH_DEG
        )
        for case_id, mode, lines, purpose, sza in specs:
            case_dir = root / case_id
            case_dir.mkdir(parents=True, exist_ok=True)
            input_path = case_dir / f"{case_id}.tp5"
            input_path.write_text("\n".join(lines) + "\n", encoding="ascii")
            cfg = humidity.BANDS[band]
            rows.append({
                "case_id": case_id, "mode": mode, "band": band,
                "atmosphere_model": "Mid-Latitude Summer", "aerosol_model": "Rural",
                "humidity_profile": profile,
                "humidity_semantics": ("unmodified MLS default" if rh is None else
                                       "scaled MLS water column targeting stated surface RH; not constant RH aloft"),
                "target_surface_rh_percent": "" if rh is None else f"{rh:g}",
                "h2ostr_scale": "" if rh is None else f"{rh / humidity.MLS_SURFACE_RH_PERCENT:.9f}",
                "h2oaer": "" if rh is None else "T", "aerrh_percent": "" if rh is None else f"{rh:g}",
                "visibility_km": f"{visibility:g}", "observer_alt_km": f"{altitude:g}",
                "target_alt_km": f"{altitude:g}", "range_km": f"{RANGE_KM:g}",
                "solar_zenith_deg": "" if sza is None else f"{sza:g}",
                "wavelength_low_um": f"{cfg['lo_um']:g}", "wavelength_high_um": f"{cfg['hi_um']:g}",
                "wavenumber_low_cm1": f"{cfg['wn_lo']:.4f}", "wavenumber_high_cm1": f"{cfg['wn_hi']:.4f}",
                "wavenumber_increment_cm1": f"{humidity.WN_STEP_CM1:.4f}",
                "response_mode": "RectangularBand", "purpose": purpose,
                "input_file": str(input_path), "input_sha256": sha256(input_path),
            })

    if len(rows) != 160:
        raise AssertionError(f"expected 160 missing LOS component runs, got {len(rows)}")
    with (root / "case_manifest.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader(); writer.writerows(rows)
    (root / "case_manifest.json").write_text(
        json.dumps(rows, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    provenance = {
        "schema": "HwaSimIR.P12.Modtran2kmGeneration.1",
        "scope": "missing LOS components only; existing target-only solar and sky components are reused by exact environment key",
        "componentRuns": len(rows), "formalVerticesAfterAssembly": 96,
        "bands": list(humidity.BANDS), "rangeKm": RANGE_KM,
        "altitudesKm": ALTITUDES_KM, "visibilitiesKm": VISIBILITIES_KM,
        "solarZenithDeg": SOLAR_ZENITH_DEG, "profiles": [item[0] for item in PROFILES],
        "generator": str(Path(__file__).resolve()), "generatorSha256": sha256(Path(__file__).resolve()),
    }
    (root / "generation_provenance.json").write_text(
        json.dumps(provenance, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(provenance, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
