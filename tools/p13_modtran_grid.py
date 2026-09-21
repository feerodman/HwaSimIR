#!/usr/bin/env python3
"""Generate P13 real-MODTRAN SWIR/MWIR high-altitude slant-path cases.

The grid has two deliberately separate scopes:

* ``pilot`` is one real unequal-altitude vertex used to prove the complete
  five-component pipeline before a long batch is started.
* ``track50`` is the Cartesian cell that contains every geometry in the
  immutable DataDrivenTestQT/1.txt input and extends the *same* audited
  high-altitude cell to 50 km.  The original file itself reaches only about
  22.27 km; the 35/50 km vertices are a separate declared capability.

No value is synthesized here.  Every component is backed by an input deck for
the licensed MODTRAN 5.2.1 executable.  Target-only direct/sky products are
deduplicated because they do not depend on observer altitude or LOS range.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import itertools
import json
from pathlib import Path
import sys


TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
import p11_modtran_humidity_grid as p11  # noqa: E402


TRACK_OBSERVER_ALT_KM = (10.95, 12.0)
TRACK_TARGET_ALT_KM = (9.7, 10.05)
TRACK_RANGE_KM = (2.4, 5.0, 10.0, 20.0, 23.0, 35.0, 50.0)
TRACK_VISIBILITY_KM = (6.0,)
TRACK_SOLAR_ZENITH_DEG = (20.0, 45.0)
TRACK_HUMIDITY_PERCENT = (30.0, 60.0, 85.0)

# P14 extends the already-published 50 km high-altitude cell along the
# visibility axis.  Keeping this mode in the audited generator avoids a second
# implementation of the MODTRAN deck format; the P13 modes remain byte-for-byte
# unchanged.
P14_MIX_VISIBILITY_KM = (23.0,)

PILOT_OBSERVER_ALT_KM = (11.5,)
PILOT_TARGET_ALT_KM = (9.9,)
PILOT_RANGE_KM = (20.0,)
PILOT_VISIBILITY_KM = (6.0,)
PILOT_SOLAR_ZENITH_DEG = (45.0,)
PILOT_HUMIDITY_PERCENT = (85.0,)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def token(value: float) -> str:
    return f"{value:g}".replace(".", "p")


def los_lines(band: str, iemsct: int, observer_alt_km: float,
              target_alt_km: float, range_km: float,
              visibility_km: float, rh_percent: float) -> list[str]:
    if range_km + 1.0e-9 < abs(observer_alt_km - target_alt_km):
        raise ValueError(
            f"invalid slant geometry obs={observer_alt_km}, target={target_alt_km}, "
            f"range={range_km}")
    return [
        f"T F 2    2    {iemsct:d}    0    0    0    0    0    0    0    0    0    0   0.0000.00000",
        p11.card1a(rh_percent),
        f"    1    0    1    0    0    0{visibility_km:10.3f}     0.000     0.000     0.000     0.000",
        (f"{observer_alt_km:10.6f}{target_alt_km:10.6f}{0.0:10.6f}"
         f"{range_km:10.5f}  0.000000  0.000000    0       0.000000  0.000000"),
        p11.spectral_card(band),
        "    0",
    ]


def scattering_lines(band: str, observer_alt_km: float,
                     target_alt_km: float, range_km: float,
                     visibility_km: float, solar_zenith_deg: float,
                     rh_percent: float) -> list[str]:
    return los_lines(
        band, 2, observer_alt_km, target_alt_km, range_km,
        visibility_km, rh_percent)[:4] + [
        "    2    0  172    0",
        (f"    90.000 {solar_zenith_deg:9.3f}     0.000     0.000"
         "     0.000     0.000     0.000     0.000"),
        p11.spectral_card(band),
        "    0",
    ]


def axes(mode: str) -> dict[str, tuple[float, ...]]:
    if mode == "pilot":
        return {
            "observer": PILOT_OBSERVER_ALT_KM,
            "target": PILOT_TARGET_ALT_KM,
            "range": PILOT_RANGE_KM,
            "visibility": PILOT_VISIBILITY_KM,
            "sza": PILOT_SOLAR_ZENITH_DEG,
            "rh": PILOT_HUMIDITY_PERCENT,
        }
    # ``p14mix`` reuses the P13 high-altitude geometry/RH/SZA vertices and adds
    # the missing 23 km visibility face.  The existing vis=6 face is not rerun.
    return {
        "observer": TRACK_OBSERVER_ALT_KM,
        "target": TRACK_TARGET_ALT_KM,
        "range": TRACK_RANGE_KM,
        "visibility": P14_MIX_VISIBILITY_KM if mode == "p14mix" else TRACK_VISIBILITY_KM,
        "sza": TRACK_SOLAR_ZENITH_DEG,
        "rh": TRACK_HUMIDITY_PERCENT,
    }


def expected_counts(mode: str) -> tuple[int, int]:
    a = axes(mode)
    bands = len(p11.BANDS)
    profiles = len(a["rh"])
    los_vertices = (len(a["observer"]) * len(a["target"]) *
                    len(a["range"]) * len(a["visibility"]))
    target_vertices = len(a["target"]) * len(a["visibility"]) * len(a["sza"])
    components = bands * profiles * (
        2 * los_vertices + len(a["sza"]) * los_vertices + 2 * target_vertices)
    formal = bands * profiles * los_vertices * len(a["sza"])
    return components, formal


def add_case(rows: list[dict[str, str]], root: Path, *, case_id: str,
             mode: str, band: str, rh: float, observer: float | None,
             target: float, range_km: float | None, visibility: float,
             sza: float | None, purpose: str, lines: list[str]) -> None:
    case_dir = root / case_id
    case_dir.mkdir(parents=True, exist_ok=True)
    input_path = case_dir / f"{case_id}.tp5"
    input_path.write_text("\n".join(lines) + "\n", encoding="ascii")
    cfg = p11.BANDS[band]
    rows.append({
        "case_id": case_id,
        "mode": mode,
        "band": band,
        "atmosphere_model": "Mid-Latitude Summer",
        "aerosol_model": "Rural",
        "humidity_profile": p11.humidity_profile(rh),
        "humidity_semantics": (
            "scaled MLS water column targeting stated surface RH; "
            "not constant RH aloft"),
        "mls_reference_surface_rh_percent": f"{p11.MLS_SURFACE_RH_PERCENT:.2f}",
        "target_surface_rh_percent": f"{rh:g}",
        "h2ostr_scale": f"{rh / p11.MLS_SURFACE_RH_PERCENT:.9f}",
        "h2oaer": "T",
        "aerrh_percent": f"{rh:g}",
        "visibility_km": f"{visibility:g}",
        "observer_alt_km": "" if observer is None else f"{observer:g}",
        "target_alt_km": f"{target:g}",
        "range_km": "" if range_km is None else f"{range_km:g}",
        "solar_zenith_deg": "" if sza is None else f"{sza:g}",
        "wavelength_low_um": f"{cfg['lo_um']:g}",
        "wavelength_high_um": f"{cfg['hi_um']:g}",
        "wavenumber_low_cm1": f"{cfg['wn_lo']:.4f}",
        "wavenumber_high_cm1": f"{cfg['wn_hi']:.4f}",
        "wavenumber_increment_cm1": f"{p11.WN_STEP_CM1:.4f}",
        "response_mode": "RectangularBand",
        "purpose": purpose,
        "input_file": str(input_path.resolve()),
        "input_sha256": sha256(input_path),
    })


def generate(mode: str, root: Path) -> list[dict[str, str]]:
    a = axes(mode)
    rows: list[dict[str, str]] = []
    prefix = ("P13PILOT" if mode == "pilot" else
              "P14MIX" if mode == "p14mix" else "P13TRACK50")
    for band, rh in itertools.product(p11.BANDS, a["rh"]):
        profile = p11.humidity_profile(rh)
        for observer, target, range_km, visibility in itertools.product(
                a["observer"], a["target"], a["range"], a["visibility"]):
            suffix = (f"{band}_{profile}_obs{token(observer)}_tar{token(target)}_"
                      f"rng{token(range_km)}_vis{token(visibility)}")
            add_case(
                rows, root, case_id=f"{prefix}_trans_{suffix}",
                mode="Transmittance", band=band, rh=rh,
                observer=observer, target=target, range_km=range_km,
                visibility=visibility, sza=None, purpose="tau_los",
                lines=los_lines(band, 0, observer, target, range_km, visibility, rh))
            add_case(
                rows, root, case_id=f"{prefix}_thermal_{suffix}",
                mode="ThermalRadiance", band=band, rh=rh,
                observer=observer, target=target, range_km=range_km,
                visibility=visibility, sza=None, purpose="path_thermal",
                lines=los_lines(band, 1, observer, target, range_km, visibility, rh))
            for sza in a["sza"]:
                add_case(
                    rows, root,
                    case_id=f"{prefix}_scatter_{suffix}_sza{token(sza)}",
                    mode="RadianceWithScattering", band=band, rh=rh,
                    observer=observer, target=target, range_km=range_km,
                    visibility=visibility, sza=sza,
                    purpose="los_path_solar_scattering",
                    lines=scattering_lines(
                        band, observer, target, range_km, visibility, sza, rh))

        # These are target-local quantities and are therefore generated once
        # per target/environment key, never copied from an unrelated altitude.
        for target, visibility, sza in itertools.product(
                a["target"], a["visibility"], a["sza"]):
            target_suffix = (f"{band}_{profile}_tar{token(target)}_"
                             f"vis{token(visibility)}_sza{token(sza)}")
            add_case(
                rows, root,
                case_id=f"{prefix}_solar_{target_suffix}",
                mode="DirectSolarIrradiance", band=band, rh=rh,
                observer=None, target=target, range_km=None,
                visibility=visibility, sza=sza,
                purpose="target_direct_solar",
                lines=p11.solar_lines(band, target, visibility, sza, rh))
            add_case(
                rows, root,
                case_id=f"{prefix}_flux_{target_suffix}",
                mode="SpectralFlux", band=band, rh=rh,
                observer=None, target=target, range_km=None,
                visibility=visibility, sza=sza,
                purpose="target_downward_sky_flux",
                lines=p11.flux_lines(band, target, visibility, sza, rh))
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("pilot", "track50", "p14mix"), default="pilot")
    parser.add_argument("--output-root", type=Path)
    args = parser.parse_args()
    default_root = (Path("logs/p14/atmosphere/highalt_vis23")
                    if args.mode == "p14mix"
                    else Path(f"logs/p13/atmosphere/{args.mode}"))
    root = (args.output_root or default_root).resolve()
    root.mkdir(parents=True, exist_ok=True)
    rows = generate(args.mode, root)
    expected_components, formal_vertices = expected_counts(args.mode)
    if len(rows) != expected_components:
        raise AssertionError(
            f"generated {len(rows)} component cases, expected {expected_components}")
    with (root / "case_manifest.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (root / "case_manifest.json").write_text(
        json.dumps(rows, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    provenance = {
        "schema": ("HwaSimIR.P14.ModtranGrid.1" if args.mode == "p14mix"
                   else "HwaSimIR.P13.ModtranGrid.1"),
        "mode": args.mode,
        "componentRuns": len(rows),
        "formalVertices": formal_vertices,
        "bands": list(p11.BANDS),
        "axes": axes(args.mode),
        "originalInputRangeIsSeparateFrom50kmExtension": True,
        "originalInputMeasuredLosKm": [2.427569473, 22.269183094],
        "noExtrapolation": True,
        "noMissingComponentFill": True,
        "generator": str(Path(__file__).resolve()),
        "generatorSha256": sha256(Path(__file__).resolve()),
    }
    (root / "generation_provenance.json").write_text(
        json.dumps(provenance, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(provenance, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
