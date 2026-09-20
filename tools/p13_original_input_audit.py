#!/usr/bin/env python3
"""Audit the immutable DataDrivenTestQT replay with production-equivalent geometry.

This tool is intentionally read-only with respect to the input, model assets, and
formal LUT.  It emits an evidence JSON, a concise Markdown report, and the exact
per-row SWIR/MWIR query requirements used by the P13 coverage work.
"""

from __future__ import annotations

import argparse
import configparser
import csv
import hashlib
import json
import math
import re
from collections import Counter
from datetime import date
from pathlib import Path


EARTH_RADIUS_M = 6371008.8


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def schema_from_header(path: Path) -> list[str]:
    text = path.read_text(encoding="utf-8")
    match = re.search(r'QStringLiteral\("([^"]+)"\)\.split', text)
    if not match:
        raise RuntimeError(f"cannot extract replay schema from {path}")
    return match.group(1).split(",")


def geodetic_los_m(observer: tuple[float, float, float], target: tuple[float, float, float]) -> tuple[float, float]:
    """Match DataDrivenTestQT::geodeticLosMeters exactly (spherical haversine + dz)."""
    lat1 = math.radians(observer[0])
    lat2 = math.radians(target[0])
    delta_lat = math.radians(target[0] - observer[0])
    delta_lon = math.radians(target[1] - observer[1])
    sin_lat = math.sin(delta_lat * 0.5)
    sin_lon = math.sin(delta_lon * 0.5)
    haversine = sin_lat * sin_lat + math.cos(lat1) * math.cos(lat2) * sin_lon * sin_lon
    central = 2.0 * math.asin(math.sqrt(max(0.0, min(1.0, haversine))))
    horizontal = EARTH_RADIUS_M * central
    vertical = target[2] - observer[2]
    return math.hypot(horizontal, vertical), horizontal


def wrap360(value: float) -> float:
    value = math.fmod(value, 360.0)
    return value + 360.0 if value < 0.0 else value


def wrap180(value: float) -> float:
    value = wrap360(value)
    return value - 360.0 if value > 180.0 else value


def julian_day(year: int, month: int, day: int, hour: float) -> float:
    if month <= 2:
        year -= 1
        month += 12
    a = year // 100
    b = 2 - a + a // 4
    fractional_day = float(day) + hour / 24.0
    return math.floor(365.25 * (year + 4716)) + math.floor(30.6001 * (month + 1)) + fractional_day + b - 1524.5


def solar_position(latitude: float, longitude: float, utc_date: date, utc_hour: float) -> tuple[float, float, float]:
    """Match IRSolarPosition.cpp for audit/query-manifest reproducibility."""
    jd = julian_day(utc_date.year, utc_date.month, utc_date.day, utc_hour)
    n = jd - 2451545.0
    mean_longitude = wrap360(280.460 + 0.9856474 * n)
    mean_anomaly = wrap360(357.528 + 0.9856003 * n)
    ecliptic_longitude = wrap360(
        mean_longitude + 1.915 * math.sin(math.radians(mean_anomaly))
        + 0.020 * math.sin(2.0 * math.radians(mean_anomaly))
    )
    obliquity = 23.439 - 0.0000004 * n
    lam = math.radians(ecliptic_longitude)
    eps = math.radians(obliquity)
    right_ascension = math.atan2(math.cos(eps) * math.sin(lam), math.cos(lam))
    declination = math.asin(math.sin(eps) * math.sin(lam))
    gmst = wrap360(280.46061837 + 360.98564736629 * n)
    hour_angle = math.radians(wrap180(gmst + longitude - math.degrees(right_ascension)))
    latitude_rad = math.radians(latitude)
    sin_elevation = (
        math.sin(latitude_rad) * math.sin(declination)
        + math.cos(latitude_rad) * math.cos(declination) * math.cos(hour_angle)
    )
    elevation = math.asin(max(-1.0, min(1.0, sin_elevation)))
    azimuth = wrap360(math.degrees(math.atan2(
        math.sin(hour_angle),
        math.cos(hour_angle) * math.sin(latitude_rad) - math.tan(declination) * math.cos(latitude_rad),
    )) + 180.0)
    elevation_deg = math.degrees(elevation)
    return azimuth, elevation_deg, 90.0 - elevation_deg


def parse_date(value: str) -> date:
    year, month, day = (int(part) for part in value.split("-"))
    return date(year, month, day)


