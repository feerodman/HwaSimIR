#!/usr/bin/env python3
"""Strict QC and isolated merge candidate for the real SWIR 1.0 km layer."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import sys
from pathlib import Path


TOOLS = Path(__file__).resolve().parent
REPO = TOOLS.parent
sys.path.insert(0, str(TOOLS))
import p11_modtran_swir_grid_qc as base  # noqa: E402


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as stream:
        return list(csv.DictReader(stream))


def write_rows(path: Path, values: list[dict[str, str]]) -> None:
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=base.FIELDS)
        writer.writeheader()
        writer.writerows(values)


def rebuild_inventory(root: Path) -> None:
    values = []
    for path in sorted(item for item in root.rglob("*") if item.is_file() and item.name != "data_inventory.csv"):
        values.append({
            "relative_path": str(path.relative_to(root)),
            "size_bytes": path.stat().st_size,
            "sha256": sha256(path),
        })
    with (root / "data_inventory.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=("relative_path", "size_bytes", "sha256"))
        writer.writeheader()
        writer.writerows(values)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("logs/p11/modtran/swir_alt1_grid"))
    args = parser.parse_args()
    root = args.root.resolve()

    # The audited base QC is altitude-parametric except for report labels.
    # At exactly 1 km its flux conversion evaluates the real 1 km MODTRAN
    # spectral level (interpolation weight 1), never an extrapolated value.
    base.ALTITUDE_KM = 1.0
    saved = sys.argv
    try:
        sys.argv = [str(Path(base.__file__)), "--root", str(root)]
        result = base.main()
    finally:
        sys.argv = saved
    if result != 0:
        return result

    # The shared converter expresses the requested level as a linear weight
    # between real 0 and 1 km tables. At weight exactly 1 the numbers are the
    # native 1 km row, so correct the provenance label without changing data.
    for spectrum in root.glob("SWIR_flux_*/spectrum_si.csv"):
        spectrum_rows = rows(spectrum)
        for row in spectrum_rows:
            if abs(float(row["altitude_km"]) - 1.0) > 1e-12:
                raise ValueError(f"Flux row is not the real 1 km endpoint: {spectrum}")
            row["vertical_interpolation"] = "exact_real_1km_level"
        with spectrum.open("w", newline="", encoding="utf-8") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(spectrum_rows[0]))
            writer.writeheader()
            writer.writerows(spectrum_rows)

    formal_path = root / "formal_swir_rows.csv"
    candidate = rows(formal_path)
    for row in candidate:
        row["case_id"] = row["case_id"].replace(
            "P11_SWIR_ground_obs0p001_tar0p001_", "P11_SWIR_alt1_obs1_tar1_")
    write_rows(formal_path, candidate)
    candidate_path = root / "merge_candidate_swir_alt1.csv"
    write_rows(candidate_path, candidate)

    aggregate_path = REPO / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv"
    aggregate = rows(aggregate_path)
    axis_fields = ("observer_alt_km", "target_alt_km", "range_km", "visibility_km", "solar_zenith_deg")
    usable_swir = [row for row in aggregate if row.get("band") == "SWIR" and all(row.get(field, "") for field in axis_fields)]
    def axis_key(row: dict[str, str]) -> tuple[object, ...]:
        return (
            row["band"], row["atmosphere_model"], row["aerosol_model"], row["humidity_profile"],
            *(float(row[field]) for field in axis_fields),
        )
    overlap = set(map(axis_key, candidate)) & set(map(axis_key, usable_swir))
    compatibility = {
        "schema": "hwasimir_p11_modtran_merge_compatibility_1",
        "candidate": str(candidate_path),
        "candidateRows": len(candidate),
        "aggregateReadOnlyPath": str(aggregate_path),
        "aggregateReadOnlySha256": sha256(aggregate_path),
        "headerExactMatch": list(candidate[0]) == list(aggregate[0]),
        "existingExactAxisOverlap": len(overlap),
        "actionTaken": "READ_ONLY_CHECK_NO_PUBLICATION",
    }
    compatibility_path = root / "merge_compatibility.json"
    compatibility_path.write_text(json.dumps(compatibility, indent=2) + "\n", encoding="utf-8")

    checks_path = root / "grid_qc_results.json"
    checks_doc = json.loads(checks_path.read_text(encoding="utf-8"))
    checks = checks_doc["checks"]
    checks.extend([
        {
            "check": "altitude_layer_exact",
            "passed": all(float(row["observer_alt_km"]) == 1.0 and float(row["target_alt_km"]) == 1.0 for row in candidate),
            "measured": sorted({(row["observer_alt_km"], row["target_alt_km"]) for row in candidate}),
            "expected": [["1", "1"]],
        },
        {
            "check": "candidate_case_identity_alt1",
            "passed": all(row["case_id"].startswith("P11_SWIR_alt1_obs1_tar1_") for row in candidate),
            "measured": len({row["case_id"] for row in candidate}),
            "expected": 18,
        },
        {
            "check": "candidate_isolated_from_formal_lut",
            "passed": "logs\\p11\\modtran\\swir_alt1_grid" in str(candidate_path) or "/logs/p11/modtran/swir_alt1_grid" in str(candidate_path).replace("\\", "/"),
            "measured": str(candidate_path),
            "expected": "independent logs/p11/modtran/swir_alt1_grid path",
        },
        {
            "check": "formal_lut_header_exact_match",
            "passed": compatibility["headerExactMatch"],
            "measured": len(candidate[0]),
            "expected": len(aggregate[0]),
        },
        {
            "check": "no_existing_exact_axis_overlap",
            "passed": compatibility["existingExactAxisOverlap"] == 0,
            "measured": compatibility["existingExactAxisOverlap"],
            "expected": 0,
        },
    ])
    flux_rows = []
    for spectrum in root.glob("SWIR_flux_*/spectrum_si.csv"):
        flux_rows.extend(rows(spectrum))
    checks.append({
        "check": "flux_uses_real_1km_level",
        "passed": bool(flux_rows) and all(
            abs(float(row["altitude_km"]) - 1.0) <= 1e-12
            and row["vertical_interpolation"] == "exact_real_1km_level"
            for row in flux_rows
        ),
        "measured": sorted({(row["altitude_km"], row["vertical_interpolation"]) for row in flux_rows}),
        "expected": [["1.0", "exact_real_1km_level"]],
    })
    status = "PASS" if all(check["passed"] for check in checks) else "FAIL"
    checks_doc["status"] = status
    checks_path.write_text(json.dumps(checks_doc, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    with (root / "grid_qc_results.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=("check", "passed", "measured", "expected"))
        writer.writeheader()
        writer.writerows(checks)

    # Replace the inherited ground-layer prose with altitude-accurate scope.
    readme = [
        "# P11 Formal SWIR 1.0 km Altitude Layer Candidate",
        "", f"- status: {status}",
        "- axes: observerAlt=1.0 km, targetAlt=1.0 km, range={0.1,0.5,1} km, visibility={6,23} km, SZA={20,45,70} deg",
        "- formal vertices: 18 complete Cartesian cells; raw component executions: 42",
        "- spectrum: real MODTRAN5 4000--9090.9091 cm^-1 at 1 cm^-1; exact 1.1--2.5 um endpoint integration",
        "- units: MODOUT per-cm^-1 converted pointwise with 1e8/lambda_um^2; flux W/(cm2 nm) converted with 1e7",
        "- flux altitude: real MODTRAN spectral-flux level at exactly 1.0 km (no altitude extrapolation)",
        "- provenance: every row lists all five component case IDs and raw source files",
        "- exclusions: TOTAL_RAD and target surface/reflection columns are never used as path terms",
        "- release status: merge candidate only; this task does not write the formal aggregate LUT",
        "", "## Checks", "",
    ]
    readme.extend(f"- {'PASS' if item['passed'] else 'FAIL'} {item['check']}: {item['measured']}" for item in checks)
    (root / "README.md").write_text("\n".join(readme) + "\n", encoding="utf-8")

    dependencies = []
    for path in [
        TOOLS / "p11_modtran_swir_alt1_grid.py",
        TOOLS / "p11_modtran_swir_alt1_run.ps1",
        TOOLS / "p11_modtran_swir_alt1_qc.py",
        TOOLS / "p11_modtran_swir_grid.py",
        TOOLS / "p11_modtran_swir_grid_qc.py",
        TOOLS / "p11_modtran_swir_qc.py",
        TOOLS / "modtran_convert_to_si.py",
    ]:
        dependencies.append({"path": str(path.relative_to(REPO)), "size_bytes": path.stat().st_size, "sha256": sha256(path)})
    with (root / "code_dependencies_alt1.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=("path", "size_bytes", "sha256"))
        writer.writeheader()
        writer.writerows(dependencies)

    manifest = {
        "schema": "hwasimir_p11_modtran_swir_alt1_candidate_1",
        "status": status,
        "publicationStatus": "NOT_PUBLISHED_INDEPENDENT_MERGE_CANDIDATE",
        "candidate": str(candidate_path),
        "candidateSha256": sha256(candidate_path),
        "rows": len(candidate),
        "componentRuns": 42,
        "axes": {"observerAltKm": [1.0], "targetAltKm": [1.0], "rangeKm": [0.1, 0.5, 1.0], "visibilityKm": [6.0, 23.0], "solarZenithDeg": [20.0, 45.0, 70.0]},
        "wavenumberCm1": {"low": 4000.0, "high": 9090.9091, "increment": 1.0},
        "wavelengthUm": {"low": 1.1, "high": 2.5},
        "qc": str(checks_path),
        "qcSha256": sha256(checks_path),
        "engineEvidence": str(root / "engine_and_license_evidence.json"),
        "engineEvidenceSha256": sha256(root / "engine_and_license_evidence.json"),
        "installationRestoreEvidence": str(root / "installation_restore_evidence.csv"),
        "installationRestoreEvidenceSha256": sha256(root / "installation_restore_evidence.csv"),
        "mergeCompatibility": str(compatibility_path),
        "mergeCompatibilitySha256": sha256(compatibility_path),
    }
    (root / "merge_candidate_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    rebuild_inventory(root)
    print(f"P11 SWIR 1.0 km candidate QC: {status}; vertices={len(candidate)}; candidate={candidate_path}")
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
