#!/usr/bin/env python3
"""Atomically append QC-passed P12 2 km rows while byte-preserving the P11 LUT prefix."""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import os
import shutil
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path


PREFIX = "P12_2KM_"
PROFILES = {"default", "scaled_mls_surface_rh30", "scaled_mls_surface_rh60", "scaled_mls_surface_rh85"}
VALUES = ["tau_up", "path_thermal_W_m2_sr_um", "direct_solar_irradiance_at_target_W_m2_um",
          "downward_sky_diffuse_irradiance_W_m2_um", "los_path_scattering_radiance_W_m2_sr_um"]


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def read(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open(newline="", encoding="utf-8-sig") as stream:
        reader = csv.DictReader(stream)
        return list(reader.fieldnames or []), list(reader)


def rows_digest(rows: list[dict[str, str]], fields: list[str]) -> str:
    canonical = "\n".join("\x1f".join(row.get(field, "") for field in fields) for row in rows)
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", type=Path, default=Path("logs/p12/p12c/modtran_2km/formal_2km_rows.csv"))
    parser.add_argument("--qc", type=Path, default=Path("logs/p12/p12c/modtran_2km/qc_results.json"))
    parser.add_argument("--formal", type=Path, default=Path("HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv"))
    parser.add_argument("--evidence-dir", type=Path, default=Path("logs/p12/p12c/modtran_2km/formal_publish"))
    args = parser.parse_args()
    candidate, qc_path, formal = args.candidate.resolve(), args.qc.resolve(), args.formal.resolve()
    evidence = args.evidence_dir.resolve(); evidence.mkdir(parents=True, exist_ok=True)
    qc = json.loads(qc_path.read_text(encoding="utf-8"))
    if qc.get("status") != "PASS" or any(not item.get("passed") for item in qc.get("checks", [])):
        raise ValueError("P12 2 km QC is not all-PASS")
    fields, existing = read(formal)
    candidate_fields, additions = read(candidate)
    if fields != candidate_fields:
        raise ValueError("candidate/formal schemas differ")
    if len(additions) != 96 or len({row["case_id"] for row in additions}) != 96:
        raise ValueError("candidate must contain 96 unique rows")
    if any(not row["case_id"].startswith(PREFIX) or float(row["range_km"]) != 2.0 or
           row["humidity_profile"] not in PROFILES or any(not row[field] for field in VALUES) for row in additions):
        raise ValueError("candidate scope/components invalid")
    expected = Counter({(band, profile): 12 for band in ("SWIR", "MWIR") for profile in PROFILES})
    counts = Counter((row["band"], row["humidity_profile"]) for row in additions)
    if counts != expected:
        raise ValueError(f"coverage mismatch: {counts}")
    if any(row["case_id"].startswith(PREFIX) for row in existing):
        raise ValueError("formal LUT already contains P12 2 km rows; refusing non-byte-preserving replacement")

    before_bytes = formal.read_bytes()
    before_hash = sha256_bytes(before_bytes)
    before_digest = rows_digest(existing, fields)
    newline = "\r\n" if b"\r\n" in before_bytes[:4096] else "\n"
    text = io.StringIO(newline="")
    writer = csv.DictWriter(text, fieldnames=fields, lineterminator=newline)
    writer.writerows(additions)
    append_bytes = text.getvalue().encode("utf-8")
    if not before_bytes.endswith((b"\n", b"\r")):
        append_bytes = newline.encode("ascii") + append_bytes

    archive = formal.parent / "Archive" / "P12"; archive.mkdir(parents=True, exist_ok=True)
    backup = archive / f"band_lut_si_pre_p12_2km_{before_hash[:16]}.csv"
    if backup.exists() and sha256(backup) != before_hash:
        raise ValueError(f"backup collision: {backup}")
    if not backup.exists():
        shutil.copy2(formal, backup)
    shutil.copy2(formal, evidence / "band_lut_si_before.csv")
    temp = formal.with_suffix(formal.suffix + ".p12_2km_tmp")
    temp.write_bytes(before_bytes + append_bytes)
    os.replace(temp, formal)

    after_bytes = formal.read_bytes()
    after_fields, after = read(formal)
    if not after_bytes.startswith(before_bytes):
        raise ValueError("pre-existing LUT bytes were not preserved as an exact prefix")
    if after_fields != fields or len(after) != len(existing) + 96 or after[:len(existing)] != existing:
        raise ValueError("post-publish row preservation/coverage verification failed")
    if rows_digest(after[:len(existing)], fields) != before_digest:
        raise ValueError("pre-existing canonical row digest changed")
    manifest = {
        "schema": "HwaSimIR.P12.Modtran2kmPublish.1", "publishedAtUtc": datetime.now(timezone.utc).isoformat(),
        "formalPath": str(formal), "candidatePath": str(candidate), "qcPath": str(qc_path),
        "beforeSha256": before_hash, "afterSha256": sha256(formal), "beforeRows": len(existing),
        "addedRows": 96, "afterRows": len(after), "preExistingBytesPreservedAsExactPrefix": True,
        "preExistingRowDigestBefore": before_digest,
        "preExistingRowDigestAfter": rows_digest(after[:len(existing)], fields),
        "coverage": {f"{band}/{profile}": count for (band, profile), count in sorted(counts.items())},
        "afterBandCounts": dict(Counter(row["band"] for row in after)),
        "archiveBackup": str(backup), "archiveBackupSha256": sha256(backup),
        "publishStatus": "SUCCESS_ATOMIC_APPEND_BYTE_PRESERVED_ALL_PREEXISTING_ROWS",
    }
    (evidence / "publish_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
