#!/usr/bin/env python3
"""Convert, unit-check, integrate and assemble the real P11 3--5 um ground grid."""

from __future__ import annotations

import argparse
import csv
import hashlib
import itertools
import json
import math
import sys
from pathlib import Path


TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
import modtran_convert_to_si as convert  # noqa: E402


LO = 3.0
HI = 5.0
WIDTH = HI - LO
RANGES = (0.1, 0.5, 1.0)
VISIBILITIES = (6.0, 23.0)
SZAS = (20.0, 45.0, 70.0)
ALTITUDES = (0.001, 1.0)
FIELDS = [
    "schema_version", "case_id", "band", "atmosphere_model", "aerosol_model",
    "humidity_profile", "visibility_km", "observer_alt_km", "target_alt_km", "range_km",
    "solar_zenith_deg", "tau_up", "path_thermal_W_m2_sr_um",
    "direct_solar_irradiance_at_target_W_m2_um", "downward_sky_diffuse_irradiance_W_m2_um",
    "los_path_scattering_radiance_W_m2_sr_um", "radiance_unit", "irradiance_unit", "tau_unit",
    "response_mode", "conversion_method", "modtran_source_fields", "source_case_ids", "source_files",
]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def token(value: float) -> str:
    return f"{value:g}".replace(".", "p")


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as f:
        return list(csv.DictReader(f))


def clipped(rows: list[dict[str, str]]) -> list[dict[str, str]]:
    return sorted((r for r in rows if LO - 1e-12 <= float(r["wavelength_um"]) <= HI + 1e-12),
                  key=lambda r: float(r["wavelength_um"]))


def trapz(rows: list[dict[str, str]], x: str, y: str) -> float:
    points = sorted((float(r[x]), float(r[y])) for r in rows if r.get(y, "") != "")
    return sum(0.5 * (y0 + y1) * (x1 - x0)
               for (x0, y0), (x1, y1) in zip(points, points[1:]))


def relative_error(actual: float, expected: float) -> float:
    return abs(actual - expected) / max(abs(expected), 1e-30)


def response_integral(rows: list[dict[str, str]], field: str) -> float:
    """Integrate in wavelength with exact endpoint interpolation and no extrapolation."""
    points = sorted((float(r["wavelength_um"]), float(r[field]))
                    for r in rows if r.get(field, "") != "")
    if not points or points[0][0] > LO or points[-1][0] < HI:
        raise ValueError(f"{field} lacks bracketing samples for {LO}--{HI} um")

    def at(x: float) -> float:
        for px, py in points:
            if abs(px - x) <= 1e-14:
                return py
        for (x0, y0), (x1, y1) in zip(points, points[1:]):
            if x0 < x < x1:
                return y0 + (y1 - y0) * (x - x0) / (x1 - x0)
        raise ValueError(f"cannot interpolate {field} at {x}")

    band = [(x, y) for x, y in points if LO < x < HI]
    band.insert(0, (LO, at(LO)))
    band.append((HI, at(HI)))
    return sum(0.5 * (y0 + y1) * (x1 - x0)
               for (x0, y0), (x1, y1) in zip(band, band[1:]))


def find_one(rows: list[dict[str, str]], mode: str, range_km: float | None,
             altitude: float, visibility: float, sza: float | None) -> dict[str, str]:
    matches = []
    for row in rows:
        if (row["mode"] != mode or float(row["visibility_km"]) != visibility or
                float(row["target_alt_km"]) != altitude):
            continue
        row_range = None if not row["range_km"] else float(row["range_km"])
        row_sza = None if not row["solar_zenith_deg"] else float(row["solar_zenith_deg"])
        if row_range == range_km and row_sza == sza:
            matches.append(row)
    if len(matches) != 1:
        raise ValueError(f"Expected one {mode} alt={altitude} range={range_km} vis={visibility} sza={sza}; got {len(matches)}")
    return matches[0]


