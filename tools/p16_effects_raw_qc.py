#!/usr/bin/env python3
"""Signed raw comparison for P16 production-effect fixtures.

The acceptance direction is always ``first - second``.  Absolute differences
are reported as secondary diagnostics only and cannot satisfy the plume check.
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
EXPECTED_ELF = "3d8d97ded05014a11e78983770bd4dacfb0136ded7156e38cefcac4672c40bde"
EXPECTED_LUT = "48432459ac56dbb6980f3ca25e2d1e4701763ba3674703051cb0fd45da8c7cd7"
EXPECTED_COVERAGE = "f5d27c304b663218b94c9e45bd2bbbb968de1a66695f551ba5c4633f9c4c5d7b"
ROI = (80, 100, 720, 700)
CORE_ROI = (360, 360, 440, 440)
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
    # P12DiagnosticWriter writes rows in displayed top-to-bottom order.
    return values.reshape(height, width, channels).astype(np.float64), scale


def only(path: Path, pattern: str) -> Path:
    found = list(path.glob(pattern))
    if len(found) != 1:
        raise RuntimeError(f"expected one {pattern} below {path}, got {len(found)}")
    return found[0]


def load_case(path: Path, seq: int) -> dict:
    result = json.loads((path / "case_result.json").read_text(encoding="utf-8"))
    plan = json.loads((path / "case_plan.json").read_text(encoding="utf-8"))
    log_text = (path / "board.log").read_text(encoding="utf-8", errors="replace")
    pfm = only(path / "diagnostic_linear", f"*_linear_seq{seq}.pfm")
    preview = only(path / "diagnostic_linear", f"*_linear_seq{seq}_rgb8.png")
    raw, scale = read_pfm(pfm)
    mp4 = Path(result.get("products", {}).get("mp4", ""))
    failures: list[str] = []
    if result.get("result") != "PASS":
        failures.append("case_result_not_pass")
    if result.get("materialView") != 0:
        failures.append("material_view_not_normal")
    if result.get("resolution") != "800x800":
        failures.append("resolution_not_800x800")
    if not result.get("ddsOnly"):
        failures.append("not_dds_only")
    if not mp4.is_file() or mp4.stat().st_size <= 0:
        failures.append("missing_nonempty_mp4")
    preflight = "\n".join(result.get("boardPreflight", []))
    for key, value in (
        ("ElfSha256", EXPECTED_ELF),
        ("FormalLutSha256", EXPECTED_LUT),
        ("CoverageManifestSha256", EXPECTED_COVERAGE),
    ):
        if f"{key}={value}" not in preflight:
            failures.append(f"{key}_mismatch")
    return {
        "path": path,
        "result": result,
        "plan": plan,
        "log": log_text,
        "raw": raw,
        "scale": scale,
        "pfm": pfm,
        "preview": preview,
        "mp4": mp4,
        "failures": failures,
    }


def stats(values: np.ndarray, comparison: str) -> dict:
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
        "percentiles": {
            str(q): float(np.percentile(flat, q))
            for q in (0.1, 1, 25, 50, 75, 99, 99.9)
        },
        "positiveSamples": int(positive.size),
        "negativeSamples": int(negative.size),
        "nearZeroSamples": int(near_zero.size),
        "positiveSum": positive_sum,
        "negativeSum": negative_sum,
        "positiveToNegativeMagnitudeRatio": (
            positive_sum / abs(negative_sum) if negative_sum else None
        ),
        "unit": "W/(m^2 sr um)",
        "comparison": comparison,
    }


def save_signed_visual(diff: np.ndarray, output: Path) -> dict:
    gray = diff.mean(axis=2)
    scale = float(np.percentile(np.abs(gray), 99.9))
    normalized = (
        np.zeros_like(gray)
        if scale <= 0.0
        else np.clip(gray / scale, -1.0, 1.0)
    )
    image = np.zeros((gray.shape[0], gray.shape[1], 3), dtype=np.uint8)
    image[..., 0] = np.clip(normalized, 0.0, 1.0) * 255
    image[..., 2] = np.clip(-normalized, 0.0, 1.0) * 255
    rendered = Image.fromarray(image, "RGB")
    draw = ImageDraw.Draw(rendered)
    draw.rectangle(ROI, outline=(255, 255, 255), width=2)
    draw.rectangle(CORE_ROI, outline=(255, 255, 0), width=2)
    rendered.save(output)
    return {
        "path": str(output),
        "sha256": sha256(output),
        "legend": "red=positive first-minus-second radiance; blue=negative; white=full plume ROI; yellow=geometry-fixed hot-core ROI",
        "normalization": "symmetric full-frame 99.9 percentile absolute mean-channel delta",
        "scaleRadiance": scale,
    }


def check_plume(first: dict, second: dict, failures: list[str]) -> dict:
    first_switch = first["result"].get("controlledFixture", {}).get("syntheticHeatSource")
    second_switch = second["result"].get("controlledFixture", {}).get("syntheticHeatSource")
    if first_switch != "On" or second_switch != "Off":
        failures.append("plume_switch_pair_not_on_then_off")
    for label, case, enabled in (("on", first, "1"), ("off", second, "0")):
        result = case["result"]
        if result.get("band") != "MWIR" or result.get("protocolBand") != 2:
            failures.append(f"{label}_not_mwir_protocol_2")
        if result.get("targetTypeProtocolCode") != "0x55":
            failures.append(f"{label}_not_civil_protocol_0x55")
        patterns = {
            "switch": rf"\[P5GraphicsTest\].*materialView=0.*syntheticHeatSourceEnabled={enabled}",
            "shader": r"\[GameSpriteResources\].*priority=100.*shader=Config/GameVFX/sprite\.frag.*haloMask=.*/Config/Weather/Textures/smoke\.png",
            "formal": r"\[Stage5 Plume\].*targetType=0x55.*formalTauReady=1.*coreVisible=1.*haloVisible=1",
            "identity": rf"\[P14 AtmosphereIdentity\].*status=PASS.*actualLutSha256={EXPECTED_LUT}.*actualManifestSha256={EXPECTED_COVERAGE}.*originalInputBusinessDependency=0",
        }
        for name, pattern in patterns.items():
            if not re.search(pattern, case["log"]):
                failures.append(f"{label}_missing_log_{name}")
    return {
        "firstSwitch": first_switch,
        "secondSwitch": second_switch,
        "acceptance": "signed core ROI must be positive; full ROI retains and reports both hot emission and cold-smoke absorption",
    }


def case_meta(case: dict) -> dict:
    return {
        "directory": str(case["path"]),
        "name": case["result"].get("name"),
        "band": case["result"].get("band"),
        "weather": case["result"].get("weather"),
        "inputSha256": case["result"].get("inputSha256"),
        "pfm": str(case["pfm"]),
        "pfmSha256": sha256(case["pfm"]),
        "preview": str(case["preview"]),
        "previewSha256": sha256(case["preview"]),
        "mp4": str(case["mp4"]),
        "mp4Sha256": sha256(case["mp4"]) if case["mp4"].is_file() else None,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--first", type=Path, required=True)
    parser.add_argument("--second", type=Path, required=True)
    parser.add_argument("--seq", type=int, default=900)
    parser.add_argument("--mode", choices=("plume", "descriptive"), default="plume")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    first = load_case(args.first.resolve(), args.seq)
    second = load_case(args.second.resolve(), args.seq)
    failures = list(first["failures"] + second["failures"])
    for field in ("inputSha256", "band", "protocolBand", "resolution", "materialView"):
        if first["result"].get(field) != second["result"].get(field):
            failures.append(f"pair_{field}_mismatch")
    if first["raw"].shape != second["raw"].shape:
        failures.append("raw_shape_mismatch")
        diff = np.empty((0, 0, 0))
    else:
        diff = first["raw"] - second["raw"]
    if diff.shape != (800, 800, 3):
        failures.append(f"unexpected_raw_shape_{diff.shape}")

    mode_details: dict = {}
    if args.mode == "plume":
        mode_details = check_plume(first, second, failures)
    comparison = f"{first['result'].get('name')} minus {second['result'].get('name')}"
    x0, y0, x1, y1 = ROI
    cx0, cy0, cx1, cy1 = CORE_ROI
    full = stats(diff, comparison) if diff.size else {}
    roi = stats(diff[y0:y1, x0:x1, :], comparison) if diff.size else {}
    core_roi = stats(diff[cy0:cy1, cx0:cx1, :], comparison) if diff.size else {}
    if args.mode == "plume" and core_roi:
        if core_roi["signedMean"] <= 0.0:
            failures.append("core_roi_signed_mean_not_positive")
        if core_roi["signedSum"] <= 0.0:
            failures.append("core_roi_signed_sum_not_positive")
        if core_roi["positiveSamples"] <= core_roi["negativeSamples"]:
            failures.append("core_roi_positive_samples_not_dominant")
        ratio = core_roi["positiveToNegativeMagnitudeRatio"]
        if ratio is not None and ratio <= 1.0:
            failures.append("core_roi_positive_energy_not_dominant")
        if roi.get("negativeSamples", 0) <= 0:
            failures.append("full_roi_missing_cold_smoke_absorption")

    visual = (
        save_signed_visual(diff, args.output / f"seq{args.seq}_first_minus_second_signed.png")
        if diff.size
        else {}
    )
    report = {
        "schema": "hwasimir.p16.effects-signed-raw-qc.v1",
        "status": "PASS" if not failures else "FAIL",
        "mode": args.mode,
        "sequence": args.seq,
        "comparison": comparison,
        "acceptanceBasis": "signed physical-radiance difference; absolute difference is secondary only",
        "calibrationStatus": "NOT_VERIFIED_CALIBRATION",
        "rawStorage": "renderer RGBA16F SI readback serialized as RGB PFM",
        "fixedRoi": {"x0": x0, "y0": y0, "x1": x1, "y1": y1},
        "fixedCoreRoi": {"x0": cx0, "y0": cy0, "x1": cx1, "y1": cy1},
        "fullFrame": full,
        "roi": roi,
        "coreRoi": core_roi,
        "interpretation": {
            "hotCore": "positive signed On-minus-Off radiance in the geometry-fixed core ROI",
            "coldSmoke": "negative signed contribution is retained as absorption and is not relabeled as failed drawing",
            "netSign": "the full plume ROI may be negative when the wider cold smoke area exceeds the compact hot core; acceptance never uses absolute difference",
            "displayPolarity": "raw radiance sign is evaluated before white-hot/black-hot display mapping",
        },
        "modeDetails": mode_details,
        "visual": visual,
        "identity": {
            "elfSha256": EXPECTED_ELF,
            "lutSha256": EXPECTED_LUT,
            "coverageManifestSha256": EXPECTED_COVERAGE,
        },
        "cases": {"first": case_meta(first), "second": case_meta(second)},
        "failures": failures,
    }
    report_path = args.output / f"seq{args.seq}_signed_raw_qc.json"
    report_path.write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps({"status": report["status"], "roi": roi, "report": str(report_path)}))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
