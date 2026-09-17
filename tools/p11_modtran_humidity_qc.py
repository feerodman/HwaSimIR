#!/usr/bin/env python3
"""Audit and assemble isolated P11 MODTRAN humidity pilot/grid products."""

from __future__ import annotations

import argparse
import csv
import hashlib
import itertools
import json
import math
import re
import sys
from pathlib import Path


TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
import modtran_convert_to_si as convert  # noqa: E402
import p11_modtran_humidity_grid as generator  # noqa: E402


FORMAL_FIELDS = [
    "schema_version", "case_id", "band", "atmosphere_model", "aerosol_model",
    "humidity_profile", "visibility_km", "observer_alt_km", "target_alt_km", "range_km",
    "solar_zenith_deg", "tau_up", "path_thermal_W_m2_sr_um",
    "direct_solar_irradiance_at_target_W_m2_um", "downward_sky_diffuse_irradiance_W_m2_um",
    "los_path_scattering_radiance_W_m2_sr_um", "radiance_unit", "irradiance_unit", "tau_unit",
    "response_mode", "conversion_method", "modtran_source_fields", "source_case_ids", "source_files",
]

NUMBER = r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[Ee][-+]?\d+)?"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as f:
        return list(csv.DictReader(f))


def token(value: float) -> str:
    return f"{value:g}".replace(".", "p")


def relerr(actual: float, expected: float) -> float:
    return abs(actual - expected) / max(abs(expected), 1e-30)


def trapz(rows: list[dict[str, object]], xfield: str, yfield: str,
          lo: float | None = None, hi: float | None = None) -> float:
    points = sorted((float(row[xfield]), float(row[yfield])) for row in rows
                    if row.get(yfield, "") != "" and
                    (lo is None or float(row[xfield]) >= lo) and
                    (hi is None or float(row[xfield]) <= hi))
    return sum(0.5 * (y0 + y1) * (x1 - x0)
               for (x0, y0), (x1, y1) in zip(points, points[1:]))


def response_integral(rows: list[dict[str, object]], field: str,
                      lo_um: float, hi_um: float) -> float:
    points = sorted((float(row["wavelength_um"]), float(row[field]))
                    for row in rows if row.get(field, "") != "")
    if not points or points[0][0] > lo_um or points[-1][0] < hi_um:
        raise ValueError(f"{field} lacks real samples bracketing {lo_um}--{hi_um} um")

    def at(x: float) -> float:
        for px, py in points:
            if abs(px - x) <= 1e-14:
                return py
        for (x0, y0), (x1, y1) in zip(points, points[1:]):
            if x0 < x < x1:
                return y0 + (y1 - y0) * (x - x0) / (x1 - x0)
        raise ValueError(f"cannot interpolate {field} at {x}")

    selected = [(x, y) for x, y in points if lo_um < x < hi_um]
    selected.insert(0, (lo_um, at(lo_um)))
    selected.append((hi_um, at(hi_um)))
    return sum(0.5 * (y0 + y1) * (x1 - x0)
               for (x0, y0), (x1, y1) in zip(selected, selected[1:]))


def parse_modout1(path: Path) -> dict[str, float]:
    text = path.read_text(encoding="utf-8", errors="replace")
    water: dict[str, float] = {}
    for label in ("INITIAL", "INPUT", "FINAL"):
        match = re.search(rf"^\s*{label}:\s*({NUMBER})\s+GM\s*/\s*CM2", text, re.MULTILINE)
        if not match:
            raise ValueError(f"No {label} water-column record in {path}")
        water[f"water_{label.lower()}_g_cm2"] = float(match.group(1))

    lines = text.splitlines()
    surface_rh = None
    for i, line in enumerate(lines):
        if "AER1*RH" not in line or "RH (%)" not in line:
            continue
        for candidate in lines[i + 1:i + 8]:
            values = re.findall(NUMBER, candidate)
            if len(values) >= 10 and int(float(values[0])) == 1 and abs(float(values[1])) <= 1e-8:
                surface_rh = float(values[9])
                break
        if surface_rh is not None:
            break
    if surface_rh is None:
        raise ValueError(f"No post-scaling surface RH profile row in {path}")

    mean_aer_rh = None
    for i, line in enumerate(lines):
        if "MEAN AER RH" not in line:
            continue
        for candidate in lines[i + 1:i + 8]:
            values = re.findall(NUMBER, candidate)
            if len(values) >= 8:
                mean_aer_rh = float(values[-1])
                break
        if mean_aer_rh is not None:
            break
    if mean_aer_rh is None:
        raise ValueError(f"No mean aerosol RH path record in {path}")
    return {**water, "surface_rh_after_percent": surface_rh,
            "mean_aerosol_rh_percent": mean_aer_rh}


