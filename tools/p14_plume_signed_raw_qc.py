#!/usr/bin/env python3
"""Verify the P14 controlled generic heat-source contribution in signed SI space.

This deliberately compares ``On - Off``.  Absolute differences are reported only
as descriptive secondary data and can never satisfy the acceptance condition.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
EXPECTED_ELF = "80267b01054d9fe8e8ef2c3fd8f3e726003794dafc6cc1086958e9713f18841c"
EXPECTED_LUT = "48432459ac56dbb6980f3ca25e2d1e4701763ba3674703051cb0fd45da8c7cd7"
EXPECTED_COVERAGE = "f5d27c304b663218b94c9e45bd2bbbb968de1a66695f551ba5c4633f9c4c5d7b"
ROI = (80, 100, 720, 700)  # fixed before analysis: production P5 end-view sensor plane
EPSILON = 1.0e-6


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_pfm(path: Path) -> tuple[np.ndarray, float]:
    with path.open("rb") as stream:
        magic = stream.readline().strip()
        if magic not in (b"PF", b"Pf"):
            raise RuntimeError(f"unsupported PFM magic {magic!r}: {path}")
        width, height = map(int, stream.readline().split())
        scale = float(stream.readline())
        channels = 3 if magic == b"PF" else 1
        values = np.fromfile(stream, dtype="<f4" if scale < 0 else ">f4")
    expected = width * height * channels
    if values.size != expected:
        raise RuntimeError(f"PFM payload size {values.size}, expected {expected}: {path}")
    # P12DiagnosticWriter serializes displayed top-to-bottom rows.
    return values.reshape(height, width, channels).astype(np.float64), scale


def only(path: Path, pattern: str) -> Path:
    found = list(path.glob(pattern))
    if len(found) != 1:
        raise RuntimeError(f"expected one {pattern} below {path}, got {len(found)}")
    return found[0]


def load_case(path: Path, expected_switch: str) -> dict:
    plan = json.loads((path / "case_plan.json").read_text(encoding="utf-8"))
    result = json.loads((path / "case_result.json").read_text(encoding="utf-8"))
    log_text = (path / "board.log").read_text(encoding="utf-8", errors="replace")
    pfm_path = only(path / "diagnostic_linear", "*_linear_seq660.pfm")
    png_path = only(path / "diagnostic_linear", "*_linear_seq660_rgb8.png")

    failures: list[str] = []
    if result.get("result") != "PASS":
        failures.append("case_result_not_pass")
    if plan.get("controlledFixture", {}).get("syntheticHeatSource") != expected_switch:
        failures.append("controlled_switch_mismatch")
    if plan.get("targetTypeProtocolCode") != "0x55":
        failures.append("not_civil_truck_protocol_0x55")
    if plan.get("materialView") != 0:
        failures.append("material_view_not_normal")
    if plan.get("band") != "MWIR" or plan.get("protocolBand") != 2:
        failures.append("not_mwir_protocol_2")
    if plan.get("inputBoundary") != "immutable_original_replay":
        failures.append("input_not_immutable_original")
    preflight = "\n".join(plan.get("boardPreflight", []))
    for key, value in (("ElfSha256", EXPECTED_ELF), ("FormalLutSha256", EXPECTED_LUT),
                       ("CoverageManifestSha256", EXPECTED_COVERAGE)):
        if f"{key}={value}" not in preflight:
            failures.append(f"{key}_mismatch")
    switch_value = "1" if expected_switch == "On" else "0"
    required_log_patterns = {
        "actual_p5_switch": rf"\[P5GraphicsTest\].*materialView=0.*syntheticHeatSourceEnabled={switch_value}",
        "actual_sprite_shader": r"\[GameSpriteResources\].*priority=100.*shader=Config/GameVFX/sprite\.frag.*rgbContract=formal_W_per_m2_sr_um_or_explicit_legacy_linear",
        "formal_plume": r"\[Stage5 Plume\].*targetType=0x55.*formalTauReady=1.*spriteRgbEquation=tau_times_source_plus_path.*blend=straight_alpha",
        "formal_identity": rf"\[P14 AtmosphereIdentity\].*status=PASS.*actualLutSha256={EXPECTED_LUT}.*actualManifestSha256={EXPECTED_COVERAGE}.*originalInputBusinessDependency=0",
    }
    matched = {}
    for name, pattern in required_log_patterns.items():
        match = re.search(pattern, log_text)
        matched[name] = bool(match)
        if not match:
            failures.append(f"missing_log_{name}")
    mp4 = Path(result.get("products", {}).get("mp4", ""))
    if not mp4.is_file() or mp4.stat().st_size <= 0:
        failures.append("missing_nonempty_mp4")

    raw, scale = read_pfm(pfm_path)
    return {
        "path": path,
        "plan": plan,
        "result": result,
        "raw": raw,
        "pfm": pfm_path,
        "png": png_path,
        "pfmScale": scale,
        "matchedLogs": matched,
        "failures": failures,
        "mp4": mp4,
    }


def stats(values: np.ndarray) -> dict:
    flat = values.reshape(-1)
    positive = flat[flat > EPSILON]
    negative = flat[flat < -EPSILON]
    near_zero = flat[(flat >= -EPSILON) & (flat <= EPSILON)]
    positive_sum = float(positive.sum()) if positive.size else 0.0
    negative_sum = float(negative.sum()) if negative.size else 0.0
    return {
        "samples": int(flat.size),
        "signedMean": float(flat.mean()),
        "signedSum": float(flat.sum()),
        "meanAbsoluteSecondaryOnly": float(np.abs(flat).mean()),
        "minimum": float(flat.min()),
        "maximum": float(flat.max()),
        "percentiles": {str(q): float(np.percentile(flat, q)) for q in (0.1, 1, 25, 50, 75, 99, 99.9)},
        "positiveSamples": int(positive.size),
        "negativeSamples": int(negative.size),
        "nearZeroSamples": int(near_zero.size),
        "positiveSum": positive_sum,
        "negativeSum": negative_sum,
        "positiveToNegativeMagnitudeRatio": (
            positive_sum / abs(negative_sum) if negative_sum else None
        ),
        "unit": "W/(m^2 sr um)",
        "comparison": "On minus Off",
    }


def save_signed_visual(diff: np.ndarray, output: Path) -> dict:
    gray = diff.mean(axis=2)
    magnitude_scale = float(np.percentile(np.abs(gray), 99.9))
    normalized = np.zeros_like(gray) if magnitude_scale <= 0 else np.clip(gray / magnitude_scale, -1.0, 1.0)
    image = np.zeros((gray.shape[0], gray.shape[1], 3), dtype=np.uint8)
    image[..., 0] = np.clip(normalized, 0.0, 1.0) * 255  # positive = red
    image[..., 2] = np.clip(-normalized, 0.0, 1.0) * 255  # negative = blue
    visual = Image.fromarray(image, "RGB")
    draw = ImageDraw.Draw(visual)
    draw.rectangle(ROI, outline=(255, 255, 255), width=2)
    visual.save(output)
    return {
        "path": str(output),
        "sha256": sha256(output),
        "legend": "red=positive heat-emission contribution, blue=negative absorption/cooling contribution, white=fixed ROI",
        "normalization": "symmetric signed scale at full-frame 99.9 percentile absolute mean-channel delta",
        "scaleRadiance": magnitude_scale,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--on", type=Path, default=ROOT / "logs/p14/runs/p14_final4_plume_truck_mwir_on_raw")
    parser.add_argument("--off", type=Path, default=ROOT / "logs/p14/runs/p14_final4_plume_truck_mwir_off_raw")
    parser.add_argument("--output", type=Path, default=ROOT / "logs/p14/plume_signed_raw_qc_final4")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    on = load_case(args.on.resolve(), "On")
    off = load_case(args.off.resolve(), "Off")
    failures = on["failures"] + off["failures"]
    if on["raw"].shape != off["raw"].shape:
        failures.append("raw_shape_mismatch")
        diff = np.empty((0, 0, 0))
    else:
        diff = on["raw"] - off["raw"]
    if diff.shape != (800, 800, 3):
        failures.append(f"unexpected_raw_shape_{diff.shape}")

    x0, y0, x1, y1 = ROI
    full_stats = stats(diff) if diff.size else {}
    roi_stats = stats(diff[y0:y1, x0:x1, :]) if diff.size else {}
    if roi_stats:
        if roi_stats["signedMean"] <= 0.0:
            failures.append("roi_signed_mean_not_positive")
        if roi_stats["signedSum"] <= 0.0:
            failures.append("roi_signed_sum_not_positive")
        if roi_stats["positiveSamples"] <= roi_stats["negativeSamples"]:
            failures.append("roi_positive_samples_not_dominant")
        ratio = roi_stats["positiveToNegativeMagnitudeRatio"]
        if ratio is not None and ratio <= 1.0:
            failures.append("roi_positive_energy_not_dominant")

    visual_path = args.output / "on_minus_off_signed_raw.png"
    visual_meta = save_signed_visual(diff, visual_path) if diff.size else {}
    report = {
        "schema": "hwasimir.p14.plume-signed-raw-qc.v1",
        "status": "PASS" if not failures else "FAIL",
        "acceptanceBasis": "signed physical-radiance difference; absolute difference alone never passes",
        "p13AbsoluteDifferenceInterpretation": "rejected_as_directionless",
        "calibrationStatus": "NOT_VERIFIED_CALIBRATION",
        "rawStorage": "renderer RGBA16F SI readback serialized as RGB PFM",
        "comparison": "controlled generic heat source On minus Off",
        "fixedRoi": {"x0": x0, "y0": y0, "x1": x1, "y1": y1, "pixels": (x1-x0)*(y1-y0)},
        "fullFrame": full_stats,
        "roi": roi_stats,
        "interpretation": {
            "positive": "added thermal emission reaching the sensor",
            "negative": "local absorption/cooling or rasterization differences; retained and counted",
            "displayPolarity": "raw sign is evaluated before display mapping; black-hot reverses perceived brightness but not radiance sign",
        },
        "visual": visual_meta,
        "identity": {"elfSha256": EXPECTED_ELF, "lutSha256": EXPECTED_LUT, "coverageManifestSha256": EXPECTED_COVERAGE},
        "cases": {
            "on": {"directory": str(on["path"]), "pfm": str(on["pfm"]), "pfmSha256": sha256(on["pfm"]),
                   "preview": str(on["png"]), "previewSha256": sha256(on["png"]), "mp4": str(on["mp4"]),
                   "mp4Sha256": sha256(on["mp4"]), "matchedLogs": on["matchedLogs"]},
            "off": {"directory": str(off["path"]), "pfm": str(off["pfm"]), "pfmSha256": sha256(off["pfm"]),
                    "preview": str(off["png"]), "previewSha256": sha256(off["png"]), "mp4": str(off["mp4"]),
                    "mp4Sha256": sha256(off["mp4"]), "matchedLogs": off["matchedLogs"]},
        },
        "failures": failures,
    }
    report_path = args.output / "plume_signed_raw_qc.json"
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": report["status"], "roi": roi_stats, "report": str(report_path)}, ensure_ascii=False))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
