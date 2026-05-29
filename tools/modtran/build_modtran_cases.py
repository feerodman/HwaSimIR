#!/usr/bin/env python3
"""Build PcModWin5/MODTRAN case manifests and dry-run modin files."""

from __future__ import annotations

import argparse
import csv
import json
import re
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple


MODE_SLUG = {
    "Transmittance": "transmittance",
    "ThermalRadiance": "thermal",
    "DirectSolarIrradiance": "solar",
    "RadianceWithScattering": "scattering",
}

MANIFEST_COLUMNS = [
    "case_id",
    "band",
    "mode",
    "grid",
    "atmosphere_model",
    "aerosol_model",
    "humidity_profile",
    "visibility_km",
    "observer_alt_km",
    "target_alt_km",
    "range_km",
    "solar_zenith_deg",
    "wavelength_low_um",
    "wavelength_high_um",
    "wavenumber_start_cm",
    "wavenumber_end_cm",
    "wavenumber_increment_cm",
    "fwhm_cm",
    "template_file",
    "modin_file",
    "status",
]

PATH_COLUMNS = [
    "case_id",
    "band",
    "mode",
    "atmosphere_model",
    "aerosol_model",
    "humidity_profile",
    "visibility_km",
    "observer_alt_km",
    "target_alt_km",
    "range_km",
    "wavenumber_cm",
    "wavelength_um",
    "tau_up",
    "path_radiance",
    "unit_radiance",
    "source_file",
]

SOLAR_COLUMNS = [
    "case_id",
    "band",
    "mode",
    "atmosphere_model",
    "aerosol_model",
    "humidity_profile",
    "visibility_km",
    "target_alt_km",
    "solar_zenith_deg",
    "wavenumber_cm",
    "wavelength_um",
    "tau_down",
    "solar_irradiance",
    "unit_irradiance",
    "source_file",
]

SKY_COLUMNS = [
    "case_id",
    "band",
    "mode",
    "atmosphere_model",
    "aerosol_model",
    "humidity_profile",
    "visibility_km",
    "observer_alt_km",
    "target_alt_km",
    "range_km",
    "solar_zenith_deg",
    "view_zenith_deg",
    "wavenumber_cm",
    "wavelength_um",
    "sky_radiance",
    "path_scattering_radiance",
    "unit_radiance",
    "source_file",
]

BAND_COLUMNS = [
    "band",
    "atmosphere_model",
    "aerosol_model",
    "humidity_profile",
    "visibility_km",
    "observer_alt_km",
    "target_alt_km",
    "range_km",
    "solar_zenith_deg",
    "tau_up_band",
    "tau_down_band",
    "path_radiance_band",
    "sky_radiance_band",
    "solar_irradiance_band",
]

NUMBER_RE = re.compile(r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[Ee][-+]?\d+)?")


@dataclass(frozen=True)
class Case:
    case_id: str
    band: str
    mode: str
    grid: str
    atmosphere_model: str
    aerosol_model: str
    humidity_profile: str
    visibility_km: float
    observer_alt_km: float
    target_alt_km: float
    range_km: float
    solar_zenith_deg: float
    wavelength_low_um: float
    wavelength_high_um: float
    wavenumber_start_cm: float
    wavenumber_end_cm: float
    wavenumber_increment_cm: float
    fwhm_cm: float
    template_file: str
    modin_file: str
    status: str = "dry_run"


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def fmt_id(value: object) -> str:
    text = f"{float(value):g}" if isinstance(value, (float, int)) else str(value)
    return text.replace(".", "p").replace("-", "m").replace(" ", "")


def wavenumber_bounds(wavelength_low_um: float, wavelength_high_um: float) -> Tuple[float, float]:
    start = 10000.0 / wavelength_high_um
    end = 10000.0 / wavelength_low_um
    return start, end


def write_csv_header(path: Path, columns: Sequence[str]) -> None:
    if path.exists() and path.stat().st_size > 0:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle)
        writer.writerow(columns)