def convert_case(row: dict[str, str]) -> tuple[Path, list[dict[str, object]]]:
    case_dir = Path(row["input_file"]).parent
    if row["mode"] == "SpectralFlux":
        source = case_dir / "spectral_flux.flx"
        altitude = float(row["target_alt_km"])
        if abs(altitude - 1.0) <= 1e-12:
            parsed = convert.parse_flux(source, 1.0)
            for item in parsed:
                item["vertical_interpolation"] = "none_exact_real_1km_level"
        else:
            low = convert.parse_flux(source, 0.0)
            high = convert.parse_flux(source, 1.0)
            if len(low) != len(high):
                raise ValueError(f"Flux level row mismatch in {source}")
            parsed = []
            for a, b in zip(low, high):
                if abs(float(a["wavelength_um"]) - float(b["wavelength_um"])) > 1e-12:
                    raise ValueError(f"Flux wavelength mismatch in {source}")
                item = dict(a)
                for field in [
                    "upward_diffuse_native_W_cm2_nm", "downward_diffuse_native_W_cm2_nm",
                    "direct_solar_native_W_cm2_nm", "upward_diffuse_W_m2_um",
                    "downward_diffuse_W_m2_um", "direct_solar_W_m2_um",
                ]:
                    item[field] = float(a[field]) + (float(b[field]) - float(a[field])) * altitude
                item["altitude_km"] = altitude
                item["vertical_interpolation"] = "linear_between_real_0km_and_1km_levels"
                parsed.append(item)
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
    return source, parsed


def spectral_metrics(row: dict[str, str], parsed: list[dict[str, object]]) -> dict[str, object]:
    band = generator.BANDS[row["band"]]
    lo = float(band["lo_um"])
    hi = float(band["hi_um"])
    width = hi - lo
    mode_fields = {
        "Transmittance": ("tau_los", None),
        "ThermalRadiance": ("path_thermal_W_m2_sr_um", "path_thermal_native"),
        "RadianceWithScattering": ("solar_scatter_W_m2_sr_um", "solar_scatter_native"),
        "DirectSolarIrradiance": ("direct_solar_W_m2_um", "direct_solar_native"),
        "SpectralFlux": ("downward_diffuse_W_m2_um", "downward_diffuse_native_W_cm2_nm"),
    }
    si_field, native_field = mode_fields[row["mode"]]
    wavelengths = [float(item["wavelength_um"]) for item in parsed]
    value = response_integral(parsed, si_field, lo, hi) / width
    jacobian_error = 0.0
    pointwise_error = 0.0
    if row["mode"] == "SpectralFlux":
        selected = [item for item in parsed if lo <= float(item["wavelength_um"]) <= hi]
        native_area = trapz(selected, "wavelength_nm", native_field) * 1e4
        si_area = trapz(selected, "wavelength_um", si_field)
        jacobian_error = relerr(si_area, native_area)
        pointwise_error = max(
            relerr(float(item[si_field]), float(item[native_field]) * 1e7) for item in selected)
    elif native_field:
        selected = [item for item in parsed if lo <= float(item["wavelength_um"]) <= hi]
        native_area = trapz(selected, "wavenumber_cm1", native_field) * 1e4
        si_area = trapz(selected, "wavelength_um", si_field)
        jacobian_error = relerr(si_area, native_area)
        pointwise_error = max(
            relerr(float(item[si_field]), float(item[native_field]) * 1e8 /
                   float(item["wavelength_um"]) ** 2) for item in selected)
    return {
        "case_id": row["case_id"], "band": row["band"], "mode": row["mode"],
        "humidity_profile": row["humidity_profile"], "raw_rows": len(parsed),
        "wavelength_min_um": min(wavelengths), "wavelength_max_um": max(wavelengths),
        "response_low_um": lo, "response_high_um": hi,
        "boundary_bracketed": min(wavelengths) <= lo and max(wavelengths) >= hi,
        "response_mean_value_si": value, "jacobian_integral_relative_error": jacobian_error,
        "pointwise_conversion_max_relative_error": pointwise_error,
        "all_parsed_numeric_finite": all(
            math.isfinite(float(value)) for item in parsed for value in item.values()
            if isinstance(value, (int, float))),
    }


