#!/usr/bin/env python3
"""QC and integrate the P11 real-MODTRAN SWIR pilot spectra.

This script is deliberately independent of the runtime LUT builder.  It checks
native-wavenumber versus SI-wavelength integrals, clips every product to the
declared 1.1--2.5 um rectangular response, and excludes TOTAL_RAD and target
surface columns from the path-radiance candidate.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
from pathlib import Path
from typing import Iterable


LO = 1.1
HI = 2.5
WIDTH = HI - LO


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as f:
        return list(csv.DictReader(f))


def clipped(rows: Iterable[dict[str, str]]) -> list[dict[str, str]]:
    result = [row for row in rows if LO - 1e-12 <= float(row["wavelength_um"]) <= HI + 1e-12]
    return sorted(result, key=lambda row: float(row["wavelength_um"]))


def response_integral(rows: list[dict[str, str]], field: str) -> float:
    """Integrate an SI wavelength-density field with interpolated exact endpoints."""
    points = sorted((float(row["wavelength_um"]), float(row[field]))
                    for row in rows if row.get(field, "") != "")
    if not points or points[0][0] > LO or points[-1][0] < HI:
        raise ValueError(f"{field} lacks bracketing samples for {LO}--{HI} um")

    def sample_at(x: float) -> float:
        for px, py in points:
            if abs(px - x) <= 1e-14:
                return py
        for (x0, y0), (x1, y1) in zip(points, points[1:]):
            if x0 < x < x1:
                return y0 + (y1 - y0) * (x - x0) / (x1 - x0)
        raise ValueError(f"Cannot interpolate {field} at {x}")

    band = [(x, y) for x, y in points if LO < x < HI]
    band.insert(0, (LO, sample_at(LO)))
    band.append((HI, sample_at(HI)))
    return sum(0.5 * (y0 + y1) * (x1 - x0)
               for (x0, y0), (x1, y1) in zip(band, band[1:]))


def trapz(rows: list[dict[str, str]], x: str, y: str) -> float:
    points = sorted((float(row[x]), float(row[y])) for row in rows if row.get(y, "") != "")
    return sum(0.5 * (y0 + y1) * (x1 - x0) for (x0, y0), (x1, y1) in zip(points, points[1:]))


def relative_error(actual: float, expected: float) -> float:
    return abs(actual - expected) / max(abs(expected), 1e-30)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", type=Path, default=Path("logs/p11/modtran/pilot"))
    args = ap.parse_args()
    root = args.root.resolve()
    paths = {
        "trans": root / "SWIR_pilot_trans" / "spectrum_si.csv",
        "thermal": root / "SWIR_pilot_thermal" / "spectrum_si.csv",
        "scattering": root / "SWIR_pilot_scattering" / "spectrum_si.csv",
        "solar": root / "SWIR_pilot_solar" / "spectrum_si.csv",
        "flux": root / "SWIR_pilot_flux" / "spectrum_si.csv",
    }
    spectra_raw = {key: read_rows(path) for key, path in paths.items()}
    spectra = {key: clipped(rows) for key, rows in spectra_raw.items()}
    checks: list[dict[str, object]] = []

    def check(name: str, passed: bool, measured: object, expected: object) -> None:
        checks.append({"check": name, "passed": bool(passed), "measured": measured, "expected": expected})

    for key, rows in spectra.items():
        wavelengths = [float(row["wavelength_um"]) for row in spectra_raw[key]]
        check(f"{key}_has_real_rows", len(rows) >= 5000, len(rows), ">=5000")
        check(f"{key}_supports_low_boundary", min(wavelengths) <= LO, min(wavelengths), f"<={LO}")
        check(f"{key}_supports_high_boundary", max(wavelengths) >= HI, max(wavelengths), f">={HI}")

    cm1_fields = [
        ("thermal", "path_thermal_native", "path_thermal_W_m2_sr_um"),
        ("scattering", "solar_scatter_native", "solar_scatter_W_m2_sr_um"),
        ("solar", "direct_solar_native", "direct_solar_W_m2_um"),
    ]
    for key, native_field, si_field in cm1_fields:
        rows = spectra[key]
        native_area = trapz(rows, "wavenumber_cm1", native_field) * 1.0e4
        si_area = trapz(rows, "wavelength_um", si_field)
        err = relative_error(si_area, native_area)
        check(f"{key}_{native_field}_jacobian_integral", err <= 5e-6,
              {"native_domain_W_m2": native_area, "wavelength_domain_W_m2": si_area, "relative_error": err},
              "relative_error<=5e-6")
        point_errors = []
        for row in rows:
            lam = float(row["wavelength_um"])
            expected = float(row[native_field]) * 1.0e8 / (lam * lam)
            point_errors.append(relative_error(float(row[si_field]), expected))
        check(f"{key}_{native_field}_jacobian_pointwise", max(point_errors) <= 1e-12,
              max(point_errors), "<=1e-12")

    flux_rows = spectra["flux"]
    native_flux_area = trapz(flux_rows, "wavelength_nm", "downward_diffuse_native_W_cm2_nm") * 1.0e4
    si_flux_area = trapz(flux_rows, "wavelength_um", "downward_diffuse_W_m2_um")
    flux_err = relative_error(si_flux_area, native_flux_area)
    check("flux_nm_to_um_integral", flux_err <= 1e-12,
          {"native_domain_W_m2": native_flux_area, "wavelength_domain_W_m2": si_flux_area,
           "relative_error": flux_err}, "relative_error<=1e-12")

    tau_mean = response_integral(spectra_raw["trans"], "tau_los") / WIDTH
    thermal_mean = response_integral(spectra_raw["thermal"], "path_thermal_W_m2_sr_um") / WIDTH
    scatter_mean = response_integral(spectra_raw["scattering"], "solar_scatter_W_m2_sr_um") / WIDTH
    solar_mean = response_integral(spectra_raw["solar"], "direct_solar_W_m2_um") / WIDTH
    sky_mean = response_integral(spectra_raw["flux"], "downward_diffuse_W_m2_um") / WIDTH
    candidate = {
        "scenario_id": "SWIR_MLS_Rural_vis23_obs10_tar5_rng10_sza45",
        "status": "PILOT_CANDIDATE_NOT_YET_PRODUCTION_LUT",
        "band": "SWIR",
        "response": "ideal rectangular reference",
        "wavelength_low_um": LO,
        "wavelength_high_um": HI,
        "tau_los_response_weighted_mean": tau_mean,
        "path_thermal_response_weighted_mean_W_m2_sr_um": thermal_mean,
        "path_solar_scatter_response_weighted_mean_W_m2_sr_um": scatter_mean,
        "path_total_excluding_target_response_weighted_mean_W_m2_sr_um": thermal_mean + scatter_mean,
        "target_direct_solar_response_weighted_mean_W_m2_um": solar_mean,
        "target_downward_diffuse_flux_response_weighted_mean_W_m2_um": sky_mean,
        "excluded_columns": ["SURF_EMIS", "GRND_RFLT", "DRCT_RFLT", "TOTAL_RAD"],
        "note": "TOTAL_RAD is diagnostic only; no target contribution is added from MODTRAN.",
    }
    finite = all(math.isfinite(value) for value in candidate.values() if isinstance(value, float))
    check("candidate_values_finite", finite, finite, True)
    check("tau_mean_in_unit_interval", 0.0 <= tau_mean <= 1.0, tau_mean, "0<=tau<=1")

    (root / "candidate_band_row.json").write_text(
        json.dumps(candidate, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    (root / "qc_results.json").write_text(
        json.dumps({"status": "PASS" if all(row["passed"] for row in checks) else "FAIL",
                    "checks": checks}, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    with (root / "qc_results.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["check", "passed", "measured", "expected"])
        writer.writeheader()
        writer.writerows(checks)

    repo_root = Path(__file__).resolve().parent.parent
    dependencies = []
    for rel in [
        "tools/p11_modtran_swir_pilot.py",
        "tools/p11_modtran_swir_pilot.ps1",
        "tools/p11_modtran_swir_qc.py",
        "tools/modtran_convert_to_si.py",
        "tools/test_modtran_units.py",
    ]:
        path = repo_root / rel
        dependencies.append({"path": rel, "size_bytes": path.stat().st_size, "sha256": sha256(path)})
    with (root / "code_dependencies.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["path", "size_bytes", "sha256"])
        writer.writeheader()
        writer.writerows(dependencies)

    docs = []
    for path in [Path("F:/Programs/PcModWin5/MODTRAN_R_5.2.1.pdf"),
                 Path("F:/Programs/PcModWin5/PcModWin5Manual.pdf")]:
        docs.append({"path": str(path), "size_bytes": path.stat().st_size, "sha256": sha256(path)})
    with (root / "source_document_hashes.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["path", "size_bytes", "sha256"])
        writer.writeheader()
        writer.writerows(docs)

    coverage = []
    for key, rows in spectra_raw.items():
        wavelengths = [float(row["wavelength_um"]) for row in rows]
        wavenumbers = [float(row["wavenumber_cm1"]) for row in rows if row.get("wavenumber_cm1", "")]
        coverage.append({
            "product": key,
            "raw_rows": len(rows),
            "wavelength_min_um": min(wavelengths),
            "wavelength_max_um": max(wavelengths),
            "wavenumber_min_cm1": min(wavenumbers) if wavenumbers else 10000.0 / max(wavelengths),
            "wavenumber_max_cm1": max(wavenumbers) if wavenumbers else 10000.0 / min(wavelengths),
            "response_low_um": LO,
            "response_high_um": HI,
            "exact_endpoint_interpolation": True,
        })
    with (root / "spectral_coverage.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(coverage[0]))
        writer.writeheader()
        writer.writerows(coverage)

    status = "PASS" if all(row["passed"] for row in checks) else "FAIL"
    lines = [
        "# P11 MODTRAN SWIR Pilot",
        "",
        f"- status: {status}",
        "- real engine: PcModWin5 MODTRAN 5.2.1.0 command-line engine",
        "- scenario: Mid-Latitude Summer, Rural aerosol, default humidity, visibility 23 km, observer/target/range 10/5/10 km, SZA 45 deg",
        "- requested spectral support: 4000--9090.9091 cm^-1 (1.1--2.5 um), 1 cm^-1 increment",
        "- response integration: ideal rectangular 1.1--2.5 um; boundary convolution samples outside the response are retained in raw data but excluded from the integral",
        "- radiance conversion: L_lambda[W/(m2 sr um)] = L_sigma[W/(cm2 sr cm^-1)] * 1e8 / lambda_um^2",
        "- irradiance conversion: same Jacobian without sr; flux-table W/(cm2 nm) uses factor 1e7 to W/(m2 um)",
        "- exclusion: TOTAL_RAD, SURF_EMIS, GRND_RFLT and DRCT_RFLT are not used as target-independent path terms",
        "- manual evidence: PcModWin5 Manual p.649 defines radiance units and Path Thermal/Single Scatter/Surface Emission/Ground Reflected/Total Radiance semantics; p.650 defines transmitted-solar and transmittance products; p.540 defines spectral flux as upwelling/downwelling/direct solar at each layer",
        "- visual QA: manual pages 540, 649 and 650 were rendered and inspected; the raw flux file also declares W CM-2 / NM in its own header",
        "",
        "## Candidate band values",
        "",
    ]
    for key, value in candidate.items():
        if isinstance(value, (float, int)):
            lines.append(f"- {key}: {value:.12g}")
    lines += ["", "## Checks", ""]
    for row in checks:
        lines.append(f"- {'PASS' if row['passed'] else 'FAIL'} {row['check']}: {row['measured']}")
    (root / "README.md").write_text("\n".join(lines) + "\n", encoding="utf-8")

    inventory = []
    for path in sorted(p for p in root.rglob("*") if p.is_file() and p.name != "data_inventory.csv"):
        inventory.append({"relative_path": str(path.relative_to(root)), "size_bytes": path.stat().st_size,
                          "sha256": sha256(path)})
    with (root / "data_inventory.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=["relative_path", "size_bytes", "sha256"])
        writer.writeheader()
        writer.writerows(inventory)

    print(f"P11 MODTRAN SWIR pilot QC: {status}; checks={len(checks)}")
    print(json.dumps(candidate, indent=2, ensure_ascii=False))
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
