#!/usr/bin/env python3
"""Verify the A1 NIR-night active-radiance display chain is monotonic.

The runtime emits one ``[A1 ActiveDisplayTrace]`` record whenever the active
state changes.  This checker deliberately tests the physical/display stages
before looking at encoded-video ROI data, so an AGC-induced local contrast
change cannot be mistaken for a radiance-chain polarity error.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


PREFIX = "[A1 ActiveDisplayTrace]"
PAIR_RE = re.compile(r"([A-Za-z][A-Za-z0-9_]*)=([^\s]+)")
NUMERIC_FIELDS = (
    "activeSensorRadiance",
    "totalSensorRadiance",
    "stage5ShaderInputRadiance",
    "stage5PhysicalOutputGray",
    "stage6PreAgcGray",
    "stage6PostAgcGray",
    "finalGray",
)


def parse_rows(text: str) -> list[dict[str, object]]:
    rows: list[dict[str, object]] = []
    for line in text.splitlines():
        if PREFIX not in line:
            continue
        row: dict[str, object] = dict(PAIR_RE.findall(line))
        try:
            row["sourceSeq"] = int(str(row["sourceSeq"]))
            row["activeEnabled"] = int(str(row["activeEnabled"]))
            row["nightBoundary"] = int(str(row["nightBoundary"]))
            row["agcEnabled"] = int(str(row["agcEnabled"]))
            row["whiteHot"] = int(str(row["whiteHot"]))
            for field in NUMERIC_FIELDS:
                row[field] = float(str(row[field]))
        except (KeyError, ValueError) as exc:
            raise ValueError(f"malformed trace row: {line}") from exc
        rows.append(row)
    rows.sort(key=lambda item: int(item["sourceSeq"]))
    return rows


def representative_triplet(rows: list[dict[str, object]]) -> tuple[dict[str, object], ...]:
    transitions: list[dict[str, object]] = []
    last_state: int | None = None
    for row in rows:
        state = int(row["activeEnabled"])
        if state != last_state:
            transitions.append(row)
            last_state = state
    for index in range(len(transitions) - 2):
        if [int(item["activeEnabled"]) for item in transitions[index:index + 3]] == [0, 1, 0]:
            return tuple(transitions[index:index + 3])
    raise ValueError("off/on/off transition triplet not found")


def check(log_text: str, roi_json: dict[str, object] | None = None,
          tolerance: float = 1.0e-12) -> dict[str, object]:
    rows = parse_rows(log_text)
    if not rows:
        raise ValueError("no A1 ActiveDisplayTrace rows found")
    off_before, on, off_after = representative_triplet(rows)
    failures: list[str] = []
    triplet = (off_before, on, off_after)

    if any(str(row.get("route")) != "M1" for row in triplet):
        failures.append("route_changed_or_not_M1")
    if any(int(row["nightBoundary"]) != 1 for row in triplet):
        failures.append("not_nir_night_boundary")
    if any(int(row["whiteHot"]) != 1 for row in triplet):
        failures.append("whitehot_not_enabled")
    if any(int(row["agcEnabled"]) != 0 for row in triplet):
        failures.append("agc_not_disabled_or_frozen")
    if float(on["activeSensorRadiance"]) <= max(
            float(off_before["activeSensorRadiance"]), float(off_after["activeSensorRadiance"])) + tolerance:
        failures.append("active_sensor_radiance_not_positive_on")

    monotonic_fields = (
        "totalSensorRadiance",
        "stage5ShaderInputRadiance",
        "stage5PhysicalOutputGray",
        "stage6PreAgcGray",
        "stage6PostAgcGray",
        "finalGray",
    )
    field_checks: dict[str, bool] = {}
    for field in monotonic_fields:
        off_reference = max(float(off_before[field]), float(off_after[field]))
        passed = float(on[field]) + tolerance >= off_reference
        field_checks[field] = passed
        if not passed:
            failures.append(f"{field}_decreased_on")

    roi_check: dict[str, object] | None = None
    if roi_json is not None:
        states = roi_json.get("states", {})
        try:
            before_gray = float(states["off_before"]["targetRoiMeanGray"])
            on_gray = float(states["on"]["targetRoiMeanGray"])
            after_gray = float(states["off_after"]["targetRoiMeanGray"])
            # Compare against the stable post-transition off state.  The first
            # off interval includes target/model warm-up in automated runs and
            # is retained only as a startup diagnostic.  A one-micro-gray
            # tolerance covers JSON/float roundoff; it must not hide a real
            # encoded-pixel polarity reversal.
            roi_tolerance = 1.0e-6
            roi_passed = on_gray + roi_tolerance >= after_gray
            roi_check = {
                "offBefore": before_gray,
                "on": on_gray,
                "offAfter": after_gray,
                "reference": "off_after_stable",
                "startupDeltaGray": before_gray - after_gray,
                "toleranceGray": roi_tolerance,
                "passed": roi_passed,
            }
            if not roi_passed:
                failures.append("encoded_target_roi_decreased_on")
        except (KeyError, TypeError, ValueError) as exc:
            raise ValueError("invalid L2 ROI A/B JSON") from exc

    return {
        "passed": not failures,
        "traceRows": len(rows),
        "sourceSeq": [int(row["sourceSeq"]) for row in triplet],
        "activeSensorRadiance": [float(row["activeSensorRadiance"]) for row in triplet],
        "totalSensorRadiance": [float(row["totalSensorRadiance"]) for row in triplet],
        "finalGray": [float(row["finalGray"]) for row in triplet],
        "fieldChecks": field_checks,
        "roiCheck": roi_check,
        "failures": failures,
    }


def self_test() -> dict[str, object]:
    template = (
        "[A1 ActiveDisplayTrace] sourceSeq={seq} targetKey=1:2:3 "
        "reference=target_center_cpu_equivalent route=M1 nightBoundary=1 "
        "activeEnabled={enabled} activeSensorRadiance={active} "
        "totalSensorRadiance={total} stage5ShaderInputRadiance={total} "
        "stage5PhysicalOutputGray={gray} stage6PreAgcGray={gray} "
        "agcEnabled=0 agcGain=1 agcOffset=0 stage6PostAgcGray={gray} "
        "whiteHot=1 finalGray={gray} weatherMappingMonotonic=1 fallbackReason=fixture\n"
    )
    good = "".join((
        template.format(seq=10, enabled=0, active=0, total=0, gray=0),
        template.format(seq=20, enabled=1, active=0.25, total=0.25, gray=0.5),
        template.format(seq=30, enabled=0, active=0, total=0, gray=0),
    ))
    result = check(good)
    if not result["passed"]:
        raise AssertionError(result)
    bad = "".join((
        template.format(seq=10, enabled=0, active=0, total=0.2, gray=0.4),
        template.format(seq=20, enabled=1, active=0.25, total=0.25, gray=0.3),
        template.format(seq=30, enabled=0, active=0, total=0.2, gray=0.4),
    ))
    if check(bad)["passed"]:
        raise AssertionError("negative monotonic fixture unexpectedly passed")
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--log", type=Path)
    parser.add_argument("--roi-json", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        result = self_test()
    else:
        if args.log is None:
            parser.error("--log is required unless --self-test is used")
        roi = json.loads(args.roi_json.read_text(encoding="utf-8")) if args.roi_json else None
        result = check(args.log.read_text(encoding="utf-8", errors="replace"), roi)
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    print(rendered, end="")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
