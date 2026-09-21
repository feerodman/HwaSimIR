#!/usr/bin/env python3
"""QC and assemble P13 licensed-MODTRAN high-altitude five-component rows."""

from __future__ import annotations

import argparse
import csv
import hashlib
import itertools
import json
import math
import re
from pathlib import Path
import sys


TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
import modtran_convert_to_si as convert  # noqa: E402
import p11_modtran_humidity_grid as p11  # noqa: E402
import p11_modtran_humidity_qc as p11qc  # noqa: E402
import p13_modtran_grid as generator  # noqa: E402


FORMAL_FIELDS = p11qc.FORMAL_FIELDS
VALUE_FIELDS = [
    "tau_up", "path_thermal_W_m2_sr_um",
    "direct_solar_irradiance_at_target_W_m2_um",
    "downward_sky_diffuse_irradiance_W_m2_um",
    "los_path_scattering_radiance_W_m2_sr_um",
]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as stream:
        return list(csv.DictReader(stream))


def flux_altitudes(path: Path) -> list[float]:
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    header = " ".join(lines[:35])
    levels = [float(value) for value in re.findall(
        r"([-+]?\d+(?:\.\d+)?)\s+KM", header)]
    if not levels:
        raise ValueError(f"No flux altitude levels in {path}")
    return levels


def parse_flux_at_target(path: Path, target_alt_km: float) -> tuple[list[dict[str, object]], dict[str, object]]:
    levels = sorted(set(flux_altitudes(path)))
    exact = next((value for value in levels if abs(value - target_alt_km) <= 1e-9), None)
    if exact is not None:
        rows = convert.parse_flux(path, exact)
        for row in rows:
            row["vertical_interpolation"] = "none_exact_real_level"
        return rows, {"targetAltKm": target_alt_km, "lowLevelKm": exact,
                      "highLevelKm": exact, "fraction": 0.0,
                      "method": "exact_real_flux_level"}
    lower = [value for value in levels if value < target_alt_km]
    upper = [value for value in levels if value > target_alt_km]
    if not lower or not upper:
        raise ValueError(
            f"Target altitude {target_alt_km} is outside real flux levels in {path}")
    low_level, high_level = max(lower), min(upper)
    low = convert.parse_flux(path, low_level)
    high = convert.parse_flux(path, high_level)
    if len(low) != len(high):
        raise ValueError(f"Flux level row mismatch in {path}")
    fraction = (target_alt_km - low_level) / (high_level - low_level)
    numeric = [
        "upward_diffuse_native_W_cm2_nm", "downward_diffuse_native_W_cm2_nm",
        "direct_solar_native_W_cm2_nm", "upward_diffuse_W_m2_um",
        "downward_diffuse_W_m2_um", "direct_solar_W_m2_um",
    ]
    rows: list[dict[str, object]] = []
    for a, b in zip(low, high):
        if abs(float(a["wavelength_um"]) - float(b["wavelength_um"])) > 1e-12:
            raise ValueError(f"Flux wavelength mismatch in {path}")
        item = dict(a)
        for field in numeric:
            item[field] = float(a[field]) + (float(b[field]) - float(a[field])) * fraction
        item["altitude_km"] = target_alt_km
        item["vertical_interpolation"] = "linear_between_adjacent_real_flux_levels"
        rows.append(item)
    return rows, {"targetAltKm": target_alt_km, "lowLevelKm": low_level,
                  "highLevelKm": high_level, "fraction": fraction,
                  "method": "linear_between_adjacent_real_flux_levels"}


