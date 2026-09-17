#!/usr/bin/env python3
"""Static/selection checker for the P11 RK3588 DDS-only image matrix.

This checker never opens sockets and never starts the renderer.  It keeps the
large remote evidence run bounded by expanding the exact band/case/variant
selection up front and by proving that the representative profile covers each
required P11 image category in both formal bands.
"""

from __future__ import annotations

import argparse
import fnmatch
import json
from pathlib import Path
from typing import Any, Dict, Iterable, List, Sequence


REPRESENTATIVE_CASES: Sequence[str] = (
    "target_near_100m",
    "exhaust_plume_on",
    "cloud",
    "rain",
    "snow",
    "visibility_6km",
    "active_in_band_on",
    "solar_az_180_el_45",
    "target_altitude_high",
    "speed_static_15mps",
)

REQUIRED_FACTORS: Sequence[str] = (
    "target",
    "exhaust_plume",
    "cloud",
    "rain",
    "snow",
    "visibility",
    "active_illumination",
    "solar_azimuth_elevation",
    "target_altitude",
    "target_speed",
)

EXPECTED_BLOCKERS = {
    "cloud_target_front_behind": "BLOCKED_METADATA",
    "cloud_individual_id_disable": "BLOCKED_METADATA",
    "target_range_2km_physical_imagery": "BLOCKED_DATA",
}


def load_json(path: Path) -> Dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(value, dict):
        raise ValueError("matrix root must be an object")
    return value


def expand_cases(scenarios: Sequence[Dict[str, Any]], expression: str) -> List[Dict[str, Any]]:
    selectors = [token.strip() for token in expression.split(",") if token.strip()]
    if not selectors:
        raise ValueError("case selector is empty")
    if len(selectors) == 1 and selectors[0].lower() == "representative":
        selectors = list(REPRESENTATIVE_CASES)
    known = {str(row.get("id")): row for row in scenarios}
    selected: List[Dict[str, Any]] = []
    seen = set()
    for selector in selectors:
        matches = [row for case_id, row in known.items()
                   if fnmatch.fnmatchcase(case_id, selector)]
        if not matches:
            raise ValueError("case selector matched nothing: %s" % selector)
        for row in matches:
            case_id = str(row["id"])
            if case_id not in seen:
                selected.append(row)
                seen.add(case_id)
    return selected


def names(rows: Iterable[Dict[str, Any]]) -> List[str]:
    return [str(row["name"]) for row in rows]


def check(args: argparse.Namespace) -> Dict[str, Any]:
    matrix = load_json(args.matrix)
    errors: List[str] = []
    if matrix.get("schema") != "hwasimir_p11_windows_evidence_matrix_1":
        errors.append("unexpected source matrix schema")

    bands = matrix.get("bands") or []
    expected_bands = {"SWIR": 0, "MWIR": 2}
    actual_bands = {str(row.get("name")): int(row.get("protocolValue", -1)) for row in bands}
    if actual_bands != expected_bands:
        errors.append("formal band/protocol map must be SWIR=0, MWIR=2")
    variants = matrix.get("captureVariants") or []
    if names(variants) != ["fixed", "agc", "annotated"]:
        errors.append("capture variants must be fixed/agc/annotated in that order")

    scenarios = matrix.get("scenarios") or []
    ids = [str(row.get("id", "")) for row in scenarios]
    if not ids or len(ids) != len(set(ids)) or any(not value for value in ids):
        errors.append("scenario IDs must be non-empty and unique")
    try:
        selected_cases = expand_cases(scenarios, args.case)
    except ValueError as exc:
        errors.append(str(exc))
        selected_cases = []

    selected_bands = [row for row in bands if args.band == "ALL" or row["name"] == args.band]
    selected_variants = [row for row in variants
                         if args.variant == "ALL" or row["name"] == args.variant]
    if not selected_bands:
        errors.append("band selection is empty")
    if not selected_variants:
        errors.append("variant selection is empty")

    blockers = matrix.get("blockedCoverage") or []
    actual_blockers = {str(row.get("id")): str(row.get("status")) for row in blockers}
    if actual_blockers != EXPECTED_BLOCKERS:
        errors.append("the three audited coverage blockers changed")

    runnable = [row for row in selected_cases if row.get("runnable") is True]
    blocked = [row for row in selected_cases if row.get("runnable") is not True]
    if not runnable and not blocked:
        errors.append("case selection is empty")

    factor_coverage = sorted({str(row.get("factor")) for row in runnable})
    missing_factors = sorted(set(REQUIRED_FACTORS) - set(factor_coverage))
    representative_requested = args.case.strip().lower() == "representative"
    if representative_requested and missing_factors:
        errors.append("representative profile missing factors: %s" % ",".join(missing_factors))
    if representative_requested and args.variant == "ALL" and "annotated" not in names(selected_variants):
        errors.append("representative profile must include annotated output")

    selected_count = len(selected_bands) * len(selected_cases) * len(selected_variants)
    runnable_count = len(selected_bands) * len(runnable) * len(selected_variants)
    blocked_count = len(selected_bands) * len(blocked) * len(selected_variants)
    result = "PASS" if not errors else "FAIL"
    return {
        "schema": "hwasimir.p11.rk3588.dds-image-matrix-check.v1",
        "result": result,
        "mutation": "none",
        "process_launch": "none",
        "network_access": "none",
        "transport_contract": {
            "control": "DDS",
            "video": "DDS H.264 Annex-B",
            "forbidden_payload_transports": ["UDP", "TCP"],
        },
        "matrix": str(args.matrix.resolve()),
        "selection": {
            "band": args.band,
            "case": args.case,
            "variant": args.variant,
            "bands": names(selected_bands),
            "case_ids": [str(row["id"]) for row in selected_cases],
            "variants": names(selected_variants),
            "selected_runs": selected_count,
            "runnable_runs": runnable_count,
            "blocked_runs": blocked_count,
        },
        "representative_profile": {
            "requested": representative_requested,
            "case_ids": list(REPRESENTATIVE_CASES),
            "required_factors": list(REQUIRED_FACTORS),
            "observed_factors": factor_coverage,
            "missing_factors": missing_factors,
            "annotation_is_capture_variant": True,
        },
        "blocked_coverage": blockers,
        "errors": errors,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--matrix", type=Path, required=True)
    parser.add_argument("--band", choices=["ALL", "SWIR", "MWIR"], default="ALL")
    parser.add_argument("--case", default="Representative")
    parser.add_argument("--variant", choices=["ALL", "fixed", "agc", "annotated"],
                        default="ALL")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = check(args)
    text = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
    print("[P11 RK3588 DDS Image Check] result=%s selected=%d runnable=%d blocked=%d" %
          (result["result"], result["selection"]["selected_runs"],
           result["selection"]["runnable_runs"], result["selection"]["blocked_runs"]))
    for error in result["errors"]:
        print("[P11 RK3588 DDS Image Check][ERROR] %s" % error)
    return 0 if result["result"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
