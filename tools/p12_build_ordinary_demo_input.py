#!/usr/bin/env python3
"""Build the deterministic P12 ordinary-startup trajectory without touching 1.txt."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    repo = Path(__file__).resolve().parents[1]
    parser.add_argument("--source", type=Path, default=repo / "DataDrivenTestQT" / "1.txt")
    parser.add_argument(
        "--output",
        type=Path,
        default=repo / "DataDrivenTestQT" / "ordinary_demo_1km.txt",
    )
    parser.add_argument("--manifest", type=Path)
    parser.add_argument(
        "--min-rows",
        type=int,
        default=0,
        help="build at least this many 60 Hz samples by cycling source telemetry; geometry is still rewritten",
    )
    args = parser.parse_args()

    source = args.source.resolve()
    output = args.output.resolve()
    manifest = (args.manifest or output.with_suffix(output.suffix + ".json")).resolve()
    if source == output:
        raise SystemExit("refusing to overwrite the source trajectory")

    # The target is approximately 600 m north of the observer at 40 N. Both
    # endpoints are exactly 1 km MSL, a measured MODTRAN grid endpoint.
    observer_lat = 39.994604
    target_lat = 40.0
    longitude = 100.0
    altitude_m = 1000.0

    with source.open("r", encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.reader(stream))
    if not rows:
        raise SystemExit("empty source trajectory")
    header = rows[0]
    required = {
        "distanceDAM_RTState(m)",
        "RedLat(degree)",
        "RedLon(degree)",
        "RedAlt(m)",
        "MissileLat(degree)",
        "MissileLon(degree)",
        "MissileAlt(m)",
        "TargetLatFake(degree)",
        "TargetLonFake(degree)",
        "TargetAltFake(m)",
    }
    missing = sorted(required.difference(header))
    if missing:
        raise SystemExit(f"missing source columns: {missing}")
    column = {name: header.index(name) for name in required}

    source_rows: list[list[str]] = []
    for line_number, row in enumerate(rows[1:], start=2):
        if not row or (len(row) == 1 and not row[0].strip()):
            continue
        if len(row) != len(header):
            raise SystemExit(
                f"source row {line_number} has {len(row)} columns, expected {len(header)}"
            )
        source_rows.append(list(row))
    if not source_rows:
        raise SystemExit("source trajectory has no data rows")

    requested_rows = max(len(source_rows), args.min_rows)
    rewritten: list[list[str]] = [header]
    for output_index in range(requested_rows):
        row = list(source_rows[output_index % len(source_rows)])
        row[column["distanceDAM_RTState(m)"]] = "600.000"
        row[column["RedLat(degree)"]] = f"{observer_lat:.6f}"
        row[column["RedLon(degree)"]] = f"{longitude:.6f}"
        row[column["RedAlt(m)"]] = f"{altitude_m:.3f}"
        row[column["MissileLat(degree)"]] = f"{target_lat:.6f}"
        row[column["MissileLon(degree)"]] = f"{longitude:.6f}"
        row[column["MissileAlt(m)"]] = f"{altitude_m:.3f}"
        row[column["TargetLatFake(degree)"]] = f"{target_lat:.6f}"
        row[column["TargetLonFake(degree)"]] = f"{longitude:.6f}"
        row[column["TargetAltFake(m)"]] = f"{altitude_m:.3f}"
        rewritten.append(row)

    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8", newline="") as stream:
        csv.writer(stream, lineterminator="\n").writerows(rewritten)

    record = {
        "schema": "hwasimir_p12_ordinary_demo_input_v1",
        "source": str(source),
        "source_sha256": sha256(source),
        "output": str(output),
        "output_sha256": sha256(output),
        "rows": len(rewritten) - 1,
        "source_rows": len(source_rows),
        "source_cycles": (len(rewritten) - 2) // len(source_rows) + 1,
        "geometry": {
            "observer_lat_deg": observer_lat,
            "observer_lon_deg": longitude,
            "observer_alt_m": altitude_m,
            "target_lat_deg": target_lat,
            "target_lon_deg": longitude,
            "target_alt_m": altitude_m,
            "declared_los_m": 600.0,
        },
        "purpose": "ordinary startup and display regression; controlled geometry only",
        "physical_claim": "none; original timing, attitude and speed columns retained or deterministically cycled for UI/performance regression",
        "original_modified": False,
    }
    manifest.parent.mkdir(parents=True, exist_ok=True)
    manifest.write_text(json.dumps(record, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps(record, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
