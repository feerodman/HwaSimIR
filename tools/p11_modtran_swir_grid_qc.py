#!/usr/bin/env python3
"""Convert, QC and assemble the real MODTRAN P11 SWIR ground-grid LUT rows."""

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
import p11_modtran_swir_qc as common  # noqa: E402


RANGES = (0.1, 0.5, 1.0)
VISIBILITIES = (6.0, 23.0)
SZAS = (20.0, 45.0, 70.0)
ALTITUDE_KM = 0.001
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


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as f:
        return list(csv.DictReader(f))


def find_one(rows: list[dict[str, str]], mode: str, range_km: float | None,
             visibility: float, sza: float | None) -> dict[str, str]:
    matches = []
    for row in rows:
        if row["mode"] != mode or float(row["visibility_km"]) != visibility:
            continue
        row_range = None if not row["range_km"] else float(row["range_km"])
        row_sza = None if not row["solar_zenith_deg"] else float(row["solar_zenith_deg"])
        if row_range == range_km and row_sza == sza:
            matches.append(row)
    if len(matches) != 1:
        raise ValueError(f"Expected one {mode} range={range_km} vis={visibility} sza={sza}; got {len(matches)}")
    return matches[0]


def convert_case(row: dict[str, str]) -> tuple[Path, list[dict[str, str]]]:
    case_dir = Path(row["input_file"]).parent
    mode = row["mode"]
    if mode == "SpectralFlux":
        source = case_dir / "spectral_flux.flx"
        # PcModWin's standard MLS flux table has levels at 0 and 1 km.  The
        # close-up grid is at 0.001 km so interpolate vertically between two
        # real levels instead of silently relabeling the ground value.
        low = convert.parse_flux(source, 0.0)
        high = convert.parse_flux(source, 1.0)
        if len(low) != len(high):
            raise ValueError(f"Flux level row mismatch in {source}")
        parsed = []
        t = ALTITUDE_KM
        for a, b in zip(low, high):
            if abs(float(a["wavelength_um"]) - float(b["wavelength_um"])) > 1e-12:
                raise ValueError(f"Flux wavelength mismatch in {source}")
            item = dict(a)
            for field in [
                "upward_diffuse_native_W_cm2_nm", "downward_diffuse_native_W_cm2_nm",
                "direct_solar_native_W_cm2_nm", "upward_diffuse_W_m2_um",
                "downward_diffuse_W_m2_um", "direct_solar_W_m2_um",
            ]:
                item[field] = float(a[field]) + (float(b[field]) - float(a[field])) * t
            item["altitude_km"] = ALTITUDE_KM
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
    band = common.clipped(rows)
    native_area = common.trapz(band, "wavenumber_cm1", native) * 1.0e4
    si_area = common.trapz(band, "wavelength_um", si)
    return common.relative_error(si_area, native_area)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=Path("logs/p11/modtran/swir_ground_grid"))
    args = ap.parse_args()
    root = args.root.resolve()
    manifest = read_csv(root / "case_manifest.csv")
    checks: list[dict[str, object]] = []
    metrics: list[dict[str, object]] = []

    def check(name: str, passed: bool, measured: object, expected: object) -> None:
        checks.append({"check": name, "passed": bool(passed), "measured": measured, "expected": expected})

    check("component_manifest_count", len(manifest) == 42, len(manifest), 42)
    run_rows = read_csv(root / "run_manifest.csv")
    check("real_run_case_count", len({row["case_id"] for row in run_rows}) == 42,
          len({row["case_id"] for row in run_rows}), 42)
    restore = read_csv(root / "installation_restore_evidence.csv")
    check("pcmodwin_fixed_files_restored", all(row["restored_exactly"] == "True" for row in restore),
          [row["fixed_name"] for row in restore if row["restored_exactly"] != "True"], [])

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
        check(f"{case_id}_boundary_support", min(wavelengths) <= 1.1 and max(wavelengths) >= 2.5,
              [min(wavelengths), max(wavelengths)], "covers 1.1--2.5 um")
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
            band = common.clipped(parsed)
            native_area = common.trapz(band, "wavelength_nm", "downward_diffuse_native_W_cm2_nm") * 1e4
            si_area = common.trapz(band, "wavelength_um", "downward_diffuse_W_m2_um")
            err = common.relative_error(si_area, native_area)
            check(f"{case_id}_flux_units", err <= 1e-12, err, "<=1e-12")

    lut_rows: list[dict[str, object]] = []
    tau_values: dict[tuple[float, float], float] = {}
    solar_values: dict[tuple[float, float], float] = {}
    for range_km, visibility, sza in itertools.product(RANGES, VISIBILITIES, SZAS):
        trans = find_one(manifest, "Transmittance", range_km, visibility, None)
        thermal = find_one(manifest, "ThermalRadiance", range_km, visibility, None)
        scatter = find_one(manifest, "RadianceWithScattering", range_km, visibility, sza)
        solar = find_one(manifest, "DirectSolarIrradiance", None, visibility, sza)
        flux = find_one(manifest, "SpectralFlux", None, visibility, sza)
        tau = common.response_integral(spectra[trans["case_id"]], "tau_los") / common.WIDTH
        path_thermal = common.response_integral(
            spectra[thermal["case_id"]], "path_thermal_W_m2_sr_um") / common.WIDTH
        path_scatter = common.response_integral(
            spectra[scatter["case_id"]], "solar_scatter_W_m2_sr_um") / common.WIDTH
        direct = common.response_integral(
            spectra[solar["case_id"]], "direct_solar_W_m2_um") / common.WIDTH
        sky = common.response_integral(
            spectra[flux["case_id"]], "downward_diffuse_W_m2_um") / common.WIDTH
        tau_values[(range_km, visibility)] = tau
        solar_values[(visibility, sza)] = direct
        component_rows = [trans, thermal, scatter, solar, flux]
        case_ids = [row["case_id"] for row in component_rows]
        source_paths = [str(sources[case_id].resolve()) for case_id in case_ids]
        lut_rows.append({
            "schema_version": "1",
            "case_id": f"P11_SWIR_ground_obs0p001_tar0p001_rng{range_km:g}_vis{visibility:g}_sza{sza:g}",
            "band": "SWIR", "atmosphere_model": "Mid-Latitude Summer", "aerosol_model": "Rural",
            "humidity_profile": "default", "visibility_km": f"{visibility:g}",
            "observer_alt_km": f"{ALTITUDE_KM:g}", "target_alt_km": f"{ALTITUDE_KM:g}", "range_km": f"{range_km:g}",
            "solar_zenith_deg": f"{sza:g}", "tau_up": f"{tau:.12g}",
            "path_thermal_W_m2_sr_um": f"{path_thermal:.12g}",
            "direct_solar_irradiance_at_target_W_m2_um": f"{direct:.12g}",
            "downward_sky_diffuse_irradiance_W_m2_um": f"{sky:.12g}",
            "los_path_scattering_radiance_W_m2_sr_um": f"{path_scatter:.12g}",
            "radiance_unit": "W/(m^2 sr um)", "irradiance_unit": "W/(m^2 um)",
            "tau_unit": "dimensionless", "response_mode": "RectangularBand",
            "conversion_method": "pointwise native*1e8/lambda_um^2 then wavelength-domain trapezoidal mean; exact endpoint interpolation within bracketing real samples",
            "modtran_source_fields": "COMBIN TRANS;PTH_THRML;SOL TR;.flx DOWNWARD;SOL_SCAT",
            "source_case_ids": ";".join(case_ids), "source_files": ";".join(source_paths),
        })
        metrics.append({
            "range_km": range_km, "visibility_km": visibility, "solar_zenith_deg": sza,
            "tau_up": tau, "path_thermal_W_m2_sr_um": path_thermal,
            "direct_solar_W_m2_um": direct, "downward_sky_W_m2_um": sky,
            "path_scattering_W_m2_sr_um": path_scatter,
        })

    check("formal_vertex_count", len(lut_rows) == 18, len(lut_rows), 18)
    actual_vertices = {(float(r["range_km"]), float(r["visibility_km"]), float(r["solar_zenith_deg"])) for r in lut_rows}
    expected_vertices = set(itertools.product(RANGES, VISIBILITIES, SZAS))
    check("complete_cartesian_vertices", actual_vertices == expected_vertices,
          sorted(actual_vertices), sorted(expected_vertices))
    check("all_formal_values_finite_nonnegative",
          all(math.isfinite(float(row[field])) and float(row[field]) >= 0 for row in lut_rows
              for field in ["tau_up", "path_thermal_W_m2_sr_um",
                            "direct_solar_irradiance_at_target_W_m2_um",
                            "downward_sky_diffuse_irradiance_W_m2_um",
                            "los_path_scattering_radiance_W_m2_sr_um"]), "all rows", "finite and >=0")
    for visibility in VISIBILITIES:
        values = [tau_values[(range_km, visibility)] for range_km in RANGES]
        check(f"tau_nonincreasing_range_vis{visibility:g}", values[0] >= values[1] >= values[2], values,
              "tau(0.1)>=tau(0.5)>=tau(1.0)")
    for range_km in RANGES:
        values = [tau_values[(range_km, visibility)] for visibility in VISIBILITIES]
        check(f"tau_visibility_effect_range{range_km:g}", values[0] < values[1], values, "tau(vis6)<tau(vis23)")
    for visibility in VISIBILITIES:
        values = [solar_values[(visibility, sza)] for sza in SZAS]
        check(f"direct_solar_sza_response_vis{visibility:g}", values[0] > values[1] > values[2], values,
              "direct(20)>direct(45)>direct(70)")

    with (root / "formal_swir_rows.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDS)
        writer.writeheader(); writer.writerows(lut_rows)
    with (root / "vertex_metrics.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(metrics[0]))
        writer.writeheader(); writer.writerows(metrics)
    status = "PASS" if all(row["passed"] for row in checks) else "FAIL"
    (root / "grid_qc_results.json").write_text(
        json.dumps({"status": status, "checks": checks}, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    with (root / "grid_qc_results.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["check", "passed", "measured", "expected"])
        writer.writeheader(); writer.writerows(checks)
    lines = [
        "# P11 Formal SWIR Ground Grid",
        "", f"- status: {status}",
        "- axes: targetAlt=0.001 km, observerAlt=0.001 km, range={0.1,0.5,1} km, visibility={6,23} km, SZA={20,45,70} deg",
        "- formal vertices: 18 complete Cartesian cells; raw component executions: 42",
        "- spectrum: real MODTRAN5 4000--9090.9091 cm^-1 at 1 cm^-1, response-integrated to exact 1.1--2.5 um endpoints",
        "- units: MODOUT native per-cm^-1 converted pointwise using 1e8/lambda_um^2; flux W/(cm2 nm) converted using 1e7",
        "- flux altitude: explicit linear interpolation to 0.001 km between real 0 and 1 km spectral-flux levels",
        "- provenance: every formal row lists all five source case IDs and raw files",
        "- exclusions: TOTAL_RAD and target surface/reflection columns are never used as path terms",
        "- scope: production-ready only for the declared 1 m/near-range interpolation box; altitude or axis extrapolation remains invalid",
        "", "## Checks", "",
    ]
    lines.extend(f"- {'PASS' if row['passed'] else 'FAIL'} {row['check']}: {row['measured']}"
                 for row in checks)
    (root / "README.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    repo_root = TOOLS.parent
    dependencies = []
    for rel in [
        "tools/p11_modtran_swir_grid.py", "tools/p11_modtran_swir_pilot.ps1",
        "tools/p11_modtran_swir_grid_qc.py", "tools/p11_modtran_swir_grid_query.cpp",
        "tools/p11_modtran_swir_grid_query_check.ps1", "tools/p11_modtran_publish_swir.py",
        "tools/modtran_convert_to_si.py", "tools/modtran_build_lut.py", "tools/modtran_qc.py",
    ]:
        path = repo_root / rel
        dependencies.append({"path": rel, "size_bytes": path.stat().st_size, "sha256": sha256(path)})
    with (root / "code_dependencies.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(dependencies[0]))
        writer.writeheader(); writer.writerows(dependencies)
    formal_lut = repo_root / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv"
    if formal_lut.exists():
        (root / "formal_lut_reference.json").write_text(json.dumps({
            "path": str(formal_lut.resolve()), "sha256": sha256(formal_lut),
            "expected_rows": 1358, "expected_swir_rows": 18,
        }, indent=2) + "\n", encoding="utf-8")
    inventory = []
    for path in sorted(p for p in root.rglob("*") if p.is_file() and p.name != "data_inventory.csv"):
        inventory.append({"relative_path": str(path.relative_to(root)), "size_bytes": path.stat().st_size,
                          "sha256": sha256(path)})
    with (root / "data_inventory.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(inventory[0]))
        writer.writeheader(); writer.writerows(inventory)
    print(f"P11 formal SWIR ground grid QC: {status}; checks={len(checks)}; vertices={len(lut_rows)}")
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
