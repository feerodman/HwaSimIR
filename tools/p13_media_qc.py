#!/usr/bin/env python3
"""Independent media, identity, and clock audit for a P13 DDS replay case."""

from __future__ import annotations

import argparse
import configparser
import csv
import datetime as dt
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def one(root: Path, name: str) -> Path:
    matches = list(root.rglob(name))
    if len(matches) != 1:
        raise RuntimeError(f"expected exactly one {name}, found {len(matches)}")
    return matches[0]


def tool_path(name: str) -> str:
    found = shutil.which(name)
    if found:
        return found
    fallback = Path(r"F:\Programs\Panda3D-1.10.15-x64\bin") / f"{name}.exe"
    if fallback.is_file():
        return str(fallback)
    raise RuntimeError(f"cannot locate {name}")


def parse_runtime_metrics(text: str) -> list[dict]:
    result = []
    for match in re.finditer(r"\[RuntimeMetricsV2\]\s+(\{[^\r\n]+\})", text):
        try:
            result.append(json.loads(match.group(1)))
        except json.JSONDecodeError:
            pass
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("case_dir", type=Path)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--expected-frames", type=int, default=4318)
    args = parser.parse_args()

    case_dir = args.case_dir.resolve()
    input_path = args.input.resolve()
    expected = args.expected_frames
    expected_duration = expected / 60.0
    duration_tolerance = max(1.0, expected_duration * 0.01)
    plan_path = case_dir / "case_plan.json"
    plan = json.loads(plan_path.read_text(encoding="utf-8"))
    mp4 = one(case_dir / "recording", "output.mp4")
    status_path = one(case_dir / "recording", "recording_status.json")
    index_path = one(case_dir / "recording", "frame_index.jsonl")
    producer_path = one(case_dir / "recording", "producer_annotations.jsonl")
    received_h264 = case_dir / "received.h264"
    sender_text = "\n".join(
        (case_dir / name).read_text(encoding="utf-8", errors="replace")
        for name in ("sender.out.log", "sender.err.log")
    )
    receiver_text = "\n".join(
        (case_dir / name).read_text(encoding="utf-8", errors="replace")
        for name in ("receiver.out.log", "receiver.err.log")
    )
    board_text = (case_dir / "board.log").read_text(encoding="utf-8", errors="replace")

    probe_command = [
        tool_path("ffprobe"), "-v", "error", "-count_frames", "-select_streams", "v:0",
        "-show_entries",
        "stream=codec_name,width,height,avg_frame_rate,r_frame_rate,nb_frames,nb_read_frames,duration:format=duration,size,format_name",
        "-of", "json", str(mp4),
    ]
    probe = json.loads(subprocess.check_output(probe_command, text=True, encoding="utf-8"))
    stream = probe["streams"][0]
    fmt = probe["format"]
    full_decode = subprocess.run(
        [tool_path("ffmpeg"), "-v", "error", "-i", str(mp4), "-map", "0:v:0", "-f", "null", os.devnull],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", errors="replace",
    )
    status = json.loads(status_path.read_text(encoding="utf-8"))

    with input_path.open("r", encoding="utf-8-sig", newline="") as stream_in:
        reader = csv.reader(stream_in)
        header = next(reader)
        source_rows = [row for row in reader if row and any(value.strip() for value in row)]
    source_times = [float(row[0]) for row in source_rows]

    frame_rows = []
    with index_path.open("r", encoding="utf-8") as stream_in:
        for line in stream_in:
            if line.strip():
                frame_rows.append(json.loads(line))
    producer_by_seq: dict[int, dict] = {}
    with producer_path.open("r", encoding="utf-8") as stream_in:
        for line in stream_in:
            if line.strip():
                record = json.loads(line)
                producer_by_seq[int(record["sourceSeq"])] = record

    timing_by_seq: dict[int, dict[str, str]] = {}
    timing_pattern = re.compile(
        r"\[StimFrameTime\] sourceSeq=(\d+) rowIndex=(\d+) sourceLine=(\d+) "
        r"sourceTimeMs=([0-9.]+) sourceOffsetMs=([0-9.]+) simulationEpochMs=([0-9.]+) "
        r"sendWallMs=(\d+) nominalVideoPtsMs=([0-9.]+)"
    )
    for match in timing_pattern.finditer(sender_text):
        timing_by_seq[int(match.group(1))] = {
            "senderRowIndex": match.group(2), "senderSourceLine": match.group(3),
            "senderSourceTimeMs": match.group(4), "senderSourceOffsetMs": match.group(5),
            "senderSimulationEpochMs": match.group(6), "sendWallMs": match.group(7),
            "senderNominalVideoPtsMs": match.group(8),
        }

    ini = configparser.ConfigParser()
    ini.optionxform = str
    ini.read(case_dir / "DataDrivenTestQT.NetworkConfig.ini", encoding="utf-8-sig")
    utc_date = ini.get("Demo", "UtcDate")
    utc_hour = float(ini.get("Demo", "UtcHour"))
    base_date = dt.datetime.strptime(utc_date, "%Y-%m-%d").replace(tzinfo=dt.timezone.utc)
    simulation_base_ms = int(base_date.timestamp() * 1000 + utc_hour * 3600000)

    identity_path = case_dir / "frame_identity.csv"
    fields = [
        "sourceSeq", "sourceLine", "sourceTimeMs", "sourceOffsetMs",
        "expectedSimulationEpochMs", "producerSimulationEpochMs", "sendWallMs",
        "nominalVideoPtsMs", "producerPtsMs", "mp4PtsMs", "frameSeq",
        "width", "height", "receivedAuSha256", "annotationSha256",
    ]
    with identity_path.open("w", encoding="utf-8", newline="") as stream_out:
        writer = csv.DictWriter(stream_out, fieldnames=fields)
        writer.writeheader()
        for index, frame in enumerate(frame_rows):
            seq = int(frame["sourceSeq"])
            source_time = source_times[seq - 1]
            offset = source_time - source_times[0]
            producer = producer_by_seq.get(seq, {})
            sparse = timing_by_seq.get(seq, {})
            writer.writerow({
                "sourceSeq": seq,
                "sourceLine": seq + 1,
                "sourceTimeMs": f"{source_time:.3f}",
                "sourceOffsetMs": f"{offset:.3f}",
                "expectedSimulationEpochMs": f"{simulation_base_ms + offset:.3f}",
                "producerSimulationEpochMs": producer.get("simTimeMs", ""),
                "sendWallMs": sparse.get("sendWallMs", ""),
                "nominalVideoPtsMs": f"{index * 1000.0 / 60.0:.6f}",
                "producerPtsMs": frame.get("producerPtsMs", frame.get("ptsMs", "")),
                "mp4PtsMs": f"{int(frame.get('mp4PtsUs', 0)) / 1000.0:.3f}",
                "frameSeq": frame["frameSeq"], "width": frame["width"], "height": frame["height"],
                "receivedAuSha256": frame.get("receivedAuSha256", ""),
                "annotationSha256": frame.get("annotationSha256", ""),
            })

    source_seq = [int(row["sourceSeq"]) for row in frame_rows]
    frame_seq = [int(row["frameSeq"]) for row in frame_rows]
    producer_pts = [int(row["producerPtsMs"]) for row in frame_rows]
    metrics = parse_runtime_metrics(receiver_text)
    max_decode_errors = max((int(item.get("decodeErrors", 0)) for item in metrics), default=-1)
    max_dds_errors = max((int(item.get("ddsErrors", 0)) for item in metrics), default=-1)
    checks = {
        "input_sha_matches_plan": sha256(input_path) == plan["inputSha256"],
        "input_row_count": len(source_rows) == expected,
        "strict_parser_all_rows": bool(re.search(rf"\[StimInputParse\] result=ACCEPTED.*rows={expected}.*malformedRows=0 partialReplay=0", sender_text)),
        "sender_wrote_all_rows": bool(re.search(rf"\[StimFinal\].*successfulRealtimeWrites={expected}", sender_text)),
        "renderer_stop_status": "[StimStopLifecycle] phase=renderer_stop_status result=PASS" in sender_text,
        "dds_ack_drain": "[StimStopLifecycle] phase=dds_ack_drain result=PASS" in sender_text,
        "board_output_drain": bool(re.search(rf"\[OutputRoundDrain\] reason=stop round=1 targetFrames={expected} completedFrames={expected}", board_text)),
        "atmosphere_identity": (
            "[P14 AtmosphereIdentity] status=PASS" in board_text or
            "[P13 AtmosphereIdentity] status=PASS" in board_text
        ),
        "formal_m1": "[M1 PhysicsConfig] CompareOnly=0 EnableRuntime=1" in board_text,
        "normal_material_view_requested": int(plan["materialView"]) == 0,
        "video_status_auto_applied": bool(re.search(r"\[VideoStatus\] applied=1 .*width=800 height=800 fps=60", receiver_text)),
        "decode_errors_zero": max_decode_errors == 0,
        "dds_errors_zero": max_dds_errors == 0,
        "recording_complete": int(status["completeProducts"]) == expected,
        "recording_muxer_finalized": bool(status["muxerFinalized"]) and not bool(status["fileError"]),
        "recording_source_continuous": bool(status["sourceSeqContinuous"]),
        "frame_index_count": len(frame_rows) == expected,
        "frame_source_sequence_exact": source_seq == list(range(1, expected + 1)),
        "frame_sequence_exact": frame_seq == list(range(1, expected + 1)),
        "producer_pts_monotonic": all(a < b for a, b in zip(producer_pts, producer_pts[1:])),
        "mp4_h264": stream["codec_name"] == "h264",
        "mp4_800x800": int(stream["width"]) == 800 and int(stream["height"]) == 800,
        "mp4_frame_count": int(stream["nb_read_frames"]) == expected,
        "mp4_full_decode": full_decode.returncode == 0 and not full_decode.stderr.strip(),
        "mp4_duration_matches_accepted_rows": abs(float(fmt["duration"]) - expected_duration) <= duration_tolerance,
        "received_annexb_present": received_h264.is_file() and received_h264.stat().st_size > 0,
    }

    evidence_patterns = (
        "[P14 AtmosphereIdentity]", "[P13 AtmosphereIdentity]", "[M1 PhysicsConfig]", "[Stage6 FinalPipeline]",
        "[DisplayEffective]", "[Stage5 Plume]", "[Stage7 Weather]", "[OutputRoundDrain]",
    )
    evidence_lines = [line for line in board_text.splitlines() if any(p in line for p in evidence_patterns)]
    (case_dir / "display_atmosphere_weather_evidence.txt").write_text(
        "\n".join(evidence_lines) + "\n", encoding="utf-8"
    )

    stop_match = re.search(r"phase=renderer_stop_status result=PASS elapsedMs=(\d+)", sender_text)
    ack_match = re.search(r"phase=dds_ack_drain result=PASS elapsedMs=(\d+)", sender_text)
    final_match = re.search(r"\[StimFinal\].*successfulRealtimeWrites=(\d+) elapsedMs=(\d+)", sender_text)
    timeline = {
        "sourceClock": {"firstMs": source_times[0], "lastMs": source_times[-1], "durationMs": source_times[-1] - source_times[0]},
        "sendClock": {"writes": int(final_match.group(1)), "elapsedMs": int(final_match.group(2))} if final_match else None,
        "simulationClock": {"utcBaseMs": simulation_base_ms, "lastMs": simulation_base_ms + source_times[-1] - source_times[0]},
        "videoClock": {"firstProducerPtsMs": producer_pts[0], "lastProducerPtsMs": producer_pts[-1], "mp4DurationSec": float(fmt["duration"])},
        "stopLifecycle": {
            "rendererStopStatusElapsedMs": int(stop_match.group(1)) if stop_match else None,
            "ddsAckDrainElapsedMs": int(ack_match.group(1)) if ack_match else None,
            "recorderMuxerFinalized": bool(status["muxerFinalized"]),
        },
        "relationship": "source Time(ms) drives simulation epoch by source offset; accepted rows are paced independently at 60 Hz; encoder PTS is output-frame time; MP4 PTS is monotonic remux time",
    }
    (case_dir / "event_timeline.json").write_text(json.dumps(timeline, indent=2) + "\n", encoding="utf-8")

    keyframe_dir = case_dir / "keyframes"
    keyframe_dir.mkdir(exist_ok=True)
    ffmpeg = tool_path("ffmpeg")
    duration = float(fmt["duration"])
    keyframes = []
    for label, seconds in (("start", min(2.0, duration * 0.03)), ("middle", duration * 0.5), ("near_end", duration * 0.97)):
        output = keyframe_dir / f"{label}.png"
        subprocess.check_call([ffmpeg, "-y", "-loglevel", "error", "-ss", f"{seconds:.3f}", "-i", str(mp4), "-frames:v", "1", str(output)])
        keyframes.append({"label": label, "seconds": seconds, "path": str(output), "sha256": sha256(output)})

    report = {
        "schema": "hwasimir.p14.media-qc.v1" if str(plan.get("schema", "")).startswith("hwasimir.p14") else "hwasimir.p13.media-qc.v1",
        "case": plan["name"], "band": plan["band"], "weather": plan["weather"],
        "result": "PASS" if all(checks.values()) else "FAIL",
        "checks": checks,
        "probe": probe,
        "fullDecode": {"returnCode": full_decode.returncode, "stderr": full_decode.stderr.strip()},
        "expectedVideo": {
            "acceptedRows": expected,
            "fps": 60,
            "durationSec": expected_duration,
            "durationToleranceSec": duration_tolerance,
        },
        "receiverMetrics": {"maxDecodeErrors": max_decode_errors, "maxDdsErrors": max_dds_errors},
        "products": {
            "mp4": str(mp4), "mp4Sha256": sha256(mp4), "mp4Bytes": mp4.stat().st_size,
            "receivedH264": str(received_h264), "receivedH264Sha256": sha256(received_h264),
            "frameIndex": str(index_path), "frameIndexSha256": sha256(index_path),
            "frameIdentity": str(identity_path), "frameIdentitySha256": sha256(identity_path),
            "keyframes": keyframes,
        },
        "timeline": timeline,
        "calibrationStatus": "NOT_VERIFIED_CALIBRATION",
    }
    (case_dir / "media_qc.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"[MediaQC] result={report['result']} case={plan['name']} frames={len(frame_rows)} durationSec={float(fmt['duration']):.3f} mp4={mp4}")
    if report["result"] != "PASS":
        for name, passed in checks.items():
            if not passed:
                print(f"FAIL {name}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
