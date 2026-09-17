#!/usr/bin/env python3
"""Read-only post-run gate for the P11 Windows aerodynamic-heating matrix.

The gate deliberately does not launch HwaSim_IR and never rewrites evidence.
It proves that the speed cases used the production aero route in both bands,
that whole-body heating stayed disabled, and that raw target pixels changed
between otherwise identical static-speed fixtures.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import re
import sys
from pathlib import Path
from typing import Any

import numpy as np


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_MATRIX = ROOT / "tools/p11_windows_evidence_matrix.json"
BANDS = ("SWIR", "MWIR")
VARIANTS = ("fixed", "agc", "annotated")
STATIC_CASES = (
    "target_speed_static",
    "speed_static_15mps",
    "target_speed_25mps",
    "speed_static_30mps",
)
DYNAMIC_CASE = "speed_dynamic_0_15_30mps"
SPEED_CASES = STATIC_CASES + (DYNAMIC_CASE,)
PROGRAM_ROLES = {"HwaSim_IR", "HwaSim_IR_VideoDisplay", "DataDrivenTestQT"}
ZERO_TOL = 1.0e-9
SPEED_TOL_MPS = 0.05
RAW_DELTA_FLOOR = 1.0e-7


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def parse_fields(line: str) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in re.findall(r"\b([A-Za-z][A-Za-z0-9_]*)=([^\s]+)", line):
        if key in result:
            continue
        try:
            parsed = float(value)
            result[key] = int(parsed) if parsed.is_integer() else parsed
        except ValueError:
            result[key] = value
    return result


def tagged_entries(tag: str, text: str) -> list[dict[str, object]]:
    prefix = f"[{tag}]"
    return [parse_fields(line) for line in text.splitlines() if line.startswith(prefix)]


def number(value: object) -> float | None:
    if isinstance(value, bool):
        return float(value)
    if isinstance(value, (int, float)) and math.isfinite(float(value)):
        return float(value)
    if isinstance(value, str):
        try:
            parsed = float(value)
            return parsed if math.isfinite(parsed) else None
        except ValueError:
            return None
    return None


def boolean(value: object) -> bool | None:
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)) and value in (0, 1):
        return bool(value)
    if isinstance(value, str):
        normalized = value.strip().lower()
        if normalized in {"1", "true", "yes", "on"}:
            return True
        if normalized in {"0", "false", "no", "off"}:
            return False
    return None


def read_pfm(path: Path) -> tuple[np.ndarray, float]:
    with path.open("rb") as stream:
        magic = stream.readline().strip()
        if magic not in (b"PF", b"Pf"):
            raise ValueError(f"unsupported_pfm_magic:{magic!r}")
        dimensions = stream.readline().split()
        if len(dimensions) != 2:
            raise ValueError("invalid_pfm_dimensions")
        width, height = map(int, dimensions)
        scale = float(stream.readline())
        channels = 3 if magic == b"PF" else 1
        dtype = "<f4" if scale < 0.0 else ">f4"
        values = np.fromfile(stream, dtype=dtype)
    expected = width * height * channels
    if values.size != expected:
        raise ValueError(f"pfm_size_mismatch:{values.size}!={expected}")
    # P6LinearCapture writes display-order rows; annotation y coordinates map
    # directly to these rows (the same convention as p11_surface_pixel_check).
    return values.reshape((height, width, channels)), scale


def canonical_static_fixture(document: dict[str, object]) -> tuple[str, dict[str, object]]:
    normalized = copy.deepcopy(document)
    normalized.pop("Description", None)
    targets = normalized.get("SyntheticTelemetryTargets")
    if isinstance(targets, list):
        for target in targets:
            if isinstance(target, list) and len(target) > 9:
                target[9] = "<normalized-speed-kmh>"
    canonical = json.dumps(normalized, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest(), normalized


def target_bbox(case_dir: Path, source_seq: int, expected_key: tuple[int, int, int]) -> tuple[int, int, int, int]:
    recording = case_dir / "variants/fixed/receiver_recording"
    index_path = recording / "frame_index.jsonl"
    row: dict[str, object] | None = None
    for line in index_path.read_text(encoding="utf-8-sig").splitlines():
        candidate = json.loads(line)
        if int(candidate.get("sourceSeq", -1)) == source_seq:
            row = candidate
            break
    if row is None:
        raise ValueError(f"frame_index_source_seq_missing:{source_seq}")
    offset = int(row["annotationBodyOffset"])
    size = int(row["annotationBodyBytes"])
    annotation_path = recording / "producer_annotations.jsonl"
    with annotation_path.open("rb") as stream:
        stream.seek(offset)
        payload = stream.read(size)
    if len(payload) != size:
        raise ValueError(f"producer_annotation_short_read:{len(payload)}!={size}")
    expected_hash = str(row.get("annotationBodySha256", "")).lower()
    if hashlib.sha256(payload).hexdigest() != expected_hash:
        raise ValueError("producer_annotation_hash_mismatch")
    annotation = json.loads(payload.decode("utf-8"))
    if int(annotation.get("sourceSeq", -1)) != source_seq:
        raise ValueError("producer_annotation_source_seq_mismatch")
    selected = None
    for target in annotation.get("targets", []):
        key = (int(target.get("targetType", -1)), int(target.get("targetPlatID", -1)), int(target.get("targetID", -1)))
        if key == expected_key:
            selected = target
            break
    if selected is None:
        raise ValueError(f"producer_annotation_target_key_missing:{expected_key}")
    corners = selected.get("bboxCorners", [])
    if len(corners) < 2:
        raise ValueError("producer_annotation_bbox_missing")
    xs = [int(point["x"]) for point in corners]
    ys = [int(point["y"]) for point in corners]
    return min(xs), min(ys), max(xs), max(ys)


def program_identity_key(request: dict[str, object], errors: list[str], prefix: str) -> tuple[tuple[str, str, int], ...] | None:
    rows = request.get("formalProgramIdentities")
    if not isinstance(rows, list):
        errors.append(f"{prefix}:formal_program_identities_missing")
        return None
    parsed: list[tuple[str, str, int]] = []
    roles: set[str] = set()
    for index, row in enumerate(rows):
        if not isinstance(row, dict):
            errors.append(f"{prefix}:formal_program_identity_not_object:{index}")
            continue
        role = str(row.get("role", ""))
        digest = str(row.get("sha256", "")).lower()
        try:
            size = int(row.get("bytes", 0))
        except (TypeError, ValueError):
            size = 0
        path = str(row.get("path", ""))
        if role not in PROGRAM_ROLES:
            errors.append(f"{prefix}:formal_program_identity_role:{role or index}")
        if not re.fullmatch(r"[0-9a-f]{64}", digest):
            errors.append(f"{prefix}:formal_program_identity_sha256:{role or index}")
        if size <= 0:
            errors.append(f"{prefix}:formal_program_identity_bytes:{role or index}")
        if not path:
            errors.append(f"{prefix}:formal_program_identity_path:{role or index}")
        roles.add(role)
        parsed.append((role, digest, size))
    if roles != PROGRAM_ROLES or len(parsed) != len(PROGRAM_ROLES):
        errors.append(f"{prefix}:formal_program_identity_role_set")
    return tuple(sorted(parsed)) if len(parsed) == len(PROGRAM_ROLES) else None


def validate_variant(
    variant_dir: Path,
    prefix: str,
    expected_speed_mps: float,
    dynamic: bool,
    errors: list[str],
) -> dict[str, object]:
    summary_path = variant_dir / "formal_tcp_run/phase2a_sync60_save_summary.json"
    log_path = variant_dir / "formal_tcp_run/hwa.out.log"
    try:
        summary = read_json(summary_path)
        text = log_path.read_text(encoding="utf-8", errors="replace")
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        errors.append(f"{prefix}:variant_evidence_unreadable:{exc}")
        return {}

    for key in ("enableAeroThermalModel", "applyAeroToRadiance"):
        if boolean(summary.get(key)) is not True:
            errors.append(f"{prefix}:summary_{key}_not_true:{summary.get(key)!r}")
    if summary.get("aeroApplyOnlyBand") != "SWIR_MWIR":
        errors.append(f"{prefix}:summary_aeroApplyOnlyBand:{summary.get('aeroApplyOnlyBand')!r}")

    configs = tagged_entries("Stage5 AeroThermalConfig", text)
    if not configs:
        errors.append(f"{prefix}:aero_config_missing")
    for row in configs:
        if boolean(row.get("EnableAeroThermalModel")) is not True:
            errors.append(f"{prefix}:config_EnableAeroThermalModel_not_true")
        if boolean(row.get("ApplyAeroToRadiance")) is not True:
            errors.append(f"{prefix}:config_ApplyAeroToRadiance_not_true")
        if row.get("AeroApplyOnlyBand") != "SWIR_MWIR":
            errors.append(f"{prefix}:config_AeroApplyOnlyBand:{row.get('AeroApplyOnlyBand')!r}")
        if row.get("distribution") != "gpu_local_bounds_normalized":
            errors.append(f"{prefix}:config_distribution:{row.get('distribution')!r}")
        if row.get("wholeBodyHeating") != "forbidden":
            errors.append(f"{prefix}:config_wholeBodyHeating:{row.get('wholeBodyHeating')!r}")

    rows = tagged_entries("Stage5 AeroThermal", text)
    if not rows:
        errors.append(f"{prefix}:aero_runtime_missing")
        return {"runtimeRows": 0}
    speeds: list[float] = []
    local_positive_rows = 0
    malformed_speed_rows = 0
    local_keys = ("noseAeroDeltaKEffective", "edgeAeroDeltaKEffective", "rearAeroDeltaKEffective")
    for index, row in enumerate(rows):
        row_prefix = f"{prefix}:runtime:{index}"
        speed = number(row.get("speedMps"))
        if speed is None:
            # Console writers can interleave one foreign tag into a partially
            # written line.  Do not let one such row erase the intact speed
            # samples around it; the scenario gates below still require the
            # expected low/mid/high or exact static speed observations.
            malformed_speed_rows += 1
        else:
            speeds.append(speed)
        if boolean(row.get("valid")) is not True:
            errors.append(f"{row_prefix}:valid_not_true")
        if row.get("fallbackReason") != "none":
            errors.append(f"{row_prefix}:fallbackReason:{row.get('fallbackReason')!r}")
        if row.get("aeroDistribution") != "gpu_local_bounds_normalized":
            errors.append(f"{row_prefix}:distribution:{row.get('aeroDistribution')!r}")
        if boolean(row.get("aeroAppliedToRadiance")) is not True:
            errors.append(f"{row_prefix}:aeroAppliedToRadiance_not_true")
        for key in ("wholeBodyAeroDeltaK", "bodyAeroDeltaKEffective"):
            value = number(row.get(key))
            if value is None or abs(value) > ZERO_TOL:
                errors.append(f"{row_prefix}:{key}_not_zero:{row.get(key)!r}")
        local_values = [number(row.get(key)) for key in local_keys]
        if all(value is not None and value > 0.0 for value in local_values):
            local_positive_rows += 1

    if dynamic:
        if not speeds or min(speeds) > 0.1:
            errors.append(f"{prefix}:dynamic_zero_speed_not_observed")
        if not speeds or max(speeds) < 25.0:
            errors.append(f"{prefix}:dynamic_high_speed_not_observed")
        if not any(10.0 <= value <= 20.0 for value in speeds):
            errors.append(f"{prefix}:dynamic_mid_speed_not_observed")
        high_rows = [
            row for row in rows
            if (number(row.get("speedMps")) or 0.0) >= 10.0
        ]
        if not high_rows or not any(
            all((number(row.get(key)) or 0.0) > 0.0 for key in local_keys)
            for row in high_rows
        ):
            errors.append(f"{prefix}:dynamic_local_effective_not_positive")
    else:
        matching = [row for row in rows if number(row.get("speedMps")) is not None and abs(float(number(row["speedMps"])) - expected_speed_mps) <= SPEED_TOL_MPS]
        if not matching:
            errors.append(f"{prefix}:static_expected_speed_not_observed:{expected_speed_mps:g}")
        elif expected_speed_mps == 0.0:
            if any(abs(number(row.get(key)) or 0.0) > ZERO_TOL for row in matching for key in local_keys):
                errors.append(f"{prefix}:zero_speed_local_effective_not_zero")
        elif not any(all((number(row.get(key)) or 0.0) > 0.0 for key in local_keys) for row in matching):
            errors.append(f"{prefix}:nonzero_speed_local_effective_not_positive")

    return {
        "runtimeRows": len(rows),
        "speedMpsMin": min(speeds) if speeds else None,
        "speedMpsMax": max(speeds) if speeds else None,
        "localPositiveRows": local_positive_rows,
        "malformedSpeedRowsIgnored": malformed_speed_rows,
    }


def check(matrix_root: Path, matrix_path: Path) -> dict[str, object]:
    errors: list[str] = []
    details: dict[str, object] = {"cases": {}, "staticComparisons": {}}
    try:
        matrix = read_json(matrix_path)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        return {"schema": "hwasimir_p11_windows_aero_matrix_check_1", "result": "FAIL", "errors": [f"matrix_unreadable:{exc}"]}
    scenarios = {str(row.get("id")): row for row in matrix.get("scenarios", []) if isinstance(row, dict)}
    for case_id in SPEED_CASES:
        if case_id not in scenarios:
            errors.append(f"matrix_speed_case_missing:{case_id}")
    if errors:
        return {"schema": "hwasimir_p11_windows_aero_matrix_check_1", "result": "FAIL", "matrixRoot": str(matrix_root), "errors": errors, "details": details}

    identity_keys: dict[str, tuple[tuple[str, str, int], ...]] = {}
    static_fixtures: dict[str, str] = {}
    static_records: dict[str, dict[str, dict[str, object]]] = {band: {} for band in BANDS}

    for band in BANDS:
        for case_id in SPEED_CASES:
            prefix = f"{band}/{case_id}"
            case_dir = matrix_root / band / case_id
            case_details: dict[str, object] = {"variants": {}}
            details["cases"][prefix] = case_details
            try:
                request = read_json(case_dir / "case_request.json")
                case_summary = read_json(case_dir / "case.json")
                fixture = read_json(case_dir / "input_fixture.json")
            except (OSError, ValueError, json.JSONDecodeError) as exc:
                errors.append(f"{prefix}:case_evidence_unreadable:{exc}")
                continue
            scenario = scenarios[case_id]
            expected_speed = float(scenario.get("targetSpeedMps", 0.0))
            dynamic = case_id == DYNAMIC_CASE
            if request.get("caseId") != case_id or str(request.get("band", "")).upper() != band:
                errors.append(f"{prefix}:request_identity_mismatch")
            if case_summary.get("result") != "PASS":
                errors.append(f"{prefix}:case_result_not_pass:{case_summary.get('result')!r}")

            identity = program_identity_key(request, errors, prefix)
            if identity is not None:
                identity_keys[prefix] = identity
                case_details["formalProgramIdentities"] = [list(row) for row in identity]

            targets = fixture.get("SyntheticTelemetryTargets")
            try:
                target = targets[0]
                fixture_speed_kmh = float(target[9])
                target_key = (int(target[0]), int(target[1]), int(target[2]))
            except (TypeError, IndexError, ValueError):
                errors.append(f"{prefix}:fixture_target_invalid")
                continue
            if not dynamic and abs(fixture_speed_kmh / 3.6 - expected_speed) > SPEED_TOL_MPS:
                errors.append(f"{prefix}:fixture_speed_mismatch:{fixture_speed_kmh / 3.6:g}!={expected_speed:g}")
            if not dynamic:
                fixture_hash, _ = canonical_static_fixture(fixture)
                static_fixtures[prefix] = fixture_hash
                case_details["normalizedFixtureSha256"] = fixture_hash

            variant_details: dict[str, object] = case_details["variants"]  # type: ignore[assignment]
            for variant in VARIANTS:
                variant_details[variant] = validate_variant(
                    case_dir / "variants" / variant,
                    f"{prefix}/{variant}",
                    expected_speed,
                    dynamic,
                    errors,
                )

            try:
                selected = int(case_summary["selectedFrame"]["requestedSourceSeq"])
                bbox = target_bbox(case_dir, selected, target_key)
                raw_path = case_dir / "raw_radiance.pfm"
                raw, scale = read_pfm(raw_path)
                if not np.isfinite(raw).all():
                    errors.append(f"{prefix}:raw_nonfinite")
                declared_hash = str(case_summary.get("rawRadiance", {}).get("sha256", "")).lower()
                actual_hash = sha256(raw_path)
                if declared_hash != actual_hash:
                    errors.append(f"{prefix}:raw_sha256_mismatch")
                left, top, right, bottom = bbox
                if left < 0 or top < 0 or right >= raw.shape[1] or bottom >= raw.shape[0] or left > right or top > bottom:
                    errors.append(f"{prefix}:bbox_out_of_bounds:{bbox}")
                case_details.update({"bbox": list(bbox), "rawSha256": actual_hash, "pfmScale": scale})
                if not dynamic:
                    static_records[band][case_id] = {"raw": raw, "bbox": bbox, "sha256": actual_hash}
            except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as exc:
                errors.append(f"{prefix}:raw_or_annotation_unreadable:{exc}")

    if identity_keys:
        unique_identities = set(identity_keys.values())
        details["formalProgramIdentitySetCount"] = len(unique_identities)
        if len(identity_keys) != len(BANDS) * len(SPEED_CASES):
            errors.append(f"formal_program_identity_coverage:{len(identity_keys)}!={len(BANDS) * len(SPEED_CASES)}")
        if len(unique_identities) != 1:
            errors.append(f"formal_program_identity_mismatch:{len(unique_identities)}_sets")

    unique_fixture_hashes = set(static_fixtures.values())
    details["normalizedStaticFixtureSetCount"] = len(unique_fixture_hashes)
    if len(static_fixtures) != len(BANDS) * len(STATIC_CASES):
        errors.append(f"static_fixture_coverage:{len(static_fixtures)}!={len(BANDS) * len(STATIC_CASES)}")
    if len(unique_fixture_hashes) != 1:
        errors.append(f"static_fixture_not_identical_after_speed_normalization:{len(unique_fixture_hashes)}_sets")

    for band in BANDS:
        records = static_records[band]
        comparison: dict[str, object] = {}
        details["staticComparisons"][band] = comparison
        if set(records) != set(STATIC_CASES):
            errors.append(f"{band}:static_raw_coverage:{sorted(records)}")
            continue
        bboxes = {tuple(record["bbox"]) for record in records.values()}
        if len(bboxes) != 1:
            errors.append(f"{band}:static_bbox_mismatch:{sorted(bboxes)}")
            continue
        bbox = tuple(next(iter(bboxes)))
        baseline = np.asarray(records[STATIC_CASES[0]]["raw"])
        left, top, right, bottom = bbox
        base_roi = baseline[top:bottom + 1, left:right + 1, :]
        comparison["bbox"] = list(bbox)
        comparison["baselineCase"] = STATIC_CASES[0]
        comparison["nonzeroCases"] = {}
        for case_id in STATIC_CASES[1:]:
            candidate = np.asarray(records[case_id]["raw"])
            if candidate.shape != baseline.shape:
                errors.append(f"{band}/{case_id}:raw_shape_mismatch:{candidate.shape}!={baseline.shape}")
                continue
            delta = candidate[top:bottom + 1, left:right + 1, :] - base_roi
            max_positive = float(np.max(delta))
            max_absolute = float(np.max(np.abs(delta)))
            changed = int(np.count_nonzero(np.abs(delta) > RAW_DELTA_FLOOR))
            comparison["nonzeroCases"][case_id] = {
                "rawSha256": records[case_id]["sha256"],
                "maxPositiveTargetRoiDeltaWm2SrUm": max_positive,
                "maxAbsTargetRoiDeltaWm2SrUm": max_absolute,
                "changedTargetRoiValuesAboveFloor": changed,
            }
            if records[case_id]["sha256"] == records[STATIC_CASES[0]]["sha256"]:
                errors.append(f"{band}/{case_id}:raw_sha256_unchanged_from_zero_speed")
            if changed == 0 or max_positive <= RAW_DELTA_FLOOR:
                errors.append(f"{band}/{case_id}:target_roi_raw_not_increased_from_zero_speed")

    return {
        "schema": "hwasimir_p11_windows_aero_matrix_check_1",
        "result": "PASS" if not errors else "FAIL",
        "matrixRoot": str(matrix_root),
        "matrix": str(matrix_path),
        "contract": {
            "bands": list(BANDS),
            "cases": list(SPEED_CASES),
            "variants": list(VARIANTS),
            "requiredAeroBand": "SWIR_MWIR",
            "wholeBodyAeroDeltaK": 0,
            "localEffectivePositiveForNonzeroSpeed": True,
            "rawTargetRoiDeltaFloorWm2SrUm": RAW_DELTA_FLOOR,
            "cameraMotionConfoundControl": "static fixtures identical after speed-only normalization and target bbox identical within band",
            "binaryBinding": "three formal program role/size/SHA-256 identities present and identical across all ten band/case runs",
        },
        "errors": errors,
        "details": details,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("matrix_root", type=Path, help="Completed P11 Windows matrix root")
    parser.add_argument("--matrix", type=Path, default=DEFAULT_MATRIX)
    parser.add_argument("--compact", action="store_true", help="Emit compact JSON")
    parser.add_argument(
        "--output",
        type=Path,
        help="Write the exact JSON result to this file as well as stdout",
    )
    args = parser.parse_args()
    result = check(args.matrix_root.resolve(), args.matrix.resolve())
    rendered = json.dumps(
        result,
        ensure_ascii=False,
        indent=None if args.compact else 2,
    ) + "\n"
    if args.output is not None:
        output = args.output.resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(rendered, encoding="utf-8")
    sys.stdout.write(rendered)
    return 0 if result["result"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
