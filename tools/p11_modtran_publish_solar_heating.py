#!/usr/bin/env python3
"""Atomically add QC-passed P11 civil rows to the formal solar-heating LUT."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import shutil
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path


PREFIX = "SWHEAT_P11_"
PROFILES = {"default", "scaled_mls_surface_rh30", "scaled_mls_surface_rh60", "scaled_mls_surface_rh85"}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open(newline="", encoding="utf-8-sig") as f:
        reader = csv.DictReader(f)
        return list(reader.fieldnames or []), list(reader)


def digest_rows(rows: list[dict[str, str]], fields: list[str]) -> str:
    canonical = "\n".join("\x1f".join(row.get(field, "") for field in fields) for row in rows)
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--candidate", type=Path,
                    default=Path("logs/p11/modtran/solar_heating_ground_grid/formal_solar_heating_rows.csv"))
    ap.add_argument("--qc", type=Path,
                    default=Path("logs/p11/modtran/solar_heating_ground_grid/solar_heating_qc_results.json"))
    ap.add_argument("--formal", type=Path,
                    default=Path("HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/solar_heating_lut_si.csv"))
    ap.add_argument("--evidence-dir", type=Path,
                    default=Path("logs/p11/modtran/solar_heating_ground_grid/formal_publish"))
    ap.add_argument("--check-only", action="store_true")
    args = ap.parse_args()
    candidate, qc_path, formal = args.candidate.resolve(), args.qc.resolve(), args.formal.resolve()
    evidence = args.evidence_dir.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    qc = json.loads(qc_path.read_text(encoding="utf-8"))
    if qc.get("status") != "PASS" or any(not x.get("passed") for x in qc.get("checks", [])):
        raise ValueError("Solar-heating QC is not all PASS")
    fields, existing = read(formal)
    candidate_fields, additions = read(candidate)
    if fields != candidate_fields:
        raise ValueError("Solar-heating candidate/formal schemas differ")
    if len(additions) != 48 or len({r["case_id"] for r in additions}) != 48:
        raise ValueError("P11 solar-heating candidate must have 48 unique rows")
    counts = Counter(r["humidity_profile"] for r in additions)
    if counts != Counter({profile: 12 for profile in PROFILES}):
        raise ValueError(f"P11 solar-heating profile coverage mismatch: {counts}")
    if any(not r["case_id"].startswith(PREFIX) or
           r["band"] != "SOLAR_SHORTWAVE_0.30_2.50_UM" or
           r["spectral_range_um"] != "0.30-2.50" or r["irradiance_unit"] != "W/m^2"
           for r in additions):
        raise ValueError("P11 solar-heating candidate identity/unit mismatch")

    replaced = [r for r in existing if r["case_id"].startswith(PREFIX)]
    preserved = [r for r in existing if not r["case_id"].startswith(PREFIX)]
    preserved_digest = digest_rows(preserved, fields)
    plan = {
        "schema": "HwaSimIR.P11.SolarHeatingFormalPublish.v1",
        "formal_path": str(formal), "candidate_path": str(candidate), "qc_path": str(qc_path),
        "candidate_sha256": sha256(candidate), "qc_sha256": sha256(qc_path),
        "before_sha256": sha256(formal), "before_rows": len(existing),
        "replaced_p11_rows": len(replaced), "preserved_rows": len(preserved),
        "preserved_row_digest": preserved_digest, "added_p11_rows": 48,
        "planned_after_rows": len(preserved) + 48, "profile_counts": dict(counts),
    }
    (evidence / "publish_plan.json").write_text(json.dumps(plan, indent=2) + "\n", encoding="utf-8")
    shutil.copy2(candidate, evidence / "publisher_merge_input.csv")
    if args.check_only:
        plan["publish_status"] = "CHECK_ONLY_VALIDATED_NOT_WRITTEN"
        print(json.dumps(plan, indent=2))
        return 0

    archive = formal.parent / "Archive" / "P11"
    archive.mkdir(parents=True, exist_ok=True)
    before_hash = sha256(formal)
    backup = archive / f"solar_heating_lut_si_pre_p11_ground_{before_hash[:16]}.csv"
    if backup.exists() and sha256(backup) != before_hash:
        raise ValueError(f"Backup collision: {backup}")
    if not backup.exists():
        shutil.copy2(formal, backup)
    shutil.copy2(formal, evidence / "solar_heating_lut_si_before.csv")
    combined = preserved + additions
    temporary = formal.with_suffix(formal.suffix + ".p11tmp")
    with temporary.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields); writer.writeheader(); writer.writerows(combined)
    os.replace(temporary, formal)
    after_fields, after = read(formal)
    after_preserved = [r for r in after if not r["case_id"].startswith(PREFIX)]
    after_p11 = [r for r in after if r["case_id"].startswith(PREFIX)]
    if (after_fields != fields or len(after_p11) != 48 or len(after) != len(preserved) + 48 or
            digest_rows(after_preserved, fields) != preserved_digest):
        raise ValueError("Post-publish solar-heating verification failed; archive is available")
    manifest = {
        **plan, "published_at_utc": datetime.now(timezone.utc).isoformat(),
        "after_sha256": sha256(formal), "after_rows": len(after),
        "after_profile_counts": dict(Counter(r["humidity_profile"] for r in after)),
        "archive_backup": str(backup), "archive_backup_sha256": sha256(backup),
        "publish_status": "SUCCESS_ATOMIC_MERGE_PRESERVED_LEGACY_45_ROWS",
    }
    (evidence / "publish_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
