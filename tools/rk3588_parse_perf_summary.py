#!/usr/bin/env python3
import argparse
import csv
import re
from pathlib import Path


FIELDS = [
    "case",
    "outputFps",
    "renderFps",
    "udpFps",
    "pandaDoFrameMs",
    "renderMs",
    "sceneUpdateMs",
    "annotationMs",
    "annotationBBoxMs",
    "annotationOcclusionMs",
    "readbackMs",
    "frameCopyMs",
    "jpegMs",
    "tcpSendMs",
    "inputQueueDepth",
    "sourceSeqLag",
    "latencyAvgMs",
    "readbackMode",
    "quietPerfMode",
    "headlessWidth",
    "headlessHeight",
    "bboxFastMode",
]

KEY_VALUE_RE = re.compile(r"([A-Za-z][A-Za-z0-9_]*)=([^ \r\n]+)")
SIZE_RE = re.compile(r"(?:HeadlessSize|finalSensorSize|renderTexEffectiveSize)=([0-9]+)x([0-9]+)")


def parse_line(line, metrics):
    for key, value in KEY_VALUE_RE.findall(line):
        normalized = {
            "pandaDoFrameMs": "pandaDoFrameMs",
            "renderMs": "renderMs",
            "sceneUpdateMs": "sceneUpdateMs",
            "annotationMs": "annotationMs",
            "annotationBBoxMs": "annotationBBoxMs",
            "annotationOcclusionMs": "annotationOcclusionMs",
            "readbackMs": "readbackMs",
            "frameCopyMs": "frameCopyMs",
            "jpegMs": "jpegMs",
            "tcpSendMs": "tcpSendMs",
            "outputFps": "outputFps",
            "renderFps": "renderFps",
            "udpFps": "udpFps",
            "inputQueueDepth": "inputQueueDepth",
            "sourceSeqLag": "sourceSeqLag",
            "latencyAvgMs": "latencyAvgMs",
            "readbackMode": "readbackMode",
            "quietPerfMode": "quietPerfMode",
            "bboxFastMode": "bboxFastMode",
            "BBoxFastMode": "bboxFastMode",
        }.get(key)
        if normalized:
            metrics[normalized] = value.rstrip(",")

    size_match = SIZE_RE.search(line)
    if size_match:
        metrics["headlessWidth"] = size_match.group(1)
        metrics["headlessHeight"] = size_match.group(2)


def parse_case(case_dir):
    metrics = {field: "" for field in FIELDS}
    metrics["case"] = case_dir.name
    for log_name in ("hwa.out.log", "hwa.err.log"):
        log_path = case_dir / log_name
        if not log_path.exists():
            continue
        with log_path.open("r", encoding="utf-8", errors="ignore") as handle:
            for line in handle:
                if line.startswith(("[Perf]", "[RenderPerfProbe]", "[ScenePerfProbe]", "[Stage6 Capture]", "[TcpPerf]", "[SyncFrame]", "[RenderBackend]", "[RenderBackendConfig]", "[Stage6 FinalPipeline]", "[EffectiveRuntimeConfig]")):
                    parse_line(line, metrics)
    return metrics


def main():
    parser = argparse.ArgumentParser(description="Parse HwaSimIR RK3588 H5 performance logs into CSV.")
    parser.add_argument("log_root", help="Directory containing one subdirectory per A/B case.")
    parser.add_argument("-o", "--output", help="CSV output path. Defaults to stdout.")
    args = parser.parse_args()

    root = Path(args.log_root)
    case_dirs = [p for p in sorted(root.iterdir()) if p.is_dir()]
    rows = [parse_case(case_dir) for case_dir in case_dirs]

    if args.output:
        out = open(args.output, "w", newline="", encoding="utf-8")
    else:
        out = None
    try:
        handle = out if out is not None else __import__("sys").stdout
        writer = csv.DictWriter(handle, fieldnames=FIELDS)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)
    finally:
        if out is not None:
            out.close()


if __name__ == "__main__":
    main()
