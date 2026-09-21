#!/usr/bin/env python3
"""Generate the independent P14 ground-truck DDS/weather fixture and query matrix.

The fixture is synthetic civil-scene geometry.  It does not read, resample, or
rename DataDrivenTestQT/1.txt and must never be presented as the original replay.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import re
from datetime import date
from pathlib import Path


EARTH_RADIUS_M = 6_371_008.8
ROWS = 1800
FPS = 60.0
INTEGER_COLUMNS = {17, 28, 29, 53, 54, 55, 62}
ENVIRONMENTS = (
    {"name": "clear", "envSky": 0, "visibilityKm": 23.0, "relativeHumidityPercent": 30.0, "utcHour": 5.0},
    {"name": "cloud_visibility", "envSky": 1, "visibilityKm": 12.0, "relativeHumidityPercent": 60.0, "utcHour": 3.5},
    {"name": "rain", "envSky": 2, "visibilityKm": 6.0, "relativeHumidityPercent": 85.0, "utcHour": 1.0},
    {"name": "snow", "envSky": 3, "visibilityKm": 6.0, "relativeHumidityPercent": 85.0, "utcHour": 3.0},
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def schema(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    match = re.search(r'QStringLiteral\("([^"]+)"\)\.split', text)
    if not match:
        raise RuntimeError(f"cannot extract schema from {path}")
    fields = match.group(1).split(",")
    if len(fields) != 63:
        raise RuntimeError(f"expected 63 fields, got {len(fields)}")
    return fields


def geodetic_los_m(observer: tuple[float, float, float], target: tuple[float, float, float]) -> tuple[float, float]:
    lat1, lon1, alt1 = observer
    lat2, lon2, alt2 = target
    phi1, phi2 = math.radians(lat1), math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    hav = math.sin(dphi / 2.0) ** 2 + math.cos(phi1) * math.cos(phi2) * math.sin(dlon / 2.0) ** 2
    horizontal = EARTH_RADIUS_M * 2.0 * math.asin(math.sqrt(max(0.0, min(1.0, hav))))
    return math.hypot(horizontal, alt2 - alt1), horizontal


def wrap360(value: float) -> float:
    value = math.fmod(value, 360.0)
    return value + 360.0 if value < 0.0 else value


def wrap180(value: float) -> float:
    value = wrap360(value)
    return value - 360.0 if value > 180.0 else value


def solar_zenith(latitude: float, longitude: float, utc_date: date, utc_hour: float) -> float:
    year, month = utc_date.year, utc_date.month
    if month <= 2:
        year -= 1
        month += 12
    a = year // 100
    b = 2 - a + a // 4
    day = utc_date.day + utc_hour / 24.0
    jd = math.floor(365.25 * (year + 4716)) + math.floor(30.6001 * (month + 1)) + day + b - 1524.5
    n = jd - 2451545.0
    mean_lon = wrap360(280.460 + 0.9856474 * n)
    mean_anomaly = wrap360(357.528 + 0.9856003 * n)
    ecliptic_lon = wrap360(mean_lon + 1.915 * math.sin(math.radians(mean_anomaly))
                               + 0.020 * math.sin(2.0 * math.radians(mean_anomaly)))
    obliquity = 23.439 - 0.0000004 * n
    lam, eps = math.radians(ecliptic_lon), math.radians(obliquity)
    right_ascension = math.atan2(math.cos(eps) * math.sin(lam), math.cos(lam))
    declination = math.asin(math.sin(eps) * math.sin(lam))
    gmst = wrap360(280.46061837 + 360.98564736629 * n)
    hour_angle = math.radians(wrap180(gmst + longitude - math.degrees(right_ascension)))
    lat = math.radians(latitude)
    sin_elevation = math.sin(lat) * math.sin(declination) + math.cos(lat) * math.cos(declination) * math.cos(hour_angle)
    elevation = math.degrees(math.asin(max(-1.0, min(1.0, sin_elevation))))
    return 90.0 - elevation


def fmt(value: float, integer: bool = False) -> str:
    if integer:
        return str(int(round(value)))
    if abs(value) < 5.0e-13:
        value = 0.0
    text = f"{value:.9f}".rstrip("0").rstrip(".")
    return text if text not in ("", "-0") else "0"


def generate(root: Path, query_log: Path | None) -> dict:
    schema_path = root / "DataDrivenTestQT/ReplayCsvSchema.h"
    fields = schema(schema_path)
    output = root / "DataDrivenTestQT/p14_ground_truck_weather_30s.txt"
    release = root / "build-DataDrivenTestQT-codex-mingw73_64-Release/release/p14_ground_truck_weather_30s.txt"
    evidence = root / "logs/p14/ground_fixture_query"
    evidence.mkdir(parents=True, exist_ok=True)
    query_path = evidence / "p14_ground_query_manifest.csv"
    utc_date = date(2026, 9, 6)

    rows: list[list[float]] = []
    ranges: list[float] = []
    horizontals: list[float] = []
    for index in range(ROWS):
        elapsed_sec = index / FPS
        east_m = elapsed_sec * 2.0  # shared motion; relative LOS remains stable
        longitude_shift = math.degrees(east_m / (EARTH_RADIUS_M * math.cos(math.radians(40.0))))
        # The preserved ground MODTRAN cells contain equal-altitude pairs
        # (0.001/0.001 km and 1/1 km), not their Cartesian cross-product.
        # Keep both sensor and civil target on the legal 1 m ground cell.
        observer = (40.0, 100.0 + longitude_shift, 1.0)
        target = (40.0044966, 100.0 + longitude_shift, 1.0)
        los, horizontal = geodetic_los_m(observer, target)
        row = [0.0] * len(fields)
        row[0] = 20.0 + index * 1000.0 / FPS
        row[1] = los
        row[2:9] = [observer[0], observer[1], observer[2], 90.0, 0.0, 0.0, 36.0]
        row[9] = 1.0
        row[10:18] = [target[0], target[1], target[2], 90.0, 0.0, 0.0, 36.0, 0.0]
        row[47:51] = [0.0, 0.0, 0.0, 1.0]
        row[53:56] = [1.0, 0.0, 0.0]
        row[59:63] = [target[0], target[1], target[2], 1.0]
        rows.append(row)
        ranges.append(los)
        horizontals.append(horizontal)

    payload_lines = [",".join(fields)]
    payload_lines.extend(",".join(fmt(value, column in INTEGER_COLUMNS) for column, value in enumerate(row)) for row in rows)
    payload = ("\n".join(payload_lines) + "\n").encode("utf-8")
    output.write_bytes(payload)
    release.parent.mkdir(parents=True, exist_ok=True)
    release.write_bytes(payload)

    query_fields = [
        "rowIndex", "sourceLine", "sourceTimeMs", "environment", "envSky", "band",
        "observerLatDeg", "observerLonDeg", "observerAltKm", "targetLatDeg", "targetLonDeg", "targetAltKm",
        "productionPreflightLosKm", "visibilityKm", "relativeHumidityPercent", "utcDate", "utcHour",
        "solarZenithDeg", "atmosphereModel", "aerosolModel",
    ]
    sza_ranges: dict[str, list[float]] = {environment["name"]: [] for environment in ENVIRONMENTS}
    with query_path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=query_fields)
        writer.writeheader()
        for index, row in enumerate(rows):
            for environment in ENVIRONMENTS:
                sza = solar_zenith(row[2], row[3], utc_date, float(environment["utcHour"]))
                sza_ranges[environment["name"]].append(sza)
                for band in ("SWIR", "MWIR"):
                    writer.writerow({
                        "rowIndex": index,
                        "sourceLine": index + 2,
                        "sourceTimeMs": row[0],
                        "environment": environment["name"],
                        "envSky": environment["envSky"],
                        "band": band,
                        "observerLatDeg": row[2], "observerLonDeg": row[3], "observerAltKm": row[4] / 1000.0,
                        "targetLatDeg": row[10], "targetLonDeg": row[11], "targetAltKm": row[12] / 1000.0,
                        "productionPreflightLosKm": ranges[index] / 1000.0,
                        "visibilityKm": environment["visibilityKm"],
                        "relativeHumidityPercent": environment["relativeHumidityPercent"],
                        "utcDate": utc_date.isoformat(), "utcHour": environment["utcHour"],
                        "solarZenithDeg": sza,
                        "atmosphereModel": "Mid-Latitude Summer", "aerosolModel": "Rural",
                    })

    query_rows = ROWS * len(ENVIRONMENTS) * 2
    query_binding: dict[str, object] = {
        "queryManifest": str(query_path), "queryManifestSha256": sha256(query_path),
        "productionQueryRows": query_rows, "productionQueryValid": 0,
        "productionQueryFailures": query_rows, "result": "PENDING",
    }
    if query_log is not None:
        text = query_log.read_text(encoding="utf-8", errors="replace")
        match = re.search(r"^SUMMARY loadedEntries=(\d+) queryRows=(\d+) valid=(\d+) failures=(\d+) "
                          r"vertexFailures=(\d+) boundaryFailures=(\d+) tauMin=([0-9.eE+-]+) tauMax=([0-9.eE+-]+)$",
                          text, re.MULTILINE)
        if not match:
            raise RuntimeError(f"cannot parse production query summary: {query_log}")
        loaded, queried, valid, failures, vertex_failures, boundary_failures, tau_min, tau_max = match.groups()
        passed = (int(queried) == query_rows and int(valid) == query_rows and int(failures) == 0
                  and int(vertex_failures) == 0 and int(boundary_failures) == 0)
        if not passed:
            raise RuntimeError(f"production query did not pass: {match.group(0)}")
        query_binding.update({
            "productionQueryCheckLog": str(query_log), "productionQueryCheckLogSha256": sha256(query_log),
            "loadedEntries": int(loaded), "productionQueryRows": int(queried),
            "productionQueryValid": int(valid), "productionQueryFailures": int(failures),
            "vertexFailures": int(vertex_failures), "boundaryFailures": int(boundary_failures),
            "tauMin": float(tau_min), "tauMax": float(tau_max), "result": "PASS",
        })

    manifest = {
        "schema": "hwasimir.p14.ground-truck-weather-fixture.v1",
        "purpose": "p14_ground_truck_mixed_weather_30s",
        "identity": {"name": output.name, "sha256": sha256(output), "bytes": output.stat().st_size,
                     "rows": ROWS, "uniqueObserverTargetStates": ROWS},
        "provenance": {
            "kind": "independently_generated_civil_ground_weather_fixture",
            "readsOriginal1Txt": False, "sourceInputBusinessDependency": False,
            "schemaSource": str(schema_path), "schemaSha256": sha256(schema_path),
            "generator": str(Path(__file__).resolve()), "generatorSha256": sha256(Path(__file__).resolve()),
        },
        "time": {"firstMs": rows[0][0], "lastMs": rows[-1][0], "rowsPerSecond": FPS,
                 "nominalDurationSec": ROWS / FPS, "oneAcceptedRowPerPacket": True},
        "geometry": {
            "observerAltitudeM": [1.0, 1.0], "targetAltitudeM": [1.0, 1.0],
            "slantRangeM": [min(ranges), max(ranges)], "horizontalRangeM": [min(horizontals), max(horizontals)],
            "observerSpeedKmh": 36.0, "targetSpeedKmh": 36.0,
            "viewValid": 1, "targetProtocolCodeAppliedByUi": "0x55",
            "physicalClaim": "generic synthetic civil geometry only",
        },
        "environments": [dict(environment, solarZenithDeg=[min(sza_ranges[environment["name"]]),
                                                            max(sza_ranges[environment["name"]])])
                         for environment in ENVIRONMENTS],
        "productionQueryEvidence": query_binding,
        "boundaries": {
            "notOriginalReplay": True, "mayReplaceOriginal1Txt": False,
            "mustNotBeRenamedAs1Txt": True, "sourceInputBusinessDependency": False,
            "notEquipmentSpecific": True, "calibrationStatus": "NOT_VERIFIED_CALIBRATION",
        },
    }
    manifest_payload = json.dumps(manifest, ensure_ascii=False, indent=2) + "\n"
    output.with_suffix(output.suffix + ".json").write_text(manifest_payload, encoding="utf-8")
    release.with_suffix(release.suffix + ".json").write_text(manifest_payload, encoding="utf-8")
    return {"status": query_binding["result"], "fixture": str(output), "fixtureSha256": sha256(output),
            "manifest": str(output.with_suffix(output.suffix + ".json")), "queryManifest": str(query_path),
            "queryRows": query_rows, "losM": [min(ranges), max(ranges)]}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--query-log", type=Path)
    args = parser.parse_args()
    result = generate(args.root.resolve(), args.query_log.resolve() if args.query_log else None)
    print(json.dumps(result, ensure_ascii=False))
    return 0 if result["status"] in ("PENDING", "PASS") else 1


if __name__ == "__main__":
    raise SystemExit(main())
