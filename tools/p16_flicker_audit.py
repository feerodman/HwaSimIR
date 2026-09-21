#!/usr/bin/env python3
"""Read-only temporal audit of existing P14/P15 media.

The script never compares images from different runs as if they were the same
frame.  It reports candidates, not a production root cause.  Derived PNG/JSON
files are written only below the requested output directory.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def run_text(args: list[str]) -> str:
    return subprocess.check_output(args, text=True, encoding="utf-8", errors="replace")


def probe(path: Path) -> dict:
    data = json.loads(run_text([
        "ffprobe", "-v", "error", "-count_frames", "-select_streams", "v:0",
        "-show_entries", "stream=codec_name,width,height,avg_frame_rate,nb_read_frames,duration",
        "-show_entries", "format=duration", "-of", "json", str(path),
    ]))
    stream = data["streams"][0]
    return {
        "codec": stream.get("codec_name"),
        "width": int(stream["width"]),
        "height": int(stream["height"]),
        "avgFrameRate": stream.get("avg_frame_rate"),
        "frames": int(stream.get("nb_read_frames") or 0),
        "durationSeconds": float(stream.get("duration") or data.get("format", {}).get("duration") or 0),
    }


def load_annotations(path: Path) -> dict[int, dict]:
    out: dict[int, dict] = {}
    with path.open("r", encoding="utf-8") as stream:
        for line in stream:
            row = json.loads(line)
            if not row.get("targets"):
                continue
            target = row["targets"][0]
            corners = target.get("bboxCorners") or []
            if not corners:
                continue
            xs = [int(p["x"]) for p in corners]
            ys = [int(p["y"]) for p in corners]
            out[int(row["frameIndex"])] = {
                "frameIndex": int(row["frameIndex"]),
                "ptsMs": int(row.get("ptsMs", 0)),
                "bbox": [min(xs), min(ys), max(xs), max(ys)],
                "modelLabel": target.get("modelLabel", ""),
            }
    return out


def framemd5(path: Path) -> list[str]:
    text = run_text(["ffmpeg", "-v", "error", "-i", str(path), "-map", "0:v:0", "-f", "framemd5", "-"])
    return [line.rsplit(",", 1)[-1].strip() for line in text.splitlines() if line and not line.startswith("#")]


def crop_metrics(frame: np.ndarray, box: list[int]) -> tuple[float, float, float, np.ndarray] | None:
    height, width = frame.shape
    x0, y0, x1, y1 = box
    x0, x1 = max(0, x0 + 2), min(width - 1, x1 - 2)
    y0, y1 = max(0, y0 + 2), min(height - 1, y1 - 2)
    if x1 < x0 or y1 < y0:
        return None
    roi = frame[y0:y1 + 1, x0:x1 + 1].astype(np.float32)
    pad = max(8, int(max(x1 - x0 + 1, y1 - y0 + 1) * 0.6))
    bx0, bx1 = max(0, x0 - pad), min(width - 1, x1 + pad)
    by0, by1 = max(0, y0 - pad), min(height - 1, y1 + pad)
    ring = frame[by0:by1 + 1, bx0:bx1 + 1].astype(np.float32).copy()
    mask = np.ones(ring.shape, dtype=bool)
    mask[y0 - by0:y1 - by0 + 1, x0 - bx0:x1 - bx0 + 1] = False
    background_mean = float(ring[mask].mean()) if mask.any() else float("nan")
    gx = np.abs(np.diff(roi, axis=1)).mean() if roi.shape[1] > 1 else 0.0
    gy = np.abs(np.diff(roi, axis=0)).mean() if roi.shape[0] > 1 else 0.0
    return float(roi.mean()), background_mean, float((gx + gy) * 0.5), roi


def audit_case(case: dict, output: Path) -> dict:
    video = case["video"]
    info = probe(video)
    annotations = load_annotations(case["annotations"])
    width, height = info["width"], info["height"]
    frame_bytes = width * height
    command = ["ffmpeg", "-v", "error", "-i", str(video), "-f", "rawvideo", "-pix_fmt", "gray", "-"]
    process = subprocess.Popen(command, stdout=subprocess.PIPE)
    samples = []
    selected = {}
    previous = None
    frame_index = 0
    selection_indices = set()
    for seconds in case.get("reviewSeconds", []):
        selection_indices.add(max(1, int(round(seconds * 60.0)) + 1))
    while True:
        raw = process.stdout.read(frame_bytes) if process.stdout else b""
        if len(raw) != frame_bytes:
            break
        frame_index += 1
        frame = np.frombuffer(raw, dtype=np.uint8).reshape(height, width)
        if frame_index in selection_indices:
            selected[frame_index] = frame.copy()
        annotation = annotations.get(frame_index)
        if not annotation:
            previous = None
            continue
        x0, y0, x1, y1 = annotation["bbox"]
        box_width, box_height = x1 - x0 + 1, y1 - y0 + 1
        values = crop_metrics(frame, annotation["bbox"])
        if values is None:
            previous = None
            continue
        target_mean, background_mean, spatial_hf, roi = values
        sample = {
            "frameIndex": frame_index,
            "ptsMs": annotation["ptsMs"],
            "bbox": annotation["bbox"],
            "bboxWidth": box_width,
            "bboxHeight": box_height,
            "targetMean": target_mean,
            "backgroundMean": background_mean,
            "targetMinusBackground": target_mean - background_mean,
            "globalMean": float(frame.mean()),
            "spatialHighFrequency": spatial_hf,
            "sameBboxAsPrevious": False,
            "targetMeanDelta": None,
            "backgroundMeanDelta": None,
            "globalMeanDelta": None,
            "meanAbsoluteRoiDelta": None,
        }
        if previous and previous["bbox"] == annotation["bbox"] and previous["roi"].shape == roi.shape:
            sample["sameBboxAsPrevious"] = True
            sample["targetMeanDelta"] = target_mean - previous["targetMean"]
            sample["backgroundMeanDelta"] = background_mean - previous["backgroundMean"]
            sample["globalMeanDelta"] = sample["globalMean"] - previous["globalMean"]
            sample["meanAbsoluteRoiDelta"] = float(np.abs(roi - previous["roi"]).mean())
        samples.append(sample)
        previous = {
            "bbox": annotation["bbox"], "roi": roi, "targetMean": target_mean,
            "backgroundMean": background_mean, "globalMean": sample["globalMean"],
        }
    return_code = process.wait()
    if return_code != 0:
        raise RuntimeError(f"ffmpeg decode failed for {video}: {return_code}")

    large = [s for s in samples if s["bboxWidth"] >= 20 and s["bboxHeight"] >= 20]
    comparable = [s for s in large if s["sameBboxAsPrevious"] and s["meanAbsoluteRoiDelta"] is not None]
    top = sorted(comparable, key=lambda s: s["meanAbsoluteRoiDelta"], reverse=True)[:10]
    target_deltas = np.array([s["targetMeanDelta"] for s in comparable], dtype=float)
    background_deltas = np.array([s["backgroundMeanDelta"] for s in comparable], dtype=float)
    correlation = None
    if len(comparable) >= 3 and target_deltas.std() > 1e-9 and background_deltas.std() > 1e-9:
        correlation = float(np.corrcoef(target_deltas, background_deltas)[0, 1])

    case_dir = output / case["caseId"]
    case_dir.mkdir(parents=True, exist_ok=True)
    for index, frame in selected.items():
        image = Image.fromarray(frame, mode="L").convert("RGB")
        annotation = annotations.get(index)
        if annotation:
            draw = ImageDraw.Draw(image)
            draw.rectangle(annotation["bbox"], outline=(255, 80, 80), width=2)
        image.save(case_dir / f"review_frame_{index:05d}.png")

    max_box = max(samples, key=lambda s: s["bboxWidth"] * s["bboxHeight"]) if samples else None
    return {
        "caseId": case["caseId"],
        "evidenceClass": case["evidenceClass"],
        "video": str(video.resolve()),
        "videoSha256": sha256(video),
        "annotations": str(case["annotations"].resolve()),
        "annotationsSha256": sha256(case["annotations"]),
        "probe": info,
        "targetFrames": len(samples),
        "modelLabel": next(iter(annotations.values()))["modelLabel"] if annotations else None,
        "maxBbox": max_box,
        "textureResolvableFrames20px": len(large),
        "sameBboxComparableFrames20px": len(comparable),
        "targetBackgroundDeltaCorrelation": correlation,
        "topSameBboxTemporalCandidates": top,
        "interpretation": (
            "INSUFFICIENT_SPATIAL_RESOLUTION" if not large else
            "CANDIDATES_ONLY_MOTION_AND_H264_NOT_EXCLUDED"
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = args.root.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)

    def one(pattern: str) -> Path:
        matches = list(root.glob(pattern))
        if len(matches) != 1:
            raise RuntimeError(f"expected one match for {pattern}, got {len(matches)}")
        return matches[0]

    cases = [
        {
            "caseId": "p15_ordinary_mwir",
            "evidenceClass": "CURRENT_P15_ORDINARY_ENTRY",
            "video": root / "logs/p15/ordinary_feedback_probe/P15_ordinary_original_1txt_mwir_dds.mp4",
            "annotations": root / "logs/p15/ordinary_feedback_probe/dds_producer_annotations.jsonl",
            "elementary": None,
            "reviewSeconds": [12, 24, 32],
        },
        {
            "caseId": "p14_original_swir",
            "evidenceClass": "HISTORICAL_CONTROLLED_ORIGINAL_1TXT",
            "video": one("logs/p14/runs/p14_final4_original_1_swir_clear/recording/round_*/output.mp4"),
            "annotations": one("logs/p14/runs/p14_final4_original_1_swir_clear/recording/round_*/producer_annotations.jsonl"),
            "elementary": root / "logs/p14/runs/p14_final4_original_1_swir_clear/received.h264",
            "reviewSeconds": [50, 60, 68, 70],
        },
        {
            "caseId": "p14_original_mwir",
            "evidenceClass": "HISTORICAL_CONTROLLED_ORIGINAL_1TXT",
            "video": one("logs/p14/runs/p14_final4_original_1_mwir_clear/recording/round_*/output.mp4"),
            "annotations": one("logs/p14/runs/p14_final4_original_1_mwir_clear/recording/round_*/producer_annotations.jsonl"),
            "elementary": root / "logs/p14/runs/p14_final4_original_1_mwir_clear/received.h264",
            "reviewSeconds": [50, 60, 68, 70],
        },
    ]

    results = [audit_case(case, output) for case in cases]
    for case, result in zip(cases, results):
        elementary = case.get("elementary")
        if elementary and elementary.exists():
            mp4_hashes = framemd5(case["video"])
            elementary_hashes = framemd5(elementary)
            result["postEncodeContainerComparison"] = {
                "scope": "decoded received H264 elementary stream versus decoded MP4 remux; not pre-encode raw",
                "elementary": str(elementary.resolve()),
                "elementarySha256": sha256(elementary),
                "mp4Frames": len(mp4_hashes),
                "elementaryFrames": len(elementary_hashes),
                "decodedFrameHashesEqual": mp4_hashes == elementary_hashes,
            }
        else:
            result["postEncodeContainerComparison"] = {
                "scope": "not available for this case",
                "decodedFrameHashesEqual": None,
            }

    report = {
        "schema": "HwaSimIR.P16.ReadOnlyFlickerAudit.1",
        "method": {
            "sameSourceOnly": True,
            "ordinaryFrameDifferenceTreatedAsDefect": False,
            "thresholds": {"textureResolvableBboxMinWidth": 20, "textureResolvableBboxMinHeight": 20},
            "limits": [
                "No existing clip was captured specifically for the user's texture-flicker report.",
                "No frame-aligned pre-encode raw image exists for these videos.",
                "Annotation, object motion, view change, H264 quantization and sub-pixel sampling remain confounders.",
                "Qt paint tail is a separate timing clue and is not assigned as a texture-flicker cause.",
            ],
        },
        "cases": results,
        "classification": {
            "fineTextureJumpsWithMotion": "NOT_ISOLATED_FROM_MOTION_OR_SAMPLING",
            "localSurfaceAlternates": "NO_REPEATABLE_SAME_SOURCE_SEGMENT_IDENTIFIED",
            "wholeFrameBrightnessSynchronous": "NO_REPEATABLE_ANOMALOUS_EVENT_IDENTIFIED",
            "videoStableWindowJumps": "NOT_TESTABLE_WITH_FRAME_ALIGNED_EXISTING_EVIDENCE",
            "preEncodeVersusPostDecode": "NOT_TESTABLE_PRE_ENCODE_RAW_MISSING",
            "postEncodeElementaryVersusMp4Remux": "TESTED_WHERE_AVAILABLE",
        },
        "reproductionStatus": "NO_REPRODUCER",
        "rootCauseStatus": "NOT_LOCATED",
        "productionFixStatus": "NOT_ATTEMPTED",
    }
    (output / "p16_flicker_audit.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({
        "result": report["reproductionStatus"],
        "rootCause": report["rootCauseStatus"],
        "cases": [{"caseId": r["caseId"], "resolvable": r["textureResolvableFrames20px"],
                   "comparable": r["sameBboxComparableFrames20px"], "interpretation": r["interpretation"]}
                  for r in results],
    }, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
