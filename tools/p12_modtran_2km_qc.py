#!/usr/bin/env python3
"""QC and assemble P12 real-MODTRAN 2 km SWIR/MWIR five-component rows."""

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
ROOT = TOOLS.parent
sys.path.insert(0, str(TOOLS))
import p11_modtran_humidity_qc as humidity_qc  # noqa: E402
import p11_modtran_humidity_grid as humidity_grid  # noqa: E402
import p12_modtran_2km_grid as generator  # noqa: E402


FORMAL_FIELDS = humidity_qc.FORMAL_FIELDS
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


def find_base(rows: list[dict[str, str]], band: str, profile: str, altitude: float,
              visibility: float, sza: float) -> dict[str, str]:
    matches = [row for row in rows if row["band"] == band and row["humidity_profile"] == profile and
               abs(float(row["observer_alt_km"]) - altitude) <= 1e-12 and
               abs(float(row["target_alt_km"]) - altitude) <= 1e-12 and
               abs(float(row["range_km"]) - 1.0) <= 1e-12 and
               abs(float(row["visibility_km"]) - visibility) <= 1e-12 and
               abs(float(row["solar_zenith_deg"]) - sza) <= 1e-12 and
               all(row[field] != "" for field in VALUE_FIELDS)]
    if len(matches) != 1:
        raise ValueError(f"expected one complete 1 km base row for {band}/{profile}/alt={altitude}/vis={visibility}/sza={sza}; got {len(matches)}")
    return matches[0]


