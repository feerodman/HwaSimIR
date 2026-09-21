#!/usr/bin/env python3
"""Compare deterministic RK3588 P5 model captures before/after texture sampling changes.

The input clips use the same fixture, camera path, 800x800 output and 60 Hz
timeline.  Metrics are restricted to decoded foreground pixels so the black
background cannot dilute temporal deltas.  This is a renderer diagnostic, not
an assertion about sensor calibration or an ordinary M1 scene.
"""

from __future__ import annotations

import csv
import hashlib
import json
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "logs" / "p16" / "production_effects" / "model_flicker"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def probe(path: Path) -> dict:
    raw = subprocess.check_output(
        [
            "ffprobe", "-v", "error", "-count_frames", "-select_streams", "v:0",
            "-show_entries", "stream=codec_name,width,height,avg_frame_rate,nb_read_frames",
            "-of", "json", str(path),
        ],
        text=True,
        encoding="utf-8",
    )
    stream = json.loads(raw)["streams"][0]
    return {
        "codec": stream["codec_name"],
        "width": int(stream["width"]),
        "height": int(stream["height"]),
        "avgFrameRate": stream["avg_frame_rate"],
        "frames": int(stream["nb_read_frames"]),
    }


def decode(path: Path, width: int, height: int):
    command = ["ffmpeg", "-v", "error", "-i", str(path), "-pix_fmt", "gray", "-f", "rawvideo", "-"]
    process = subprocess.Popen(command, stdout=subprocess.PIPE)
    frame_bytes = width * height
    try:
        while True:
            raw = process.stdout.read(frame_bytes) if process.stdout else b""
            if not raw:
                break
            if len(raw) != frame_bytes:
                raise RuntimeError(f"short decoded frame in {path}: {len(raw)} != {frame_bytes}")
            yield np.frombuffer(raw, dtype=np.uint8).reshape(height, width).copy()
    finally:
        if process.stdout:
            process.stdout.close()
        code = process.wait()
        if code != 0:
            raise RuntimeError(f"ffmpeg decode failed for {path}: {code}")


def percentile(values: list[float], value: float) -> float:
    return float(np.percentile(np.asarray(values, dtype=np.float64), value)) if values else 0.0


def analyze(path: Path, review_indices: set[int]) -> tuple[dict, dict[int, np.ndarray]]:
    info = probe(path)
    previous = None
    temporal_mad: list[float] = []
    temporal_changed2: list[float] = []
    foreground_mean: list[float] = []
    spatial_hf: list[float] = []
    hashes: set[str] = set()
    decoded_sequence = hashlib.sha256()
    selected: dict[int, np.ndarray] = {}
    decoded = 0
    bbox_union = [info["width"], info["height"], -1, -1]

    for decoded, frame in enumerate(decode(path, info["width"], info["height"]), start=1):
        hashes.add(hashlib.md5(frame.tobytes()).hexdigest())
        decoded_sequence.update(frame.tobytes())
        mask = frame > 4
        ys, xs = np.nonzero(mask)
        if xs.size:
            bbox_union[0] = min(bbox_union[0], int(xs.min()))
            bbox_union[1] = min(bbox_union[1], int(ys.min()))
            bbox_union[2] = max(bbox_union[2], int(xs.max()))
            bbox_union[3] = max(bbox_union[3], int(ys.max()))
            foreground_mean.append(float(frame[mask].mean()))

            horizontal = mask[:, 1:] & mask[:, :-1]
            vertical = mask[1:, :] & mask[:-1, :]
            gx = np.abs(frame[:, 1:].astype(np.int16) - frame[:, :-1].astype(np.int16))
            gy = np.abs(frame[1:, :].astype(np.int16) - frame[:-1, :].astype(np.int16))
            contributions = []
            if horizontal.any():
                contributions.append(gx[horizontal])
            if vertical.any():
                contributions.append(gy[vertical])
            if contributions:
                spatial_hf.append(float(np.concatenate(contributions).mean()))

        if previous is not None:
            pair_mask = (previous > 4) | mask
            if pair_mask.any():
                delta = np.abs(frame.astype(np.int16) - previous.astype(np.int16))[pair_mask]
                temporal_mad.append(float(delta.mean()))
                temporal_changed2.append(float(np.count_nonzero(delta > 2) / delta.size))
        if decoded in review_indices:
            selected[decoded] = frame.copy()
        previous = frame

    if decoded != info["frames"]:
        raise RuntimeError(f"frame count mismatch for {path}: decoded={decoded}, probed={info['frames']}")

    return {
        "path": str(path.resolve()),
        "sha256": sha256(path),
        "probe": info,
        "decodedFrames": decoded,
        "uniqueDecodedFrames": len(hashes),
        "decodedSequenceSha256": decoded_sequence.hexdigest(),
        "foregroundThreshold": 4,
        "foregroundUnionBbox": bbox_union,
        "foregroundMeanStd": float(np.std(np.asarray(foreground_mean, dtype=np.float64))),
        "spatialHighFrequencyMean": float(np.mean(spatial_hf)),
        "temporalForegroundMadMean": float(np.mean(temporal_mad)),
        "temporalForegroundMadP95": percentile(temporal_mad, 95),
        "temporalForegroundMadP99": percentile(temporal_mad, 99),
        "temporalForegroundChangedOver2Mean": float(np.mean(temporal_changed2)),
        "temporalForegroundChangedOver2P99": percentile(temporal_changed2, 99),
    }, selected


