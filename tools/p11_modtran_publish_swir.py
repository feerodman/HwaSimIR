#!/usr/bin/env python3
"""Atomically publish the QC-passed P11 SWIR rows into the formal SI LUT."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import shutil
from datetime import datetime, timezone
from pathlib import Path


PREFIX = "P11_SWIR_"
GROUND_PREFIX = "P11_SWIR_ground_"
ALT1_PREFIX = "P11_SWIR_alt1_"


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


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--candidate", type=Path, default=Path("logs/p11/modtran/swir_ground_grid/formal_swir_rows.csv"))
    ap.add_argument("--alt-candidate", type=Path,
                    default=Path("logs/p11/modtran/swir_alt1_grid/merge_candidate_swir_alt1.csv"))
    ap.add_argument("--formal", type=Path,
                    default=Path("HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv"))
    ap.add_argument("--evidence-dir", type=Path, default=Path("logs/p11/modtran/swir_ground_grid/formal_publish"))
    args = ap.parse_args()
    candidate = args.candidate.resolve()
    alt_candidate = args.alt_candidate.resolve()
    formal, evidence = args.formal.resolve(), args.evidence_dir.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    fields, existing = read(formal)
    candidate_fields, ground_rows = read(candidate)
    alt_fields, alt_rows = read(alt_candidate)
    if fields != candidate_fields or fields != alt_fields:
        raise ValueError("Candidate/formal CSV schemas differ")
    if len(ground_rows) != 18 or any(row["band"] != "SWIR" or
                                     not row["case_id"].startswith(GROUND_PREFIX) for row in ground_rows):
        raise ValueError("Ground candidate must contain exactly 18 P11 SWIR rows")
    if len(alt_rows) != 18 or any(row["band"] != "SWIR" or
                                  not row["case_id"].startswith(ALT1_PREFIX) for row in alt_rows):
        raise ValueError("Altitude candidate must contain exactly 18 P11 SWIR rows")
    additions = ground_rows + alt_rows
    if len({row["case_id"] for row in additions}) != len(additions):
        raise ValueError("Duplicate P11 SWIR candidate case_id")
    before_hash = sha256(formal)
    replaced = [row for row in existing if row["case_id"].startswith(PREFIX)]
    p11_mwir = [row for row in existing if row["case_id"].startswith("P11_MWIR_ground_")]
    preserved = [row for row in existing if not row["case_id"].startswith(PREFIX) and
                 not row["case_id"].startswith("P11_MWIR_ground_")]
    archive = formal.parent / "Archive" / "P11"
    archive.mkdir(parents=True, exist_ok=True)
    backup = archive / f"band_lut_si_pre_p11_swir_{before_hash[:16]}.csv"
    if backup.exists() and sha256(backup) != before_hash:
        raise ValueError(f"Backup collision: {backup}")
    if not backup.exists():
        shutil.copy2(formal, backup)
    shutil.copy2(formal, evidence / "band_lut_si_before.csv")

    # Match modtran_build_lut.py's canonical, byte-reproducible order: the
    # established audit rows, both P11 SWIR altitude planes, then P11 MWIR.
    combined = preserved + additions + p11_mwir
    temp = formal.with_suffix(formal.suffix + ".p11tmp")
    with temp.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader(); writer.writerows(combined)
    os.replace(temp, formal)
    after_fields, after_rows = read(formal)
    if after_fields != fields or len(after_rows) != len(preserved) + len(p11_mwir) + 36:
        raise ValueError("Post-publish formal LUT verification failed")
    if sum(row["band"] == "SWIR" and row["case_id"].startswith(PREFIX) for row in after_rows) != 36:
        raise ValueError("Post-publish P11 SWIR row count mismatch")
    after_hash = sha256(formal)
    manifest = {
        "schema": "HwaSimIR.P11.SWIRFormalPublish.v1",
        "published_at_utc": datetime.now(timezone.utc).isoformat(),
        "formal_path": str(formal),
        "candidate_path": str(candidate),
        "candidate_sha256": sha256(candidate),
        "alt_candidate_path": str(alt_candidate),
        "alt_candidate_sha256": sha256(alt_candidate),
        "before_sha256": before_hash,
        "after_sha256": after_hash,
        "before_rows": len(existing),
        "replaced_p11_rows": len(replaced),
        "after_rows": len(after_rows),
        "nir_rows": sum(row["band"] == "NIR" for row in after_rows),
        "swir_rows": sum(row["band"] == "SWIR" for row in after_rows),
        "mwir_rows": sum(row["band"] == "MWIR" for row in after_rows),
        "archive_backup": str(backup),
        "archive_backup_sha256": sha256(backup),
        "publish_status": "SUCCESS_ATOMIC_REPLACE_TWO_ALTITUDE_PLANES",
    }
    (evidence / "publish_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