def ensure_processed_headers(processed_dir: Path) -> None:
    write_csv_header(processed_dir / "path_lut_spectral.csv", PATH_COLUMNS)
    write_csv_header(processed_dir / "solar_lut_spectral.csv", SOLAR_COLUMNS)
    write_csv_header(processed_dir / "sky_lut_spectral.csv", SKY_COLUMNS)
    write_csv_header(processed_dir / "band_lut.csv", BAND_COLUMNS)
    write_csv_header(processed_dir / "manifest.csv", MANIFEST_COLUMNS)
    qc = processed_dir / "qc_report.md"
    if not qc.exists():
        qc.write_text(
            "# MODTRAN LUT QC Report\n\n"
            "This placeholder is created during the dry-run stage. It will be extended by parser and pilot runs.\n\n"
            "## Source Columns\n\n"
            "- Transmittance: FREQ/CM-1, COMBIN TRANS -> tau_up\n"
            "- ThermalRadiance: FREQ, TOT_TRANS, PTH_THRML -> tau_up, path_radiance\n"
            "- DirectSolarIrradiance: FREQ, TRANS, SOL TR, SOLAR -> tau_down, solar_irradiance\n"
            "- RadianceWithScattering: FREQ, TOT_TRANS, SOL_SCAT, SING_SCAT, TOTAL_RAD -> path_scattering_radiance, sky_radiance\n",
            encoding="utf-8",
        )


def replace_nth_number(line: str, index: int, value: float) -> str:
    matches = list(NUMBER_RE.finditer(line))
    if index >= len(matches):
        return line
    match = matches[index]
    width = max(match.end() - match.start(), 7)
    decimals = 3 if abs(value) < 1000 else 4
    replacement = f"{value:{width}.{decimals}f}"
    return line[: match.start()] + replacement + line[match.end() :]


def replace_frequency_line(line: str, start_cm: float, end_cm: float, increment_cm: float, fwhm_cm: float) -> str:
    alpha = re.search(r"[A-Za-z]", line)
    tail = line[alpha.start() :].rstrip() if alpha else "W                 0     0.000"
    return f"{start_cm:10.4f}{end_cm:10.4f}{increment_cm:10.4f}{fwhm_cm:10.4f} {tail}"


def find_frequency_line(lines: Sequence[str]) -> int:
    for index, line in enumerate(lines):
        if "W" in line.upper() and len(NUMBER_RE.findall(line)) >= 4:
            return index
    raise RuntimeError("Could not find the wavenumber range line in modin template.")


def rewrite_modin(template_text: str, case: Case) -> str:
    lines = template_text.splitlines()
    if len(lines) < 5:
        raise RuntimeError("modin template is too short to rewrite safely.")

    lines[2] = replace_nth_number(lines[2], 6, case.visibility_km)

    if case.mode == "DirectSolarIrradiance":
        lines[3] = replace_nth_number(lines[3], 0, case.target_alt_km)
        lines[3] = replace_nth_number(lines[3], 2, case.solar_zenith_deg)
    else:
        lines[3] = replace_nth_number(lines[3], 0, case.observer_alt_km)
        lines[3] = replace_nth_number(lines[3], 1, case.target_alt_km)
        lines[3] = replace_nth_number(lines[3], 3, case.range_km)
        if case.mode == "RadianceWithScattering" and len(lines) > 5:
            lines[5] = replace_nth_number(lines[5], 1, case.solar_zenith_deg)

    frequency_index = find_frequency_line(lines)
    lines[frequency_index] = replace_frequency_line(
        lines[frequency_index],
        case.wavenumber_start_cm,
        case.wavenumber_end_cm,
        case.wavenumber_increment_cm,
        case.fwhm_cm,
    )
    return "\n".join(lines) + "\n"


def template_candidates(band: str, mode: str) -> List[str]:
    slug = MODE_SLUG[mode]
    candidates = [f"{band}_{slug}_modin.txt"]
    if mode in {"DirectSolarIrradiance", "RadianceWithScattering"} and band in {"VIS", "SWIR"}:
        candidates.append(f"NIR_{slug}_modin.txt")
    if mode == "ThermalRadiance" and band == "LWIR":
        candidates.append("MWIR_thermal_modin.txt")
    return candidates


def resolve_template(templates_dir: Path, band: str, mode: str) -> Path:
    for name in template_candidates(band, mode):
        path = templates_dir / name
        if path.exists():
            return path
    tried = ", ".join(template_candidates(band, mode))
    raise RuntimeError(f"No modin template found for {band}/{mode}; tried: {tried}")