def finite_float(value: str, line: int, column: int, name: str) -> float:
    try:
        parsed = float(value)
    except ValueError as error:
        raise ValueError(f"line {line}, column {column} ({name}): expected number") from error
    if not math.isfinite(parsed):
        raise ValueError(f"line {line}, column {column} ({name}): expected finite number")
    return parsed


def minmax(values: list[float]) -> dict[str, float]:
    return {"min": min(values), "max": max(values)}


def read_query_result(path: Path, expected_rows: int, query_manifest_path: Path) -> dict[str, object] | None:
    """Bind a completed production-query log to the deterministic query manifest."""
    if not path.is_file():
        return None
    text = path.read_text(encoding="utf-8", errors="replace")
    matches = re.findall(
        r"^SUMMARY loadedEntries=(\d+) queryRows=(\d+) valid=(\d+) failures=(\d+) "
        r"vertexFailures=(\d+) boundaryFailures=(\d+) tauMin=([0-9.eE+-]+) tauMax=([0-9.eE+-]+)$",
        text,
        flags=re.MULTILINE,
    )
    if not matches:
        raise RuntimeError(f"cannot parse production-query SUMMARY from {path}")
    loaded_entries, query_rows, valid, failures, vertex_failures, boundary_failures, tau_min, tau_max = matches[-1]
    result = {
        "verified": True,
        "logPath": str(path),
        "logSha256": sha256(path),
        "queryManifestSha256": sha256(query_manifest_path),
        "loadedEntries": int(loaded_entries),
        "queryRows": int(query_rows),
        "valid": int(valid),
        "failures": int(failures),
        "vertexFailures": int(vertex_failures),
        "boundaryFailures": int(boundary_failures),
        "tauMin": float(tau_min),
        "tauMax": float(tau_max),
    }
    passed = (
        result["queryRows"] == expected_rows
        and result["valid"] == expected_rows
        and result["failures"] == 0
        and result["vertexFailures"] == 0
        and result["boundaryFailures"] == 0
    )
    result["result"] = "PASS" if passed else "FAIL"
    if not passed:
        raise RuntimeError(f"production-query verification failed: {result}")
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--input", type=Path)
    parser.add_argument("--config", type=Path)
    parser.add_argument("--runtime-config", type=Path)
    parser.add_argument("--lut", type=Path)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--query-result-log", type=Path)
    parser.add_argument("--audit-date", default="2026-09-18")
    args = parser.parse_args()

    root = args.root.resolve()
    input_path = (args.input or root / "DataDrivenTestQT/1.txt").resolve()
    config_path = (args.config or root / "DataDrivenTestQT/NetworkConfig.ini").resolve()
    runtime_config_path = (args.runtime_config or root / "HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini").resolve()
    lut_path = (args.lut or root / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv").resolve()
    schema_path = root / "DataDrivenTestQT/ReplayCsvSchema.h"
    output_dir = (args.output_dir or root / "logs/p13/input_audit").resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    query_result_path = (args.query_result_log or output_dir / "query_check/p13_track_query.log").resolve()

    schema = schema_from_header(schema_path)
    raw = input_path.read_bytes()
    text = raw.decode("utf-8-sig")
    physical_lines = text.splitlines()
    parsed_rows: list[dict[str, object]] = []
    blank_lines: list[int] = []
    header_line = 0
    first_nonblank = True
    for line_number, physical_line in enumerate(physical_lines, 1):
        if not physical_line.strip():
            blank_lines.append(line_number)
            continue
        fields = next(csv.reader([physical_line]))
        fields = [field.strip() for field in fields]
        if first_nonblank:
            first_nonblank = False
            if fields != schema:
                raise ValueError(f"line {line_number}: header does not exactly match ReplayCsvSchema.h")
            header_line = line_number
            continue
        if len(fields) != len(schema):
            raise ValueError(f"line {line_number}: expected {len(schema)} columns, got {len(fields)}")
        values = [finite_float(field, line_number, index + 1, schema[index]) for index, field in enumerate(fields)]
        parsed_rows.append({"sourceLine": line_number, "tokens": fields, "values": values})
    if not parsed_rows:
        raise ValueError("no valid data rows")

    columns = list(zip(*(row["values"] for row in parsed_rows)))
    field_stats: list[dict[str, object]] = []
    for index, values_tuple in enumerate(columns):
        values = list(values_tuple)
        unique = sorted(set(values))
        item: dict[str, object] = {
            "index": index,
            "column": index + 1,
            "name": schema[index],
            "min": min(values),
            "max": max(values),
            "uniqueCount": len(unique),
        }
        if len(unique) <= 20:
            item["uniqueValues"] = unique
        field_stats.append(item)

    source_times = [float(row["values"][0]) for row in parsed_rows]
    time_steps = [source_times[index] - source_times[index - 1] for index in range(1, len(source_times))]
    non_increasing = [parsed_rows[index]["sourceLine"] for index in range(1, len(parsed_rows)) if time_steps[index - 1] <= 0.0]
    observers = [(float(row["values"][2]), float(row["values"][3]), float(row["values"][4])) for row in parsed_rows]
    targets = [(float(row["values"][10]), float(row["values"][11]), float(row["values"][12])) for row in parsed_rows]
    geometries = [geodetic_los_m(observer, target) for observer, target in zip(observers, targets)]
    los_ranges = [item[0] for item in geometries]
    horizontal_ranges = [item[1] for item in geometries]
    file_ranges = [float(row["values"][1]) for row in parsed_rows]
    file_minus_computed = [file_value - computed for file_value, computed in zip(file_ranges, los_ranges)]

    config = configparser.ConfigParser(interpolation=None)
    config.optionxform = str
    config.read(config_path, encoding="utf-8")
    runtime_config = configparser.ConfigParser(interpolation=None)
    runtime_config.optionxform = str
    runtime_config.read(runtime_config_path, encoding="utf-8")
    utc_hour = config.getfloat("Demo", "UtcHour", fallback=-1.0)
    controller_date_text = config.get("Demo", "UtcDate", fallback=args.audit_date)
    controller_date_source = "NetworkConfig.ini:Demo/UtcDate" if config.has_option("Demo", "UtcDate") else "run_utc_date_baseline"
    controller_date = parse_date(controller_date_text)
    renderer_date_text = runtime_config.get("M1NirMwirPhysics", "FallbackUtcDate", fallback="2026-09-06")
    renderer_date = parse_date(renderer_date_text)
    if utc_hour < 0.0:
        raise ValueError("audit needs deterministic Demo/UtcHour")
    controller_solar = [solar_position(observer[0], observer[1], controller_date, utc_hour) for observer in observers]
    renderer_solar = [solar_position(observer[0], observer[1], renderer_date, utc_hour) for observer in observers]

    send_step_ms = config.getfloat("RenderControl", "sendStepMs", fallback=1000.0 / 60.0)
    video_fps = config.getfloat("RenderControl", "videoFps", fallback=60.0)
    visibility_m = config.getfloat("WeatherInit", "envVisibility", fallback=6000.0)
    humidity_percent = config.getfloat("WeatherInit", "envHumidity", fallback=76.18)
    requested_band = config.getint("SensorInit", "trackerSensorBand", fallback=2)
    target_type_text = config.get("Demo", "TargetType", fallback="0x11")
    target_type = int(target_type_text, 0)
    nominal_wall_ms = [(index * send_step_ms) for index in range(len(parsed_rows))]
    nominal_video_pts_ms = [(index * 1000.0 / video_fps) for index in range(len(parsed_rows))]
    source_offsets_ms = [value - source_times[0] for value in source_times]

    query_manifest_path = output_dir / "input_1_query_manifest.csv"
    with query_manifest_path.open("w", encoding="utf-8", newline="") as stream:
        fieldnames = [
            "rowIndex", "sourceLine", "sourceTimeMs", "sourceOffsetMs", "nominalSendWallMs",
            "nominalVideoPtsMs", "band", "observerLatDeg", "observerLonDeg", "observerAltKm",
            "targetLatDeg", "targetLonDeg", "targetAltKm", "fileDistanceM", "horizontalDistanceM",
            "productionPreflightLosKm", "visibilityKm", "relativeHumidityPercent", "utcDate", "utcHour",
            "solarZenithDeg", "atmosphereModel", "aerosolModel", "humidityContract",
        ]
        writer = csv.DictWriter(stream, fieldnames=fieldnames)
        writer.writeheader()
        for index, row in enumerate(parsed_rows):
            for band in ("SWIR", "MWIR"):
                writer.writerow({
                    "rowIndex": index,
                    "sourceLine": row["sourceLine"],
                    "sourceTimeMs": source_times[index],
                    "sourceOffsetMs": source_offsets_ms[index],
                    "nominalSendWallMs": nominal_wall_ms[index],
                    "nominalVideoPtsMs": nominal_video_pts_ms[index],
                    "band": band,
                    "observerLatDeg": observers[index][0],
                    "observerLonDeg": observers[index][1],
                    "observerAltKm": observers[index][2] / 1000.0,
                    "targetLatDeg": targets[index][0],
                    "targetLonDeg": targets[index][1],
                    "targetAltKm": targets[index][2] / 1000.0,
                    "fileDistanceM": file_ranges[index],
                    "horizontalDistanceM": horizontal_ranges[index],
                    "productionPreflightLosKm": los_ranges[index] / 1000.0,
                    "visibilityKm": visibility_m / 1000.0,
                    "relativeHumidityPercent": humidity_percent,
                    "utcDate": controller_date.isoformat(),
                    "utcHour": utc_hour,
                    "solarZenithDeg": controller_solar[index][2],
                    "atmosphereModel": "Mid-Latitude Summer",
                    "aerosolModel": "Rural",
                    "humidityContract": "numeric interpolation over measured RH30/RH60/RH85 profiles",
                })

    query_result = read_query_result(query_result_path, len(parsed_rows) * 2, query_manifest_path)

    consumed_ranges = {
        "sourceTimeMs": minmax(source_times),
        "sourceStepMs": {
            **minmax(time_steps),
            "nonIncreasingCount": len(non_increasing),
            "frequency": {str(key): count for key, count in sorted(Counter(time_steps).items())},
        },
        "fileDistanceM": minmax(file_ranges),
        "productionPreflightLosM": minmax(los_ranges),
        "horizontalDistanceM": minmax(horizontal_ranges),
        "fileMinusComputedLosM": minmax(file_minus_computed),
        "observer": {
            "latitudeDeg": minmax([item[0] for item in observers]),
            "longitudeDeg": minmax([item[1] for item in observers]),
            "altitudeM": minmax([item[2] for item in observers]),
            "yawDeg": minmax(list(columns[5])),
            "pitchDeg": minmax(list(columns[6])),
            "rollDeg": minmax(list(columns[7])),
            "speedKmh": minmax(list(columns[8])),
        },
        "target": {
            "latitudeDeg": minmax([item[0] for item in targets]),
            "longitudeDeg": minmax([item[1] for item in targets]),
            "altitudeM": minmax([item[2] for item in targets]),
            "yawDeg": minmax(list(columns[13])),
            "pitchDeg": minmax(list(columns[14])),
            "rollDeg": minmax(list(columns[15])),
            "speedKmh": minmax(list(columns[16])),
        },
        "controllerSolarZenithDeg": minmax([item[2] for item in controller_solar]),
        "rendererSolarZenithDeg": minmax([item[2] for item in renderer_solar]),
    }

    first = parsed_rows[0]
    last = parsed_rows[-1]
    audit = {
        "schema": "HwaSimIR.P13.OriginalInputAudit.v1",
        "status": "PASS_INPUT_AND_ATMOSPHERE_QUERY" if query_result else "PASS_INPUT_INTEGRITY_PENDING_ATMOSPHERE_QUERY",
        "input": {
            "path": str(input_path),
            "sha256": hashlib.sha256(raw).hexdigest(),
            "bytes": len(raw),
            "physicalLines": len(physical_lines),
            "headerLine": header_line,
            "blankLines": blank_lines,
            "validRows": len(parsed_rows),
            "columns": len(schema),
            "header": schema,
            "firstData": {"sourceLine": first["sourceLine"], "sourceTimeMs": first["values"][0]},
            "lastData": {"sourceLine": last["sourceLine"], "sourceTimeMs": last["values"][0]},
            "modified": False,
        },
        "codeIdentity": {
            "schema": {"path": str(schema_path), "sha256": sha256(schema_path)},
            "controlSource": {"path": str(root / "DataDrivenTestQT/mainwindow.cpp"), "sha256": sha256(root / "DataDrivenTestQT/mainwindow.cpp")},
            "config": {"path": str(config_path), "sha256": sha256(config_path)},
            "runtimeConfig": {"path": str(runtime_config_path), "sha256": sha256(runtime_config_path)},
            "formalLut": {"path": str(lut_path), "sha256": sha256(lut_path)},
        },
        "consumedRanges": consumed_ranges,
        "fieldStats": field_stats,
        "productionMapping": {
            "rowFields": {
                "observerPosition": "columns 2..4 RedLat/RedLon/RedAlt",
                "observerAttitude": "columns 5..7 RedYaw/RedPitch/RedRoll",
                "observerSpeed": "column 8 RedSpeedAir(km/h)",
                "targetPosition": "columns 10..12 MissileLat/MissileLon/MissileAlt",
                "targetAttitude": "columns 13..15 MissileYaw/MissilePitch/MissileRoll",
                "targetSpeed": "column 16 MissileSpeedAir(km/h); P13 candidate sends it for non-0x11 target types",
                "fileDistance": "column 1 is retained for audit only; coverage uses reconstructed geodetic LOS",
                "sourceTime": "column 0 retained by P13 candidate; simulation offset is sourceTime-firstSourceTime",
            },
            "fullProtocolIdentity": {
                "outerPlatID": config.getint("Identity", "platID", fallback=1001),
                "sensorID": config.getint("Identity", "sensorID", fallback=2),
                "targetType": target_type,
                "targetTypeSource": "NetworkConfig.ini:Demo/TargetType",
                "targetPlatID": 3,
                "targetID": 3,
                "targetKeySource": "ordinary sender constants plus configured target type",
            },
            "validityAndState": {
                "fileMissileType": {"column": 17, "unique": sorted(set(columns[17]))},
                "fileStrickenFlag": {"column": 28, "unique": sorted(set(columns[28]))},
                "fileDamageFlag": {"column": 29, "unique": sorted(set(columns[29]))},
                "fileViewValid": {"column": 53, "unique": sorted(set(columns[53]))},
                "effectiveEngineState": "ordinary/test control; no engine-state column exists in 1.txt",
                "damageSemantics": "not connected; no damage model added",
                "targetState": "existing ordinary constant 0x01; TrackingState/HitState remain unconnected",
            },
        },
        "environmentAndQuery": {
            "requestedBand": requested_band,
            "requestedBandSource": "NetworkConfig.ini:SensorInit/trackerSensorBand",
            "weatherCode": config.getint("Demo", "envSky", fallback=0),
            "visibilityM": visibility_m,
            "relativeHumidityPercent": humidity_percent,
            "simulationUtcDate": controller_date.isoformat(),
            "simulationUtcDateSource": controller_date_source,
            "simulationUtcHour": utc_hour,
            "simulationUtcHourSource": "NetworkConfig.ini:Demo/UtcHour",
            "rendererFallbackUtcDate": renderer_date.isoformat(),
            "dateIdentityMatches": controller_date == renderer_date,
            "atmosphereModel": "Mid-Latitude Summer",
            "aerosolModel": "Rural",
            "humidityProfileContract": "protocol RH -> RH30/RH60/RH85 measured-profile interpolation",
            "activeIllumination": "ordinary default off; explicit CLI/event controls only",
            "productionQueryVerification": query_result or {
                "verified": False,
                "result": "PENDING",
                "expectedLogPath": str(query_result_path),
            },
        },
        "timeRelationship": {
            "sourceTime": "immutable file Time(ms), 20..43190 with 10 ms increments",
            "simulationTime": "P13 candidate: sourceTimeMs-firstSourceTimeMs; no row skipping",
            "sendWallTime": f"nominal rowIndex*{send_step_ms:.15g} ms at row-by-row {1000.0/send_step_ms:.9g} Hz pacing",
            "videoPTS": f"nominal sourceSeq/{video_fps:.9g} s; actual recorder PTS must be verified by ffprobe",
            "sourceDurationMs": source_times[-1] - source_times[0],
            "nominalSendDurationMs": nominal_wall_ms[-1],
            "nominalVideoDurationMs": nominal_video_pts_ms[-1],
            "rowMapping": "one accepted input row per realtime packet; no interpolation, resampling, or skipping",
            "actualRuntimeVerified": False,
        },
        "declaredCoverageScopes": {
            "originalFileActual": {
                "observerAltitudeKm": {key: value / 1000.0 for key, value in consumed_ranges["observer"]["altitudeM"].items()},
                "targetAltitudeKm": {key: value / 1000.0 for key, value in consumed_ranges["target"]["altitudeM"].items()},
                "losRangeKm": {key: value / 1000.0 for key, value in consumed_ranges["productionPreflightLosM"].items()},
                "sourceDurationS": (source_times[-1] - source_times[0]) / 1000.0,
            },
            "additionalGenericRequirement": {
                "rangeKm": {"min": 0.1, "max": 50.0},
                "separateFromOriginalFile": True,
                "note": "50 km is a separately tested declared boundary and is not claimed to occur in 1.txt",
            },
        },
        "artifacts": {
            "queryManifest": str(query_manifest_path),
            "queryResultLog": str(query_result_path) if query_result else None,
        },
    }

    audit_path = output_dir / "input_1_audit.json"
    audit_path.write_text(json.dumps(audit, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    report = [
        "# P13 原始 DataDrivenTestQT/1.txt 全量输入审计",
        "",
        f"- 原文件：`{input_path}`",
        f"- SHA-256：`{audit['input']['sha256']}`；字节数：{len(raw)}；有效行：{len(parsed_rows)}；列数：{len(schema)}。",
        f"- 源时间：{source_times[0]:g}–{source_times[-1]:g} ms；步长 {min(time_steps):g}–{max(time_steps):g} ms；非递增步数 {len(non_increasing)}。",
        f"- observer 高度：{consumed_ranges['observer']['altitudeM']['min']:.6f}–{consumed_ranges['observer']['altitudeM']['max']:.6f} m；target 高度：{consumed_ranges['target']['altitudeM']['min']:.6f}–{consumed_ranges['target']['altitudeM']['max']:.6f} m。",
        f"- 文件距离列：{min(file_ranges):.6f}–{max(file_ranges):.6f} m；生产预检同语义重算斜距：{min(los_ranges):.6f}–{max(los_ranges):.6f} m。",
        f"- observer 速度：{min(columns[8]):.6f}–{max(columns[8]):.6f} km/h；target 速度：{min(columns[16]):.6f}–{max(columns[16]):.6f} km/h。",
        f"- 姿态范围：observer yaw/pitch/roll={min(columns[5]):.3f}..{max(columns[5]):.3f}/{min(columns[6]):.3f}..{max(columns[6]):.3f}/{min(columns[7]):.3f}..{max(columns[7]):.3f}°；target={min(columns[13]):.3f}..{max(columns[13]):.3f}/{min(columns[14]):.3f}..{max(columns[14]):.3f}/{min(columns[15]):.3f}..{max(columns[15]):.3f}°。",
        "",
        "## 实际消费与来源",
        "",
        "位置、姿态及两端速度来自表中 Red/Missile 对应列；文件 distance 列只作旁证，覆盖查询使用经纬高重算斜距。完整协议键为配置的 targetType 加普通发送端 targetPlatID=3、targetID=3；外层 DDS identity 为 1001/2。1.txt 没有 engineState 列；damage 不接入，TrackingState/HitState 也不新增语义。",
        "",
        "## 时间关系",
        "",
        f"源物理时长 {(source_times[-1]-source_times[0])/1000.0:.3f} s；逐行 {1000.0/send_step_ms:.3f} Hz 发送的名义墙钟/视频时长 {nominal_wall_ms[-1]/1000.0:.3f} s。P13 按 sourceTime-firstSourceTime 驱动 simulationTime，但仍一行一包、不跳行；视频 PTS 由 {video_fps:g} FPS 编码时基产生，最终须用实际 MP4/ffprobe 复核。",
        "",
        "## 大气范围口径",
        "",
        f"原文件实际重算斜距仅 {min(los_ranges)/1000.0:.6f}–{max(los_ranges)/1000.0:.6f} km，不含 50 km。额外约 50 km 是独立通用边界案例；不得把它写成原文件范围。当前 query manifest 共 {len(parsed_rows)*2} 行（逐输入行、逐 SWIR/MWIR）。",
        "",
        f"控制端日期 `{controller_date}` 与渲染端基线 FallbackUtcDate `{renderer_date}` " + ("一致。" if controller_date == renderer_date else "不一致，列为 P13 待修复身份问题。"),
        (
            f"生产查询核验：{query_result['valid']}/{query_result['queryRows']} 有效，失败 {query_result['failures']}，"
            f"tau={query_result['tauMin']:.12g}–{query_result['tauMax']:.12g}。"
            if query_result else
            f"生产查询核验尚未绑定；预期日志 `{query_result_path}`。"
        ),
        "",
        "完整逐列统计、字段来源与查询条件见 `input_1_audit.json` 和 `input_1_query_manifest.csv`。",
    ]
    report_path = output_dir / "input_1_audit.md"
    report_path.write_text("\n".join(report) + "\n", encoding="utf-8")
    print(json.dumps({
        "status": audit["status"],
        "inputSha256": audit["input"]["sha256"],
        "validRows": len(parsed_rows),
        "losRangeKm": audit["declaredCoverageScopes"]["originalFileActual"]["losRangeKm"],
        "outputs": [str(audit_path), str(report_path), str(query_manifest_path)],
    }, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