def find_one(manifest: list[dict[str, str]], band: str, profile: str, mode: str,
             altitude: float, range_km: float | None, visibility: float,
             sza: float | None) -> dict[str, str]:
    matches = []
    for row in manifest:
        if (row["band"] != band or row["humidity_profile"] != profile or row["mode"] != mode or
                float(row["target_alt_km"]) != altitude or float(row["visibility_km"]) != visibility):
            continue
        actual_range = None if not row["range_km"] else float(row["range_km"])
        actual_sza = None if not row["solar_zenith_deg"] else float(row["solar_zenith_deg"])
        if actual_range == range_km and actual_sza == sza:
            matches.append(row)
    if len(matches) != 1:
        raise ValueError(f"Expected one {band}/{profile}/{mode} alt={altitude} "
                         f"range={range_km} vis={visibility} sza={sza}; got {len(matches)}")
    return matches[0]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--mode", choices=("pilot", "grid"), default="pilot")
    ap.add_argument("--root", type=Path)
    args = ap.parse_args()
    root = (args.root or Path(f"logs/p11/modtran/humidity_{args.mode}")).resolve()
    manifest = read_csv(root / "case_manifest.csv")
    checks: list[dict[str, object]] = []

    def check(name: str, passed: bool, measured: object, expected: object) -> None:
        checks.append({"check": name, "passed": bool(passed),
                       "measured": measured, "expected": expected})

    expected_cases = 30 if args.mode == "pilot" else 504
    expected_vertices = 6 if args.mode == "pilot" else 216
    expected_profiles = {generator.humidity_profile(rh) for rh in generator.HUMIDITIES_PERCENT}
    check("component_manifest_count", len(manifest) == expected_cases, len(manifest), expected_cases)
    check("only_explicit_scaled_profiles",
          {r["humidity_profile"] for r in manifest} == expected_profiles,
          sorted({r["humidity_profile"] for r in manifest}), sorted(expected_profiles))
    check("no_default_or_fake_76p18_profile",
          all(r["humidity_profile"] != "default" and "76.18" not in r["humidity_profile"] for r in manifest),
          sorted({r["humidity_profile"] for r in manifest}), "30/60/85 only; default retained separately")

    input_failures = []
    card_failures = []
    for row in manifest:
        path = Path(row["input_file"])
        if not path.exists() or sha256(path) != row["input_sha256"]:
            input_failures.append(row["case_id"])
            continue
        lines = path.read_text(encoding="ascii").splitlines()
        card = lines[1]
        expected_scale = float(row["target_surface_rh_percent"]) / generator.MLS_SURFACE_RH_PERCENT
        ok = (len(card) == 110 and abs(float(card[20:30]) - expected_scale) <= 5.1e-7 and
              card[47] == "T" and abs(float(card[90:100]) - float(row["aerrh_percent"])) <= 5.1e-4)
        if not ok:
            card_failures.append(row["case_id"])
    check("all_input_hashes_match_manifest", not input_failures, input_failures, [])
    check("all_card1a_fixed_fields_round_trip", not card_failures, card_failures, [])

    run_rows = read_csv(root / "run_manifest.csv")
    executed = {r["case_id"] for r in run_rows}
    check("real_modtran_case_count", len(executed) == expected_cases, len(executed), expected_cases)
    check("all_manifest_cases_executed", executed == {r["case_id"] for r in manifest},
          sorted({r["case_id"] for r in manifest} - executed), [])
    engine_hashes = sorted({r["engine_sha256"] for r in run_rows})
    check("single_nonblank_engine_hash", len(engine_hashes) == 1 and len(engine_hashes[0]) == 64,
          engine_hashes, "one SHA-256")
    restore = read_csv(root / "installation_restore_evidence.csv")
    check("pcmodwin_fixed_files_restored",
          bool(restore) and all(r["restored_exactly"] == "True" for r in restore),
          [r["fixed_name"] for r in restore if r["restored_exactly"] != "True"], [])

    humidity_rows = []
    for row in manifest:
        if row["mode"] != "Transmittance":
            continue
        modout = Path(row["input_file"]).parent / "MODOUT1.txt"
        metrics = parse_modout1(modout)
        target_rh = float(row["target_surface_rh_percent"])
        scale = target_rh / generator.MLS_SURFACE_RH_PERCENT
        humidity_rows.append({
            "case_id": row["case_id"], "band": row["band"],
            "humidity_profile": row["humidity_profile"],
            "observer_alt_km": row["observer_alt_km"], "target_alt_km": row["target_alt_km"],
            "range_km": row["range_km"], "visibility_km": row["visibility_km"],
            "target_surface_rh_percent": target_rh, "h2ostr_scale": scale,
            "h2oaer": row["h2oaer"], "aerrh_percent": float(row["aerrh_percent"]), **metrics,
            "water_expected_from_mls_g_cm2": metrics["water_initial_g_cm2"] * scale,
        })
    water_failures = []
    for row in humidity_rows:
        altitude = float(row["observer_alt_km"])
        mean_aerosol_rh = float(row["mean_aerosol_rh_percent"])
        target_rh = float(row["target_surface_rh_percent"])
        # AERRH is explicitly a boundary-layer aerosol RH control, not a
        # declaration that every altitude has the same RH. A near-surface LOS
        # should match AERRH; a 1 km horizontal LOS samples the scaled vertical
        # profile and must respond monotonically without being forced to equal
        # the surface value.
        aerosol_response_ok = (
            abs(mean_aerosol_rh - target_rh) <= 0.05 if altitude <= 0.01
            else 0.0 < mean_aerosol_rh < target_rh
        )
        if not (
            abs(float(row["water_final_g_cm2"]) - float(row["water_input_g_cm2"])) <= 2e-5 and
            abs(float(row["water_input_g_cm2"]) - float(row["water_expected_from_mls_g_cm2"])) <= 2e-5 and
            abs(float(row["surface_rh_after_percent"]) - target_rh) <= 0.02 and
            aerosol_response_ok
        ):
            water_failures.append(row["case_id"])
    check("modout1_water_surface_and_altitude_aware_aerosol_response",
          not water_failures, water_failures, [])
    for key, group in itertools.groupby(sorted(humidity_rows, key=lambda r: (
            str(r["band"]), str(r["observer_alt_km"]), str(r["range_km"]),
            str(r["visibility_km"]), float(r["target_surface_rh_percent"]))),
            key=lambda r: (r["band"], r["observer_alt_km"], r["range_km"], r["visibility_km"])):
        values = list(group)
        check(f"modout1_monotonic_{key[0]}_alt{key[1]}_rng{key[2]}_vis{key[3]}",
              [r["target_surface_rh_percent"] for r in values] == [30.0, 60.0, 85.0] and
              all(values[i]["water_final_g_cm2"] < values[i + 1]["water_final_g_cm2"] and
                  values[i]["surface_rh_after_percent"] < values[i + 1]["surface_rh_after_percent"] and
                  values[i]["mean_aerosol_rh_percent"] < values[i + 1]["mean_aerosol_rh_percent"]
                  for i in range(2)),
              [[r["water_final_g_cm2"], r["surface_rh_after_percent"], r["mean_aerosol_rh_percent"]]
               for r in values], "strictly increases 30<60<85")

    integrated: dict[str, float] = {}
    sources: dict[str, Path] = {}
    spectral_rows = []
    for number, row in enumerate(manifest, 1):
        source, parsed = convert_case(row)
        sources[row["case_id"]] = source
        metrics = spectral_metrics(row, parsed)
        integrated[row["case_id"]] = float(metrics["response_mean_value_si"])
        metrics["source_file"] = str(source.resolve())
        metrics["source_sha256"] = sha256(source)
        metrics["spectrum_si_file"] = str((Path(row["input_file"]).parent / "spectrum_si.csv").resolve())
        metrics["spectrum_si_sha256"] = sha256(Path(metrics["spectrum_si_file"]))
        spectral_rows.append(metrics)
        if number % 50 == 0:
            print(f"Converted/audited {number}/{len(manifest)} component spectra", flush=True)
    boundary_failures = [r["case_id"] for r in spectral_rows if not r["boundary_bracketed"]]
    finite_failures = [r["case_id"] for r in spectral_rows if not r["all_parsed_numeric_finite"]]
    jacobian_failures = [r["case_id"] for r in spectral_rows
                         if float(r["jacobian_integral_relative_error"]) > 5e-6]
    pointwise_failures = [r["case_id"] for r in spectral_rows
                          if float(r["pointwise_conversion_max_relative_error"]) > 1e-12]
    check("all_spectra_bracket_exact_response", not boundary_failures, boundary_failures, [])
    check("all_spectra_finite", not finite_failures, finite_failures, [])
    check("all_jacobian_integrals_within_tolerance", not jacobian_failures,
          {"failures": jacobian_failures,
           "max_relative_error": max(float(r["jacobian_integral_relative_error"]) for r in spectral_rows)},
          "<=5e-6")
    check("all_pointwise_unit_conversions_exact", not pointwise_failures,
          {"failures": pointwise_failures,
           "max_relative_error": max(float(r["pointwise_conversion_max_relative_error"]) for r in spectral_rows)},
          "<=1e-12")

    if args.mode == "pilot":
        altitudes = (generator.PILOT_ALTITUDE_KM,)
        ranges = (generator.PILOT_RANGE_KM,)
        visibilities = (generator.PILOT_VISIBILITY_KM,)
        szas = (generator.PILOT_SZA_DEG,)
    else:
        altitudes = generator.ALTITUDES_KM
        ranges = generator.RANGES_KM
        visibilities = generator.VISIBILITIES_KM
        szas = generator.SOLAR_ZENITH_DEG

    formal_rows = []
    vertex_metrics = []
    for band, rh, altitude, range_km, visibility, sza in itertools.product(
            generator.BANDS, generator.HUMIDITIES_PERCENT, altitudes, ranges, visibilities, szas):
        profile = generator.humidity_profile(rh)
        components = [
            find_one(manifest, band, profile, "Transmittance", altitude, range_km, visibility, None),
            find_one(manifest, band, profile, "ThermalRadiance", altitude, range_km, visibility, None),
            find_one(manifest, band, profile, "RadianceWithScattering", altitude, range_km, visibility, sza),
            find_one(manifest, band, profile, "DirectSolarIrradiance", altitude, None, visibility, sza),
            find_one(manifest, band, profile, "SpectralFlux", altitude, None, visibility, sza),
        ]
        values = [integrated[r["case_id"]] for r in components]
        case_ids = [r["case_id"] for r in components]
        source_files = [str(sources[c].resolve()) for c in case_ids]
        formal_rows.append({
            "schema_version": "1",
            "case_id": (f"P11_{band}_humidity_{profile}_obs{token(altitude)}_tar{token(altitude)}_"
                        f"rng{token(range_km)}_vis{token(visibility)}_sza{token(sza)}"),
            "band": band, "atmosphere_model": "Mid-Latitude Summer", "aerosol_model": "Rural",
            "humidity_profile": profile, "visibility_km": f"{visibility:g}",
            "observer_alt_km": f"{altitude:g}", "target_alt_km": f"{altitude:g}",
            "range_km": f"{range_km:g}", "solar_zenith_deg": f"{sza:g}",
            "tau_up": f"{values[0]:.12g}", "path_thermal_W_m2_sr_um": f"{values[1]:.12g}",
            "direct_solar_irradiance_at_target_W_m2_um": f"{values[3]:.12g}",
            "downward_sky_diffuse_irradiance_W_m2_um": f"{values[4]:.12g}",
            "los_path_scattering_radiance_W_m2_sr_um": f"{values[2]:.12g}",
            "radiance_unit": "W/(m^2 sr um)", "irradiance_unit": "W/(m^2 um)",
            "tau_unit": "dimensionless", "response_mode": "RectangularBand",
            "conversion_method": "pointwise native*1e8/lambda_um^2 then wavelength-domain trapezoidal mean; exact endpoint interpolation within bracketing real samples",
            "modtran_source_fields": "COMBIN TRANS;PTH_THRML;SOL TR;.flx DOWNWARD;SOL_SCAT",
            "source_case_ids": ";".join(case_ids), "source_files": ";".join(source_files),
        })
        vertex_metrics.append({
            "band": band, "humidity_profile": profile, "target_surface_rh_percent": rh,
            "observer_alt_km": altitude, "target_alt_km": altitude, "range_km": range_km,
            "visibility_km": visibility, "solar_zenith_deg": sza, "tau_up": values[0],
            "path_thermal_W_m2_sr_um": values[1], "direct_solar_W_m2_um": values[3],
            "downward_sky_W_m2_um": values[4], "path_scattering_W_m2_sr_um": values[2],
        })
    check("formal_vertex_count", len(formal_rows) == expected_vertices,
          len(formal_rows), expected_vertices)
    numeric_fields = ["tau_up", "path_thermal_W_m2_sr_um",
                      "direct_solar_irradiance_at_target_W_m2_um",
                      "downward_sky_diffuse_irradiance_W_m2_um",
                      "los_path_scattering_radiance_W_m2_sr_um"]
    check("all_vertices_have_five_finite_nonnegative_components",
          all(r[field] != "" and math.isfinite(float(r[field])) and float(r[field]) >= 0
              for r in formal_rows for field in numeric_fields), "all rows", "five finite values >=0")
    check("all_tau_in_unit_interval",
          all(0 <= float(r["tau_up"]) <= 1 for r in formal_rows),
          [min(float(r["tau_up"]) for r in formal_rows), max(float(r["tau_up"]) for r in formal_rows)],
          "0<=tau<=1")

    # Humidity response must be physically present; never accept three labels
    # backed by identical spectra. We require strict tau reduction with RH.
    for band, altitude, range_km, visibility in itertools.product(
            generator.BANDS, altitudes, ranges, visibilities):
        values = []
        for rh in generator.HUMIDITIES_PERCENT:
            profile = generator.humidity_profile(rh)
            values.append(next(float(r["tau_up"]) for r in formal_rows
                               if r["band"] == band and r["humidity_profile"] == profile and
                               float(r["observer_alt_km"]) == altitude and
                               float(r["range_km"]) == range_km and
                               float(r["visibility_km"]) == visibility))
        check(f"tau_humidity_response_{band}_alt{altitude:g}_rng{range_km:g}_vis{visibility:g}",
              values[0] > values[1] > values[2], values, "tau(RH30)>tau(RH60)>tau(RH85)")
    if args.mode == "grid":
        for band, rh, altitude, visibility in itertools.product(
                generator.BANDS, generator.HUMIDITIES_PERCENT, altitudes, visibilities):
            profile = generator.humidity_profile(rh)
            values = [next(float(r["tau_up"]) for r in formal_rows
                           if r["band"] == band and r["humidity_profile"] == profile and
                           float(r["observer_alt_km"]) == altitude and float(r["range_km"]) == distance and
                           float(r["visibility_km"]) == visibility)
                      for distance in ranges]
            check(f"tau_range_response_{band}_{profile}_alt{altitude:g}_vis{visibility:g}",
                  values[0] >= values[1] >= values[2], values, "tau(0.1)>=tau(0.5)>=tau(1.0)")
        for band, rh, altitude, visibility in itertools.product(
                generator.BANDS, generator.HUMIDITIES_PERCENT, altitudes, visibilities):
            profile = generator.humidity_profile(rh)
            values = [next(float(r["direct_solar_irradiance_at_target_W_m2_um"]) for r in formal_rows
                           if r["band"] == band and r["humidity_profile"] == profile and
                           float(r["observer_alt_km"]) == altitude and
                           float(r["visibility_km"]) == visibility and
                           float(r["solar_zenith_deg"]) == sza) for sza in szas]
            check(f"direct_solar_sza_response_{band}_{profile}_alt{altitude:g}_vis{visibility:g}",
                  values[0] > values[1] > values[2], values, "direct(20)>direct(45)>direct(70)")

    humidity_fields = list(humidity_rows[0])
    with (root / "modout1_humidity_metrics.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=humidity_fields); writer.writeheader(); writer.writerows(humidity_rows)
    with (root / "spectral_qc.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(spectral_rows[0])); writer.writeheader(); writer.writerows(spectral_rows)
    with (root / "vertex_metrics.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(vertex_metrics[0])); writer.writeheader(); writer.writerows(vertex_metrics)
    output_name = "formal_humidity_rows.csv" if args.mode == "grid" else "pilot_candidate_rows.csv"
    with (root / output_name).open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=FORMAL_FIELDS); writer.writeheader(); writer.writerows(formal_rows)

    status = "PASS" if all(c["passed"] for c in checks) else "FAIL"
    (root / "humidity_qc_results.json").write_text(
        json.dumps({"status": status, "checks": checks}, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8")
    with (root / "humidity_qc_results.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["check", "passed", "measured", "expected"])
        writer.writeheader(); writer.writerows(checks)

    manual = Path("F:/Programs/PcModWin5/MODTRAN_R_5.2.1.pdf")
    dependencies = [
        Path(__file__).resolve(), TOOLS / "p11_modtran_humidity_grid.py",
        TOOLS / "p11_modtran_swir_pilot.ps1", TOOLS / "modtran_convert_to_si.py",
    ]
    with (root / "source_hashes.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["kind", "path", "size_bytes", "sha256"])
        writer.writeheader()
        for kind, path in [("manual", manual)] + [("code", p) for p in dependencies]:
            writer.writerow({"kind": kind, "path": str(path), "size_bytes": path.stat().st_size,
                             "sha256": sha256(path)})

    report = [
        f"# P11 MODTRAN Humidity {args.mode.title()}", "", f"- status: {status}",
        f"- real component runs: {expected_cases}; complete five-component vertices: {expected_vertices}",
        "- profiles: scaled_mls_surface_rh30, scaled_mls_surface_rh60, scaled_mls_surface_rh85",
        "- semantics: H2OSTR = requested surface RH / 76.18% MLS reference; H2OAER=T; AERRH=requested RH",
        "- important: profile labels describe scaled MLS columns targeting the stated surface RH; they do not assert constant RH throughout the vertical profile",
        "- AERRH is a boundary-layer control: near-ground path mean aerosol RH matches the requested value, while a 1 km horizontal LOS reports the lower altitude-dependent profile mean; QC requires the latter to remain positive, below surface RH, and strictly ordered",
        "- default rows: deliberately absent from this isolated product and preserved unchanged in the production LUT",
        "- MODTRAN manual evidence: MODTRAN_R_5.2.1.pdf p16 CARD1A layout, p17 H2OSTR behavior, p19 H2OAER response, p20 AERRH definition (PDF page numbers)",
        "- spectral handling: real native samples bracket each band; per-cm^-1 radiance/irradiance uses the exact pointwise Jacobian; integration is in wavelength with interpolated exact response endpoints",
        "- exclusion: TOTAL_RAD and target surface/reflection columns are not used as target-independent path terms",
        "", "## MODOUT1 response", "",
    ]
    for row in humidity_rows[:18 if args.mode == "pilot" else 12]:
        report.append(
            f"- {row['case_id']}: water {row['water_initial_g_cm2']:.5g}->{row['water_final_g_cm2']:.5g} g/cm2; "
            f"surface RH {row['surface_rh_after_percent']:.5g}%; mean aerosol RH {row['mean_aerosol_rh_percent']:.5g}%")
    report += ["", "## Checks", ""]
    report.extend(f"- {'PASS' if c['passed'] else 'FAIL'} {c['check']}: {c['measured']}" for c in checks)
    (root / "README.md").write_text("\n".join(report) + "\n", encoding="utf-8")

    inventory = []
    for path in sorted(p for p in root.rglob("*") if p.is_file() and p.name != "data_inventory.csv"):
        inventory.append({"relative_path": str(path.relative_to(root)),
                          "size_bytes": path.stat().st_size, "sha256": sha256(path)})
    with (root / "data_inventory.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(inventory[0])); writer.writeheader(); writer.writerows(inventory)
    print(f"P11 MODTRAN humidity {args.mode} QC: {status}; checks={len(checks)}")
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
