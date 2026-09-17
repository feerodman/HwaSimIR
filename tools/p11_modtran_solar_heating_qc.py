#!/usr/bin/env python3
"""Build and QC the real 0.30--2.50 um P11 civil-altitude solar LUT rows."""

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
import modtran_build_solar_heating_lut as build  # noqa: E402
import modtran_convert_to_si as convert  # noqa: E402
import p11_modtran_humidity_qc as humidity_qc  # noqa: E402
import p11_modtran_solar_heating_grid as generator  # noqa: E402


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as f:
        return list(csv.DictReader(f))


def relerr(actual: float, expected: float) -> float:
    return abs(actual - expected) / max(abs(expected), 1e-30)


def trapz(rows: list[dict[str, object]], x: str, y: str) -> float:
    points = sorted((float(r[x]), float(r[y])) for r in rows if r.get(y, "") != "")
    return sum(0.5 * (y0 + y1) * (x1 - x0)
               for (x0, y0), (x1, y1) in zip(points, points[1:]))


def flux_at_altitude(path: Path, altitude: float) -> list[dict[str, object]]:
    if abs(altitude - 1.0) <= 1e-12:
        rows = convert.parse_flux(path, 1.0)
        for row in rows:
            row["vertical_interpolation"] = "none_exact_real_1km_level"
        return rows
    low = convert.parse_flux(path, 0.0)
    high = convert.parse_flux(path, 1.0)
    if len(low) != len(high):
        raise ValueError(f"Flux level mismatch in {path}")
    rows = []
    for a, b in zip(low, high):
        if abs(float(a["wavelength_um"]) - float(b["wavelength_um"])) > 1e-12:
            raise ValueError(f"Flux wavelength mismatch in {path}")
        row = dict(a)
        for field in [
            "upward_diffuse_native_W_cm2_nm", "downward_diffuse_native_W_cm2_nm",
            "direct_solar_native_W_cm2_nm", "upward_diffuse_W_m2_um",
            "downward_diffuse_W_m2_um", "direct_solar_W_m2_um",
        ]:
            row[field] = float(a[field]) + (float(b[field]) - float(a[field])) * altitude
        row["altitude_km"] = altitude
        row["vertical_interpolation"] = "linear_between_real_0km_and_1km_levels"
        rows.append(row)
    return rows


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path,
                    default=Path("logs/p11/modtran/solar_heating_ground_grid"))
    args = ap.parse_args()
    root = args.root.resolve()
    manifest = read_csv(root / "case_manifest.csv")
    checks: list[dict[str, object]] = []

    def check(name: str, passed: bool, measured: object, expected: object) -> None:
        checks.append({"check": name, "passed": bool(passed),
                       "measured": measured, "expected": expected})

    profiles = [p for p, _ in generator.PROFILES]
    check("component_manifest_count", len(manifest) == 96, len(manifest), 96)
    check("profile_set_exact", {r["humidity_profile"] for r in manifest} == set(profiles),
          sorted({r["humidity_profile"] for r in manifest}), sorted(profiles))
    input_failures = []
    card_failures = []
    for row in manifest:
        path = Path(row["input_file"])
        if not path.is_file() or sha256(path) != row["input_sha256"]:
            input_failures.append(row["case_id"])
            continue
        card = path.read_text(encoding="ascii").splitlines()[1]
        profile = row["humidity_profile"]
        if profile == "default":
            # Preserve the historical cards exactly; this is a category, not a
            # fabricated numerical 76.18% row.
            expected = generator.profile_card(profile, None, flux=row["mode"] == "SpectralFlux")
            ok = card == expected
        else:
            rh = float(row["target_surface_rh_percent"])
            ok = (len(card) == 110 and
                  abs(float(card[20:30]) - rh / 76.18) <= 5.1e-7 and
                  card[47] == "T" and abs(float(card[90:100]) - rh) <= 5.1e-4)
        if not ok:
            card_failures.append(row["case_id"])
    check("all_input_hashes_match_manifest", not input_failures, input_failures, [])
    check("all_default_and_scaled_cards_exact", not card_failures, card_failures, [])

    runs = read_csv(root / "run_manifest.csv")
    executed = {r["case_id"] for r in runs}
    check("real_modtran_case_count", len(executed) == 96, len(executed), 96)
    check("all_manifest_cases_executed", executed == {r["case_id"] for r in manifest},
          sorted({r["case_id"] for r in manifest} - executed), [])
    check("single_engine_sha256", len({r["engine_sha256"] for r in runs}) == 1,
          sorted({r["engine_sha256"] for r in runs}), "one real executable")
    restore = read_csv(root / "installation_restore_evidence.csv")
    check("pcmodwin_fixed_files_restored",
          bool(restore) and all(r["restored_exactly"] == "True" for r in restore),
          [r["fixed_name"] for r in restore if r["restored_exactly"] != "True"], [])

    parsed_by_case: dict[str, list[dict[str, object]]] = {}
    source_by_case: dict[str, Path] = {}
    spectral_qc = []
    humidity_metrics = []
    for row in manifest:
        case_dir = Path(row["input_file"]).parent
        altitude = float(row["target_alt_km"])
        if row["mode"] == "DirectSolarIrradiance":
            source = case_dir / "MODOUT2.txt"
            parsed = convert.parse_solar(source)
            native_field = "direct_solar_native"
            si_field = "direct_solar_W_m2_um"
        else:
            source = case_dir / "spectral_flux.flx"
            parsed = flux_at_altitude(source, altitude)
            native_field = "downward_diffuse_native_W_cm2_nm"
            si_field = "downward_diffuse_W_m2_um"
        parsed_by_case[row["case_id"]] = parsed
        source_by_case[row["case_id"]] = source
        convert.write_rows(case_dir / "spectrum_si.csv", parsed, source, row["case_id"])
        wavelengths = [float(r["wavelength_um"]) for r in parsed]
        selected = [r for r in parsed if build.LOW_UM <= float(r["wavelength_um"]) <= build.HIGH_UM]
        if row["mode"] == "DirectSolarIrradiance":
            native_area = trapz(selected, "wavenumber_cm1", native_field) * 1e4
            point_err = max(relerr(float(r[si_field]), float(r[native_field]) * 1e8 /
                                   float(r["wavelength_um"]) ** 2) for r in selected)
        else:
            native_area = trapz(selected, "wavelength_nm", native_field) * 1e4
            point_err = max(relerr(float(r[si_field]), float(r[native_field]) * 1e7) for r in selected)
        si_area = trapz(selected, "wavelength_um", si_field)
        spectral_qc.append({
            "case_id": row["case_id"], "mode": row["mode"],
            "humidity_profile": row["humidity_profile"], "target_alt_km": altitude,
            "raw_rows": len(parsed), "wavelength_min_um": min(wavelengths),
            "wavelength_max_um": max(wavelengths),
            "exact_band_bracketed": min(wavelengths) <= build.LOW_UM and max(wavelengths) >= build.HIGH_UM,
            "jacobian_integral_relative_error": relerr(si_area, native_area),
            "pointwise_conversion_max_relative_error": point_err,
            "source_file": str(source.resolve()), "source_sha256": sha256(source),
            "spectrum_si_sha256": sha256(case_dir / "spectrum_si.csv"),
        })
        if row["mode"] == "DirectSolarIrradiance":
            metrics = humidity_qc.parse_modout1(case_dir / "MODOUT1.txt")
            humidity_metrics.append({
                "case_id": row["case_id"], "humidity_profile": row["humidity_profile"],
                "target_alt_km": row["target_alt_km"], "visibility_km": row["visibility_km"],
                "solar_zenith_deg": row["solar_zenith_deg"], **metrics,
            })
    check("all_real_spectra_bracket_0p30_2p50",
          all(r["exact_band_bracketed"] for r in spectral_qc),
          [r["case_id"] for r in spectral_qc if not r["exact_band_bracketed"]], [])
    check("jacobian_integrals_within_tolerance",
          all(float(r["jacobian_integral_relative_error"]) <= 2e-5 for r in spectral_qc),
          max(float(r["jacobian_integral_relative_error"]) for r in spectral_qc), "<=2e-5")
    check("pointwise_unit_conversions_exact",
          all(float(r["pointwise_conversion_max_relative_error"]) <= 1e-12 for r in spectral_qc),
          max(float(r["pointwise_conversion_max_relative_error"]) for r in spectral_qc), "<=1e-12")

    index = {(r["humidity_profile"], float(r["target_alt_km"]),
              float(r["visibility_km"]), float(r["solar_zenith_deg"]), r["mode"]): r
             for r in manifest}
    output = []
    metrics = []
    for profile, altitude, visibility, sza in itertools.product(
            profiles, generator.ALTITUDES_KM, generator.VISIBILITIES_KM, generator.SZAS_DEG):
        direct_meta = index[profile, altitude, visibility, sza, "DirectSolarIrradiance"]
        flux_meta = index[profile, altitude, visibility, sza, "SpectralFlux"]
        direct_rows = parsed_by_case[direct_meta["case_id"]]
        flux_rows = parsed_by_case[flux_meta["case_id"]]
        direct = build.integrate_band(direct_rows, "direct_solar_W_m2_um")
        diffuse = build.integrate_band(flux_rows, "downward_diffuse_W_m2_um")
        flux_direct = build.integrate_band(flux_rows, "direct_solar_W_m2_um")
        output.append({
            "schema_version": "1",
            "case_id": (f"SWHEAT_P11_{profile}_tar{altitude:g}_"
                        f"vis{visibility:g}_sza{sza:g}"),
            "band": "SOLAR_SHORTWAVE_0.30_2.50_UM",
            "atmosphere_model": "Mid-Latitude Summer", "aerosol_model": "Rural",
            "humidity_profile": profile, "target_alt_km": f"{altitude:g}",
            "visibility_km": f"{visibility:g}", "solar_zenith_deg": f"{sza:g}",
            "direct_shortwave_solar_irradiance_W_m2": f"{direct:.12g}",
            "diffuse_shortwave_down_irradiance_W_m2": f"{diffuse:.12g}",
            "flux_direct_horizontal_qc_W_m2": f"{flux_direct:.12g}",
            "irradiance_unit": "W/m^2", "spectral_range_um": "0.30-2.50",
            "response_mode": "BroadbandIntegral", "direct_source_field": "MODOUT2 SOL TR",
            "diffuse_source_field": ".flx DOWNWARD", "flux_direct_policy": "QC_only_not_added_to_diffuse",
            "raw_solar_unit": "W/(cm^2 cm^-1)", "raw_flux_unit": "W/(cm^2 nm)",
            "conversion_method": "SOL_TR pointwise native*1e8/lambda_um^2; FLX DOWNWARD pointwise native*1e7; wavelength-domain trapezoidal integral 0.30-2.50 um with exact interpolated endpoints",
            "source_case_ids": f"{direct_meta['case_id']};{flux_meta['case_id']}",
            "source_files": (f"{source_by_case[direct_meta['case_id']].resolve()};"
                             f"{source_by_case[flux_meta['case_id']].resolve()}"),
            "source_sha256": (f"{sha256(source_by_case[direct_meta['case_id']])};"
                              f"{sha256(source_by_case[flux_meta['case_id']])}"),
        })
        metrics.append({
            "humidity_profile": profile, "target_alt_km": altitude,
            "visibility_km": visibility, "solar_zenith_deg": sza,
            "direct_W_m2": direct, "diffuse_W_m2": diffuse,
            "flux_direct_horizontal_qc_W_m2": flux_direct,
        })
    check("formal_vertex_count", len(output) == 48, len(output), 48)
    check("all_integrals_finite_nonnegative",
          all(math.isfinite(float(r[field])) and float(r[field]) >= 0 for r in output
              for field in ("direct_shortwave_solar_irradiance_W_m2",
                            "diffuse_shortwave_down_irradiance_W_m2",
                            "flux_direct_horizontal_qc_W_m2")), "all 48", "finite and >=0")
    for profile, altitude, visibility in itertools.product(
            profiles, generator.ALTITUDES_KM, generator.VISIBILITIES_KM):
        values = [next(float(r["direct_shortwave_solar_irradiance_W_m2"]) for r in output
                       if r["humidity_profile"] == profile and float(r["target_alt_km"]) == altitude and
                       float(r["visibility_km"]) == visibility and float(r["solar_zenith_deg"]) == sza)
                  for sza in generator.SZAS_DEG]
        check(f"direct_sza_response_{profile}_alt{altitude:g}_vis{visibility:g}",
              values[0] > values[1] > values[2], values, "direct(20)>direct(45)>direct(70)")
    for altitude, visibility, sza in itertools.product(
            generator.ALTITUDES_KM, generator.VISIBILITIES_KM, generator.SZAS_DEG):
        values = [next(float(r["direct_shortwave_solar_irradiance_W_m2"]) for r in output
                       if r["humidity_profile"] == f"scaled_mls_surface_rh{rh:g}" and
                       float(r["target_alt_km"]) == altitude and float(r["visibility_km"]) == visibility and
                       float(r["solar_zenith_deg"]) == sza) for rh in (30.0, 60.0, 85.0)]
        check(f"direct_humidity_response_alt{altitude:g}_vis{visibility:g}_sza{sza:g}",
              values[0] > values[1] > values[2], values, "direct(RH30)>direct(RH60)>direct(RH85)")

    fields = list(output[0])
    with (root / "formal_solar_heating_rows.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields); writer.writeheader(); writer.writerows(output)
    for filename, rows in (("solar_heating_metrics.csv", metrics),
                           ("spectral_qc.csv", spectral_qc),
                           ("modout1_humidity_metrics.csv", humidity_metrics)):
        with (root / filename).open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=list(rows[0])); writer.writeheader(); writer.writerows(rows)
    status = "PASS" if all(c["passed"] for c in checks) else "FAIL"
    (root / "solar_heating_qc_results.json").write_text(
        json.dumps({"status": status, "checks": checks}, indent=2) + "\n", encoding="utf-8")
    with (root / "solar_heating_qc_results.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["check", "passed", "measured", "expected"])
        writer.writeheader(); writer.writerows(checks)
    dependencies = [Path(__file__).resolve(), TOOLS / "p11_modtran_solar_heating_grid.py",
                    TOOLS / "modtran_build_solar_heating_lut.py", TOOLS / "modtran_convert_to_si.py",
                    TOOLS / "p11_modtran_swir_pilot.ps1"]
    manual = Path("F:/Programs/PcModWin5/MODTRAN_R_5.2.1.pdf")
    with (root / "source_hashes.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["kind", "path", "size_bytes", "sha256"])
        writer.writeheader()
        for kind, path in [("manual", manual)] + [("code", p) for p in dependencies]:
            writer.writerow({"kind": kind, "path": str(path), "size_bytes": path.stat().st_size,
                             "sha256": sha256(path)})
    report = [
        "# P11 Civil-Altitude Broadband Solar Heating Grid", "", f"- status: {status}",
        "- spectral response: true 0.30--2.50 um broadband integration; never extrapolated from SWIR 1.1--2.5 um",
        "- native request: 3990--33340 cm^-1, providing real bracketing samples beyond both response endpoints",
        "- coverage: default plus scaled_mls_surface_rh30/60/85; target altitude 0.001/1 km; visibility 6/23 km; SZA 20/45/70 deg",
        "- output: 48 complete direct+diffuse vertices from 96 real component executions",
        "- direct: MODOUT2 SOL TR; diffuse: .flx DOWNWARD; .flx DIRECT is QC-only and is not added",
        "- flux altitude: 0.001 km linearly samples real 0/1 km flux levels; 1 km uses the exact level",
        "- default is retained as an unmodified MLS category, not relabeled as numerical 76.18%",
        "", "## Checks", "",
    ]
    report.extend(f"- {'PASS' if c['passed'] else 'FAIL'} {c['check']}: {c['measured']}" for c in checks)
    (root / "README.md").write_text("\n".join(report) + "\n", encoding="utf-8")
    inventory = []
    for path in sorted(p for p in root.rglob("*") if p.is_file() and p.name != "data_inventory.csv"):
        inventory.append({"relative_path": str(path.relative_to(root)),
                          "size_bytes": path.stat().st_size, "sha256": sha256(path)})
    with (root / "data_inventory.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(inventory[0])); writer.writeheader(); writer.writerows(inventory)
    print(f"P11 broadband solar-heating QC: {status}; checks={len(checks)}")
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
