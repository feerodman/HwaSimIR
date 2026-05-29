#!/usr/bin/env python3
"""Snapshot the current MODTRAN processed directory before production runs."""

from __future__ import annotations

import argparse
import shutil
from datetime import datetime
from pathlib import Path
from typing import Sequence


SNAPSHOT_FILES = [
    "path_lut_spectral.csv",
    "solar_lut_spectral.csv",
    "sky_lut_spectral.csv",
    "band_lut.csv",
    "band_lut_visibility_smoke.csv",
    "manifest.csv",
    "qc_report.md",
]

OPTIONAL_SNAPSHOT_FILES = [
    "production_invalid_geometry_manifest.csv",
    "invalid_geometry_audit.csv",
    "visibility_failure_diagnosis.csv",
    "path_radiance_failure_diagnosis.csv",
    "modout_units_audit.csv",
]


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--processed-dir", required=True, help="MODTRAN processed directory to snapshot.")
    parser.add_argument("--label", default="pilot72_visibility_smoke", help="Snapshot label prefix.")
    parser.add_argument("--timestamp", default="", help="Optional timestamp suffix; defaults to local time.")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)

    processed_dir = Path(args.processed_dir)
    if not processed_dir.exists() or not processed_dir.is_dir():
        print(f"ERROR: processed directory not found: {processed_dir}")
        return 2

    missing = [name for name in SNAPSHOT_FILES if not (processed_dir / name).exists()]
    if missing:
        print("ERROR: required processed files are missing:")
        for name in missing:
            print(f"  - {processed_dir / name}")
        return 2

    timestamp = args.timestamp or datetime.now().strftime("%Y%m%d_%H%M%S")
    snapshots_root = processed_dir.parent / "processed_snapshots"
    snapshot_dir = snapshots_root / f"{args.label}_{timestamp}"
    if snapshot_dir.exists():
        print(f"ERROR: snapshot directory already exists: {snapshot_dir}")
        return 2

    snapshot_dir.mkdir(parents=True)
    copied_files = list(SNAPSHOT_FILES)
    for name in SNAPSHOT_FILES:
        shutil.copy2(processed_dir / name, snapshot_dir / name)
    for name in OPTIONAL_SNAPSHOT_FILES:
        path = processed_dir / name
        if path.exists():
            shutil.copy2(path, snapshot_dir / name)
            copied_files.append(name)

    manifest = snapshot_dir / "snapshot_manifest.txt"
    manifest.write_text(
        "\n".join(
            [
                f"label={args.label}",
                f"timestamp={timestamp}",
                f"source={processed_dir}",
                "files=" + ",".join(copied_files),
                "",
            ]
        ),
        encoding="utf-8",
    )

    print(f"Snapshot created: {snapshot_dir}")
    for name in copied_files:
        print(f"  {name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