def convert_case(row: dict[str, str]) -> tuple[Path, list[dict[str, object]], dict[str, object] | None]:
    case_dir = Path(row["input_file"]).parent
    flux_meta: dict[str, object] | None = None
    if row["mode"] == "SpectralFlux":
        source = case_dir / "spectral_flux.flx"
        parsed, flux_meta = parse_flux_at_target(source, float(row["target_alt_km"]))
    else:
        source = case_dir / "MODOUT2.txt"
        parser = {
            "Transmittance": convert.parse_transmittance,
            "ThermalRadiance": convert.parse_radiance,
            "RadianceWithScattering": convert.parse_radiance,
            "DirectSolarIrradiance": convert.parse_solar,
        }[row["mode"]]
        parsed = parser(source)
    convert.write_rows(case_dir / "spectrum_si.csv", parsed, source, row["case_id"])
    return source, parsed, flux_meta


def find_one(manifest: list[dict[str, str]], *, band: str, profile: str,
             mode: str, observer: float | None, target: float,
             range_km: float | None, visibility: float,
             sza: float | None) -> dict[str, str]:
    matches: list[dict[str, str]] = []
    for row in manifest:
        if (row["band"] != band or row["humidity_profile"] != profile or
                row["mode"] != mode or
                abs(float(row["target_alt_km"]) - target) > 1e-9 or
                abs(float(row["visibility_km"]) - visibility) > 1e-9):
            continue
        actual_observer = None if not row["observer_alt_km"] else float(row["observer_alt_km"])
        actual_range = None if not row["range_km"] else float(row["range_km"])
        actual_sza = None if not row["solar_zenith_deg"] else float(row["solar_zenith_deg"])
        if actual_observer == observer and actual_range == range_km and actual_sza == sza:
            matches.append(row)
    if len(matches) != 1:
        raise ValueError(
            f"expected one component, got {len(matches)} for "
            f"{band}/{profile}/{mode}/{observer}/{target}/{range_km}/{visibility}/{sza}")
    return matches[0]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("pilot", "track50", "p14mix"), default="pilot")
    parser.add_argument("--root", type=Path)
    args = parser.parse_args()
    default_root = (Path("logs/p14/atmosphere/highalt_vis23")
                    if args.mode == "p14mix"
                    else Path(f"logs/p13/atmosphere/{args.mode}"))
    root = (args.root or default_root).resolve()
    manifest = read_csv(root / "case_manifest.csv")
    expected_components, expected_vertices = generator.expected_counts(args.mode)
    a = generator.axes(args.mode)
    checks: list[dict[str, object]] = []

    def check(name: str, passed: bool, measured: object, expected: object) -> None:
        checks.append({"check": name, "passed": bool(passed),
                       "measured": measured, "expected": expected})

    check("component_manifest_count", len(manifest) == expected_components,
          len(manifest), expected_components)
    check("manifest_case_ids_unique",
          len({row["case_id"] for row in manifest}) == len(manifest),
          len({row["case_id"] for row in manifest}), len(manifest))
    input_failures: list[str] = []
    geometry_failures: list[str] = []
    for row in manifest:
        path = Path(row["input_file"])
        if not path.is_file() or sha256(path) != row["input_sha256"]:
            input_failures.append(row["case_id"])
        if row["range_km"] and float(row["range_km"]) + 1e-9 < abs(
                float(row["observer_alt_km"]) - float(row["target_alt_km"])):
            geometry_failures.append(row["case_id"])
    check("all_input_hashes_match", not input_failures, input_failures, [])
    check("all_slant_geometries_physical", not geometry_failures, geometry_failures, [])

    run_rows = read_csv(root / "run_manifest.csv")
    executed = {row["case_id"] for row in run_rows}
    check("real_modtran_case_count", len(executed) == expected_components,
          len(executed), expected_components)
    check("all_manifest_cases_executed",
          executed == {row["case_id"] for row in manifest},
          sorted({row["case_id"] for row in manifest} - executed), [])
    engine_hashes = sorted({row["engine_sha256"] for row in run_rows})
    check("single_real_engine_sha256",
          len(engine_hashes) == 1 and len(engine_hashes[0]) == 64,
          engine_hashes, "one executable SHA-256")
    restore = read_csv(root / "installation_restore_evidence.csv")
    check("pcmodwin_fixed_files_restored",
          bool(restore) and all(row["restored_exactly"] == "True" for row in restore),
          [row["fixed_name"] for row in restore if row["restored_exactly"] != "True"], [])

    integrated: dict[str, float] = {}
    sources: dict[str, Path] = {}
    spectral_rows: list[dict[str, object]] = []
    flux_rows: list[dict[str, object]] = []
    humidity_rows: list[dict[str, object]] = []
    for number, row in enumerate(manifest, 1):
        case_dir = Path(row["input_file"]).parent
        stderr = case_dir / "engine_stderr.txt"
        if not stderr.is_file() or stderr.stat().st_size != 0:
            raise ValueError(f"MODTRAN stderr missing/non-empty for {row['case_id']}")
        source, parsed, flux_meta = convert_case(row)
        metric = p11qc.spectral_metrics(row, parsed)
        metric["source_file"] = str(source.resolve())
        metric["source_sha256"] = sha256(source)
        spectral_rows.append(metric)
        integrated[row["case_id"]] = float(metric["response_mean_value_si"])
        sources[row["case_id"]] = source
        if flux_meta is not None:
            flux_rows.append({"case_id": row["case_id"], **flux_meta})
        if row["mode"] == "Transmittance":
            response = p11qc.parse_modout1(case_dir / "MODOUT1.txt")
            humidity_rows.append({
                "case_id": row["case_id"], "band": row["band"],
                "humidity_profile": row["humidity_profile"],
                "observer_alt_km": row["observer_alt_km"],
                "target_alt_km": row["target_alt_km"], "range_km": row["range_km"],
                "visibility_km": row["visibility_km"],
                "target_surface_rh_percent": float(row["target_surface_rh_percent"]),
                **response,
            })
        if number % 50 == 0:
            print(f"Converted/audited {number}/{len(manifest)} component spectra", flush=True)

    check("all_spectra_bracket_exact_response",
          all(bool(row["boundary_bracketed"]) for row in spectral_rows),
          [row["case_id"] for row in spectral_rows if not row["boundary_bracketed"]], [])
    check("all_spectra_finite",
          all(bool(row["all_parsed_numeric_finite"]) for row in spectral_rows),
          [row["case_id"] for row in spectral_rows if not row["all_parsed_numeric_finite"]], [])
    max_jacobian = max(float(row["jacobian_integral_relative_error"]) for row in spectral_rows)
    max_pointwise = max(float(row["pointwise_conversion_max_relative_error"]) for row in spectral_rows)
    check("all_jacobian_integrals_within_tolerance", max_jacobian <= 5e-6,
          max_jacobian, "<=5e-6")
    check("all_pointwise_unit_conversions_exact", max_pointwise <= 1e-12,
          max_pointwise, "<=1e-12")
    check("flux_vertical_interpolation_never_extrapolates",
          all(float(row["lowLevelKm"]) <= float(row["targetAltKm"]) <= float(row["highLevelKm"])
              for row in flux_rows), flux_rows, "target inside adjacent real levels")

    formal_rows: list[dict[str, str]] = []
    vertex_metrics: list[dict[str, object]] = []
    for band, rh, observer, target, range_km, visibility, sza in itertools.product(
            p11.BANDS, a["rh"], a["observer"], a["target"], a["range"],
            a["visibility"], a["sza"]):
        profile = p11.humidity_profile(rh)
        components = [
            find_one(manifest, band=band, profile=profile, mode="Transmittance",
                     observer=observer, target=target, range_km=range_km,
                     visibility=visibility, sza=None),
            find_one(manifest, band=band, profile=profile, mode="ThermalRadiance",
                     observer=observer, target=target, range_km=range_km,
                     visibility=visibility, sza=None),
            find_one(manifest, band=band, profile=profile, mode="RadianceWithScattering",
                     observer=observer, target=target, range_km=range_km,
                     visibility=visibility, sza=sza),
            find_one(manifest, band=band, profile=profile, mode="DirectSolarIrradiance",
                     observer=None, target=target, range_km=None,
                     visibility=visibility, sza=sza),
            find_one(manifest, band=band, profile=profile, mode="SpectralFlux",
                     observer=None, target=target, range_km=None,
                     visibility=visibility, sza=sza),
        ]
        values = [integrated[row["case_id"]] for row in components]
        ids = [row["case_id"] for row in components]
        files = [str(sources[case_id].resolve()) for case_id in ids]
        release_prefix = "P14_MIX" if args.mode == "p14mix" else f"P13_{args.mode.upper()}"
        case_id = (f"{release_prefix}_{band}_{profile}_"
                   f"obs{generator.token(observer)}_tar{generator.token(target)}_"
                   f"rng{generator.token(range_km)}_vis{generator.token(visibility)}_"
                   f"sza{generator.token(sza)}")
        formal_rows.append({
            "schema_version": "1", "case_id": case_id, "band": band,
            "atmosphere_model": "Mid-Latitude Summer", "aerosol_model": "Rural",
            "humidity_profile": profile, "visibility_km": f"{visibility:g}",
            "observer_alt_km": f"{observer:g}", "target_alt_km": f"{target:g}",
            "range_km": f"{range_km:g}", "solar_zenith_deg": f"{sza:g}",
            "tau_up": f"{values[0]:.12g}",
            "path_thermal_W_m2_sr_um": f"{values[1]:.12g}",
            "direct_solar_irradiance_at_target_W_m2_um": f"{values[3]:.12g}",
            "downward_sky_diffuse_irradiance_W_m2_um": f"{values[4]:.12g}",
            "los_path_scattering_radiance_W_m2_sr_um": f"{values[2]:.12g}",
            "radiance_unit": "W/(m^2 sr um)", "irradiance_unit": "W/(m^2 um)",
            "tau_unit": "dimensionless", "response_mode": "RectangularBand",
            "conversion_method": (
                "real MODTRAN5 native spectra; per-cm^-1 values converted pointwise "
                "with 1e8/lambda_um^2 then exact-endpoint wavelength-domain trapezoidal mean; "
                "sky flux exact real level or linear between adjacent real vertical levels"),
            "modtran_source_fields": "COMBIN TRANS;PTH_THRML;SOL TR;.flx DOWNWARD;SOL_SCAT",
            "source_case_ids": ";".join(ids), "source_files": ";".join(files),
        })
        vertex_metrics.append({
            "case_id": case_id, "band": band, "humidity_profile": profile,
            "relative_humidity_percent": rh, "observer_alt_km": observer,
            "target_alt_km": target, "range_km": range_km,
            "visibility_km": visibility, "solar_zenith_deg": sza,
            "tau_up": values[0], "path_thermal_W_m2_sr_um": values[1],
            "direct_solar_W_m2_um": values[3], "downward_sky_W_m2_um": values[4],
            "path_scattering_W_m2_sr_um": values[2],
        })

    check("formal_vertex_count",
          len(formal_rows) == expected_vertices and
          len({row["case_id"] for row in formal_rows}) == expected_vertices,
          len(formal_rows), expected_vertices)
    check("all_vertices_have_five_finite_nonnegative_components",
          all(row[field] != "" and math.isfinite(float(row[field])) and float(row[field]) >= 0.0
              for row in formal_rows for field in VALUE_FIELDS),
          "all rows", "five finite values >=0")
    check("all_tau_in_unit_interval",
          all(0.0 <= float(row["tau_up"]) <= 1.0 for row in formal_rows),
          [min(float(row["tau_up"]) for row in formal_rows),
           max(float(row["tau_up"]) for row in formal_rows)], "0<=tau<=1")

    # Do not hide suspicious physics behind a tolerance.  Report every sequence;
    # monotonic range behavior is a hard gate (tiny formatter noise allowed).
    if len(a["range"]) > 1:
        failures: list[dict[str, object]] = []
        for band, rh, observer, target, visibility, sza in itertools.product(
                p11.BANDS, a["rh"], a["observer"], a["target"],
                a["visibility"], a["sza"]):
            profile = p11.humidity_profile(rh)
            seq = [next(float(row["tau_up"]) for row in formal_rows
                        if row["band"] == band and row["humidity_profile"] == profile and
                        float(row["observer_alt_km"]) == observer and
                        float(row["target_alt_km"]) == target and
                        float(row["range_km"]) == distance and
                        float(row["visibility_km"]) == visibility and
                        float(row["solar_zenith_deg"]) == sza)
                   for distance in a["range"]]
            if any(b > a_value + 2e-6 for a_value, b in zip(seq, seq[1:])):
                failures.append({"band": band, "profile": profile, "observer": observer,
                                 "target": target, "sza": sza, "tau": seq})
        check("tau_nonincreasing_with_slant_range", not failures, failures, [])
    if len(a["rh"]) == 3:
        response_count = 0
        failures = []
        for band, observer, target, range_km, visibility, sza in itertools.product(
                p11.BANDS, a["observer"], a["target"], a["range"],
                a["visibility"], a["sza"]):
            seq = [next(float(row["tau_up"]) for row in formal_rows
                        if row["band"] == band and
                        row["humidity_profile"] == p11.humidity_profile(rh) and
                        float(row["observer_alt_km"]) == observer and
                        float(row["target_alt_km"]) == target and
                        float(row["range_km"]) == range_km and
                        float(row["solar_zenith_deg"]) == sza)
                   for rh in a["rh"]]
            if seq[0] > seq[1] > seq[2]:
                response_count += 1
            else:
                failures.append({"band": band, "observer": observer,
                                 "target": target, "range": range_km, "tau": seq})
        check("tau_strict_humidity_response", not failures,
              {"responsive": response_count, "failures": failures}, "RH30>RH60>RH85 tau")

    output = root / ("pilot_candidate_rows.csv" if args.mode == "pilot" else
                     "formal_p14mix_rows.csv" if args.mode == "p14mix" else
                     "formal_track50_rows.csv")
    with output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=FORMAL_FIELDS)
        writer.writeheader(); writer.writerows(formal_rows)
    for name, rows in [
            ("spectral_qc.csv", spectral_rows),
            ("flux_vertical_interpolation.csv", flux_rows),
            ("modout1_humidity_metrics.csv", humidity_rows),
            ("vertex_metrics.csv", vertex_metrics)]:
        with (root / name).open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
            writer.writeheader(); writer.writerows(rows)

    status = "PASS" if all(item["passed"] for item in checks) else "FAIL"
    results = {"schema": ("HwaSimIR.P14.ModtranQC.1" if args.mode == "p14mix"
                           else "HwaSimIR.P13.ModtranQC.1"), "status": status,
               "mode": args.mode, "componentRuns": expected_components,
               "formalVertices": expected_vertices, "checks": checks}
    (root / "qc_results.json").write_text(
        json.dumps(results, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    with (root / "qc_results.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=["check", "passed", "measured", "expected"])
        writer.writeheader(); writer.writerows(checks)
    readme = [
        f"# {'P14' if args.mode == 'p14mix' else 'P13'} real MODTRAN {args.mode}", "", f"- status: {status}",
        f"- licensed component executions: {expected_components}",
        f"- complete five-component vertices: {expected_vertices}",
        "- no missing-value fill, empirical range extension, or tau=1 fallback is accepted",
        "- 1.txt measured range and the separately declared 50 km extension remain distinct",
        "", "## Checks", "",
    ]
    readme.extend(
        f"- {'PASS' if item['passed'] else 'FAIL'} {item['check']}: {item['measured']}"
        for item in checks)
    (root / "README.md").write_text("\n".join(readme) + "\n", encoding="utf-8")
    print(json.dumps({"status": status, "checks": len(checks),
                      "components": expected_components, "vertices": expected_vertices,
                      "maxJacobianError": max_jacobian,
                      "maxPointwiseError": max_pointwise}, indent=2))
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
