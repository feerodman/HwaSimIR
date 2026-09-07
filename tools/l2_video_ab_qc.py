#!/usr/bin/env python3
"""Measure L2 target ROI off/on/off A/B from the recorded TCP consumer output."""

from __future__ import annotations

import argparse
import bisect
import json
import re
import shutil
import subprocess
from pathlib import Path


ACTIVE_RE = re.compile(
    r"\[L2 ActiveIlluminator\].*?sourceSeq=(\d+).*?protocolEnabled=(\d).*?"
    r"activeSensorWm2SrUm=([-+0-9.eE]+)"
)


def find_ffmpeg() -> Path:
    found = shutil.which("ffmpeg")
    if found:
        return Path(found)
    candidates = sorted(Path(".deps").glob("ffmpeg*/**/bin/ffmpeg.exe"))
    if candidates:
        return candidates[0]
    raise SystemExit("ffmpeg not found")


def mean(values: bytes) -> float:
    return sum(values) / len(values) if values else 0.0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--video", type=Path, required=True)
    parser.add_argument("--annotations", type=Path, required=True)
    parser.add_argument("--hwa-log", type=Path, required=True)
    parser.add_argument("--band", choices=("NIR", "MWIR"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    active_rows = []
    for match in ACTIVE_RE.finditer(args.hwa_log.read_text(encoding="utf-8", errors="replace")):
        active_rows.append((int(match.group(1)), int(match.group(2)), float(match.group(3))))
    if not active_rows:
        raise SystemExit("no L2 ActiveIlluminator rows found")
    active_rows.sort()
    active_seqs = [row[0] for row in active_rows]
    first_on = min((seq for seq, enabled, _ in active_rows if enabled), default=None)
    last_on = max((seq for seq, enabled, _ in active_rows if enabled), default=None)
    if first_on is None or last_on is None:
        raise SystemExit("protocol on interval not found")

    annotations = []
    for line in args.annotations.read_text(encoding="utf-8", errors="replace").splitlines():
        item = json.loads(line)
        if item.get("targets"):
            annotations.append(item)
    if not annotations:
        raise SystemExit("target annotations are empty")
    width = int(annotations[0]["width"])
    height = int(annotations[0]["height"])
    by_frame = {int(item["recordingFrameIndex"]) - 1: item for item in annotations}

    ffmpeg = find_ffmpeg()
    process = subprocess.Popen(
        [str(ffmpeg), "-v", "error", "-i", str(args.video), "-f", "rawvideo", "-pix_fmt", "gray", "-"],
        stdout=subprocess.PIPE,
    )
    assert process.stdout is not None
    frame_size = width * height
    accum = {
        name: {"frames": 0, "roiSum": 0.0, "surroundSum": 0.0, "black": 0, "white": 0,
               "pixels": 0, "activeSum": 0.0, "frameIndices": []}
        for name in ("off_before", "on", "off_after")
    }
    frame_index = 0
    while True:
        frame = process.stdout.read(frame_size)
        if not frame:
            break
        if len(frame) != frame_size:
            raise SystemExit("truncated decoded frame")
        annotation = by_frame.get(frame_index)
        if annotation:
            source_seq = int(annotation["sourceSeq"])
            row_index = bisect.bisect_right(active_seqs, source_seq) - 1
            if row_index >= 0:
                _, protocol_enabled, active_radiance = active_rows[row_index]
                state = "on" if protocol_enabled else ("off_before" if source_seq < first_on else "off_after")
                corners = annotation["targets"][0]["bboxCorners"]
                x0 = max(0, min(int(point["x"]) for point in corners))
                x1 = min(width - 1, max(int(point["x"]) for point in corners))
                y0 = max(0, min(int(point["y"]) for point in corners))
                y1 = min(height - 1, max(int(point["y"]) for point in corners))
                pad_x = max(2, (x1 - x0 + 1) // 2)
                pad_y = max(2, (y1 - y0 + 1) // 2)
                sx0, sx1 = max(0, x0 - pad_x), min(width - 1, x1 + pad_x)
                sy0, sy1 = max(0, y0 - pad_y), min(height - 1, y1 + pad_y)
                roi = bytearray()
                surround = bytearray()
                for y in range(sy0, sy1 + 1):
                    row = frame[y * width:(y + 1) * width]
                    for x in range(sx0, sx1 + 1):
                        if x0 <= x <= x1 and y0 <= y <= y1:
                            roi.append(row[x])
                        else:
                            surround.append(row[x])
                target = accum[state]
                target["frames"] += 1
                target["roiSum"] += mean(roi)
                target["surroundSum"] += mean(surround)
                target["black"] += sum(value <= 1 for value in frame)
                target["white"] += sum(value >= 254 for value in frame)
                target["pixels"] += frame_size
                target["activeSum"] += active_radiance
                target["frameIndices"].append(frame_index)
        frame_index += 1
    if process.wait() != 0:
        raise SystemExit("ffmpeg decode failed")

    result = {"band": args.band, "video": str(args.video.resolve()), "displayParametersChanged": False,
              "states": {}}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    for state, values in accum.items():
        count = values["frames"]
        if count == 0:
            raise SystemExit(f"no frames classified as {state}")
        roi_mean = values["roiSum"] / count
        surround_mean = values["surroundSum"] / count
        representative = values["frameIndices"][len(values["frameIndices"]) // 2]
        png = args.output.with_name(args.output.stem + f"_{state}.png")
        subprocess.check_call([
            str(ffmpeg), "-v", "error", "-i", str(args.video), "-vf", f"select=eq(n\\,{representative})",
            "-frames:v", "1", "-y", str(png)
        ])
        result["states"][state] = {
            "frames": count,
            "targetRoiMeanGray": roi_mean,
            "surroundMeanGray": surround_mean,
            "roiContrastGray": roi_mean - surround_mean,
            "activeSensorRadianceMeanWm2SrUm": values["activeSum"] / count,
            "blackRatioGrayLe1": values["black"] / values["pixels"],
            "whiteRatioGrayGe254": values["white"] / values["pixels"],
            "representativeFrameIndex": representative,
            "image": str(png.resolve()),
        }
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