def contact_sheet(before: dict[int, np.ndarray], after: dict[int, np.ndarray], destination: Path) -> None:
    indices = sorted(set(before) & set(after))
    if not indices:
        return
    crop = (160, 300, 600, 540)
    width, height = crop[2] - crop[0], crop[3] - crop[1]
    canvas = Image.new("RGB", (width * len(indices), height * 2 + 44), "#151515")
    draw = ImageDraw.Draw(canvas)
    draw.text((8, 4), "before: single-level sampling", fill="white")
    draw.text((8, height + 24), "after: mipmapped trilinear/aniso sampling", fill="white")
    for column, index in enumerate(indices):
        x = column * width
        canvas.paste(Image.fromarray(before[index], mode="L").convert("RGB").crop(crop), (x, 20))
        canvas.paste(Image.fromarray(after[index], mode="L").convert("RGB").crop(crop), (x, height + 40))
        draw.text((x + 6, 22), f"frame {index}", fill=(255, 196, 64))
        draw.text((x + 6, height + 42), f"frame {index}", fill=(255, 196, 64))
    canvas.save(destination)


def main() -> int:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    review = {1, 120, 240, 360}
    pairs = [
        ("parallax_texture", "p16_f22_parallax_texture_before", "p16_f22_parallax_texture_after"),
        ("parallax_material_id_control", "p16_f22_parallax_material_before", "p16_f22_parallax_material_after"),
        ("static_material_id_control", "p16_f22_static_material_before", "p16_f22_static_material_after"),
    ]
    results = []
    for case_id, before_name, after_name in pairs:
        before_path = ROOT / "logs" / "p5" / before_name / "received.h264"
        after_path = ROOT / "logs" / "p5" / after_name / "received.h264"
        before, before_frames = analyze(before_path, review)
        after, after_frames = analyze(after_path, review)
        selected_equal = all(
            np.array_equal(before_frames.get(index), after_frames.get(index)) for index in review
        )
        decoded_equal = before["decodedSequenceSha256"] == after["decodedSequenceSha256"]
        temporal_reduction = 1.0 - after["temporalForegroundMadMean"] / before["temporalForegroundMadMean"] \
            if before["temporalForegroundMadMean"] else 0.0
        result = {
            "caseId": case_id,
            "before": before,
            "after": after,
            "elementaryStreamsExactlyEqual": before["sha256"] == after["sha256"],
            "decodedFramesExactlyEqual": decoded_equal,
            "selectedDecodedFramesExactlyEqual": selected_equal,
            "temporalForegroundMadReductionFraction": temporal_reduction,
            "interpretation": (
                "TEXTURE_SAMPLING_CHANGED_COMPARE_METRICS" if case_id == "parallax_texture" else
                "UNCHANGED_CONTROL_EXPECTED"
            ),
        }
        results.append(result)
        contact_sheet(before_frames, after_frames, OUTPUT / f"{case_id}_before_after.png")

    report = {
        "schema": "HwaSimIR.P16.ProductionModelFlickerComparison.1",
        "scope": "Actual RK3588 HwaSim_IR P5 production model/shader diagnostic; not formal M1 calibration evidence",
        "fixture": {"model": "F22 source asset unchanged", "resolution": "800x800", "fps": 60, "frames": 360},
        "method": {
            "sameFixtureTimeline": True,
            "foregroundOnly": True,
            "backgroundDilutionExcluded": True,
            "ordinaryMotionAloneDeclaredDefect": False,
        },
        "cases": results,
    }
    (OUTPUT / "comparison.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    with (OUTPUT / "comparison.csv").open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=[
            "caseId", "beforeFrames", "afterFrames", "beforeTemporalMadMean", "afterTemporalMadMean",
            "reductionFraction", "beforeSpatialHf", "afterSpatialHf", "elementaryStreamsExactlyEqual",
            "decodedFramesExactlyEqual", "selectedDecodedFramesExactlyEqual", "interpretation",
        ])
        writer.writeheader()
        for result in results:
            writer.writerow({
                "caseId": result["caseId"],
                "beforeFrames": result["before"]["decodedFrames"],
                "afterFrames": result["after"]["decodedFrames"],
                "beforeTemporalMadMean": result["before"]["temporalForegroundMadMean"],
                "afterTemporalMadMean": result["after"]["temporalForegroundMadMean"],
                "reductionFraction": result["temporalForegroundMadReductionFraction"],
                "beforeSpatialHf": result["before"]["spatialHighFrequencyMean"],
                "afterSpatialHf": result["after"]["spatialHighFrequencyMean"],
                "elementaryStreamsExactlyEqual": result["elementaryStreamsExactlyEqual"],
                "decodedFramesExactlyEqual": result["decodedFramesExactlyEqual"],
                "selectedDecodedFramesExactlyEqual": result["selectedDecodedFramesExactlyEqual"],
                "interpretation": result["interpretation"],
            })
    print(json.dumps({
        result["caseId"]: {
            "beforeTemporalMad": result["before"]["temporalForegroundMadMean"],
            "afterTemporalMad": result["after"]["temporalForegroundMadMean"],
            "reductionFraction": result["temporalForegroundMadReductionFraction"],
            "exactStream": result["elementaryStreamsExactlyEqual"],
            "exactDecodedFrames": result["decodedFramesExactlyEqual"],
        }
        for result in results
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