def resolve_aerosol_override_template(modtran_root: Path, band: str) -> Path:
    path = modtran_root / "raw" / "test" / f"{band}_transmittance_Rural_SurfaceVIS23_modin.txt"
    if not path.exists():
        raise RuntimeError(
            f"Aerosol override smoke template not found: {path}. "
            "Generate it from PcModWin5 GUI before running -AerosolOverrideSmoke."
        )
    return path


def mode_requires_solar_zenith(mode: str) -> bool:
    return mode in {"DirectSolarIrradiance", "RadianceWithScattering"}


def build_cases(config: Dict[str, object], modtran_root: Path, grid_name: str) -> List[Case]:
    templates_dir = modtran_root / "raw" / "templates"
    generated_dir = modtran_root / "generated" / "modin"
    atmosphere_model = str(config["atmosphere_model"])
    aerosol_models = list(config["aerosol_model"])
    humidity_profiles = list(config["humidity_profile"])

    if grid_name == "pilot":
        pilot = dict(config["pilot"])
        altitude_pairs = [tuple(pair) for pair in pilot["altitude_pairs"]]
        ranges = list(pilot["range_km"])
        visibilities = list(pilot["visibility_km"])
        solar_zeniths = list(pilot["solar_zenith_deg"])
        band_names = set(pilot["bands"])
        bands = {**config["priority_bands"]}
        bands = {name: spec for name, spec in bands.items() if name in band_names}
    elif grid_name == "visibility_smoke":
        altitude_pairs = [(3, 3), (10, 10), (20, 3)]
        ranges = [50]
        visibilities = [5, 23, 50]
        solar_zeniths = [45]
        bands = {name: dict(spec) for name, spec in dict(config["priority_bands"]).items() if name in {"NIR", "MWIR"}}
        for spec in bands.values():
            spec["modes"] = ["Transmittance"]
    elif grid_name == "aerosol_override_smoke":
        altitude_pairs = [(0.1, 0.1), (3, 3), (10, 10), (20, 3)]
        ranges = [20, 50]
        visibilities = [0.5, 2, 5, 23, 50]
        solar_zeniths = [45]
        bands = {name: dict(spec) for name, spec in dict(config["priority_bands"]).items() if name in {"NIR", "MWIR"}}
        for spec in bands.values():
            spec["modes"] = ["Transmittance"]
    elif grid_name == "production_nir_mwir":
        altitude_pairs = [tuple(pair) for pair in config["altitude_pairs"]]
        ranges = list(config["range_km"])
        visibilities = list(config["visibility_km"])
        solar_zeniths = list(config["production_solar_zenith_deg"])
        bands = {name: dict(spec) for name, spec in dict(config["priority_bands"]).items() if name in {"NIR", "MWIR"}}
    else:
        priority_bands = dict(config["priority_bands"])
        support_bands = dict(config["support_bands"])
        bands = {**priority_bands, **support_bands}

    cases: List[Case] = []
    for band, spec_obj in bands.items():
        spec = dict(spec_obj)
        is_support = bool(spec.get("sparse_only", False))
        if grid_name == "production" and is_support:
            support_grid = dict(config["support_sparse_grid"])
            altitude_pairs = [tuple(pair) for pair in support_grid["altitude_pairs"]]
            ranges = list(support_grid["range_km"])
            visibilities = list(support_grid["visibility_km"])
            solar_zeniths = list(support_grid["solar_zenith_deg"])
        elif grid_name == "production":
            altitude_pairs = [tuple(pair) for pair in config["altitude_pairs"]]
            ranges = list(config["range_km"])
            visibilities = list(config["visibility_km"])
            solar_zeniths = list(config["production_solar_zenith_deg"])

        wl_low, wl_high = [float(v) for v in spec["wavelength_um"]]
        wn_start, wn_end = wavenumber_bounds(wl_low, wl_high)
        increment = float(spec.get("wavenumber_increment_cm", 1.0))
        fwhm = float(spec.get("fwhm_cm", increment))
        modes = list(spec["modes"])

        for obs_alt, target_alt in altitude_pairs:
            for range_km in ranges:
                for visibility_km in visibilities:
                    for aerosol in aerosol_models:
                        for humidity in humidity_profiles:
                            for mode in modes:
                                zenith_values = solar_zeniths if mode_requires_solar_zenith(mode) else [solar_zeniths[0]]
                                for solar_zenith in zenith_values:
                                    if grid_name == "aerosol_override_smoke":
                                        template = resolve_aerosol_override_template(modtran_root, band)
                                    else:
                                        template = resolve_template(templates_dir, band, mode)
                                    prefix = f"{band}_{MODE_SLUG[mode]}"
                                    if grid_name == "aerosol_override_smoke":
                                        prefix = f"{prefix}_aerosoloverride"
                                    case_id = (
                                        prefix +
                                        f"_obs{fmt_id(obs_alt)}_tar{fmt_id(target_alt)}"
                                        f"_rng{fmt_id(range_km)}_vis{fmt_id(visibility_km)}"
                                        f"_aer{fmt_id(aerosol)}_hum{fmt_id(humidity)}"
                                    )
                                    if mode_requires_solar_zenith(mode):
                                        case_id += f"_sza{fmt_id(solar_zenith)}"
                                    modin_file = generated_dir / f"{case_id}_modin.txt"
                                    cases.append(
                                        Case(
                                            case_id=case_id,
                                            band=band,
                                            mode=mode,
                                            grid=grid_name,
                                            atmosphere_model=atmosphere_model,
                                            aerosol_model=str(aerosol),
                                            humidity_profile=str(humidity),
                                            visibility_km=float(visibility_km),
                                            observer_alt_km=float(obs_alt),
                                            target_alt_km=float(target_alt),
                                            range_km=float(range_km),
                                            solar_zenith_deg=float(solar_zenith),
                                            wavelength_low_um=wl_low,
                                            wavelength_high_um=wl_high,
                                            wavenumber_start_cm=wn_start,
                                            wavenumber_end_cm=wn_end,
                                            wavenumber_increment_cm=increment,
                                            fwhm_cm=fwhm,
                                            template_file=str(template.relative_to(repo_root())),
                                            modin_file=str(modin_file.relative_to(repo_root())),
                                        )
                                    )
    return cases


