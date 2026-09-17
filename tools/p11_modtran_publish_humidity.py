#!/usr/bin/env python3
"""Atomically merge QC-passed P11 humidity rows while preserving all other LUT rows."""

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


PROFILES = {
    "scaled_mls_surface_rh30",
    "scaled_mls_surface_rh60",
    "scaled_mls_surface_rh85",
}
PREFIXES = ("P11_SWIR_humidity_", "P11_MWIR_humidity_")
REQUIRED_COMPONENTS = [
    "tau_up", "path_thermal_W_m2_sr_um",
    "direct_solar_irradiance_at_target_W_m2_um",
    "downward_sky_diffuse_irradiance_W_m2_um",
    "los_path_scattering_radiance_W_m2_sr_um",
]


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


def rows_digest(rows: list[dict[str, str]], fields: list[str]) -> str:
    canonical = "\n".join("\x1f".join(row.get(field, "") for field in fields) for row in rows)
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def is_humidity_row(row: dict[str, str]) -> bool:
    return row.get("humidity_profile") in PROFILES and row.get("case_id", "").startswith(PREFIXES)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--candidate", type=Path,
                    default=Path("logs/p11/modtran/humidity_grid/formal_humidity_rows.csv"))
    ap.add_argument("--qc", type=Path,
                    default=Path("logs/p11/modtran/humidity_grid/humidity_qc_results.json"))
    ap.add_argument("--formal", type=Path,
                    default=Path("HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv"))
    ap.add_argument("--evidence-dir", type=Path,
                    default=Path("logs/p11/modtran/humidity_grid/formal_publish"))
    ap.add_argument("--check-only", action="store_true")
    args = ap.parse_args()
    candidate = args.candidate.resolve()
    qc_path = args.qc.resolve()
    formal = args.formal.resolve()
    evidence = args.evidence_dir.resolve()
    evidence.mkdir(parents=True, exist_ok=True)

    qc = json.loads(qc_path.read_text(encoding="utf-8"))
    if qc.get("status") != "PASS" or any(not item.get("passed") for item in qc.get("checks", [])):
        raise ValueError("Humidity QC is not an all-PASS artifact")
    fields, existing = read(formal)
    candidate_fields, additions = read(candidate)
    if fields != candidate_fields:
        raise ValueError("Humidity candidate/formal CSV schemas differ")
    if len(additions) != 216 or len({r["case_id"] for r in additions}) != 216:
        raise ValueError("Humidity candidate must contain 216 unique rows")
    counts = Counter((r["band"], r["humidity_profile"]) for r in additions)
    expected_counts = Counter({(band, profile): 36 for band in ("SWIR", "MWIR") for profile in PROFILES})
    if counts != expected_counts:
        raise ValueError(f"Humidity coverage mismatch: {counts}")
    if any(not is_humidity_row(r) for r in additions):
        raise ValueError("Candidate has an unexpected profile or case-id prefix")
    if any(not r[field] for r in additions for field in REQUIRED_COMPONENTS):
        raise ValueError("Candidate contains a blank production component")
    if any(r["humidity_profile"] == "default" or "76.18" in r["humidity_profile"] for r in additions):
        raise ValueError("Candidate contains a default or fake 76.18 humidity profile")

    replaced = [r for r in existing if is_humidity_row(r)]
    preserved = [r for r in existing if not is_humidity_row(r)]
    preserved_digest = rows_digest(preserved, fields)
    default_rows = [r for r in preserved if r["humidity_profile"] == "default"]
    default_digest = rows_digest(default_rows, fields)
    planned = {
        "schema": "HwaSimIR.P11.HumidityFormalPublish.v1",
        "formal_path": str(formal), "candidate_path": str(candidate),
        "qc_path": str(qc_path), "candidate_sha256": sha256(candidate), "qc_sha256": sha256(qc_path),
        "before_sha256": sha256(formal), "before_rows": len(existing),
        "replaced_humidity_rows": len(replaced), "preserved_rows": len(preserved),
        "added_humidity_rows": len(additions), "planned_after_rows": len(preserved) + len(additions),
        "coverage": {f"{band}/{profile}": count for (band, profile), count in sorted(counts.items())},
        "preserved_row_digest": preserved_digest, "preserved_default_row_count": len(default_rows),
        "preserved_default_row_digest": default_digest,
    }
    (evidence / "publish_plan.json").write_text(json.dumps(planned, indent=2) + "\n", encoding="utf-8")
    shutil.copy2(candidate, evidence / "publisher_merge_input.csv")
    if args.check_only:
        planned["publish_status"] = "CHECK_ONLY_VALIDATED_NOT_WRITTEN"
        print(json.dumps(planned, indent=2))
        return 0

    archive = formal.parent / "Archive" / "P11"
    archive.mkdir(parents=True, exist_ok=True)
    before_hash = sha256(formal)
    backup = archive / f"band_lut_si_pre_p11_humidity_{before_hash[:16]}.csv"
    if backup.exists() and sha256(backup) != before_hash:
        raise ValueError(f"Backup collision: {backup}")
    if not backup.exists():
        shutil.copy2(formal, backup)
    shutil.copy2(formal, evidence / "band_lut_si_before.csv")

    combined = preserved + additions
    temporary = formal.with_suffix(formal.suffix + ".p11humiditytmp")
    with temporary.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader(); writer.writerows(combined)
    os.replace(temporary, formal)

    after_fields, after = read(formal)
    after_preserved = [r for r in after if not is_humidity_row(r)]
    after_humidity = [r for r in after if is_humidity_row(r)]
    if (after_fields != fields or len(after_humidity) != 216 or
            len(after) != len(preserved) + 216 or rows_digest(after_preserved, fields) != preserved_digest):
        raise ValueError("Post-publish preservation/coverage verification failed; archive is available")
    after_default = [r for r in after if r["humidity_profile"] == "default"]
    if len(after_default) != len(default_rows) or rows_digest(after_default, fields) != default_digest:
        raise ValueError("Default rows changed during humidity publication; archive is available")
    manifest = {
        **planned, "published_at_utc": datetime.now(timezone.utc).isoformat(),
        "after_sha256": sha256(formal), "after_rows": len(after),
        "after_band_counts": dict(Counter(r["band"] for r in after)),
        "after_profile_counts": dict(Counter(r["humidity_profile"] for r in after)),
        "archive_backup": str(backup), "archive_backup_sha256": sha256(backup),
        "publish_status": "SUCCESS_ATOMIC_MERGE_PRESERVED_ALL_NON_HUMIDITY_AND_DEFAULT_ROWS",
    }
    (evidence / "publish_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(manifest, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
