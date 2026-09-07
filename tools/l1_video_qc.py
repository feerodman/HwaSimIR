#!/usr/bin/env python3
"""Measure L1 runtime video saturation without changing display parameters."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
from pathlib import Path


def find_ffmpeg(explicit):
    if explicit:
        return explicit
    found = shutil.which("ffmpeg")
    if found:
        return Path(found)
    candidates = sorted(Path(".deps").glob("ffmpeg*/**/bin/ffmpeg.exe"))
    if candidates:
        return candidates[0]
    raise SystemExit("ffmpeg not found; pass --ffmpeg")


def probe(ffmpeg, video):
    ffprobe = ffmpeg.with_name("ffprobe.exe" if ffmpeg.suffix.lower() == ".exe" else "ffprobe")
    command = [str(ffprobe), "-v", "error", "-select_streams", "v:0",
               "-show_entries", "stream=width,height,nb_frames,avg_frame_rate",
               "-of", "json", str(video)]
    data = json.loads(subprocess.check_output(command, text=True))
    stream = data["streams"][0]
    numerator, denominator = stream["avg_frame_rate"].split("/")
    return int(stream["width"]), int(stream["height"]), int(stream.get("nb_frames", 0)), float(numerator) / float(denominator)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("video", type=Path)
    parser.add_argument("--band", required=True, choices=("NIR", "MWIR"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sample-every", type=int, default=10)
    parser.add_argument("--ffmpeg", type=Path)
    args = parser.parse_args()

    ffmpeg = find_ffmpeg(args.ffmpeg)
    width, height, reported_frames, fps = probe(ffmpeg, args.video)
    step = max(1, args.sample_every)
    filter_graph = f"select=not(mod(n\\,{step})),format=gray"
    command = [str(ffmpeg), "-v", "error", "-i", str(args.video), "-vf", filter_graph,
               "-vsync", "0", "-f", "rawvideo", "-pix_fmt", "gray", "-"]
    process = subprocess.Popen(command, stdout=subprocess.PIPE)
    assert process.stdout is not None
    frame_bytes = width * height
    sampled = pixel_count = black_count = white_count = near_black_count = near_white_count = 0
    sum_value = sum_square = roi_sum = roi_square = 0
    roi_count = 0
    x0, x1, y0, y1 = width // 3, (2 * width) // 3, height // 3, (2 * height) // 3
    while True:
        raw = process.stdout.read(frame_bytes)
        if not raw:
            break
        if len(raw) != frame_bytes:
            raise SystemExit("truncated raw frame from ffmpeg")
        sampled += 1
        pixel_count += frame_bytes
        black_count += sum(value <= 1 for value in raw)
        white_count += sum(value >= 254 for value in raw)
        near_black_count += sum(value <= 5 for value in raw)
        near_white_count += sum(value >= 250 for value in raw)
        sum_value += sum(raw)
        sum_square += sum(value * value for value in raw)
        for y in range(y0, y1):
            row = raw[y * width + x0:y * width + x1]
            roi_count += len(row)
            roi_sum += sum(row)
            roi_square += sum(value * value for value in row)
    if process.wait() != 0 or pixel_count == 0:
        raise SystemExit("ffmpeg frame decode failed")

    mean = sum_value / pixel_count
    variance = max(0.0, sum_square / pixel_count - mean * mean)
    roi_mean = roi_sum / roi_count
    roi_variance = max(0.0, roi_square / roi_count - roi_mean * roi_mean)
    result = {
        "band": args.band,
        "video": str(args.video.resolve()),
        "reportedFrameCount": reported_frames,
        "fps": fps,
        "sampledFrames": sampled,
        "grayMean": mean,
        "grayStd": variance ** 0.5,
        "blackRatioGrayLe1": black_count / pixel_count,
        "whiteRatioGrayGe254": white_count / pixel_count,
        "nearBlackRatioGrayLe5": near_black_count / pixel_count,
        "nearWhiteRatioGrayGe250": near_white_count / pixel_count,
        "centerRoiMean": roi_mean,
        "centerRoiStd": roi_variance ** 0.5,
        "displayParametersChanged": False,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    middle_sec = reported_frames / max(1.0, fps) / 2.0
    png = args.output.with_suffix(".png")
    subprocess.check_call([str(ffmpeg), "-v", "error", "-ss", f"{middle_sec:.3f}", "-i", str(args.video),
                           "-frames:v", "1", "-y", str(png)])
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