def case_to_row(case: Case) -> Dict[str, object]:
    return {column: getattr(case, column) for column in MANIFEST_COLUMNS}


def write_manifest(path: Path, cases: Sequence[Case]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=MANIFEST_COLUMNS)
        writer.writeheader()
        for case in cases:
            writer.writerow(case_to_row(case))


def write_manifest_with_status(path: Path, cases: Sequence[Case], status: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=MANIFEST_COLUMNS)
        writer.writeheader()
        for case in cases:
            row = case_to_row(case)
            row["status"] = status
            writer.writerow(row)


def invalid_slant_geometry(case: Case) -> bool:
    return case.range_km + 1e-9 < abs(case.observer_alt_km - case.target_alt_km)


def generate_modin_files(cases: Sequence[Case]) -> None:
    root = repo_root()
    for case in cases:
        template_path = root / case.template_file
        output_path = root / case.modin_file
        output_path.parent.mkdir(parents=True, exist_ok=True)
        template_text = template_path.read_text(encoding="utf-8", errors="replace")
        output_path.write_text(rewrite_modin(template_text, case), encoding="utf-8")


def clean_generated_modin(modtran_root: Path) -> None:
    generated_dir = modtran_root / "generated" / "modin"
    generated_dir.mkdir(parents=True, exist_ok=True)
    for file_path in generated_dir.glob("*_modin.txt"):
        file_path.unlink()


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True, help="Path to modtran_grid_nir_mwir_priority.json.")
    parser.add_argument("--dry-run", action="store_true", help="Generate manifest and modin files without running MODTRAN.")
    parser.add_argument("--pilot", action="store_true", help="Use the <=100 case pilot grid.")
    parser.add_argument("--visibility-smoke", action="store_true", help="Use the 18 case low/high altitude visibility smoke grid.")
    parser.add_argument("--aerosol-override-smoke", action="store_true", help="Use the 80 case aerosol/visibility override smoke grid.")
    parser.add_argument("--production-nir-mwir", action="store_true", help="Use the NIR/MWIR-only production sparse grid.")
    parser.add_argument("--clean", action="store_true", help="Remove old generated modin files before writing new ones.")
    parser.add_argument("--skip-processed-manifest", action="store_true", help="Do not overwrite processed/manifest.csv.")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)
    if not args.dry_run:
        print("ERROR: build_modtran_cases.py currently supports only --dry-run. Use run_modtran_cases.ps1 for execution.", file=sys.stderr)
        return 2

    root = repo_root()
    config_path = (root / args.config).resolve() if not Path(args.config).is_absolute() else Path(args.config)
    config = json.loads(config_path.read_text(encoding="utf-8"))
    modtran_root = root / str(config["modtran_root"])
    processed_dir = modtran_root / "processed"
    generated_dir = modtran_root / "generated"
    grid_flags = [args.pilot, args.visibility_smoke, args.aerosol_override_smoke, args.production_nir_mwir]
    if sum(1 for enabled in grid_flags if enabled) > 1:
        print("ERROR: --pilot, --visibility-smoke, --aerosol-override-smoke, and --production-nir-mwir are mutually exclusive.", file=sys.stderr)
        return 2
    if args.production_nir_mwir:
        grid_name = "production_nir_mwir"
    elif args.aerosol_override_smoke:
        grid_name = "aerosol_override_smoke"
    elif args.visibility_smoke:
        grid_name = "visibility_smoke"
    elif args.pilot:
        grid_name = "pilot"
    else:
        grid_name = "production"

    for path in [
        modtran_root / "raw" / "templates",
        modtran_root / "raw" / "samples",
        modtran_root / "raw" / "failed",
        modtran_root / "generated" / "modin",
        processed_dir,
    ]:
        path.mkdir(parents=True, exist_ok=True)

    if args.clean:
        clean_generated_modin(modtran_root)

    ensure_processed_headers(processed_dir)
    cases = build_cases(config, modtran_root, grid_name)
    invalid_cases: List[Case] = []
    if args.production_nir_mwir:
        invalid_cases = [case for case in cases if invalid_slant_geometry(case)]
        cases = [case for case in cases if not invalid_slant_geometry(case)]
    if args.aerosol_override_smoke:
        max_cases = 80
    else:
        max_cases = 18 if args.visibility_smoke else int(config["pilot"]["max_cases"] if args.pilot else config["max_production_cases"])
    if len(cases) > max_cases:
        print(f"ERROR: {grid_name} grid contains {len(cases)} cases, which exceeds limit {max_cases}. Shrink the grid.", file=sys.stderr)
        return 2

    manifest_path = generated_dir / "case_manifest.csv"
    write_manifest(manifest_path, cases)
    if args.production_nir_mwir:
        production_manifest_path = generated_dir / "production_manifest.csv"
        write_manifest(production_manifest_path, cases)
        invalid_manifest_path = generated_dir / "production_invalid_geometry_manifest.csv"
        write_manifest_with_status(invalid_manifest_path, invalid_cases, "invalid_geometry")
    if args.aerosol_override_smoke:
        write_manifest(generated_dir / "aerosol_override_smoke_manifest.csv", cases)
    if not args.skip_processed_manifest and not args.production_nir_mwir and not args.aerosol_override_smoke:
        write_manifest(processed_dir / "manifest.csv", cases)
    generate_modin_files(cases)

    print(f"Dry run complete: {len(cases)} cases.")
    print(f"Manifest: {manifest_path}")
    if args.production_nir_mwir:
        print(f"Production manifest: {generated_dir / 'production_manifest.csv'}")
        print(f"Invalid geometry manifest: {generated_dir / 'production_invalid_geometry_manifest.csv'}")
        print(f"Invalid geometry excluded: {len(invalid_cases)}")
    if args.aerosol_override_smoke:
        print(f"Aerosol override smoke manifest: {generated_dir / 'aerosol_override_smoke_manifest.csv'}")
    print(f"Generated modin dir: {modtran_root / 'generated' / 'modin'}")
    by_band_mode: Dict[Tuple[str, str], int] = {}
    for case in cases:
        by_band_mode[(case.band, case.mode)] = by_band_mode.get((case.band, case.mode), 0) + 1
    for (band, mode), count in sorted(by_band_mode.items()):
        print(f"  {band}/{mode}: {count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