def target_only_provenance(base: dict[str, str]) -> tuple[list[str], list[str]]:
    ids = base["source_case_ids"].split(";")
    files = base["source_files"].split(";")
    if len(ids) != len(files):
        raise ValueError(f"source provenance count mismatch in {base['case_id']}")
    pairs = [(case_id, path) for case_id, path in zip(ids, files)
             if "_solar_" in case_id or "_flux_" in case_id]
    if len(pairs) != 2 or not any("_solar_" in item[0] for item in pairs) or not any("_flux_" in item[0] for item in pairs):
        raise ValueError(f"cannot isolate target-only source cases in {base['case_id']}")
    for _, path in pairs:
        if not Path(path).is_file():
            raise ValueError(f"reused P11 target-only source is missing: {path}")
    return [item[0] for item in pairs], [item[1] for item in pairs]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("logs/p12/p12c/modtran_2km"))
    parser.add_argument("--formal", type=Path,
                        default=Path("HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv"))
    args = parser.parse_args()
    evidence = args.root.resolve()
    formal_path = args.formal.resolve()
    manifest = read_csv(evidence / "case_manifest.csv")
    formal_rows = read_csv(formal_path)
    checks: list[dict[str, object]] = []

    def check(name: str, passed: bool, measured: object, expected: object) -> None:
        checks.append({"check": name, "passed": bool(passed), "measured": measured, "expected": expected})

    check("component_manifest_count", len(manifest) == 160, len(manifest), 160)
    check("only_2km_los_components",
          all(float(row["range_km"]) == 2.0 and row["mode"] in
              {"Transmittance", "ThermalRadiance", "RadianceWithScattering"} for row in manifest),
          sorted({(row["range_km"], row["mode"]) for row in manifest}), "range=2; LOS modes only")
    input_failures: list[str] = []
    card_failures: list[str] = []
    for row in manifest:
        path = Path(row["input_file"])
        if not path.is_file() or sha256(path) != row["input_sha256"]:
            input_failures.append(row["case_id"]); continue
        card = path.read_text(encoding="ascii").splitlines()[1]
        if row["humidity_profile"] == "default":
            valid_card = card == generator.DEFAULT_CARD1A
        else:
            expected_rh = float(row["target_surface_rh_percent"])
            expected_scale = expected_rh / humidity_grid.MLS_SURFACE_RH_PERCENT
            valid_card = (len(card) == 110 and abs(float(card[20:30]) - expected_scale) <= 5.1e-7 and
                          card[47] == "T" and abs(float(card[90:100]) - expected_rh) <= 5.1e-4)
        if not valid_card:
            card_failures.append(row["case_id"])
    check("input_hashes_match", not input_failures, input_failures, [])
    check("default_and_humidity_card_fields_valid", not card_failures, card_failures, [])

    run_rows = read_csv(evidence / "run_manifest.csv")
    executed = {row["case_id"] for row in run_rows}
    check("real_modtran_case_count", len(executed) == 160, len(executed), 160)
    check("all_manifest_cases_executed", executed == {row["case_id"] for row in manifest},
          sorted({row["case_id"] for row in manifest} - executed), [])
    check("single_engine_sha256", len({row["engine_sha256"] for row in run_rows}) == 1,
          sorted({row["engine_sha256"] for row in run_rows}), "one real executable SHA-256")
    restore = read_csv(evidence / "installation_restore_evidence.csv")
    check("pcmodwin_fixed_files_restored", bool(restore) and all(row["restored_exactly"] == "True" for row in restore),
          [row["fixed_name"] for row in restore if row["restored_exactly"] != "True"], [])

    integrated: dict[str, float] = {}
    sources: dict[str, Path] = {}
    spectral_rows: list[dict[str, object]] = []
    humidity_rows: list[dict[str, object]] = []
    for row in manifest:
        case_dir = Path(row["input_file"]).parent
        stderr = case_dir / "engine_stderr.txt"
        check(f"{row['case_id']}_stderr_empty", stderr.is_file() and stderr.stat().st_size == 0,
              stderr.stat().st_size if stderr.is_file() else "missing", 0)
        source, parsed = humidity_qc.convert_case(row)
        metric = humidity_qc.spectral_metrics(row, parsed)
        spectral_rows.append(metric)
        integrated[row["case_id"]] = float(metric["response_mean_value_si"])
        sources[row["case_id"]] = source
        if row["humidity_profile"] != "default" and row["mode"] == "Transmittance":
            response = humidity_qc.parse_modout1(case_dir / "MODOUT1.txt")
            target_rh = float(row["target_surface_rh_percent"])
            humidity_rows.append({"case_id": row["case_id"], "band": row["band"],
                                  "humidity_profile": row["humidity_profile"], "altitude_km": row["observer_alt_km"],
                                  "visibility_km": row["visibility_km"], "target_surface_rh_percent": target_rh, **response})

    check("all_spectra_bracket_exact_response", all(bool(row["boundary_bracketed"]) for row in spectral_rows),
          [row["case_id"] for row in spectral_rows if not row["boundary_bracketed"]], [])
    check("all_spectra_finite", all(bool(row["all_parsed_numeric_finite"]) for row in spectral_rows),
          [row["case_id"] for row in spectral_rows if not row["all_parsed_numeric_finite"]], [])
    max_jacobian = max(float(row["jacobian_integral_relative_error"]) for row in spectral_rows)
    max_pointwise = max(float(row["pointwise_conversion_max_relative_error"]) for row in spectral_rows)
    check("all_jacobian_integrals_within_tolerance", max_jacobian <= 5e-6, max_jacobian, "<=5e-6")
    check("all_pointwise_unit_conversions_exact", max_pointwise <= 1e-12, max_pointwise, "<=1e-12")
    humidity_failures = [row["case_id"] for row in humidity_rows
                         if abs(float(row["surface_rh_after_percent"]) - float(row["target_surface_rh_percent"])) > 0.02]
    check("humidity_profiles_reached_requested_surface_rh", not humidity_failures, humidity_failures, [])

    candidate: list[dict[str, str]] = []
    metrics: list[dict[str, object]] = []
    profiles = [item[0] for item in generator.PROFILES]
    for band, profile, altitude, visibility, sza in itertools.product(
            humidity_grid.BANDS, profiles, generator.ALTITUDES_KM,
            generator.VISIBILITIES_KM, generator.SOLAR_ZENITH_DEG):
        trans = humidity_qc.find_one(manifest, band, profile, "Transmittance", altitude, 2.0, visibility, None)
        thermal = humidity_qc.find_one(manifest, band, profile, "ThermalRadiance", altitude, 2.0, visibility, None)
        scatter = humidity_qc.find_one(manifest, band, profile, "RadianceWithScattering", altitude, 2.0, visibility, sza)
        base = find_base(formal_rows, band, profile, altitude, visibility, sza)
        target_ids, target_files = target_only_provenance(base)
        los_ids = [trans["case_id"], thermal["case_id"], scatter["case_id"]]
        los_files = [str(sources[case_id].resolve()) for case_id in los_ids]
        tau = integrated[trans["case_id"]]
        thermal_value = integrated[thermal["case_id"]]
        scatter_value = integrated[scatter["case_id"]]
        row = {field: base.get(field, "") for field in FORMAL_FIELDS}
        row.update({
            "case_id": (f"P12_2KM_{band}_{profile}_obs{generator.token(altitude)}_tar{generator.token(altitude)}_"
                        f"rng2_vis{generator.token(visibility)}_sza{generator.token(sza)}"),
            "range_km": "2", "tau_up": f"{tau:.12g}",
            "path_thermal_W_m2_sr_um": f"{thermal_value:.12g}",
            "los_path_scattering_radiance_W_m2_sr_um": f"{scatter_value:.12g}",
            "conversion_method": ("P12 real 2 km LOS: pointwise native*1e8/lambda_um^2 then wavelength-domain trapezoidal mean; "
                                  "exact endpoint interpolation; direct solar and sky diffuse reused unchanged from exact-key P11 target-only real outputs"),
            "source_case_ids": ";".join(los_ids + target_ids),
            "source_files": ";".join(los_files + target_files),
        })
        candidate.append(row)
        metrics.append({
            "band": band, "humidity_profile": profile, "altitude_km": altitude,
            "range_km": 2.0, "visibility_km": visibility, "solar_zenith_deg": sza,
            "tau_1km": float(base["tau_up"]), "tau_2km": tau,
            "path_thermal_2km": thermal_value, "path_scattering_2km": scatter_value,
            "direct_solar_reused_exact": row["direct_solar_irradiance_at_target_W_m2_um"] == base["direct_solar_irradiance_at_target_W_m2_um"],
            "sky_diffuse_reused_exact": row["downward_sky_diffuse_irradiance_W_m2_um"] == base["downward_sky_diffuse_irradiance_W_m2_um"],
            "base_case_id": base["case_id"],
        })

    check("formal_vertex_count", len(candidate) == 96 and len({row["case_id"] for row in candidate}) == 96,
          len(candidate), 96)
    check("all_vertices_five_finite_nonnegative",
          all(row[field] != "" and math.isfinite(float(row[field])) and float(row[field]) >= 0
              for row in candidate for field in VALUE_FIELDS), "all rows", "five finite values >=0")
    check("two_km_tau_not_above_one_km",
          all(float(row["tau_2km"]) <= float(row["tau_1km"]) + 1e-12 for row in metrics),
          [row for row in metrics if float(row["tau_2km"]) > float(row["tau_1km"]) + 1e-12], [])
    check("target_only_components_reused_byte_for_byte",
          all(bool(row["direct_solar_reused_exact"]) and bool(row["sky_diffuse_reused_exact"]) for row in metrics),
          [row["base_case_id"] for row in metrics if not row["direct_solar_reused_exact"] or not row["sky_diffuse_reused_exact"]], [])
    for band, altitude, visibility in itertools.product(humidity_grid.BANDS, generator.ALTITUDES_KM, generator.VISIBILITIES_KM):
        values = []
        for rh in humidity_grid.HUMIDITIES_PERCENT:
            profile = humidity_grid.humidity_profile(rh)
            values.append(next(float(row["tau_up"]) for row in candidate if row["band"] == band and
                               row["humidity_profile"] == profile and float(row["observer_alt_km"]) == altitude and
                               float(row["visibility_km"]) == visibility))
        check(f"tau_humidity_response_{band}_alt{altitude:g}_vis{visibility:g}",
              values[0] > values[1] > values[2], values, "tau(RH30)>tau(RH60)>tau(RH85)")
    for band, profile, altitude in itertools.product(humidity_grid.BANDS, profiles, generator.ALTITUDES_KM):
        values = [next(float(row["tau_up"]) for row in candidate if row["band"] == band and
                       row["humidity_profile"] == profile and float(row["observer_alt_km"]) == altitude and
                       float(row["visibility_km"]) == visibility) for visibility in generator.VISIBILITIES_KM]
        check(f"tau_visibility_response_{band}_{profile}_alt{altitude:g}", values[0] < values[1], values,
              "tau(vis6)<tau(vis23)")

    with (evidence / "formal_2km_rows.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=FORMAL_FIELDS); writer.writeheader(); writer.writerows(candidate)
    with (evidence / "vertex_metrics.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(metrics[0])); writer.writeheader(); writer.writerows(metrics)
    with (evidence / "spectral_qc.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(spectral_rows[0])); writer.writeheader(); writer.writerows(spectral_rows)
    with (evidence / "modout1_humidity_metrics.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(humidity_rows[0])); writer.writeheader(); writer.writerows(humidity_rows)
    status = "PASS" if all(item["passed"] for item in checks) else "FAIL"
    (evidence / "qc_results.json").write_text(json.dumps({"status": status, "checks": checks}, indent=2) + "\n", encoding="utf-8")
    with (evidence / "qc_results.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=["check", "passed", "measured", "expected"]); writer.writeheader(); writer.writerows(checks)

    manual = Path("F:/Programs/PcModWin5/MODTRAN_R_5.2.1.pdf")
    source_hashes = [("manual", manual), ("generator", TOOLS / "p12_modtran_2km_grid.py"),
                     ("runner", TOOLS / "p12_modtran_2km_run.ps1"), ("qc", Path(__file__).resolve()),
                     ("converter", TOOLS / "modtran_convert_to_si.py")]
    with (evidence / "source_hashes.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=["kind", "path", "size_bytes", "sha256"]); writer.writeheader()
        for kind, path in source_hashes:
            writer.writerow({"kind": kind, "path": str(path), "size_bytes": path.stat().st_size, "sha256": sha256(path)})
    readme = [
        "# P12 real MODTRAN 2 km extension", "", f"- status: {status}",
        "- new executions: 160 LOS-only MODTRAN 5.2.1 component runs; no empirical/exponential extrapolation",
        "- output vertices: 96 = 2 bands x 4 humidity profiles x 2 equal-altitude planes x 2 visibilities x 3 solar angles",
        "- direct-solar and sky-diffuse components: exact-key P11 real target-only products reused byte-for-byte because they do not depend on LOS range",
        "- MODOUT radiance conversion: pointwise per-cm-1 to per-um Jacobian (1e8/lambda_um^2), then exact-endpoint wavelength-domain trapezoidal response mean",
        "- runtime scope after publish: horizontal equal-altitude SWIR/MWIR paths up to exactly 2 km inside the declared altitude/visibility/SZA/RH envelope; extrapolation remains fail-closed",
        "", "## Checks", "",
    ]
    readme.extend(f"- {'PASS' if item['passed'] else 'FAIL'} {item['check']}: {item['measured']}" for item in checks)
    (evidence / "README.md").write_text("\n".join(readme) + "\n", encoding="utf-8")
    print(json.dumps({"status": status, "checks": len(checks), "candidateRows": len(candidate),
                      "maxJacobianError": max_jacobian, "maxPointwiseError": max_pointwise}, indent=2))
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
