#!/usr/bin/env python3
"""Atomically append all-PASS P14 visibility rows and publish shared identity.

The existing LUT is preserved as an exact byte prefix.  This script never
invents a missing component and only accepts the licensed-MODTRAN candidate
produced by the P14 QC gate.
"""

from __future__ import annotations

import argparse
from collections import Counter
import csv
from datetime import datetime, timezone
import hashlib
import io
import json
import os
from pathlib import Path
import shutil


PREFIX = "P14_MIX_"
VALUE_FIELDS = [
    "tau_up", "path_thermal_W_m2_sr_um",
    "direct_solar_irradiance_at_target_W_m2_um",
    "downward_sky_diffuse_irradiance_W_m2_um",
    "los_path_scattering_radiance_W_m2_sr_um",
]


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def read_csv(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open(newline="", encoding="utf-8-sig") as stream:
        reader = csv.DictReader(stream)
        return list(reader.fieldnames or []), list(reader)


def canonical_digest(rows: list[dict[str, str]], fields: list[str]) -> str:
    text = "\n".join("\x1f".join(row.get(field, "") for field in fields)
                     for row in rows)
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", type=Path, default=Path(
        "logs/p14/atmosphere/highalt_vis23/formal_p14mix_rows.csv"))
    parser.add_argument("--qc", type=Path, default=Path(
        "logs/p14/atmosphere/highalt_vis23/qc_results.json"))
    parser.add_argument("--formal", type=Path, default=Path(
        "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv"))
    parser.add_argument("--input", type=Path, default=Path("DataDrivenTestQT/1.txt"))
    parser.add_argument("--p13-manifest", type=Path, default=Path(
        "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/p13_coverage_manifest.json"))
    parser.add_argument("--evidence-dir", type=Path, default=Path(
        "logs/p14/atmosphere/highalt_vis23/formal_publish"))
    args = parser.parse_args()

    candidate = args.candidate.resolve()
    qc_path = args.qc.resolve()
    formal = args.formal.resolve()
    input_path = args.input.resolve()
    p13_manifest_path = args.p13_manifest.resolve()
    evidence = args.evidence_dir.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    qc = json.loads(qc_path.read_text(encoding="utf-8"))
    if qc.get("schema") != "HwaSimIR.P14.ModtranQC.1" or qc.get("status") != "PASS":
        raise ValueError("P14 MODTRAN QC is not all-PASS")
    if any(not item.get("passed") for item in qc.get("checks", [])):
        raise ValueError("P14 MODTRAN QC contains a failed check")
    p13_manifest = json.loads(p13_manifest_path.read_text(encoding="utf-8"))

    fields, existing = read_csv(formal)
    candidate_fields, additions = read_csv(candidate)
    if fields != candidate_fields:
        raise ValueError("candidate/formal CSV schemas differ")
    if len(additions) != 336 or len({row["case_id"] for row in additions}) != 336:
        raise ValueError("P14 candidate must contain 336 unique rows")
    if any(not row["case_id"].startswith(PREFIX) for row in additions):
        raise ValueError("unexpected P14 case-id prefix")
    if any(not row[field] for row in additions for field in VALUE_FIELDS):
        raise ValueError("candidate contains a missing production component")
    if any(row["case_id"].startswith(PREFIX) for row in existing):
        raise ValueError("P14 rows already present; refusing duplicate publish")
    expected = Counter({
        (band, profile): 56
        for band in ("SWIR", "MWIR")
        for profile in ("scaled_mls_surface_rh30",
                        "scaled_mls_surface_rh60",
                        "scaled_mls_surface_rh85")
    })
    counts = Counter((row["band"], row["humidity_profile"])
                     for row in additions)
    if counts != expected:
        raise ValueError(f"P14 coverage counts differ: {counts}")
    if any(float(row["visibility_km"]) != 23.0 for row in additions):
        raise ValueError("P14 additions must be the measured 23 km visibility face")

    before_bytes = formal.read_bytes()
    before_hash = sha256_bytes(before_bytes)
    if before_hash != p13_manifest["dataIdentity"]["formalLutSha256"]:
        raise ValueError("formal LUT no longer matches the reviewed P13 identity")
    before_digest = canonical_digest(existing, fields)
    newline = "\r\n" if b"\r\n" in before_bytes[:4096] else "\n"
    text = io.StringIO(newline="")
    writer = csv.DictWriter(text, fieldnames=fields, lineterminator=newline)
    writer.writerows(additions)
    append_bytes = text.getvalue().encode("utf-8")
    if not before_bytes.endswith((b"\n", b"\r")):
        append_bytes = newline.encode("ascii") + append_bytes

    archive = formal.parent / "Archive" / "P14"
    archive.mkdir(parents=True, exist_ok=True)
    backup = archive / f"band_lut_si_pre_p14_{before_hash[:16]}.csv"
    if backup.exists() and sha256(backup) != before_hash:
        raise ValueError(f"backup collision: {backup}")
    if not backup.exists():
        shutil.copy2(formal, backup)
    shutil.copy2(formal, evidence / "band_lut_si_before.csv")
    temporary = formal.with_suffix(formal.suffix + ".p14tmp")
    temporary.write_bytes(before_bytes + append_bytes)
    os.replace(temporary, formal)

    after_bytes = formal.read_bytes()
    after_fields, after = read_csv(formal)
    if not after_bytes.startswith(before_bytes):
        raise ValueError("pre-P14 LUT bytes are not an exact prefix")
    if (after_fields != fields or len(after) != len(existing) + len(additions) or
            after[:len(existing)] != existing or
            canonical_digest(after[:len(existing)], fields) != before_digest):
        raise ValueError("post-publish row preservation failed")

    lut_hash = sha256(formal)
    input_hash = sha256(input_path)
    run_evidence = Path("logs/p14/atmosphere/highalt_vis23/engine_and_license_evidence.json")
    manifest_path = formal.parent / "p14_coverage_manifest.json"
    manifest = {
        "schema": "HwaSimIR.P14.SharedAtmosphereCoverage.1",
        "publishedAtUtc": datetime.now(timezone.utc).isoformat(),
        "dataIdentity": {
            "formalLutPathRelativeToConfigRoot":
                "Atmosphere/MODTRAN/processed/band_lut_si.csv",
            "formalLutSha256": lut_hash,
            "formalLutBytes": formal.stat().st_size,
            "formalLutRows": len(after),
            "coverageManifestFile": manifest_path.name,
            "candidateSha256": sha256(candidate),
            "qcSha256": sha256(qc_path),
            "realModtranRunEvidenceSha256": sha256(run_evidence),
            "priorP13ManifestSha256": sha256(p13_manifest_path),
        },
        "preservation": {
            "beforeSha256": before_hash,
            "beforeRows": len(existing),
            "addedRows": len(additions),
            "preExistingBytesPreservedAsExactPrefix": True,
            "preExistingCanonicalDigestBefore": before_digest,
            "preExistingCanonicalDigestAfter": canonical_digest(after[:len(existing)], fields),
            "archiveBackup": str(backup),
            "archiveBackupSha256": sha256(backup),
            "nirRowsBefore": sum(row["band"] == "NIR" for row in existing),
            "nirRowsAfter": sum(row["band"] == "NIR" for row in after),
        },
        "immutableInput": {
            "path": str(input_path),
            "sha256": input_hash,
            "bytes": input_path.stat().st_size,
            "acceptedRows": 4318,
            "role": "test_lineage_only_not_external_sender_business_dependency",
        },
        "externalSenderContract": {
            "querySource": "current_accepted_realtime_sample",
            "requiresOriginal1TxtHash": False,
            "rejectsInvalidPlaceholderBeforeQuery": True,
            "noRangeClamp": True,
            "noExtrapolation": True,
            "noMissingFill": True,
            "noTauOneFallback": True,
        },
        "additionalValidatedInputs": p13_manifest.get("additionalValidatedInputs", []),
        "original1TxtActualQueryEnvelope": p13_manifest["original1TxtActualQueryEnvelope"],
        "p13MeasuredGrid": {
            "bands": ["SWIR", "MWIR"],
            "atmosphereModel": "Mid-Latitude Summer",
            "aerosolModel": "Rural",
            "humidityProfiles": ["scaled_mls_surface_rh30",
                                 "scaled_mls_surface_rh60",
                                 "scaled_mls_surface_rh85"],
            "observerAltitudeKm": [10.95, 12.0],
            "targetAltitudeKm": [9.7, 10.05],
            "slantRangeKm": [2.4, 5.0, 10.0, 20.0, 23.0, 35.0, 50.0],
            "visibilityKm": [6.0, 23.0],
            "solarZenithDeg": [20.0, 45.0],
            "completeFiveComponentVertices": 672,
            "licensedModtranComponentRunsP13": 720,
            "licensedModtranComponentRunsP14Increment": 720,
            "interpolation": "linear_components_and_tau_optical_depth_inside_measured_cell_only",
            "noExtrapolation": True,
            "noRangeClamp": True,
            "noMissingFill": True,
            "noTauOneFallback": True,
        },
        "coverageDomains": {
            "highAltitudeUnequalAltitudeTo50Km": {
                "bands": ["SWIR", "MWIR"],
                "observerAltitudeKm": [10.95, 12.0],
                "targetAltitudeKm": [9.7, 10.05],
                "slantRangeKm": [2.4, 5.0, 10.0, 20.0, 23.0, 35.0, 50.0],
                "visibilityKm": [6.0, 23.0],
                "validatedInterpolatedVisibilityKm": [12.0],
                "solarZenithDeg": [20.0, 45.0],
                "humidityPercent": [30.0, 60.0, 85.0],
            },
            "groundMixedWeather": {
                "bands": ["SWIR", "MWIR"],
                "observerAltitudeKm": [0.001, 1.0],
                "targetAltitudeKm": [0.001, 1.0],
                "slantRangeKm": [0.1, 0.5, 1.0, 2.0],
                "visibilityKm": [6.0, 23.0],
                "validatedInterpolatedVisibilityKm": [12.0],
                "solarZenithDeg": [20.0, 45.0, 70.0],
                "humidityPercent": [30.0, 60.0, 85.0],
                "completeFiveComponentRowsPerBand": 192,
                "source": "preserved_pre_P13_licensed_MODTRAN_rows",
            },
            "notACartesianUnion": True,
        },
        "separateDeclaredExtension": {
            "description": "50 km high-altitude unequal-altitude boundary; not present in 1.txt",
            "maximumSlantRangeKm": 50.0,
        },
        "calibrationStatus": "NOT_VERIFIED_CALIBRATION",
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    publish = {
        "schema": "HwaSimIR.P14.ModtranPublish.1",
        "publishStatus": "SUCCESS_ATOMIC_APPEND_BYTE_PRESERVED_ALL_PREEXISTING_ROWS",
        "formalPath": str(formal),
        "beforeSha256": before_hash,
        "afterSha256": lut_hash,
        "beforeRows": len(existing),
        "addedRows": len(additions),
        "afterRows": len(after),
        "coverageManifest": str(manifest_path),
        "coverageManifestSha256": sha256(manifest_path),
        "inputSha256": input_hash,
        "inputRole": "test_lineage_only_not_external_sender_business_dependency",
        "archiveBackup": str(backup),
        "counts": {f"{band}/{profile}": value
                   for (band, profile), value in sorted(counts.items())},
        "calibrationStatus": "NOT_VERIFIED_CALIBRATION",
    }
    (evidence / "publish_manifest.json").write_text(
        json.dumps(publish, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(publish, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
