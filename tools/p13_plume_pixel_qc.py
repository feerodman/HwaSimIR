#!/usr/bin/env python3
"""Audit the P13 plume failure, controlled heat-source pixels and ordinary binding.

The controlled nozzle is deliberately generic and artificial.  It proves that
the normal material/composite path receives an on/off heat contribution; it is
not a calibrated signature for AIM120D, F35, or any other equipment.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

import numpy as np
from PIL import Image, ImageChops


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def identity(plan: dict, prefix: str) -> str:
    return next(line.split("=", 1)[1] for line in plan["boardPreflight"]
                if line.startswith(prefix + "="))


def roi_metrics(diff: np.ndarray, box: tuple[int, int, int, int]) -> dict:
    x0, y0, x1, y1 = box
    values = diff[y0:y1, x0:x1]
    return {
        "roi": list(box),
        "pixels": int(values.size),
        "meanAbsGray8": float(values.mean()),
        "maxAbsGray8": int(values.max()),
        "nonzeroPixels": int(np.count_nonzero(values)),
        "pixelsGe2": int(np.count_nonzero(values >= 2)),
        "sumAbsGray8": int(values.sum()),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path, default=Path("logs/p13/plume/plume_qc.json"))
    args = parser.parse_args()
    root = args.root.resolve()
    output = args.output if args.output.is_absolute() else root / args.output
    output.parent.mkdir(parents=True, exist_ok=True)

    baseline_path = root / "logs/p12/p12a/ordinary-mixed-syncgate-20260917-204034/board_hwa.log"
    on_dir = root / "logs/p13/runs/plume_generic_MWIR_on_final2"
    off_dir = root / "logs/p13/runs/plume_generic_MWIR_off_final"
    ordinary_dir = root / "logs/p13/runs/final6b_original_1_MWIR_Clear"
    source_path = root / "HwaSim_IR/HwaSim_IR/HwaSimIR.cpp"

    baseline = baseline_path.read_text(encoding="utf-8", errors="replace")
    on_log = (on_dir / "board.log").read_text(encoding="utf-8", errors="replace")
    off_log = (off_dir / "board.log").read_text(encoding="utf-8", errors="replace")
    ordinary_log = (ordinary_dir / "board.log").read_text(encoding="utf-8", errors="replace")
    source = source_path.read_text(encoding="utf-8", errors="replace")
    on_plan = json.loads((on_dir / "case_plan.json").read_text(encoding="utf-8"))
    off_plan = json.loads((off_dir / "case_plan.json").read_text(encoding="utf-8"))
    on_media = json.loads((on_dir / "media_qc.json").read_text(encoding="utf-8"))
    off_media = json.loads((off_dir / "media_qc.json").read_text(encoding="utf-8"))

    baseline_matches = list(re.finditer(
        r"^.*\[Stage5 Plume\].*formalTauReady=0.*formalTau=0\.0+.*"
        r"coreVisible=0.*haloVisible=0.*$", baseline, re.MULTILINE))
    ordinary_matches = list(re.finditer(
        r"^.*\[Stage5 Plume\].*formalTauReady=1.*formalTau=(0\.[0-9]+).*"
        r"coreVisible=1.*haloVisible=1.*$", ordinary_log, re.MULTILINE))

    on_image_path = on_dir / "keyframes/middle.png"
    off_image_path = off_dir / "keyframes/middle.png"
    on_image = Image.open(on_image_path).convert("L")
    off_image = Image.open(off_image_path).convert("L")
    diff_image = ImageChops.difference(on_image, off_image)
    diff = np.asarray(diff_image, dtype=np.uint8)
    boxes = {
        "controlledHeat": (320, 340, 480, 480),
        "leftControl": (0, 250, 160, 550),
        "rightControl": (640, 250, 800, 550),
        "topControl": (200, 0, 600, 160),
        "bottomControl": (200, 640, 600, 800),
    }
    measurements = {name: roi_metrics(diff, box) for name, box in boxes.items()}
    control_mean = max(measurements["leftControl"]["meanAbsGray8"],
                       measurements["rightControl"]["meanAbsGray8"], 1e-9)

    display_on = [line for line in on_log.splitlines() if "[DisplayEffective]" in line]
    display_off = [line for line in off_log.splitlines() if "[DisplayEffective]" in line]
    same_identity = all(identity(on_plan, key) == identity(off_plan, key) for key in (
        "ElfSha256", "RuntimeConfigSha256", "ConfigManifestSha256",
        "FormalLutSha256", "CoverageManifestSha256"))

    checks = {
        "p12FailureReproducedExactly": len(baseline_matches) >= 3,
        "baselineHashExact": sha256(baseline_path) == "83c8859b20bde20e8c715f0ab48b30ecf87a1288e0853370b0b9dc2807919a04",
        "controlledCasesSameElfAndConfig": same_identity,
        "controlledCasesSameImmutableOriginal": on_plan["inputSha256"] == off_plan["inputSha256"] ==
            "f2c3db00d71012cd28336b5077e03ff8d4ed1eda2698fd900486ae9cdfdd7901",
        "onlyControlledHeatSwitchDiffers":
            on_plan["controlledFixture"] == {"scene": "nozzle", "view": "end", "materialCase": "A",
                                             "syntheticHeatSource": "On", "values": "artificial_not_measurement"} and
            off_plan["controlledFixture"] == {"scene": "nozzle", "view": "end", "materialCase": "A",
                                              "syntheticHeatSource": "Off", "values": "artificial_not_measurement"},
        "normalMaterialFormalM1": "materialView=0 syntheticHeatSourceEnabled=1" in on_log and
            "materialView=0 syntheticHeatSourceEnabled=0" in off_log and
            "[M1 PhysicsConfig] CompareOnly=0 EnableRuntime=1" in on_log and
            "[M1 PhysicsConfig] CompareOnly=0 EnableRuntime=1" in off_log,
        "fixedMappingAgcOff": display_on == display_off and
            any("field=Gain value=3.000" in line for line in display_on) and
            any("field=Automatic value=0.000" in line for line in display_on) and
            "agcEnabled=0" in on_log and "agcEnabled=0" in off_log,
        "controlledMediaComplete": on_media.get("result") == off_media.get("result") == "PASS",
        "controlledFrames800x800": on_image.size == off_image.size == (800, 800),
        "localizedPixelContribution": measurements["controlledHeat"]["nonzeroPixels"] >= 10000 and
            measurements["controlledHeat"]["meanAbsGray8"] >= 10.0 * control_mean and
            measurements["topControl"]["nonzeroPixels"] == 0 and
            measurements["bottomControl"]["nonzeroPixels"] == 0,
        "ordinaryFormalTauAndVisibilityRestored": len(ordinary_matches) > 0 and
            all(0.0 < float(match.group(1)) < 1.0 for match in ordinary_matches),
        "profileInitAndCacheReady": "[Stage5 PlumeConfig] EnableEnginePlume=1 profile=OK" in ordinary_log and
            re.search(r"\[Stage5 PlumePerf\].*visiblePlumeCount=2.*textureLoadCountThisFrame=0", ordinary_log) is not None,
        "depthAndCompositePolicy": "set_depth_write(false)" in source and
            "set_depth_test(true)" in source and "sameOutput=1" in ordinary_log,
        "noMissingTauPromotedToOne": "formalTauReady=0 formalTau=1" not in ordinary_log,
    }

    report = {
        "schema": "hwasimir.p13.plume-pixel-qc.v1",
        "result": "PASS" if all(checks.values()) else "FAIL",
        "checks": checks,
        "p12Baseline": {
            "path": str(baseline_path),
            "sha256": sha256(baseline_path),
            "matchingLines": [baseline.count("\n", 0, match.start()) + 1 for match in baseline_matches],
            "observed": "formalTauReady=0, formalTau=0, coreVisible=0, haloVisible=0",
        },
        "controlledGenericHeatSource": {
            "claimBoundary": "artificial generic on/off heat source; not an equipment signature or calibration",
            "onImage": str(on_image_path),
            "onImageSha256": sha256(on_image_path),
            "offImage": str(off_image_path),
            "offImageSha256": sha256(off_image_path),
            "roiMeasurements": measurements,
            "heatToWorstSideControlMeanRatio": measurements["controlledHeat"]["meanAbsGray8"] / control_mean,
            "fixedMapping": {"band": "MWIR", "preset": "Game", "gain": 3.0,
                             "offsetGray": 0.0, "gamma": 2.2, "automatic": False,
                             "toneMapReinhard": True},
        },
        "ordinaryBindingRegression": {
            "case": str(ordinary_dir),
            "formalVisibleSamples": len(ordinary_matches),
            "formalTauMin": min(float(match.group(1)) for match in ordinary_matches),
            "formalTauMax": max(float(match.group(1)) for match in ordinary_matches),
        },
        "interpretation": "data identity, initialization, zero in-frame texture loads, draw visibility, depth occlusion policy and final compositing all pass; no gain or tau=1 substitution is used",
        "calibrationStatus": "NOT_VERIFIED_CALIBRATION",
    }
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"result": report["result"], "output": str(output),
                      "heatMean": measurements["controlledHeat"]["meanAbsGray8"],
                      "controlMeanMax": control_mean,
                      "formalVisibleSamples": len(ordinary_matches)}))
    return 0 if report["result"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
