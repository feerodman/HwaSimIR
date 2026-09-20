#!/usr/bin/env python3
"""Pixel and log audit for the P13 normal-material controlled sample.

The two slabs use deliberately artificial TEST_A/TEST_B coefficients.  This
tool proves that formal M1 with P5MaterialView=0 reaches ordinary pixels; it
does not make an equipment-specific radiometric or calibration claim.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageStat


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def exactly_one(root: Path, name: str) -> Path:
    matches = list(root.rglob(name))
    if len(matches) != 1:
        raise RuntimeError(f"expected exactly one {name} below {root}, found {len(matches)}")
    return matches[0]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path, default=Path("logs/p13/material/material_normal_view_qc.json"))
    args = parser.parse_args()
    root = args.root.resolve()
    output = args.output if args.output.is_absolute() else root / args.output
    output.parent.mkdir(parents=True, exist_ok=True)

    cases = {}
    all_checks: list[bool] = []
    # Fixed interior rectangles avoid the green annotation and slab edges.
    rois = {"TEST_A": (285, 360, 385, 500), "TEST_B": (415, 360, 515, 500)}
    for band in ("SWIR", "MWIR"):
        case = root / f"logs/p13/runs/material_sample_{band}_normal_view"
        plan = json.loads((case / "case_plan.json").read_text(encoding="utf-8"))
        media = json.loads((case / "media_qc.json").read_text(encoding="utf-8"))
        log = (case / "board.log").read_text(encoding="utf-8", errors="replace")
        image_path = case / "keyframes/middle.png"
        image = Image.open(image_path).convert("L")
        values = {}
        for material, box in rois.items():
            crop = image.crop(box)
            stats = ImageStat.Stat(crop)
            values[material] = {
                "roi": list(box),
                "pixels": crop.width * crop.height,
                "meanGray8": stats.mean[0],
                "rmsGray8": stats.rms[0],
                "minGray8": crop.getextrema()[0],
                "maxGray8": crop.getextrema()[1],
            }
        checks = {
            "formalM1Actual": "[M1 PhysicsConfig] CompareOnly=0 EnableRuntime=1" in log,
            "normalMaterialView0": "[P5GraphicsTest] scene=materials view=end materialView=0" in log,
            "artificialValuesExplicit": "values=artificial_game_only" in log,
            "twoMaterialIdsBound": "entries=2 gpuSlots=2" in log,
            "mediaQcPass": media.get("result") == "PASS",
            "frameIs800x800": image.size == (800, 800),
            "bothRoisNonBlack": all(item["meanGray8"] > 0.0 for item in values.values()),
            "materialPixelContributionDiffers": abs(values["TEST_A"]["meanGray8"] - values["TEST_B"]["meanGray8"]) >= 2.0,
        }
        all_checks.extend(checks.values())
        mp4 = exactly_one(case / "recording", "output.mp4")
        cases[band] = {
            "case": str(case),
            "elfSha256": next(line.split("=", 1)[1] for line in plan["boardPreflight"] if line.startswith("ElfSha256=")),
            "configManifestSha256": next(line.split("=", 1)[1] for line in plan["boardPreflight"] if line.startswith("ConfigManifestSha256=")),
            "image": str(image_path),
            "imageSha256": sha256(image_path),
            "mp4": str(mp4),
            "mp4Sha256": sha256(mp4),
            "roiMeasurements": values,
            "deltaMeanGray8BMinusA": values["TEST_B"]["meanGray8"] - values["TEST_A"]["meanGray8"],
            "checks": checks,
        }

    report = {
        "schema": "hwasimir.p13.normal-material-pixel-qc.v1",
        "result": "PASS" if all(all_checks) else "FAIL",
        "interpretation": "generic artificial TEST_A/TEST_B sample proves normal material binding and pixel contribution under formal M1; not equipment truth",
        "materialView": 0,
        "calibrationStatus": "NOT_VERIFIED_CALIBRATION",
        "cases": cases,
    }
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({
        "result": report["result"],
        "SWIR": {"A": cases["SWIR"]["roiMeasurements"]["TEST_A"]["meanGray8"],
                 "B": cases["SWIR"]["roiMeasurements"]["TEST_B"]["meanGray8"]},
        "MWIR": {"A": cases["MWIR"]["roiMeasurements"]["TEST_A"]["meanGray8"],
                 "B": cases["MWIR"]["roiMeasurements"]["TEST_B"]["meanGray8"]},
        "output": str(output),
    }))
    return 0 if report["result"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
