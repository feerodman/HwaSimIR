#!/usr/bin/env python3
"""Atomically append/replace the QC-passed P11 MWIR rows in the formal SI LUT."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import shutil
from datetime import datetime, timezone
from pathlib import Path


PREFIX = "P11_MWIR_ground_"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open(newline="", encoding="utf-8-sig") as f:
        reader = csv.DictReader(f)
        return list(reader.fieldnames or []), list(reader)


def row_digest(rows: list[dict[str, str]], fields: list[str]) -> str:
    canonical = "\n".join("\x1f".join(row.get(field, "") for field in fields) for row in rows)
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--candidate", type=Path,
                    default=Path("logs/p11/modtran/mwir_ground_grid/formal_mwir_rows.csv"))
    ap.add_argument("--formal", type=Path,
                    default=Path("HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv"))
    ap.add_argument("--evidence-dir", type=Path,
                    default=Path("logs/p11/modtran/mwir_ground_grid/formal_publish"))
    args = ap.parse_args()
    candidate, formal, evidence = args.candidate.resolve(), args.formal.resolve(), args.evidence_dir.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    fields, existing = read(formal)
    candidate_fields, additions = read(candidate)
    if fields != candidate_fields:
        raise ValueError("Candidate/formal CSV schemas differ")
    if len(additions) != 36 or any(r["band"] != "MWIR" or not r["case_id"].startswith(PREFIX) for r in additions):
        raise ValueError("Candidate must contain exactly 36 P11 MWIR rows")
    if len({r["case_id"] for r in additions}) != 36:
        raise ValueError("Duplicate P11 MWIR candidate case_id")
    required = ["tau_up", "path_thermal_W_m2_sr_um", "direct_solar_irradiance_at_target_W_m2_um",
                "downward_sky_diffuse_irradiance_W_m2_um", "los_path_scattering_radiance_W_m2_sr_um"]
    if any(not r[field] for r in additions for field in required):
        raise ValueError("A P11 MWIR row has a blank production component")

    before_hash = sha256(formal)
    replaced = [r for r in existing if r["case_id"].startswith(PREFIX)]
    preserved = [r for r in existing if not r["case_id"].startswith(PREFIX)]
    preserved_digest = row_digest(preserved, fields)
    swir_before = [r for r in preserved if r["band"] == "SWIR"]
    swir_digest = row_digest(swir_before, fields)
    archive = formal.parent / "Archive" / "P11"
    archive.mkdir(parents=True, exist_ok=True)
    backup = archive / f"band_lut_si_pre_p11_mwir_{before_hash[:16]}.csv"
    if backup.exists() and sha256(backup) != before_hash:
        raise ValueError(f"Backup collision: {backup}")
    if not backup.exists():
        shutil.copy2(formal, backup)
    shutil.copy2(formal, evidence / "band_lut_si_before.csv")

    combined = preserved + additions
    temp = formal.with_suffix(formal.suffix + ".p11mwirtmp")
    with temp.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields); writer.writeheader(); writer.writerows(combined)
    os.replace(temp, formal)

    after_fields, after_rows = read(formal)
    after_preserved = [r for r in after_rows if not r["case_id"].startswith(PREFIX)]
    if after_fields != fields or len(after_rows) != len(preserved) + 36:
        raise ValueError("Post-publish formal LUT verification failed")
    if row_digest(after_preserved, fields) != preserved_digest:
        raise ValueError("A non-P11-MWIR row changed during publication")
    swir_after = [r for r in after_rows if r["band"] == "SWIR"]
    if row_digest(swir_after, fields) != swir_digest:
        raise ValueError("P11 SWIR rows changed during MWIR publication")
    after_hash = sha256(formal)
    manifest = {
        "schema": "HwaSimIR.P11.MWIRFormalPublish.v1",
        "published_at_utc": datetime.now(timezone.utc).isoformat(),
        "formal_path": str(formal), "candidate_path": str(candidate),
        "candidate_sha256": sha256(candidate), "before_sha256": before_hash, "after_sha256": after_hash,
        "before_rows": len(existing), "replaced_p11_mwir_rows": len(replaced), "after_rows": len(after_rows),
        "nir_rows": sum(r["band"] == "NIR" for r in after_rows),
        "swir_rows": sum(r["band"] == "SWIR" for r in after_rows),
        "mwir_rows_total": sum(r["band"] == "MWIR" for r in after_rows),
        "mwir_legacy_audit_rows": sum(r["band"] == "MWIR" and not r["case_id"].startswith(PREFIX) for r in after_rows),
        "mwir_p11_formal_rows": sum(r["case_id"].startswith(PREFIX) for r in after_rows),
        "preserved_non_p11_row_digest": preserved_digest, "preserved_swir_row_digest": swir_digest,
        "archive_backup": str(backup), "archive_backup_sha256": sha256(backup),
        "publish_status": "SUCCESS_ATOMIC_REPLACE_PRESERVED_SWIR_AND_LEGACY_ROWS",
    }
    (evidence / "publish_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