def convert_case(row: dict[str, str]) -> tuple[Path, list[dict[str, str]]]:
    case_dir = Path(row["input_file"]).parent
    mode = row["mode"]
    if mode == "SpectralFlux":
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
        }[mode]
        parsed = parser(source)
    output = case_dir / "spectrum_si.csv"
    convert.write_rows(output, parsed, source, row["case_id"])
    return source, read_csv(output)


def jacobian_error(rows: list[dict[str, str]], native: str, si: str) -> float:
    band = clipped(rows)
    native_area = trapz(band, "wavenumber_cm1", native) * 1.0e4
    si_area = trapz(band, "wavelength_um", si)
    return relative_error(si_area, native_area)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=Path("logs/p11/modtran/mwir_ground_grid"))
    ap.add_argument("--formal", type=Path,
                    default=Path("HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv"))
    args = ap.parse_args()
    root = args.root.resolve()
    formal_path = args.formal.resolve()
    manifest = read_csv(root / "case_manifest.csv")
    checks: list[dict[str, object]] = []
    metrics: list[dict[str, object]] = []

    def check(name: str, passed: bool, measured: object, expected: object) -> None:
        checks.append({"check": name, "passed": bool(passed), "measured": measured, "expected": expected})

    check("component_manifest_count", len(manifest) == 84, len(manifest), 84)
    run_rows = read_csv(root / "run_manifest.csv")
    check("real_run_case_count", len({r["case_id"] for r in run_rows}) == 84,
          len({r["case_id"] for r in run_rows}), 84)
    restore = read_csv(root / "installation_restore_evidence.csv")
    check("pcmodwin_fixed_files_restored", all(r["restored_exactly"] == "True" for r in restore),
          [r["fixed_name"] for r in restore if r["restored_exactly"] != "True"], [])

    # Record why the pre-P11 MWIR table is insufficient without altering it.
    formal_rows = read_csv(formal_path)
    legacy = [r for r in formal_rows if r["band"] == "MWIR" and not r["case_id"].startswith("P11_MWIR_ground_")]
    audit = {
        "legacy_rows": len(legacy),
        "observer_altitude_min_km": min(float(r["observer_alt_km"]) for r in legacy),
        "target_altitude_min_km": min(float(r["target_alt_km"]) for r in legacy),
        "range_min_km": min(float(r["range_km"]) for r in legacy),
        "blank_solar_zenith_rows": sum(not r["solar_zenith_deg"] for r in legacy),
        "blank_direct_solar_rows": sum(not r["direct_solar_irradiance_at_target_W_m2_um"] for r in legacy),
        "blank_sky_diffuse_rows": sum(not r["downward_sky_diffuse_irradiance_W_m2_um"] for r in legacy),
        "blank_path_scattering_rows": sum(not r["los_path_scattering_radiance_W_m2_sr_um"] for r in legacy),
        "reuse_decision": "tau/path-thermal audit only; rejected for formal reflected-solar input",
    }
    (root / "legacy_mwir_335_audit.json").write_text(json.dumps(audit, indent=2) + "\n", encoding="utf-8")
    check("legacy_rows_audited", len(legacy) == 335, len(legacy), 335)
    check("legacy_not_closeup", audit["observer_altitude_min_km"] >= 3.0 and audit["range_min_km"] >= 1.0,
          [audit["observer_altitude_min_km"], audit["target_altitude_min_km"], audit["range_min_km"]],
          "minimum altitude 3 km and range 1 km")
    check("legacy_lacks_formal_solar_components",
          all(not r["direct_solar_irradiance_at_target_W_m2_um"] and
              not r["downward_sky_diffuse_irradiance_W_m2_um"] and
              not r["los_path_scattering_radiance_W_m2_sr_um"] for r in legacy),
          [audit["blank_direct_solar_rows"], audit["blank_sky_diffuse_rows"], audit["blank_path_scattering_rows"]],
          [335, 335, 335])

    spectra: dict[str, list[dict[str, str]]] = {}
    sources: dict[str, Path] = {}
    for row in manifest:
        case_id = row["case_id"]
        stderr = Path(row["input_file"]).parent / "engine_stderr.txt"
        check(f"{case_id}_stderr_empty", stderr.exists() and stderr.stat().st_size == 0,
              stderr.stat().st_size if stderr.exists() else "missing", 0)
        source, parsed = convert_case(row)
        sources[case_id] = source
        spectra[case_id] = parsed
        wavelengths = [float(item["wavelength_um"]) for item in parsed]
        check(f"{case_id}_boundary_support", min(wavelengths) <= LO and max(wavelengths) >= HI,
              [min(wavelengths), max(wavelengths)], "brackets exact 3.0--5.0 um")
        if row["mode"] == "ThermalRadiance":
            err = jacobian_error(parsed, "path_thermal_native", "path_thermal_W_m2_sr_um")
            check(f"{case_id}_jacobian", err <= 5e-6, err, "<=5e-6")
        elif row["mode"] == "RadianceWithScattering":
            err = jacobian_error(parsed, "solar_scatter_native", "solar_scatter_W_m2_sr_um")
            check(f"{case_id}_jacobian", err <= 5e-6, err, "<=5e-6")
        elif row["mode"] == "DirectSolarIrradiance":
            err = jacobian_error(parsed, "direct_solar_native", "direct_solar_W_m2_um")
            check(f"{case_id}_jacobian", err <= 5e-6, err, "<=5e-6")
        elif row["mode"] == "SpectralFlux":
            band = clipped(parsed)
            native_area = trapz(band, "wavelength_nm", "downward_diffuse_native_W_cm2_nm") * 1e4
            si_area = trapz(band, "wavelength_um", "downward_diffuse_W_m2_um")
            err = relative_error(si_area, native_area)
            check(f"{case_id}_flux_units", err <= 1e-12, err, "<=1e-12")

    lut_rows: list[dict[str, object]] = []
    tau_values: dict[tuple[float, float, float], float] = {}
    solar_values: dict[tuple[float, float, float], float] = {}
    for altitude, range_km, visibility, sza in itertools.product(ALTITUDES, RANGES, VISIBILITIES, SZAS):
        trans = find_one(manifest, "Transmittance", range_km, altitude, visibility, None)
        thermal = find_one(manifest, "ThermalRadiance", range_km, altitude, visibility, None)
        scatter = find_one(manifest, "RadianceWithScattering", range_km, altitude, visibility, sza)
        solar = find_one(manifest, "DirectSolarIrradiance", None, altitude, visibility, sza)
        flux = find_one(manifest, "SpectralFlux", None, altitude, visibility, sza)
        tau = response_integral(spectra[trans["case_id"]], "tau_los") / WIDTH
        thermal_value = response_integral(spectra[thermal["case_id"]], "path_thermal_W_m2_sr_um") / WIDTH
        scatter_value = response_integral(spectra[scatter["case_id"]], "solar_scatter_W_m2_sr_um") / WIDTH
        direct = response_integral(spectra[solar["case_id"]], "direct_solar_W_m2_um") / WIDTH
        sky = response_integral(spectra[flux["case_id"]], "downward_diffuse_W_m2_um") / WIDTH
        tau_values[(altitude, range_km, visibility)] = tau
        solar_values[(altitude, visibility, sza)] = direct
        components = [trans, thermal, scatter, solar, flux]
        case_ids = [r["case_id"] for r in components]
        source_paths = [str(sources[c].resolve()) for c in case_ids]
        lut_rows.append({
            "schema_version": "1",
            "case_id": f"P11_MWIR_ground_obs{token(altitude)}_tar{token(altitude)}_rng{range_km:g}_vis{visibility:g}_sza{sza:g}",
            "band": "MWIR", "atmosphere_model": "Mid-Latitude Summer", "aerosol_model": "Rural",
            "humidity_profile": "default", "visibility_km": f"{visibility:g}",
            "observer_alt_km": f"{altitude:g}", "target_alt_km": f"{altitude:g}",
            "range_km": f"{range_km:g}", "solar_zenith_deg": f"{sza:g}", "tau_up": f"{tau:.12g}",
            "path_thermal_W_m2_sr_um": f"{thermal_value:.12g}",
            "direct_solar_irradiance_at_target_W_m2_um": f"{direct:.12g}",
            "downward_sky_diffuse_irradiance_W_m2_um": f"{sky:.12g}",
            "los_path_scattering_radiance_W_m2_sr_um": f"{scatter_value:.12g}",
            "radiance_unit": "W/(m^2 sr um)", "irradiance_unit": "W/(m^2 um)",
            "tau_unit": "dimensionless", "response_mode": "RectangularBand",
            "conversion_method": "pointwise native*1e8/lambda_um^2 then wavelength-domain trapezoidal mean; exact endpoint interpolation within bracketing real samples",
            "modtran_source_fields": "COMBIN TRANS;PTH_THRML;SOL TR;.flx DOWNWARD;SOL_SCAT",
            "source_case_ids": ";".join(case_ids), "source_files": ";".join(source_paths),
        })
        metrics.append({
            "observer_alt_km": altitude, "target_alt_km": altitude,
            "range_km": range_km, "visibility_km": visibility, "solar_zenith_deg": sza,
            "tau_up": tau, "path_thermal_W_m2_sr_um": thermal_value,
            "direct_solar_W_m2_um": direct, "downward_sky_W_m2_um": sky,
            "path_scattering_W_m2_sr_um": scatter_value,
        })

    expected_vertices = set(itertools.product(ALTITUDES, RANGES, VISIBILITIES, SZAS))
    actual_vertices = {(float(r["observer_alt_km"]), float(r["range_km"]),
                        float(r["visibility_km"]), float(r["solar_zenith_deg"])) for r in lut_rows}
    check("formal_vertex_count", len(lut_rows) == 36, len(lut_rows), 36)
    check("complete_cartesian_vertices", actual_vertices == expected_vertices,
          sorted(actual_vertices), sorted(expected_vertices))
    value_fields = ["tau_up", "path_thermal_W_m2_sr_um", "direct_solar_irradiance_at_target_W_m2_um",
                    "downward_sky_diffuse_irradiance_W_m2_um", "los_path_scattering_radiance_W_m2_sr_um"]
    check("all_formal_values_finite_nonnegative",
          all(math.isfinite(float(r[f])) and float(r[f]) >= 0 for r in lut_rows for f in value_fields),
          "all rows", "finite and >=0")
    check("all_formal_five_components_explicit",
          all(str(r[f]) != "" for r in lut_rows for f in value_fields), "all rows", "all five nonblank")
    for altitude in ALTITUDES:
        for visibility in VISIBILITIES:
            values = [tau_values[(altitude, distance, visibility)] for distance in RANGES]
            check(f"tau_nonincreasing_range_alt{altitude:g}_vis{visibility:g}",
                  values[0] >= values[1] >= values[2], values, "tau(0.1)>=tau(0.5)>=tau(1.0)")
        for distance in RANGES:
            values = [tau_values[(altitude, distance, visibility)] for visibility in VISIBILITIES]
            check(f"tau_visibility_effect_alt{altitude:g}_range{distance:g}",
                  values[0] < values[1], values, "tau(vis6)<tau(vis23)")
        for visibility in VISIBILITIES:
            values = [solar_values[(altitude, visibility, sza)] for sza in SZAS]
            check(f"direct_solar_sza_response_alt{altitude:g}_vis{visibility:g}",
                  values[0] > values[1] > values[2], values, "direct(20)>direct(45)>direct(70)")

    with (root / "formal_mwir_rows.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDS); writer.writeheader(); writer.writerows(lut_rows)
    with (root / "vertex_metrics.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(metrics[0])); writer.writeheader(); writer.writerows(metrics)
    status = "PASS" if all(r["passed"] for r in checks) else "FAIL"
    (root / "grid_qc_results.json").write_text(
        json.dumps({"status": status, "checks": checks}, indent=2) + "\n", encoding="utf-8")
    with (root / "grid_qc_results.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["check", "passed", "measured", "expected"])
        writer.writeheader(); writer.writerows(checks)

    lines = [
        "# P11 Formal MWIR Ground Grid", "", f"- status: {status}",
        "- axes: equal target/observer altitude={0.001,1.0} km, range={0.1,0.5,1} km, visibility={6,23} km, SZA={20,45,70} deg",
        "- formal vertices: 36 complete declared cells; raw component executions: 84",
        "- spectrum: real MODTRAN5 1999--3334 cm^-1 at 1 cm^-1, integrated only over exact 3.0--5.0 um rectangular response",
        "- units: MODOUT native per-cm^-1 converted pointwise using 1e8/lambda_um^2; flux W/(cm2 nm) converted using 1e7",
        "- flux altitude: 0.001 km is linearly interpolated between real 0 and 1 km flux levels; 1.0 km uses the exact real level",
        "- provenance: every formal row lists all five source case IDs and raw files",
        "- exclusions: TOTAL_RAD and target surface/reflection columns are never used as path terms",
        "- legacy audit: original 335 MWIR rows retain useful tau/path-thermal provenance but lack near-ground geometry and all solar/sky/scattering fields, so they are not formal reflected-solar inputs",
        "- runtime requirement: production MWIR query must retain solarZenithDeg as the fifth interpolation axis and reject blank solar components",
        "- scope: range/visibility/SZA interpolate inside each declared plane; the runtime may couple the two equal-altitude planes for a horizontal intermediate-altitude query, while non-horizontal cells remain absent and altitude extrapolation is invalid",
        "", "## Checks", "",
    ]
    lines.extend(f"- {'PASS' if r['passed'] else 'FAIL'} {r['check']}: {r['measured']}" for r in checks)
    (root / "README.md").write_text("\n".join(lines) + "\n", encoding="utf-8")

    repo_root = TOOLS.parent
    dependencies = []
    for rel in [
        "tools/p11_modtran_mwir_grid.py", "tools/p11_modtran_swir_pilot.ps1",
        "tools/p11_modtran_mwir_grid_qc.py", "tools/modtran_convert_to_si.py",
        "tools/p11_modtran_publish_mwir.py", "tools/p11_modtran_mwir_grid_query.cpp",
        "tools/p11_modtran_mwir_grid_query_check.ps1",
        "tools/modtran_build_lut.py", "tools/modtran_qc.py",
        "HwaSim_IR/HwaSim_IR/IR/IRModtranRadianceLut.cpp",
        "HwaSim_IR/HwaSim_IR/IR/IRModtranRadianceLut.h",
    ]:
        path = repo_root / rel
        if path.exists():
            dependencies.append({"path": rel, "size_bytes": path.stat().st_size, "sha256": sha256(path)})
    with (root / "code_dependencies.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(dependencies[0])); writer.writeheader(); writer.writerows(dependencies)
    (root / "formal_lut_reference.json").write_text(json.dumps({
        "path": str(formal_path), "sha256": sha256(formal_path), "expected_rows": 1394,
        "expected_nir_rows": 1005, "expected_swir_rows": 18, "expected_mwir_rows": 371,
        "expected_p11_five_component_mwir_rows": 36,
    }, indent=2) + "\n", encoding="utf-8")
    inventory = []
    for path in sorted(p for p in root.rglob("*") if p.is_file() and p.name != "data_inventory.csv"):
        inventory.append({"relative_path": str(path.relative_to(root)), "size_bytes": path.stat().st_size,
                          "sha256": sha256(path)})
    with (root / "data_inventory.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(inventory[0])); writer.writeheader(); writer.writerows(inventory)
    print(f"P11 formal MWIR ground grid QC: {status}; checks={len(checks)}; vertices={len(lut_rows)}")
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
