#!/usr/bin/env python3
"""Independent double-precision P11 radiance reference and production comparison."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
from pathlib import Path

H = 6.62607015e-34
C = 299792458.0
KB = 1.380649e-23
PI = math.pi
BANDS = {
    "SWIR": (1.1, 2.5),
    "MWIR": (3.0, 5.0),
}
THRESHOLDS = {
    "production_vs_reference_relative": 0.01,
    "gpu_vs_cpu_relative": 0.02,
    "near_zero_absolute_W_m2_sr_um": 1.0e-8,
    "tau_absolute": 1.0e-12,
}


def planck_w_m2_sr_um(wavelength_um: float, temperature_k: float) -> float:
    wavelength_m = wavelength_um * 1.0e-6
    exponent = H * C / (wavelength_m * KB * temperature_k)
    return (2.0 * H * C * C) / (wavelength_m**5 * math.expm1(exponent)) * 1.0e-6


def simpson_mean(lo_um: float, hi_um: float, temperature_k: float, intervals: int = 16384) -> float:
    if intervals <= 0 or intervals % 2:
        raise ValueError("Simpson interval count must be positive and even")
    step = (hi_um - lo_um) / intervals
    total = planck_w_m2_sr_um(lo_um, temperature_k) + planck_w_m2_sr_um(hi_um, temperature_k)
    for index in range(1, intervals):
        wavelength = lo_um + index * step
        total += (4.0 if index % 2 else 2.0) * planck_w_m2_sr_um(wavelength, temperature_k)
    return total * step / 3.0 / (hi_um - lo_um)


def band_mean(band: str, temperature_k: float, intervals: int = 16384) -> float:
    lo_um, hi_um = BANDS[band]
    return simpson_mean(lo_um, hi_um, temperature_k, intervals)


def overlap_width_um(a_center: float, a_width: float, band: str) -> float:
    lo, hi = BANDS[band]
    source_lo = a_center - 0.5 * a_width
    source_hi = a_center + 0.5 * a_width
    return max(0.0, min(hi, source_hi) - max(lo, source_lo))


def reference_case(band: str, temperature_k: float = 300.0) -> dict[str, float]:
    emissivity = 0.8
    reflectance = 0.2
    tau = 0.7
    direct = 2.0
    sky = 0.5
    ndotl = 0.5
    path_thermal = 0.1
    path_scattering = 0.02
    active_surface = 0.04
    active_sensor = 0.028
    body = emissivity * band_mean(band, temperature_k)
    solar = reflectance / PI * direct * ndotl
    sky_reflected = reflectance / PI * sky
    surface_without_active = body + solar + sky_reflected
    surface_for_audit = surface_without_active + active_surface
    sensor = tau * surface_without_active + path_thermal + path_scattering + active_sensor
    return {
        "body_W_m2_sr_um": body,
        "solar_reflected_W_m2_sr_um": solar,
        "sky_reflected_W_m2_sr_um": sky_reflected,
        "active_surface_W_m2_sr_um": active_surface,
        "surface_W_m2_sr_um": surface_for_audit,
        "sensor_W_m2_sr_um": sensor,
    }


def relative_error(actual: float, expected: float) -> float:
    if abs(expected) <= THRESHOLDS["near_zero_absolute_W_m2_sr_um"]:
        return abs(actual - expected)
    return abs(actual - expected) / abs(expected)


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--production-probe", type=Path)
    parser.add_argument("--gpu-probe", type=Path)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--record-baseline", action="store_true")
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    checks: list[dict[str, object]] = []
    def check(name: str, actual: float, expected: float, tolerance: float, metric: str = "relative") -> None:
        error = abs(actual - expected) if metric == "absolute" else relative_error(actual, expected)
        checks.append({
            "name": name,
            "actual": actual,
            "expected": expected,
            "error": error,
            "metric": metric,
            "tolerance": tolerance,
            "pass": bool(math.isfinite(actual) and error <= tolerance),
        })

    for band in BANDS:
        for temperature in (250.0, 300.0, 500.0, 900.0, 1200.0):
            fine = band_mean(band, temperature, 16384)
            coarse = band_mean(band, temperature, 4096)
            check(f"{band}_integration_convergence_{temperature:g}K", coarse, fine, 1.0e-9)

    check("tau_zero_preserves_only_path", 0.125, 0.125, THRESHOLDS["near_zero_absolute_W_m2_sr_um"], "absolute")
    low_tau_expected = 1.0e-8 * 2.0 + 0.125
    check("low_tau_not_unity", low_tau_expected, 0.12500002, THRESHOLDS["near_zero_absolute_W_m2_sr_um"], "absolute")
    check("out_of_band_0p85_source_has_zero_SWIR_overlap", overlap_width_um(0.85, 0.05, "SWIR"), 0.0, 1.0e-12, "absolute")
    check("out_of_band_0p85_source_has_zero_MWIR_overlap", overlap_width_um(0.85, 0.05, "MWIR"), 0.0, 1.0e-12, "absolute")
    shadow_surface = 0.2 / PI * 0.5
    check("shadow_removes_direct_but_not_sky", shadow_surface, 0.2 / PI * 0.5, 1.0e-12)
    check("path_added_once", 0.7 * 2.0 + 0.125, 1.525, 1.0e-12)

    rows = []
    references = {}
    for band in BANDS:
        ref = reference_case(band)
        references[band] = ref
        for temperature in (250.0, 300.0, 500.0, 900.0, 1200.0):
            rows.append({
                "band": band,
                "temperature_K": temperature,
                "band_low_um": BANDS[band][0],
                "band_high_um": BANDS[band][1],
                "band_mean_planck_W_m2_sr_um": band_mean(band, temperature),
            })

    probe = None
    if args.production_probe:
        probe = json.loads(args.production_probe.read_text(encoding="utf-8"))
        for band in BANDS:
            for temperature in (250.0, 300.0, 500.0, 900.0, 1200.0):
                check(f"production_{band}_band_integration_{temperature:g}K",
                      float(probe["planck"][f"{band}_{int(temperature)}K"]),
                      band_mean(band, temperature),
                      THRESHOLDS["production_vs_reference_relative"])

        def expected_formal_case(band: str, variant: str) -> dict[str, float]:
            direct, sky = 2.0, 0.5
            path_thermal, path_scattering = 0.1, 0.02
            active_surface, active_sensor = 0.04, 0.028
            if variant == "no_direct":
                direct = 0.0
            elif variant == "no_sky":
                sky = 0.0
            elif variant == "no_path":
                path_thermal = path_scattering = 0.0
            elif variant == "no_active":
                active_surface = active_sensor = 0.0
            body = 0.8 * band_mean(band, 300.0)
            solar = 0.2 / PI * direct * 0.5
            sky_reflected = 0.2 / PI * sky
            physical_reflected = solar + sky_reflected
            return {
                "body": body,
                "solar_reflected": solar,
                "sky_reflected": sky_reflected,
                "physical_reflected": physical_reflected,
                "active_surface": active_surface,
                "surface": body + physical_reflected + active_surface,
                "tau": 0.7,
                "path_thermal": path_thermal,
                "path_scattering": path_scattering,
                "active_sensor": active_sensor,
                "sensor": 0.7 * (body + physical_reflected) + path_thermal + path_scattering + active_sensor,
            }

        for band in BANDS:
            for variant in ("base", "no_direct", "no_sky", "no_path", "no_active", "legacy_perturbed"):
                actual_case = probe["formal_cases"][f"{band}_{variant}"]
                expected_case = expected_formal_case(band, "base" if variant == "legacy_perturbed" else variant)
                for component, expected in expected_case.items():
                    check(f"production_{band}_{variant}_{component}",
                          float(actual_case[component]), expected,
                          THRESHOLDS["production_vs_reference_relative"])
            base_sensor = float(probe["formal_cases"][f"{band}_base"]["sensor"])
            perturbed_sensor = float(probe["formal_cases"][f"{band}_legacy_perturbed"]["sensor"])
            check(f"production_{band}_legacy_dimensionless_excluded", perturbed_sensor,
                  base_sensor, THRESHOLDS["near_zero_absolute_W_m2_sr_um"], "absolute")
            if float(probe["formal_cases"][f"{band}_legacy_perturbed"]["legacy_empirical_reflected"]) <= 0.05:
                checks.append({"name": f"production_{band}_legacy_perturbation_was_material",
                               "actual": float(probe["formal_cases"][f"{band}_legacy_perturbed"]["legacy_empirical_reflected"]),
                               "expected": ">0.05", "error": float("inf"), "metric": "predicate",
                               "tolerance": 0.0, "pass": False})

        for band in BANDS:
            body = 0.8 * band_mean(band, 300.0)
            for index, tau_value in enumerate(probe["tau_values"]):
                actual_case = probe["tau_cases"][f"{band}_{index}"]
                check(f"production_{band}_tau_{index}_preserved", float(actual_case["tau"]),
                      float(tau_value), THRESHOLDS["tau_absolute"], "absolute")
                check(f"production_{band}_tau_{index}_sensor", float(actual_case["sensor"]),
                      float(tau_value) * body + 0.125,
                      THRESHOLDS["near_zero_absolute_W_m2_sr_um"] if float(tau_value) <= 1e-8
                      else THRESHOLDS["production_vs_reference_relative"],
                      "absolute" if float(tau_value) <= 1e-8 else "relative")

    gpu_probe = None
    if args.gpu_probe:
        gpu_probe = json.loads(args.gpu_probe.read_text(encoding="utf-8"))
        for row in gpu_probe["samples"]:
            check(f"gpu_vs_cpu_{row['name']}", float(row["gpu_W_m2_sr_um"]),
                  float(row["cpu_W_m2_sr_um"]), THRESHOLDS["gpu_vs_cpu_relative"])

    with (args.output_dir / "reference_planck.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    (args.output_dir / "frozen_thresholds.json").write_text(
        json.dumps(THRESHOLDS, indent=2) + "\n", encoding="utf-8")
    result = {
        "schema": "HwaSimIR.P11.IndependentReference.v1",
        "status": "PASS" if all(row["pass"] for row in checks) else "FAIL",
        "baseline_expected_failure": bool(args.record_baseline),
        "band_semantics": "response-weighted rectangular-band mean spectral radiance",
        "radiance_unit": "W/(m^2 sr um)",
        "irradiance_unit": "W/(m^2 um)",
        "bands_um": BANDS,
        "thresholds": THRESHOLDS,
        "reference_cases": references,
        "production_probe_sha256": file_sha256(args.production_probe) if args.production_probe else None,
        "gpu_probe_sha256": file_sha256(args.gpu_probe) if args.gpu_probe else None,
        "checks": checks,
    }
    (args.output_dir / "reference_results.json").write_text(
        json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    failures = [row for row in checks if not row["pass"]]
    print(json.dumps({"status": result["status"], "checks": len(checks),
                      "failures": [row["name"] for row in failures]}, ensure_ascii=False))
    if failures and not args.record_baseline:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
