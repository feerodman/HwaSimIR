#!/usr/bin/env python3
"""Hard-gate one Windows P11 60 s H.264 acceptance capture.

The input directory is produced by ``p11_windows_h264_60s_acceptance.ps1`` and
contains the untouched phase2a logs plus the receiver recording directory.
This analyzer never creates image evidence.  It checks the production ledgers,
independently decodes the MP4, and preserves every >80 ms frame in CSV form.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import re
import subprocess
from pathlib import Path
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_FFMPEG = ROOT / ".deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe"
EMPTY_SHA256 = hashlib.sha256(b"").hexdigest()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig", errors="replace") if path.is_file() else ""


def parse_fields(line: str) -> dict[str, object]:
    fields: dict[str, object] = {}
    for key, value in re.findall(r"\b([A-Za-z][A-Za-z0-9_]*)=([^\s]+)", line):
        try:
            numeric = float(value)
            fields[key] = int(numeric) if numeric.is_integer() else numeric
        except ValueError:
            fields[key] = value
    return fields


def tagged(tag: str, text: str) -> list[dict[str, object]]:
    prefix = f"[{tag}]"
    return [parse_fields(line) for line in text.splitlines() if line.startswith(prefix)]


def as_int(value: object) -> int | None:
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, int):
        return value
    if isinstance(value, float) and value.is_integer():
        return int(value)
    if isinstance(value, str) and re.fullmatch(r"[+-]?\d+", value.strip()):
        return int(value)
    return None


def as_bool(value: object) -> bool | None:
    if isinstance(value, bool):
        return value
    if value in (0, 1):
        return bool(value)
    if isinstance(value, str):
        normalized = value.strip().lower()
        if normalized in ("1", "true", "yes", "on"):
            return True
        if normalized in ("0", "false", "no", "off"):
            return False
    return None


def maximum(rows: Iterable[dict[str, object]], key: str) -> int | None:
    values = [as_int(row.get(key)) for row in rows]
    exact = [value for value in values if value is not None]
    return max(exact) if exact else None


def percentile(values: list[float], p: float) -> float | None:
    ordered = sorted(values)
    if not ordered:
        return None
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * p
    low = math.floor(position)
    high = math.ceil(position)
    weight = position - low
    return ordered[low] * (1.0 - weight) + ordered[high] * weight


def metric_stats(values: list[float]) -> dict[str, float | int | None]:
    return {
        "count": len(values),
        "minMs": min(values) if values else None,
        "meanMs": sum(values) / len(values) if values else None,
        "p50Ms": percentile(values, 0.50),
        "p95Ms": percentile(values, 0.95),
        "p99Ms": percentile(values, 0.99),
        "maxMs": max(values) if values else None,
    }


def load_jsonl(path: Path, errors: list[str], label: str) -> list[dict[str, Any]]:
    if not path.is_file():
        errors.append(f"missing:{label}:{path}")
        return []
    rows: list[dict[str, Any]] = []
    for line_number, line in enumerate(read_text(path).splitlines(), 1):
        if not line.strip():
            continue
        try:
            row = json.loads(line)
        except json.JSONDecodeError as exc:
            errors.append(f"invalid_json:{label}:{line_number}:{exc.msg}")
            continue
        if not isinstance(row, dict):
            errors.append(f"not_object:{label}:{line_number}")
            continue
        rows.append(row)
    return rows


def read_annotation_body(
    body_path: Path, row: dict[str, Any], errors: list[str], ordinal: int
) -> dict[str, Any] | None:
    offset = as_int(row.get("annotationBodyOffset"))
    size = as_int(row.get("annotationBodyBytes"))
    expected_hash = row.get("annotationBodySha256")
    if offset is None or size is None or offset < 0 or size <= 0:
        errors.append(f"annotation_reference_invalid:{ordinal}")
        return None
    try:
        with body_path.open("rb") as stream:
            stream.seek(offset)
            payload = stream.read(size)
    except OSError as exc:
        errors.append(f"annotation_read_failed:{ordinal}:{exc}")
        return None
    if len(payload) != size:
        errors.append(f"annotation_short_read:{ordinal}:{len(payload)}!={size}")
        return None
    actual_hash = hashlib.sha256(payload).hexdigest()
    if actual_hash != expected_hash:
        errors.append(f"annotation_hash_mismatch:{ordinal}")
        return None
    try:
        value = json.loads(payload.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        errors.append(f"annotation_decode_failed:{ordinal}:{exc}")
        return None
    if not isinstance(value, dict):
        errors.append(f"annotation_not_object:{ordinal}")
        return None
    return value


def rational(value: object) -> float | None:
    if not isinstance(value, str):
        return None
    try:
        numerator, denominator = value.split("/", 1)
        denominator_value = float(denominator)
        return float(numerator) / denominator_value if denominator_value else None
    except (ValueError, ZeroDivisionError):
        return None


def run_json(command: list[str], errors: list[str], label: str) -> dict[str, Any]:
    completed = subprocess.run(command, text=True, capture_output=True, encoding="utf-8", errors="replace")
    if completed.returncode != 0:
        errors.append(f"{label}_failed:exit={completed.returncode}:{completed.stderr.strip()}")
        return {}
    try:
        value = json.loads(completed.stdout)
    except json.JSONDecodeError as exc:
        errors.append(f"{label}_invalid_json:{exc.msg}")
        return {}
    return value if isinstance(value, dict) else {}


def analyze(args: argparse.Namespace) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    evidence_dir = args.evidence_dir.resolve()
    formal_dir = evidence_dir / "formal_tcp_run"
    recording_dir = evidence_dir / "receiver_recording"
    errors: list[str] = []

    summary_path = formal_dir / "phase2a_sync60_save_summary.json"
    try:
        summary = json.loads(read_text(summary_path))
    except json.JSONDecodeError as exc:
        errors.append(f"summary_invalid:{exc.msg}")
        summary = {}
    if not isinstance(summary, dict):
        errors.append("summary_not_object")
        summary = {}

    hwa_text = read_text(formal_dir / "hwa.out.log") + "\n" + read_text(formal_dir / "hwa.err.log")
    video_text = read_text(formal_dir / "video.out.log") + "\n" + read_text(formal_dir / "video.err.log")
    stim_text = read_text(formal_dir / "stim.out.log") + "\n" + read_text(formal_dir / "stim.err.log")
    for name, text in (("hwa", hwa_text), ("video", video_text), ("stim", stim_text)):
        if not text.strip():
            errors.append(f"log_missing_or_empty:{name}")

    index_path = recording_dir / "frame_index.jsonl"
    body_path = recording_dir / "producer_annotations.jsonl"
    index_rows = load_jsonl(index_path, errors, "frame_index")
    if not body_path.is_file():
        errors.append(f"missing:producer_annotations:{body_path}")

    annotations: list[dict[str, Any]] = []
    associations: set[str] = set()
    source_values: list[int] = []
    frame_values: list[int] = []
    storage_values: list[int] = []
    mp4_pts_values: list[int] = []
    for ordinal, row in enumerate(index_rows, 1):
        source_seq = as_int(row.get("sourceSeq"))
        frame_seq = as_int(row.get("frameSeq"))
        storage_index = as_int(row.get("storageIndex"))
        mp4_pts = as_int(row.get("mp4PtsUs"))
        if None in (source_seq, frame_seq, storage_index, mp4_pts):
            errors.append(f"frame_index_identity_missing:{ordinal}")
            continue
        source_values.append(source_seq)
        frame_values.append(frame_seq)
        storage_values.append(storage_index)
        mp4_pts_values.append(mp4_pts)
        if (source_seq, frame_seq, storage_index) != (ordinal, ordinal, ordinal):
            errors.append(
                f"frame_identity_not_one_to_one:{ordinal}:source={source_seq}:frame={frame_seq}:storage={storage_index}"
            )
        if as_int(row.get("width")) != 800 or as_int(row.get("height")) != 800:
            errors.append(f"receiver_geometry_not_800x800:{ordinal}")
        if row.get("storageCodec") != "received_h264_remux":
            errors.append(f"not_received_h264_remux:{ordinal}:{row.get('storageCodec')}")
        received_hash = row.get("receivedAuSha256")
        if not isinstance(received_hash, str) or received_hash == EMPTY_SHA256:
            errors.append(f"empty_received_h264_access_unit:{ordinal}")
        associations.add(str(row.get("association", "")))
        if body_path.is_file():
            annotation = read_annotation_body(body_path, row, errors, ordinal)
            if annotation is not None:
                annotations.append(annotation)
                for key, expected in (
                    ("sourceSeq", source_seq), ("frameSeq", frame_seq), ("outputOrdinal", ordinal)
                ):
                    if as_int(annotation.get(key)) != expected:
                        errors.append(f"producer_identity_mismatch:{ordinal}:{key}")

    for label, values in (
        ("sourceSeq", source_values), ("frameSeq", frame_values), ("storageIndex", storage_values)
    ):
        if not values or values != list(range(1, len(values) + 1)):
            errors.append(f"not_continuous_from_one:{label}")
    if any(b <= a for a, b in zip(mp4_pts_values, mp4_pts_values[1:])):
        errors.append("mp4_pts_not_strictly_increasing")
    if associations != {"TCP_PACKET_V3_JSON"}:
        errors.append(f"unexpected_association:{sorted(associations)}")

    keyframe_sources: list[int] = []
    producer_pts_ms: list[int] = []
    for ordinal, annotation in enumerate(annotations, 1):
        if annotation.get("requestedCodec") != "h264":
            errors.append(f"producer_requested_codec_not_h264:{ordinal}")
        if annotation.get("activeCodec") != "h264_annexb" or annotation.get("payloadCodec") != "h264_annexb":
            errors.append(f"producer_active_codec_not_h264_annexb:{ordinal}")
        if as_bool(annotation.get("h264En")) is not True:
            errors.append(f"producer_h264_not_enabled:{ordinal}")
        if annotation.get("codecFallbackReason") not in (None, "", "none"):
            errors.append(f"producer_codec_fallback:{ordinal}:{annotation.get('codecFallbackReason')}")
        encoder_name = str(annotation.get("encoderName", ""))
        if not encoder_name.startswith("ffmpeg/") or "unavailable" in encoder_name:
            errors.append(f"producer_not_ffmpeg_encoder:{ordinal}:{encoder_name}")
        if as_int(annotation.get("width")) != 800 or as_int(annotation.get("height")) != 800:
            errors.append(f"producer_geometry_not_800x800:{ordinal}")
        source_seq = as_int(annotation.get("sourceSeq"))
        pts_ms = as_int(annotation.get("ptsMs"))
        if pts_ms is not None:
            producer_pts_ms.append(pts_ms)
        if as_bool(annotation.get("keyFrame")) is True and source_seq is not None:
            keyframe_sources.append(source_seq)

    configured_gop = as_int(summary.get("h264GopFrames")) or 30
    if not keyframe_sources or keyframe_sources[0] != 1:
        errors.append("first_h264_keyframe_missing")
    if len(keyframe_sources) >= 2:
        maximum_keyframe_gap = max(b - a for a, b in zip(keyframe_sources, keyframe_sources[1:]))
        if maximum_keyframe_gap > configured_gop + 1:
            errors.append(f"h264_keyframe_gap_exceeded:{maximum_keyframe_gap}>{configured_gop + 1}")
    else:
        maximum_keyframe_gap = None

    stim_final = tagged("StimFinal", stim_text)
    conservation = tagged("SyncRoundConservation", hwa_text)
    ingress = tagged("RealtimeIngress", hwa_text)
    perf = tagged("Perf", hwa_text)
    drains = [row for row in tagged("OutputRoundDrain", hwa_text) if row.get("reason") == "stop"]
    flushes = [row for row in tagged("RecorderFlush", video_text) if row.get("reason") == "close"]
    stop_results = tagged("ControlStopResult", hwa_text)
    video_perf = tagged("VideoPerf", video_text)

    accepted = maximum(conservation, "acceptedRealtime")
    counts: dict[str, int | None] = {
        "sent": maximum(stim_final, "successfulRealtimeWrites"),
        "accepted": accepted,
        "queued": maximum(ingress, "appRealtimeQueued"),
        "executed": maximum(ingress, "appRealtimeConsumed"),
        "captured": maximum(conservation, "lastCapturedSourceSeq"),
        "rendered": maximum(perf, "renderFrames"),
        "output": maximum(perf, "outputFrames"),
        "receiver": maximum(flushes, "inputFrames"),
        "written": maximum(flushes, "writtenFrames"),
        "receiverLastFrameSeq": maximum(flushes, "frameSeqWritten"),
        "receiverLastSourceSeq": maximum(flushes, "sourceSeqWritten"),
        "frameIndex": len(index_rows),
        "producerAnnotations": len(annotations),
        "mp4": None,
    }

    movie = recording_dir / "output.mp4"
    ffmpeg = args.ffmpeg.resolve()
    ffprobe = args.ffprobe.resolve()
    probe: dict[str, Any] = {}
    independent_decode = {"returnCode": None, "errors": "", "pass": False}
    probe_keyframes = 0
    if not movie.is_file():
        errors.append(f"mp4_missing:{movie}")
    if not ffmpeg.is_file() or not ffprobe.is_file():
        errors.append("ffmpeg_or_ffprobe_missing")
    elif movie.is_file():
        probe_json = run_json([
            str(ffprobe), "-v", "error", "-select_streams", "v:0", "-count_frames",
            "-show_entries", "stream=codec_name,width,height,pix_fmt,avg_frame_rate,r_frame_rate,nb_read_frames,duration",
            "-of", "json", str(movie),
        ], errors, "ffprobe_stream")
        streams = probe_json.get("streams", []) if isinstance(probe_json, dict) else []
        if len(streams) != 1 or not isinstance(streams[0], dict):
            errors.append(f"ffprobe_video_stream_count:{len(streams)}")
            stream = {}
        else:
            stream = streams[0]
        mp4_frames = as_int(stream.get("nb_read_frames"))
        counts["mp4"] = mp4_frames
        probe = {
            "codec": stream.get("codec_name"),
            "width": as_int(stream.get("width")),
            "height": as_int(stream.get("height")),
            "pixelFormat": stream.get("pix_fmt"),
            "avgFrameRate": rational(stream.get("avg_frame_rate")),
            "realFrameRate": rational(stream.get("r_frame_rate")),
            "frames": mp4_frames,
            "durationSec": float(stream["duration"]) if stream.get("duration") not in (None, "N/A") else None,
            "sha256": sha256(movie),
        }
        if probe["codec"] != "h264":
            errors.append(f"mp4_codec_not_h264:{probe['codec']}")
        if probe["width"] != 800 or probe["height"] != 800:
            errors.append(f"mp4_geometry_not_800x800:{probe['width']}x{probe['height']}")
        avg_fps = probe.get("avgFrameRate")
        if not isinstance(avg_fps, (int, float)) or abs(float(avg_fps) - args.expected_fps) > args.fps_tolerance:
            errors.append(f"mp4_fps_outside_tolerance:{avg_fps}")
        duration = probe.get("durationSec")
        if not isinstance(duration, (int, float)) or abs(float(duration) - args.expected_seconds) > args.duration_tolerance:
            errors.append(f"mp4_duration_outside_tolerance:{duration}")

        key_json = run_json([
            str(ffprobe), "-v", "error", "-skip_frame", "nokey", "-select_streams", "v:0",
            "-show_entries", "frame=key_frame", "-of", "json", str(movie),
        ], errors, "ffprobe_keyframes")
        probe_frames = key_json.get("frames", []) if isinstance(key_json, dict) else []
        probe_keyframes = sum(
            1 for row in probe_frames if isinstance(row, dict) and as_int(row.get("key_frame")) == 1
        )
        if probe_keyframes <= 0:
            errors.append("ffprobe_keyframe_missing")

        decoded = subprocess.run(
            [str(ffmpeg), "-v", "error", "-i", str(movie), "-f", "null", "-"],
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True,
            encoding="utf-8", errors="replace",
        )
        independent_decode = {
            "returnCode": decoded.returncode,
            "errors": decoded.stderr.strip(),
            "pass": decoded.returncode == 0 and not decoded.stderr.strip(),
        }
        if not independent_decode["pass"]:
            errors.append(f"independent_decode_errors:exit={decoded.returncode}")

    if accepted is None or accepted <= 0:
        errors.append("accepted_count_missing_or_zero")
    else:
        for name, value in counts.items():
            if value != accepted:
                errors.append(f"count_mismatch:{name}:{value}!={accepted}")

    expected_protocol_band = {"SWIR": 0, "NIR": 1, "MWIR": 2}[args.band]
    if as_int(summary.get("stimSensorBand")) != expected_protocol_band:
        errors.append(f"sensor_band_mismatch:{summary.get('stimSensorBand')}!={expected_protocol_band}")
    if str(summary.get("requestedCodec", "")) != "h264":
        errors.append(f"summary_requested_codec_not_h264:{summary.get('requestedCodec')}")
    if str(summary.get("activeCodec", "")) != "h264_annexb":
        errors.append(f"summary_active_codec_not_h264:{summary.get('activeCodec')}")
    if str(summary.get("decodeCodec", "")) != "h264_annexb":
        errors.append(f"summary_decode_codec_not_h264:{summary.get('decodeCodec')}")
    if str(summary.get("codecFallbackReason", "")) != "none":
        errors.append(f"summary_codec_fallback:{summary.get('codecFallbackReason')}")
    if as_int(summary.get("h264KeyFrameSeen")) != 1:
        errors.append("summary_h264_keyframe_not_seen")
    if as_int(summary.get("h264DecodeErrors")) != 0:
        errors.append(f"summary_h264_decode_errors:{summary.get('h264DecodeErrors')}")
    if maximum(video_perf, "h264DecodeErrors") != 0:
        errors.append(f"video_h264_decode_errors:{maximum(video_perf, 'h264DecodeErrors')}")
    if not any(
        as_int(row.get("configured")) == 1
        and row.get("codec") == "h264_annexb"
        and "ffmpeg" in str(row.get("decoderName", "")).lower()
        for row in tagged("VideoDecoder", video_text)
    ):
        errors.append("ffmpeg_h264_decoder_configuration_missing")
    if "[H264DecodeSuccess]" not in video_text:
        errors.append("h264_decode_success_missing")
    if "decodeFailed=1" in video_text or "parseFailed=1" in video_text:
        errors.append("receiver_decode_or_packet_error_logged")

    stop_row = stop_results[-1] if stop_results else {}
    drain_row = drains[-1] if drains else {}
    flush_row = flushes[-1] if flushes else {}
    control = {
        "initAckObserved": "[StimInitAck]" in stim_text and "received=1" in stim_text,
        "initAckWaitMs": summary.get("stimInitAckWaitMs"),
        "stopTotalMs": stop_row.get("stopTotalMs", summary.get("stopTotalMs")),
        "stopAcceptance": stop_row.get("acceptance"),
        "outputDrainTarget": as_int(drain_row.get("targetFrames")),
        "outputDrainCompleted": as_int(drain_row.get("completedFrames")),
        "outputDrainQueueDepth": as_int(drain_row.get("queueDepth")),
        "recorderFlushCompleted": as_int(flush_row.get("completed")),
        "recorderFlushQueueDepth": as_int(flush_row.get("queueDepth")),
    }
    if not control["initAckObserved"]:
        errors.append("init_ack_missing")
    stop_ms = control["stopTotalMs"]
    if not isinstance(stop_ms, (int, float)) or float(stop_ms) > 80.0:
        errors.append(f"stop_control_response_over_80ms_or_missing:{stop_ms}")
    if stop_row.get("acceptance") != "pass" or as_int(stop_row.get("outputDrainOk")) != 1:
        errors.append("stop_or_output_drain_not_accepted")
    if accepted is not None and (
        control["outputDrainTarget"] != accepted or control["outputDrainCompleted"] != accepted
    ):
        errors.append("output_drain_count_mismatch")
    if control["recorderFlushCompleted"] != 1 or control["recorderFlushQueueDepth"] != 0:
        errors.append("recorder_flush_incomplete")

    status_path = recording_dir / "recording_status.json"
    try:
        recording_status = json.loads(read_text(status_path))
    except json.JSONDecodeError as exc:
        errors.append(f"recording_status_invalid:{exc.msg}")
        recording_status = {}
    if not isinstance(recording_status, dict):
        recording_status = {}
    if as_bool(recording_status.get("muxerFinalized")) is not True:
        errors.append("mp4_muxer_not_finalized")
    if as_bool(recording_status.get("fileError")) is not False:
        errors.append(f"recording_file_error:{recording_status.get('fileError')}")
    if as_int(recording_status.get("completeProducts")) != accepted:
        errors.append("recording_status_count_mismatch")

    per_frame: list[dict[str, Any]] = []
    invalid_latency: list[dict[str, Any]] = []
    for ordinal, (row, annotation) in enumerate(zip(index_rows, annotations), 1):
        try:
            input_ns = int(annotation["udpReceiveTimeNs"])
            send_ns = int(annotation["tcpSendTimeNs"])
            receive_ns = int(row["receiveTimeNs"])
            display_ns = int(row["displayTimeNs"])
        except (KeyError, TypeError, ValueError) as exc:
            invalid_latency.append({"ordinal": ordinal, "reason": str(exc)})
            continue
        intervals = {
            "producerInputToSendMs": (send_ns - input_ns) / 1_000_000.0,
            "transportMs": (receive_ns - send_ns) / 1_000_000.0,
            "receiverToDisplayMs": (display_ns - receive_ns) / 1_000_000.0,
            "inputToDisplayMs": (display_ns - input_ns) / 1_000_000.0,
        }
        if any(value < 0.0 or value > 60_000.0 for value in intervals.values()):
            invalid_latency.append({"ordinal": ordinal, "reason": "interval_outside_0_to_60000_ms", **intervals})
            continue
        per_frame.append({
            "sourceSeq": as_int(row.get("sourceSeq")),
            "frameSeq": as_int(row.get("frameSeq")),
            "association": row.get("association"),
            **{key: round(value, 6) for key, value in intervals.items()},
            "overThreshold": intervals["inputToDisplayMs"] > args.latency_threshold_ms,
        })
    if invalid_latency or len(per_frame) != len(index_rows):
        errors.append(f"latency_rows_invalid:{len(invalid_latency)}")

    latency_metrics = {
        name: metric_stats([float(row[name]) for row in per_frame])
        for name in ("producerInputToSendMs", "transportMs", "receiverToDisplayMs", "inputToDisplayMs")
    }
    over = [row for row in per_frame if row["overThreshold"]]

    observed_fps = None
    observed_duration_ms = None
    if len(producer_pts_ms) >= 2:
        observed_duration_ms = producer_pts_ms[-1] - producer_pts_ms[0]
        if observed_duration_ms > 0:
            observed_fps = (len(producer_pts_ms) - 1) * 1000.0 / observed_duration_ms
    if observed_fps is None or abs(observed_fps - args.expected_fps) > args.fps_tolerance:
        errors.append(f"producer_fps_outside_tolerance:{observed_fps}")
    if observed_duration_ms is None or abs(observed_duration_ms / 1000.0 - args.expected_seconds) > args.duration_tolerance:
        errors.append(f"producer_duration_outside_tolerance:{observed_duration_ms}")

    preflight: dict[str, Any] = {}
    preflight_path = evidence_dir.parent / "binary_preflight.json"
    if preflight_path.is_file():
        try:
            preflight = json.loads(read_text(preflight_path))
        except json.JSONDecodeError as exc:
            errors.append(f"binary_preflight_invalid:{exc.msg}")
        if preflight.get("result") != "PASS":
            errors.append(f"binary_preflight_not_pass:{preflight.get('result')}")
    else:
        errors.append("binary_preflight_missing")

    unique_errors = sorted(set(errors))
    result: dict[str, Any] = {
        "schema": "hwasimir_p11_windows_h264_acceptance_1",
        "result": "PASS" if not unique_errors else "FAIL",
        "band": args.band,
        "expected": {
            "seconds": args.expected_seconds, "fps": args.expected_fps,
            "fpsTolerance": args.fps_tolerance, "durationToleranceSec": args.duration_tolerance,
            "resolution": "800x800", "codec": "h264", "encoder": "ffmpeg",
            "latencyReportThresholdMs": args.latency_threshold_ms,
        },
        "counts": counts,
        "countConservation": accepted is not None and all(value == accepted for value in counts.values()),
        "identity": {
            "sourceSeqContinuousFromOne": source_values == list(range(1, len(source_values) + 1)),
            "frameSeqContinuousFromOne": frame_values == list(range(1, len(frame_values) + 1)),
            "sourceFrameStorageElementwiseEqual": all(
                source == frame == storage
                for source, frame, storage in zip(source_values, frame_values, storage_values)
            ),
            "associations": sorted(associations),
        },
        "codec": {
            "summaryRequested": summary.get("requestedCodec"),
            "summaryActive": summary.get("activeCodec"),
            "summaryDecoded": summary.get("decodeCodec"),
            "encoderName": summary.get("h264EncoderName"),
            "fallbackReason": summary.get("codecFallbackReason"),
            "decodeErrors": summary.get("h264DecodeErrors"),
            "producerKeyFrames": len(keyframe_sources),
            "firstProducerKeyFrameSourceSeq": keyframe_sources[0] if keyframe_sources else None,
            "lastProducerKeyFrameSourceSeq": keyframe_sources[-1] if keyframe_sources else None,
            "maximumProducerKeyFrameGap": maximum_keyframe_gap,
            "ffprobeKeyFrames": probe_keyframes,
        },
        "rate": {
            "producerDurationMs": observed_duration_ms,
            "producerFps": observed_fps,
            "mp4": probe,
        },
        "controlAndDrain": control,
        "recordingStatus": recording_status,
        "independentDecode": independent_decode,
        "latency": {
            "scope": "same Windows host: renderer UDP ingress timestamp to receiver GUI display submission",
            "thresholdMs": args.latency_threshold_ms,
            "validRows": len(per_frame),
            "invalidRows": invalid_latency,
            "overThresholdCount": len(over),
            "overThresholdPercent": (100.0 * len(over) / len(per_frame)) if per_frame else None,
            "metrics": latency_metrics,
        },
        "artifacts": {
            "formalLogDir": str(formal_dir), "recordingDir": str(recording_dir),
            "movie": str(movie), "movieSha256": sha256(movie) if movie.is_file() else None,
            "summary": str(summary_path), "preflight": str(preflight_path),
        },
        "binaryPreflight": preflight,
        "errors": unique_errors,
    }
    return result, per_frame


def write_outputs(output_dir: Path, result: dict[str, Any], per_frame: list[dict[str, Any]]) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    columns = [
        "sourceSeq", "frameSeq", "association", "producerInputToSendMs", "transportMs",
        "receiverToDisplayMs", "inputToDisplayMs", "overThreshold",
    ]
    with (output_dir / "p11_windows_h264_per_frame_latency.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=columns)
        writer.writeheader()
        writer.writerows(per_frame)
    with (output_dir / "p11_windows_h264_over_80ms_frames.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=columns)
        writer.writeheader()
        writer.writerows(row for row in per_frame if row["overThreshold"])
    (output_dir / "p11_windows_h264_acceptance_summary.json").write_text(
        json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    input_to_display = result["latency"]["metrics"]["inputToDisplayMs"]
    counts = result["counts"]
    codec = result["codec"]
    control = result["controlAndDrain"]
    errors = result["errors"]
    lines = [
        f"# P11 Windows H.264 acceptance — {result['band']}", "",
        f"- Result: **{result['result']}**",
        f"- Count conservation: `{counts}`",
        f"- Codec: requested={codec['summaryRequested']}, active={codec['summaryActive']}, decoded={codec['summaryDecoded']}, encoder={codec['encoderName']}",
        f"- H.264: producer keyframes={codec['producerKeyFrames']}, ffprobe keyframes={codec['ffprobeKeyFrames']}, decode errors={codec['decodeErrors']}",
        f"- Geometry/rate: `{result['rate']['mp4'].get('width')}x{result['rate']['mp4'].get('height')}`, producer fps={result['rate']['producerFps']}, MP4 fps={result['rate']['mp4'].get('avgFrameRate')}",
        f"- Control/drain: INIT ACK={control['initAckObserved']} ({control['initAckWaitMs']} ms observed wait), STOP={control['stopTotalMs']} ms, drain={control['outputDrainCompleted']}/{control['outputDrainTarget']}, flush queue={control['recorderFlushQueueDepth']}",
        f"- Input-to-display latency: p95={input_to_display['p95Ms']} ms, p99={input_to_display['p99Ms']} ms, max={input_to_display['maxMs']} ms",
        f"- Frames over {result['latency']['thresholdMs']} ms: {result['latency']['overThresholdCount']} (full list: `p11_windows_h264_over_80ms_frames.csv`)",
        f"- Independent full MP4 decode: {result['independentDecode']['pass']}", "",
        "## Errors", "",
    ]
    lines.extend(f"- `{error}`" for error in errors)
    if not errors:
        lines.append("- None")
    (output_dir / "p11_windows_h264_acceptance_summary.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("evidence_dir", type=Path)
    parser.add_argument("--band", choices=("SWIR", "NIR", "MWIR"), required=True)
    parser.add_argument("--expected-seconds", type=float, default=60.0)
    parser.add_argument("--expected-fps", type=float, default=60.0)
    parser.add_argument("--fps-tolerance", type=float, default=2.0)
    parser.add_argument("--duration-tolerance", type=float, default=0.5)
    parser.add_argument("--latency-threshold-ms", type=float, default=80.0)
    parser.add_argument("--ffmpeg", type=Path, default=DEFAULT_FFMPEG)
    parser.add_argument("--ffprobe", type=Path, default=DEFAULT_FFMPEG.with_name("ffprobe.exe"))
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    result, per_frame = analyze(args)
    output_dir = (args.output_dir or args.evidence_dir).resolve()
    write_outputs(output_dir, result, per_frame)
    print(json.dumps(result, ensure_ascii=False, separators=(",", ":")))
    return 0 if result["result"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
