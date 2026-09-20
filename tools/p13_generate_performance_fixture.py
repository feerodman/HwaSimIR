#!/usr/bin/env python3
"""Generate a clearly labelled 300 s P13 performance-only replay fixture.

The immutable DataDrivenTestQT/1.txt is never edited.  The source path is
traversed once (no cycles and no repeated source frames) and resampled to
18,000 unique states.  This is timing/stability input, not the original replay
and not a calibrated or equipment-specific physical trajectory.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
from pathlib import Path


SOURCE_SHA256 = "f2c3db00d71012cd28336b5077e03ff8d4ed1eda2698fd900486ae9cdfdd7901"
EARTH_RADIUS_M = 6_371_008.8
CATEGORICAL = {17, 28, 29, 53, 54, 55, 62}
CIRCULAR_DEGREES = {5, 13}
RATE_COLUMNS = {41, 42, 43}
ACCEL_COLUMNS = {44, 45, 46}
SPEED_COLUMNS = {8, 16}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def lerp(left: float, right: float, fraction: float) -> float:
    return left + (right - left) * fraction


def lerp_degrees(left: float, right: float, fraction: float) -> float:
    delta = (right - left + 180.0) % 360.0 - 180.0
    result = (left + delta * fraction) % 360.0
    return 0.0 if abs(result - 360.0) < 1e-9 else result


def los_m(row: list[float]) -> tuple[float, float]:
    lat1, lon1, alt1 = row[2], row[3], row[4]
    lat2, lon2, alt2 = row[10], row[11], row[12]
    phi1, phi2 = math.radians(lat1), math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlambda = math.radians(lon2 - lon1)
    hav = math.sin(dphi / 2.0) ** 2 + math.cos(phi1) * math.cos(phi2) * math.sin(dlambda / 2.0) ** 2
    horizontal = EARTH_RADIUS_M * 2.0 * math.asin(math.sqrt(max(0.0, min(1.0, hav))))
    return math.hypot(horizontal, alt2 - alt1), horizontal


def format_value(value: float, *, integer: bool = False) -> str:
    if integer:
        return str(int(round(value)))
    if abs(value) < 5e-13:
        value = 0.0
    text = f"{value:.9f}".rstrip("0").rstrip(".")
    return text if text not in {"", "-0"} else "0"


def main() -> int:
    parser = argparse.ArgumentParser()
    root_default = Path(__file__).resolve().parents[1]
    parser.add_argument("--root", type=Path, default=root_default)
    parser.add_argument("--rows", type=int, default=18_000)
    parser.add_argument("--duration-sec", type=float, default=300.0)
    args = parser.parse_args()
    root = args.root.resolve()
    source = root / "DataDrivenTestQT/1.txt"
    output = root / "DataDrivenTestQT/p13_performance_highalt_300s.txt"
    release = root / "build-DataDrivenTestQT-codex-mingw73_64-Release/release/p13_performance_highalt_300s.txt"
    if sha256(source) != SOURCE_SHA256:
        raise RuntimeError("immutable source 1.txt hash mismatch")
    if args.rows < 2 or args.duration_sec < 1.0:
        raise ValueError("fixture needs at least two rows and one second")

    with source.open("r", encoding="utf-8-sig", newline="") as stream:
        reader = csv.reader(stream)
        header = next(reader)
        source_rows = [[float(value) for value in row] for row in reader if row and any(value.strip() for value in row)]
    if len(header) != 63 or len(source_rows) != 4318 or any(len(row) != 63 for row in source_rows):
        raise RuntimeError("source schema or row count mismatch")

    source_duration_ms = source_rows[-1][0] - source_rows[0][0]
    output_first_ms = source_rows[0][0]
    output_last_ms = output_first_ms + args.duration_sec * 1000.0 - 1000.0 / 60.0
    output_duration_ms = output_last_ms - output_first_ms
    time_scale = source_duration_ms / output_duration_ms
    generated: list[list[float]] = []
    for index in range(args.rows):
        position = index * (len(source_rows) - 1) / (args.rows - 1)
        left_index = int(math.floor(position))
        right_index = min(left_index + 1, len(source_rows) - 1)
        fraction = position - left_index
        left, right = source_rows[left_index], source_rows[right_index]
        row: list[float] = []
        for column in range(63):
            if column in CATEGORICAL:
                value = left[column] if fraction < 0.5 else right[column]
            elif column in CIRCULAR_DEGREES:
                value = lerp_degrees(left[column], right[column], fraction)
            else:
                value = lerp(left[column], right[column], fraction)
            if column in SPEED_COLUMNS or column in RATE_COLUMNS:
                value *= time_scale
            elif column in ACCEL_COLUMNS:
                value *= time_scale * time_scale
            row.append(value)
        row[0] = lerp(output_first_ms, output_last_ms, index / (args.rows - 1))
        row[1] = los_m(row)[0]
        generated.append(row)

    positions = {(round(row[2], 10), round(row[3], 10), round(row[4], 6),
                  round(row[10], 10), round(row[11], 10), round(row[12], 6)) for row in generated}
    if len(positions) != len(generated):
        raise RuntimeError("generated fixture contains repeated observer/target states")
    ranges = [los_m(row) for row in generated]
    los_values = [item[0] for item in ranges]
    horizontal_values = [item[1] for item in ranges]
    observer_alt = [row[4] for row in generated]
    target_alt = [row[12] for row in generated]
    support_ok = (min(observer_alt) >= 10_950.0 and max(observer_alt) <= 12_000.0 and
                  min(target_alt) >= 9_700.0 and max(target_alt) <= 10_050.0 and
                  min(los_values) >= 2_400.0 and max(los_values) <= 50_000.0)
    if not support_ok:
        raise RuntimeError("performance fixture leaves formal P13 atmosphere support")

    output.parent.mkdir(parents=True, exist_ok=True)
    release.parent.mkdir(parents=True, exist_ok=True)
    lines = [",".join(header)]
    for row in generated:
        lines.append(",".join(format_value(value, integer=column in CATEGORICAL)
                              for column, value in enumerate(row)))
    payload = ("\n".join(lines) + "\n").encode("utf-8")
    output.write_bytes(payload)
    release.write_bytes(payload)
    output_hash = sha256(output)
    if sha256(release) != output_hash:
        raise RuntimeError("source and release fixture hashes differ")

    manifest = {
        "schema": "hwasimir.p13.performance-fixture.v1",
        "purpose": "performance_only_300s_complex_weather",
        "identity": {
            "name": output.name,
            "sha256": output_hash,
            "rows": len(generated),
            "uniqueObserverTargetStates": len(positions),
        },
        "provenance": {
            "source": "DataDrivenTestQT/1.txt",
            "sourceSha256": SOURCE_SHA256,
            "sourceRows": len(source_rows),
            "sourceTraversalCount": 1,
            "sourceCycles": 0,
            "interpolation": "single_pass_linear_with_shortest_arc_yaw",
            "timeScale": time_scale,
        },
        "time": {
            "firstMs": generated[0][0], "lastMs": generated[-1][0],
            "sourceDurationMs": generated[-1][0] - generated[0][0],
            "nominalSendFps": 60, "nominalVideoDurationSec": len(generated) / 60.0,
        },
        "queryRanges": {
            "observerAltitudeM": {"min": min(observer_alt), "max": max(observer_alt)},
            "targetAltitudeM": {"min": min(target_alt), "max": max(target_alt)},
            "productionLosM": {"min": min(los_values), "max": max(los_values)},
            "horizontalM": {"min": min(horizontal_values), "max": max(horizontal_values)},
        },
        "formalSupport": {
            "result": "PASS", "bands": ["SWIR", "MWIR"], "visibilityKm": 6.0,
            "observerAltitudeKm": [10.95, 12.0], "targetAltitudeKm": [9.7, 10.05],
            "rangeKm": [2.4, 50.0],
        },
        "boundaries": {
            "notOriginalReplay": True,
            "mustNotReplaceOrBeRenamedAs1Txt": True,
            "physicalClaim": "none",
            "calibrationStatus": "NOT_VERIFIED_CALIBRATION",
        },
    }
    manifest_payload = json.dumps(manifest, ensure_ascii=False, indent=2) + "\n"
    output.with_suffix(output.suffix + ".json").write_text(manifest_payload, encoding="utf-8")
    release.with_suffix(release.suffix + ".json").write_text(manifest_payload, encoding="utf-8")
    print(json.dumps({"result": "PASS", "output": str(output), "sha256": output_hash,
                      "rows": len(generated), "uniqueStates": len(positions),
                      "losM": [min(los_values), max(los_values)]}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
