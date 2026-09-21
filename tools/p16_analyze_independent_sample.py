#!/usr/bin/env python3
"""Pixel evidence for the isolated P16 ordinary-graphics sample."""

import argparse
import csv
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image


EFFECT_CASES = [
    "effects_glow", "effects_smoke", "effects_cloud", "effects_rain", "effects_snow",
    "effects_combo_glow_smoke", "effects_combo_cloud_rain",
]
TEXTURE_CASES = [
    "texture_static_nearest", "texture_motion_nearest", "texture_motion_linear",
    "texture_overlap_before", "texture_overlap_after",
]


def load_frame(root, case_id, index):
    path = root / case_id / "frames" / ("frame_%04d.png" % index)
    return np.asarray(Image.open(str(path)).convert("RGB"), dtype=np.int16)


def mean_abs(a, b, crop):
    x0, y0, x1, y1 = crop
    delta = np.abs(a[y0:y1, x0:x1] - b[y0:y1, x0:x1])
    return float(delta.mean()), float((delta.max(axis=2) >= 3).mean())


def hash_pixels(array):
    return hashlib.sha256(array.astype(np.uint8).tobytes()).hexdigest()


def temporal(root, case_id, crop, frames):
    previous = load_frame(root, case_id, 0)
    changes = []
    hashes = [hash_pixels(previous[crop[1]:crop[3], crop[0]:crop[2]])]
    for index in range(1, frames):
        current = load_frame(root, case_id, index)
        value, fraction = mean_abs(current, previous, crop)
        changes.append({"frame": index, "meanAbsoluteDelta": value, "changedFraction3": fraction})
        hashes.append(hash_pixels(current[crop[1]:crop[3], crop[0]:crop[2]]))
        previous = current
    values = np.array([item["meanAbsoluteDelta"] for item in changes], dtype=float)
    return {
        "meanConsecutiveDelta": float(values.mean()),
        "maxConsecutiveDelta": float(values.max()),
        "uniqueRegionFrameHashes": len(set(hashes)),
        "topTransitions": sorted(changes, key=lambda row: row["meanAbsoluteDelta"], reverse=True)[:5],
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = args.root.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((root / "effects_off" / "case_manifest.json").read_text(encoding="utf-8"))
    frames = int(manifest["frames"])
    width, height = manifest["resolution"]
    # Ignore the case/disclaimer text so it cannot masquerade as an effect.
    effect_crop = (0, 52, width, height - 38)
    board_crop = (128, 114, 512, 382)

    baseline = [load_frame(root, "effects_off", index) for index in range(frames)]
    effects = []
    for case_id in EFFECT_CASES:
        values = []
        fractions = []
        for index in range(frames):
            value, fraction = mean_abs(load_frame(root, case_id, index), baseline[index], effect_crop)
            values.append(value)
            fractions.append(fraction)
        result = {
            "caseId": case_id,
            "baselineCaseId": "effects_off",
            "meanAbsoluteSignedDisplayDifference": float(np.mean(values)),
            "maxFrameMeanAbsoluteDifference": float(np.max(values)),
            "meanChangedPixelFraction3": float(np.mean(fractions)),
        }
        # Sparse effects (notably rain streaks) should be judged by both a
        # non-trivial changed-pixel footprint and a visible peak frame, rather
        # than an all-frame/all-pixel average that rewards broad haze.
        result["effectDiscernible"] = result["meanChangedPixelFraction3"] >= 0.002 and result["maxFrameMeanAbsoluteDifference"] >= 0.10
        effects.append(result)

    textures = []
    for case_id in TEXTURE_CASES:
        value = temporal(root, case_id, board_crop, frames)
        value["caseId"] = case_id
        textures.append(value)
    by_case = {row["caseId"]: row for row in textures}
    before = by_case["texture_overlap_before"]
    after = by_case["texture_overlap_after"]
    static = by_case["texture_static_nearest"]
    root_cause = {
        "scope": "independent sample only",
        "reproducedIssue": "local surface alternates when two coincident layers have an explicitly alternating unresolved order",
        "rootCauseLocated": before["uniqueRegionFrameHashes"] == 2 and before["meanConsecutiveDelta"] > 1.0,
        "fix": "assign deterministic stable layer order while preserving both checkerboard and translucent overlay",
        "problemFixed": after["uniqueRegionFrameHashes"] == 1 and after["maxConsecutiveDelta"] == 0.0,
        "staticBaselineStable": static["uniqueRegionFrameHashes"] == 1 and static["maxConsecutiveDelta"] == 0.0,
        "resolutionReduced": False,
        "boardHidden": False,
        "blurAddedAsFix": False,
        "productionInferenceAllowed": False,
    }

    report = {
        "schema": "P16.IndependentOrdinaryGraphicsPixelEvidence.1",
        "classification": "GENERAL_RGB_GRAPHICS_NOT_IR_NOT_CALIBRATION",
        "effectDiscernibilityThreshold": {
            "meanChangedPixelFractionAtLeast3GrayLevels": 0.002,
            "maxFrameMeanAbsoluteDifference": 0.10,
            "reason": "supports sparse moving effects without rewarding only broad full-frame changes",
        },
        "effectComparisonCrop": list(effect_crop),
        "textureBoardCrop": list(board_crop),
        "effects": effects,
        "textureCases": textures,
        "independentRootCauseAndFix": root_cause,
        "allRequestedSingleEffectsDiscernible": all(row["effectDiscernible"] for row in effects[:5]),
        "productionWeatherStatusChanged": False,
        "productionPlumeStatusChanged": False,
    }
    (output / "pixel_evidence.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    with (output / "pixel_evidence.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["case_id", "category", "metric", "value", "status"])
        for row in effects:
            writer.writerow([row["caseId"], "effect", "mean_changed_fraction_ge3", row["meanChangedPixelFraction3"], "PASS" if row["effectDiscernible"] else "FAIL"])
        for row in textures:
            writer.writerow([row["caseId"], "texture", "mean_consecutive_delta", row["meanConsecutiveDelta"], "MEASURED"])
    print(json.dumps({
        "effectsDiscernible": report["allRequestedSingleEffectsDiscernible"],
        "independentRootCauseLocated": root_cause["rootCauseLocated"],
        "independentFixPassed": root_cause["problemFixed"],
    }, indent=2))
    return 0 if report["allRequestedSingleEffectsDiscernible"] and root_cause["rootCauseLocated"] and root_cause["problemFixed"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
