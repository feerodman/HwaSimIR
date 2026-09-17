#!/usr/bin/env python3
"""Analyze one formal P11 RK3588 SWIR/MWIR acceptance run.

The analyzer deliberately uses only Python's standard library so it can run in
the offline delivery workspace.  It treats the board-side ledgers as the
authoritative timing domain; Windows and RK3588 steady clocks are never mixed.
"""

from __future__ import annotations

import argparse
import array
import csv
import hashlib
import json
import math
import os
import re
import struct
import sys
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple


INPUT_HEADER = [
    "ordinal", "sourceSeq", "beginNs", "endNs", "success", "queueDepth",
    "digestFNV1a64",
]
RENDER_HEADER = [
    "sourceSeq", "steadyNs", "executeToDrawBeginMs", "doFrameMs",
    "captureTaskMs", "remainingInputDepth",
]
OUTPUT_HEADER = [
    "sourceSeq", "steadyNs", "outputWorkBeginNs", "readbackMs", "convertMs",
    "encodeMs", "ddsEnqueueMs", "ddsBackpressureMs", "metaAnnotationMs",
    "totalMs",
]


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while True:
            block = stream.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def percentile(values: Sequence[float], fraction: float) -> Optional[float]:
    if not values:
        return None
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    index = (len(ordered) - 1) * fraction
    low = int(math.floor(index))
    high = int(math.ceil(index))
    if low == high:
        return ordered[low]
    weight = index - low
    return ordered[low] * (1.0 - weight) + ordered[high] * weight


def metric(text: str, name: str) -> Optional[str]:
    match = re.search(r"(?:^|\s)" + re.escape(name) + r"=([^\s,]+)", text)
    return match.group(1) if match else None


def numeric(value: Optional[str], default: float = math.nan) -> float:
    try:
        return float(value) if value is not None else default
    except ValueError:
        return default


def last_line(lines: Iterable[str], token: str) -> Optional[str]:
    found = None
    for line in lines:
        if token in line:
            found = line
    return found


class Gates:
    def __init__(self) -> None:
        self.rows: List[Dict[str, Any]] = []

    def add(self, name: str, passed: bool, detail: str) -> bool:
        self.rows.append({"gate": name, "pass": bool(passed), "detail": detail})
        return bool(passed)

    @property
    def passed(self) -> bool:
        return all(row["pass"] for row in self.rows)


def find_one(root: Path, pattern: str, gates: Gates, gate_name: str) -> Optional[Path]:
    matches = sorted(root.glob(pattern)) if root.exists() else []
    gates.add(gate_name, len(matches) == 1,
              "count=%d paths=%s" % (len(matches), ";".join(str(p) for p in matches)))
    return matches[0] if len(matches) == 1 else None


def read_csv(path: Optional[Path], expected_header: List[str], gates: Gates,
             gate_name: str) -> List[Dict[str, str]]:
    if path is None:
        return []
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as stream:
            reader = csv.DictReader(stream)
            actual = reader.fieldnames or []
            header_ok = actual == expected_header
            rows = list(reader)
        gates.add(gate_name + ".header", header_ok,
                  "actual=%s expected=%s" % (actual, expected_header))
        gates.add(gate_name + ".rows", bool(rows), "rows=%d" % len(rows))
        return rows
    except Exception as exc:  # keep a complete evidence summary on malformed input
        gates.add(gate_name + ".read", False, "error=%s" % exc)
        return []


def parse_int_rows(rows: List[Dict[str, str]], columns: Sequence[str], gates: Gates,
                   gate_name: str) -> bool:
    try:
        for row in rows:
            for column in columns:
                int(row[column])
        return gates.add(gate_name, True, "rows=%d" % len(rows))
    except (KeyError, TypeError, ValueError) as exc:
        return gates.add(gate_name, False, "error=%s" % exc)


def validate_input_ledger(rows: List[Dict[str, str]], gates: Gates, name: str,
                          require_source_sequence: bool = True) -> Dict[str, Any]:
    result: Dict[str, Any] = {"count": len(rows), "seq": [], "digest": []}
    if not rows:
        gates.add(name + ".valid", False, "empty")
        return result
    if not parse_int_rows(rows, ["ordinal", "sourceSeq", "beginNs", "endNs",
                                  "success", "queueDepth"], gates, name + ".numeric"):
        return result
    ordinals = [int(row["ordinal"]) for row in rows]
    sequences = [int(row["sourceSeq"]) for row in rows]
    begins = [int(row["beginNs"]) for row in rows]
    ends = [int(row["endNs"]) for row in rows]
    successes = [int(row["success"]) for row in rows]
    digests = [row["digestFNV1a64"].lower() for row in rows]
    result.update({
        "seq": sequences,
        "digest": digests,
        "max_queue_depth": max(int(row["queueDepth"]) for row in rows),
        "first_seq": sequences[0],
        "last_seq": sequences[-1],
    })
    gates.add(name + ".ordinal_contiguous",
              ordinals == list(range(1, len(rows) + 1)),
              "first=%d last=%d count=%d" % (ordinals[0], ordinals[-1], len(rows)))
    if require_source_sequence:
        contiguous = all(b == a + 1 for a, b in zip(sequences, sequences[1:]))
        gates.add(name + ".source_sequence", contiguous and len(set(sequences)) == len(sequences),
                  "first=%d last=%d unique=%d count=%d" %
                  (sequences[0], sequences[-1], len(set(sequences)), len(sequences)))
    gates.add(name + ".success", all(value == 1 for value in successes),
              "failures=%d" % sum(value != 1 for value in successes))
    gates.add(name + ".timestamps", all(end >= begin > 0 for begin, end in zip(begins, ends)),
              "negative_or_reversed=%d" %
              sum(not (end >= begin > 0) for begin, end in zip(begins, ends)))
    gates.add(name + ".digests", all(re.fullmatch(r"[0-9a-f]{16}", value) for value in digests),
              "invalid=%d" % sum(not re.fullmatch(r"[0-9a-f]{16}", value) for value in digests))
    return result


def validate_stage(rows: List[Dict[str, str]], gates: Gates, name: str) -> Dict[str, Any]:
    result: Dict[str, Any] = {"count": len(rows), "seq": []}
    if not rows:
        gates.add(name + ".valid", False, "empty")
        return result
    try:
        sequences = [int(row["sourceSeq"]) for row in rows]
        steady = [int(row["steadyNs"]) for row in rows]
    except (KeyError, TypeError, ValueError) as exc:
        gates.add(name + ".numeric", False, "error=%s" % exc)
        return result
    result.update({"seq": sequences, "steady_ns": steady,
                   "first_seq": sequences[0], "last_seq": sequences[-1]})
    contiguous = all(b == a + 1 for a, b in zip(sequences, sequences[1:]))
    gates.add(name + ".sequence", contiguous and len(set(sequences)) == len(sequences),
              "first=%d last=%d unique=%d count=%d" %
              (sequences[0], sequences[-1], len(set(sequences)), len(sequences)))
    gates.add(name + ".steady_monotonic", all(b >= a for a, b in zip(steady, steady[1:])),
              "first=%d last=%d" % (steady[0], steady[-1]))
    return result


def png_dimensions(path: Path) -> Tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError("invalid PNG header")
    return struct.unpack(">II", header[16:24])


def pfm_stats(path: Path) -> Dict[str, Any]:
    with path.open("rb") as stream:
        magic = stream.readline().strip()
        if magic not in (b"PF", b"Pf"):
            raise ValueError("unsupported PFM magic %r" % magic)

        def next_value() -> bytes:
            while True:
                line = stream.readline()
                if not line:
                    raise ValueError("truncated PFM header")
                stripped = line.strip()
                if stripped and not stripped.startswith(b"#"):
                    return stripped

        dimensions = next_value().split()
        if len(dimensions) != 2:
            raise ValueError("invalid PFM dimensions")
        width, height = int(dimensions[0]), int(dimensions[1])
        scale = float(next_value())
        payload = stream.read()
    channels = 3 if magic == b"PF" else 1
    expected_values = width * height * channels
    if len(payload) != expected_values * 4:
        raise ValueError("PFM payload bytes=%d expected=%d" %
                         (len(payload), expected_values * 4))
    values = array.array("f")
    values.frombytes(payload)
    file_little_endian = scale < 0
    host_little_endian = sys.byteorder == "little"
    if file_little_endian != host_little_endian:
        values.byteswap()
    finite_count = 0
    negative_count = 0
    value_min = math.inf
    value_max = -math.inf
    value_sum = 0.0
    half_lattice_checked_count = 0
    half_lattice_mismatch_count = 0
    half_lattice_overflow_count = 0
    half_lattice_max_abs_error = 0.0
    for value in values:
        if math.isfinite(value):
            finite_count += 1
            negative_count += int(value < -1.0e-6)
            value_min = min(value_min, value)
            value_max = max(value_max, value)
            value_sum += value
            try:
                half_value = struct.unpack("<e", struct.pack("<e", value))[0]
                half_lattice_checked_count += 1
                half_error = abs(value - half_value)
                half_lattice_max_abs_error = max(half_lattice_max_abs_error, half_error)
                half_lattice_mismatch_count += int(half_value != value)
            except (OverflowError, struct.error):
                half_lattice_overflow_count += 1
    return {
        "path": str(path),
        "sha256": sha256_file(path),
        "width": width,
        "height": height,
        "channels": channels,
        "scale": scale,
        "value_count": expected_values,
        "finite_count": finite_count,
        "negative_count": negative_count,
        "minimum": value_min if finite_count else None,
        "maximum": value_max if finite_count else None,
        "mean": value_sum / finite_count if finite_count else None,
        "half_lattice_checked_count": half_lattice_checked_count,
        "half_lattice_mismatch_count": half_lattice_mismatch_count,
        "half_lattice_overflow_count": half_lattice_overflow_count,
        "half_lattice_max_abs_error": half_lattice_max_abs_error,
    }


def load_lines(path: Path, gates: Gates, gate: str) -> List[str]:
    if not path.is_file():
        gates.add(gate, False, "missing=%s" % path)
        return []
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        gates.add(gate, bool(lines), "lines=%d path=%s" % (len(lines), path))
        return lines
    except OSError as exc:
        gates.add(gate, False, "error=%s" % exc)
        return []


def parse_perf(path: Path) -> Dict[str, Any]:
    result: Dict[str, Any] = {"present": path.is_file(), "rows": 0}
    if not path.is_file():
        return result
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    result["rows"] = len(rows)
    for column in ("cpu_total_pct", "hwasimir_cpu_pct", "soc_temp_mc",
                   "gpu_temp_mc", "gpu_load_pct", "render_fps", "output_fps",
                   "input_queue_depth_max", "input_overwritten", "input_queue_overflow",
                   "source_seq_gap", "input_backpressure_count", "input_backpressure_wait_ms"):
        values: List[float] = []
        for row in rows:
            value = row.get(column, "")
            try:
                parsed = float(value)
            except (TypeError, ValueError):
                continue
            if math.isfinite(parsed):
                values.append(parsed)
        if values:
            result[column] = {
                "count": len(values), "minimum": min(values), "maximum": max(values),
                "mean": sum(values) / len(values), "p95": percentile(values, 0.95),
            }
    return result


def write_json(path: Path, value: Any) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2, sort_keys=False) + "\n",
                    encoding="utf-8")


def analyze(args: argparse.Namespace) -> int:
    case_dir = args.case_dir.resolve()
    board_dir = case_dir / "board"
    local_audit = case_dir / "windows_audit"
    gates = Gates()
    gates.add("case_directory", case_dir.is_dir(), "path=%s" % case_dir)
    gates.add("band_protocol_contract",
              (args.band == "SWIR" and args.protocol_band == 0) or
              (args.band == "MWIR" and args.protocol_band == 2),
              "band=%s protocolBand=%d" % (args.band, args.protocol_band))

    accepted_path = find_one(board_dir / "input_audit", "input_accepted_*.csv", gates,
                             "file.accepted")
    execute_path = find_one(board_dir / "input_audit", "input_execute_*.csv", gates,
                            "file.execute")
    render_path = find_one(board_dir / "input_audit", "stage_render_*.csv", gates,
                           "file.render")
    output_path = find_one(board_dir / "input_audit", "stage_output_*.csv", gates,
                           "file.output")
    sender_path = find_one(local_audit, "input_sender_*.csv", gates, "file.sender")
    received_path = find_one(local_audit, "input_received_*.csv", gates, "file.received")

    accepted_rows = read_csv(accepted_path, INPUT_HEADER, gates, "accepted")
    execute_rows = read_csv(execute_path, INPUT_HEADER, gates, "execute")
    render_rows = read_csv(render_path, RENDER_HEADER, gates, "render")
    output_rows = read_csv(output_path, OUTPUT_HEADER, gates, "output")
    sender_rows = read_csv(sender_path, INPUT_HEADER, gates, "sender")
    received_rows = read_csv(received_path, INPUT_HEADER, gates, "received")

    accepted = validate_input_ledger(accepted_rows, gates, "accepted")
    execute = validate_input_ledger(execute_rows, gates, "execute")
    sender = validate_input_ledger(sender_rows, gates, "sender", require_source_sequence=False)
    received = validate_input_ledger(received_rows, gates, "received")
    render = validate_stage(render_rows, gates, "render")
    output = validate_stage(output_rows, gates, "output")

    min_frames = int(math.floor(args.duration_sec * args.min_fps))
    max_frames = int(math.ceil(args.duration_sec * args.max_fps)) + 2
    gates.add("minimum_frame_count", accepted.get("count", 0) >= min_frames,
              "accepted=%d required=%d durationSec=%d minFps=%.3f" %
              (accepted.get("count", 0), min_frames, args.duration_sec, args.min_fps))
    gates.add("maximum_frame_count", accepted.get("count", 0) <= max_frames,
              "accepted=%d maximum=%d durationSec=%d maxFps=%.3f boundaryAllowance=2" %
              (accepted.get("count", 0), max_frames, args.duration_sec, args.max_fps))
    gates.add("accepted_execute_identity",
              accepted.get("seq") == execute.get("seq") and
              accepted.get("digest") == execute.get("digest") and bool(accepted.get("seq")),
              "accepted=%d execute=%d" % (accepted.get("count", 0), execute.get("count", 0)))
    gates.add("accepted_render_identity",
              accepted.get("seq") == render.get("seq") and bool(accepted.get("seq")),
              "accepted=%d render=%d" % (accepted.get("count", 0), render.get("count", 0)))
    gates.add("accepted_output_identity",
              accepted.get("seq") == output.get("seq") and bool(accepted.get("seq")),
              "accepted=%d output=%d" % (accepted.get("count", 0), output.get("count", 0)))
    gates.add("sender_accepted_digest_identity",
              sender.get("digest") == accepted.get("digest") and bool(accepted.get("digest")),
              "sender=%d accepted=%d" % (sender.get("count", 0), accepted.get("count", 0)))
    gates.add("accepted_receiver_identity",
              received.get("seq") == accepted.get("seq") and
              received.get("digest") == accepted.get("digest") and bool(accepted.get("seq")),
              "accepted=%d receiver=%d" % (accepted.get("count", 0), received.get("count", 0)))

    output_fps = None
    active_start_index = 0
    active_end_index = 0
    steady = output.get("steady_ns", [])
    if len(steady) >= 3:
        trim_begin = min(args.warmup_frames, max(0, len(steady) // 10))
        trim_end = min(args.tail_trim_frames, max(0, len(steady) // 20))
        active_start_index = trim_begin
        active_end_index = len(steady) - trim_end
        active = steady[active_start_index:active_end_index]
        if len(active) >= 2 and active[-1] > active[0]:
            output_fps = (len(active) - 1) * 1.0e9 / (active[-1] - active[0])
    gates.add("output_fps_about_60",
              output_fps is not None and args.min_fps <= output_fps <= args.max_fps,
              "measured=%s range=[%.3f,%.3f] activeRows=%d" %
              ("%.6f" % output_fps if output_fps is not None else "unavailable",
               args.min_fps, args.max_fps, max(0, active_end_index - active_start_index)))

    latency_rows: List[Dict[str, Any]] = []
    latencies: List[float] = []
    if accepted_rows and output_rows:
        accepted_by_seq = {int(row["sourceSeq"]): int(row["beginNs"]) for row in accepted_rows}
        for row in output_rows:
            seq = int(row["sourceSeq"])
            begin = accepted_by_seq.get(seq)
            if begin is None:
                continue
            end = int(row["steadyNs"])
            latency = (end - begin) / 1.0e6
            latencies.append(latency)
            latency_rows.append({
                "sourceSeq": seq, "acceptedBeginNs": begin, "outputSteadyNs": end,
                "latencyMs": "%.6f" % latency,
                "overThreshold": int(latency > args.latency_threshold_ms),
            })
    negative_latency = sum(value < 0 for value in latencies)
    gates.add("board_latency_domain_valid",
              len(latencies) == accepted.get("count", 0) and negative_latency == 0 and bool(latencies),
              "matched=%d expected=%d negative=%d clock=board_steady" %
              (len(latencies), accepted.get("count", 0), negative_latency))
    latency_summary = {
        "clock_domain": "RK3588 steady clock only",
        "count": len(latencies),
        "threshold_ms": args.latency_threshold_ms,
        "over_threshold_count": sum(value > args.latency_threshold_ms for value in latencies),
        "mean_ms": sum(latencies) / len(latencies) if latencies else None,
        "p50_ms": percentile(latencies, 0.50),
        "p95_ms": percentile(latencies, 0.95),
        "p99_ms": percentile(latencies, 0.99),
        "maximum_ms": max(latencies) if latencies else None,
    }
    with (case_dir / "frame_latency_ms.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=[
            "sourceSeq", "acceptedBeginNs", "outputSteadyNs", "latencyMs", "overThreshold"
        ])
        writer.writeheader()
        writer.writerows(latency_rows)

    hwa_lines = load_lines(board_dir / "hwa.log", gates, "log.hwa")
    receiver_out = load_lines(case_dir / "receiver.out.log", gates, "log.receiver_out")
    receiver_err = load_lines(case_dir / "receiver.err.log", gates, "log.receiver_err")
    stim_out = load_lines(case_dir / "stim.out.log", gates, "log.stim_out")
    stim_err = load_lines(case_dir / "stim.err.log", gates, "log.stim_err")
    receiver_lines = receiver_out + receiver_err
    stim_lines = stim_out + stim_err
    hwa_text = "\n".join(hwa_lines)
    receiver_text = "\n".join(receiver_lines)
    stim_text = "\n".join(stim_lines)

    stage1_re = re.compile(
        r"\[Stage1\] Sensor profile \(init-command\): protocolBand=%d, band=%s(?:,|\s)" %
        (args.protocol_band, re.escape(args.band)))
    gates.add("formal_band_selection", bool(stage1_re.search(hwa_text)),
              "protocolBand=%d band=%s" % (args.protocol_band, args.band))
    profile_name = "default_SWIR.json" if args.band == "SWIR" else "default_MWIR.json"
    gates.add("sensor_profile_file",
              any("[SensorWave Usage]" in line and "band=" + args.band in line and
                  profile_name in line for line in hwa_lines),
              "expectedProfile=%s" % profile_name)
    gates.add("ordered_input_policy",
              any("[RenderControl]" in line and "asyncInputPolicy=OrderedQueue" in line
                  for line in hwa_lines),
              "required=OrderedQueue")

    raw_buffer_lines = [line for line in hwa_lines
                        if "[Stage6 RawRadianceBuffer]" in line and
                        "[ERROR]" not in line and
                        metric(line, "formalRequested") == "1"]
    formal_raw_storage_rows: List[Dict[str, Any]] = []
    for line in raw_buffer_lines:
        rgb_match = re.search(r"(?:^|\s)actualRgbBits=\((\d+),(\d+),(\d+)\)", line)
        rgb_bits = tuple(int(value) for value in rgb_match.groups()) if rgb_match else ()
        formal_raw_storage_rows.append({
            "requested": metric(line, "requested"),
            "formal_requested": metric(line, "formalRequested"),
            "platform_backend": metric(line, "platformBackend"),
            "domain": metric(line, "domain"),
            "unit": metric(line, "unit"),
            "quantization_model": metric(line, "quantizationModel"),
            "quantization_relative_error_bound": numeric(
                metric(line, "quantizationRelativeErrorBound")),
            "quantization_max_finite": numeric(metric(line, "quantizationMaxFinite")),
            "framebuffer_float_property": metric(line, "framebufferFloatProperty"),
            "texture_floating_point": metric(line, "textureFloatingPoint"),
            "actual_texture_component_type": metric(line, "actualTextureComponentType"),
            "actual_texture_component_width": int(
                metric(line, "actualTextureComponentWidth") or -1),
            "actual_texture_components": int(
                metric(line, "actualTextureComponents") or -1),
            "actual_rgb_bits": rgb_bits,
            "actual_alpha_bits": int(metric(line, "actualAlphaBits") or -1),
            "line": line,
        })
    valid_raw_storage_rows = [row for row in formal_raw_storage_rows
                              if row["requested"] == "RGBA16F_SI" and
                              row["formal_requested"] == "1" and
                              row["platform_backend"] == "linux_gles" and
                              row["domain"] == "W_per_m2_sr_um" and
                              row["unit"] == "W/(m^2_sr_um)" and
                              row["quantization_model"] == "IEEE754_binary16" and
                              math.isclose(row["quantization_relative_error_bound"],
                                           2.0 ** -11, rel_tol=0.0, abs_tol=1.0e-15) and
                              row["quantization_max_finite"] == 65504.0 and
                              row["texture_floating_point"] == "1" and
                              row["actual_texture_component_type"] == "half_float" and
                              row["actual_texture_component_width"] == 2 and
                              row["actual_texture_components"] == 4 and
                              len(row["actual_rgb_bits"]) == 3 and
                              min(row["actual_rgb_bits"]) >= 16 and
                              row["actual_alpha_bits"] >= 16]
    gates.add("formal_raw_storage",
              bool(formal_raw_storage_rows) and
              len(valid_raw_storage_rows) == len(formal_raw_storage_rows),
              "formalRows=%d validRgba16fRows=%d requested=RGBA16F_SI "
              "unit=W/(m^2_sr_um) texture=RGBA16F/T_half_float theoreticalRelativeBound=%.12g" %
              (len(formal_raw_storage_rows), len(valid_raw_storage_rows), 2.0 ** -11))

    framebuffer_compat_lines = [line for line in hwa_lines
                                if "[Stage6 RawFramebufferCompat]" in line and
                                "[ERROR]" not in line]
    valid_framebuffer_compat_lines = []
    for line in framebuffer_compat_lines:
        rgb_match = re.search(r"(?:^|\s)actualRgbBits=\((\d+),(\d+),(\d+)\)", line)
        rgb_bits = tuple(int(value) for value in rgb_match.groups()) if rgb_match else ()
        if (metric(line, "phase") == "post_create_pre_attach" and
                metric(line, "preFloatProperty") == "1" and
                metric(line, "postFloatProperty") == "0" and
                metric(line, "eglOutputRecreated") == "0" and
                metric(line, "valid") == "1" and
                len(rgb_bits) == 3 and min(rgb_bits) >= 16 and
                int(metric(line, "actualAlphaBits") or -1) >= 16):
            valid_framebuffer_compat_lines.append(line)
    texture_compat_lines = [line for line in hwa_lines
                            if "[Stage6 RawTextureCompat]" in line and
                            "[ERROR]" not in line]
    valid_texture_compat_lines = [line for line in texture_compat_lines
                                  if metric(line, "phase") == "post_attach_restore" and
                                  metric(line, "postType") == "half_float" and
                                  metric(line, "postWidth") == "2" and
                                  metric(line, "postComponents") == "4" and
                                  metric(line, "postHasRamImage") == "0"]
    gates.add("formal_raw_gles_compatibility",
              len(framebuffer_compat_lines) == 1 and
              len(valid_framebuffer_compat_lines) == 1 and
              len(texture_compat_lines) == 1 and
              len(valid_texture_compat_lines) == 1,
              "framebufferLines=%d validFramebufferLines=%d textureLines=%d "
              "validTextureLines=%d required=float1_to_negotiation0_half2x4_gpu_only" %
              (len(framebuffer_compat_lines), len(valid_framebuffer_compat_lines),
               len(texture_compat_lines), len(valid_texture_compat_lines)))

    raw_attachment_lines = [line for line in hwa_lines
                            if "[Stage6 RawAttachment]" in line and "[ERROR]" not in line]
    valid_raw_attachment_lines = [line for line in raw_attachment_lines
                                  if metric(line, "requested") == "RTM_bind_or_copy" and
                                  metric(line, "actual") == "RTM_bind_or_copy" and
                                  metric(line, "directBound") == "1" and
                                  metric(line, "formalSiDomain") == "1" and
                                  metric(line, "storageFormat") == "RGBA16F_SI" and
                                  metric(line, "textureStorageVerified") == "1" and
                                  metric(line, "actualTextureComponentType") == "half_float" and
                                  metric(line, "actualTextureComponentWidth") == "2" and
                                  metric(line, "actualTextureComponents") == "4"]
    gates.add("formal_raw_attachment",
              len(raw_attachment_lines) == 1 and len(valid_raw_attachment_lines) == 1,
              "actualLines=%d validDirectLines=%d required=RTM_bind_or_copy" %
              (len(raw_attachment_lines), len(valid_raw_attachment_lines)))

    formal_pipeline_lines = [line for line in hwa_lines
                             if "[Stage6 FinalPipeline]" in line and
                             "[WARN]" not in line and
                             metric(line, "formalSiDomain") == "1"]
    invalid_formal_pipeline_lines = [line for line in formal_pipeline_lines
                                     if metric(line, "renderPath") != "dual_pass" or
                                     metric(line, "finalPostprocessBypass") != "0" or
                                     metric(line, "rawStorageFormat") != "RGBA16F_SI"]
    gates.add("formal_dual_pass_route",
              bool(formal_pipeline_lines) and not invalid_formal_pipeline_lines,
              "formalLines=%d invalidDirectOrStorageLines=%d" %
              (len(formal_pipeline_lines), len(invalid_formal_pipeline_lines)))

    component_lines = [line for line in hwa_lines if "[Stage5 RadianceComponents]" in line and
                       metric(line, "targetPlatID") == "3101" and
                       metric(line, "targetID") == "5501" and
                       metric(line, "band") == args.band]
    formal_component_lines = [line for line in component_lines
                              if metric(line, "modtranRadianceValid") == "1" and
                              metric(line, "tauUpValid") == "1" and
                              metric(line, "tauFallbackReason") == "none" and
                              metric(line, "modtranFallbackReason") == "none" and
                              metric(line, "formalRuntimeAffectsImage") == "1" and
                              metric(line, "finalOutput") == "M1" and
                              numeric(metric(line, "finalSensorInputRadiance"), 0.0) > 0.0]
    gates.add("formal_m1_components", bool(formal_component_lines),
              "civilVanLines=%d validFormalLines=%d" %
              (len(component_lines), len(formal_component_lines)))

    quantization_reference_rows: List[Dict[str, Any]] = []
    quantization_overflow_count = 0
    quantization_bound_failure_count = 0
    for line in formal_component_lines:
        value = numeric(metric(line, "finalSensorInputRadiance"))
        if not math.isfinite(value):
            continue
        try:
            half_value = struct.unpack("<e", struct.pack("<e", value))[0]
        except (OverflowError, struct.error):
            quantization_overflow_count += 1
            continue
        absolute_error = abs(half_value - value)
        theoretical_absolute_bound = abs(value) * (2.0 ** -11) + (2.0 ** -25)
        relative_error = absolute_error / abs(value) if value != 0.0 else 0.0
        within_bound = absolute_error <= theoretical_absolute_bound + 1.0e-15
        quantization_bound_failure_count += int(not within_bound)
        quantization_reference_rows.append({
            "source_value": value,
            "binary16_value": half_value,
            "absolute_error": absolute_error,
            "relative_error": relative_error,
            "theoretical_absolute_bound": theoretical_absolute_bound,
            "within_bound": within_bound,
        })
    quantization_reference_summary = {
        "model": "IEEE754_binary16_round_to_nearest",
        "unit": "W/(m^2_sr_um)",
        "cpu_reference_count": len(quantization_reference_rows),
        "overflow_count": quantization_overflow_count,
        "bound_failure_count": quantization_bound_failure_count,
        "theoretical_relative_error_bound": 2.0 ** -11,
        "acceptance_relative_error_limit": 0.02,
        "maximum_absolute_error": max(
            (row["absolute_error"] for row in quantization_reference_rows), default=None),
        "maximum_relative_error": max(
            (row["relative_error"] for row in quantization_reference_rows), default=None),
    }
    gates.add("formal_raw_cpu_binary16_quantization",
              bool(quantization_reference_rows) and quantization_overflow_count == 0 and
              quantization_bound_failure_count == 0 and
              quantization_reference_summary["maximum_relative_error"] is not None and
              quantization_reference_summary["maximum_relative_error"] <= 0.02,
              "references=%d overflow=%d boundFailures=%d maxAbsError=%s "
              "maxRelError=%s acceptanceRelLimit=0.02 theoreticalRelBound=%.12g" %
              (len(quantization_reference_rows), quantization_overflow_count,
               quantization_bound_failure_count,
               quantization_reference_summary["maximum_absolute_error"],
               quantization_reference_summary["maximum_relative_error"], 2.0 ** -11))

    expected_raw_seqs = [int(value) for value in args.raw_seqs.split(",") if value]
    capture_lines = [line for line in hwa_lines if "[P6LinearCapture]" in line]
    capture_seqs = sorted({int(metric(line, "sourceSeq")) for line in capture_lines
                           if metric(line, "sourceSeq") and
                           metric(line, "domain") == "spectral_radiance" and
                           metric(line, "unit") == "W/(m^2_sr_um)" and
                           metric(line, "physicalRadiance") == "1" and
                           metric(line, "readbackRoute") == "gles_rgba_float"})
    gates.add("physical_capture_log", capture_seqs == expected_raw_seqs,
              "actual=%s expected=%s unit=W/(m^2_sr_um)" %
              (capture_seqs, expected_raw_seqs))

    raw_rows: List[Dict[str, Any]] = []
    for seq in expected_raw_seqs:
        path = board_dir / "linear" / ("raw_radiance_seq%d.pfm" % seq)
        try:
            row = pfm_stats(path)
            row["sourceSeq"] = seq
            raw_rows.append(row)
            gates.add("raw_pfm_%d" % seq,
                      row["width"] == args.width and row["height"] == args.height and
                      row["finite_count"] == row["value_count"] and
                      row["negative_count"] == 0 and
                      row["maximum"] is not None and row["minimum"] is not None and
                      row["maximum"] > row["minimum"] and row["mean"] > 0.0 and
                      row["half_lattice_checked_count"] == row["finite_count"] and
                      row["half_lattice_mismatch_count"] == 0 and
                      row["half_lattice_overflow_count"] == 0,
                      "size=%dx%d finite=%d/%d min=%s max=%s mean=%s negative=%d "
                      "halfChecked=%d halfMismatch=%d halfOverflow=%d sha256=%s" %
                      (row["width"], row["height"], row["finite_count"], row["value_count"],
                       row["minimum"], row["maximum"], row["mean"], row["negative_count"],
                       row["half_lattice_checked_count"], row["half_lattice_mismatch_count"],
                       row["half_lattice_overflow_count"],
                       row["sha256"]))
        except Exception as exc:
            gates.add("raw_pfm_%d" % seq, False, "path=%s error=%s" % (path, exc))
    unique_raw_hashes = len({row["sha256"] for row in raw_rows})
    # A fixed-geometry, fixed-atmosphere fixture may be physically deterministic.
    # Dynamicity belongs to a separately controlled factor test; this gate proves
    # that every explicitly requested source sequence produced a bound, valid PFM.
    gates.add("requested_raw_frames", len(raw_rows) == len(expected_raw_seqs),
              "captures=%d expected=%d uniqueSha256=%d informationalOnly=1" %
              (len(raw_rows), len(expected_raw_seqs), unique_raw_hashes))
    with (case_dir / "raw_radiance_metrics.csv").open("w", encoding="utf-8", newline="") as stream:
        fields = ["sourceSeq", "path", "sha256", "width", "height", "channels", "scale",
                  "value_count", "finite_count", "negative_count", "minimum", "maximum", "mean",
                  "half_lattice_checked_count", "half_lattice_mismatch_count",
                  "half_lattice_overflow_count", "half_lattice_max_abs_error"]
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(raw_rows)

    decoded_png = case_dir / "received_decode.png"
    try:
        png_width, png_height = png_dimensions(decoded_png)
        gates.add("real_receiver_decoded_png",
                  png_width == args.width and png_height == args.height,
                  "size=%dx%d sha256=%s path=%s" %
                  (png_width, png_height, sha256_file(decoded_png), decoded_png))
    except Exception as exc:
        gates.add("real_receiver_decoded_png", False, "path=%s error=%s" % (decoded_png, exc))

    h264_path = case_dir / "received_annexb.h264"
    try:
        with h264_path.open("rb") as stream:
            prefix = stream.read(4096)
        annexb = b"\x00\x00\x01" in prefix or b"\x00\x00\x00\x01" in prefix
        gates.add("real_receiver_annexb", h264_path.stat().st_size > 0 and annexb,
                  "bytes=%d annexBStartCode=%d sha256=%s" %
                  (h264_path.stat().st_size, int(annexb), sha256_file(h264_path)))
    except Exception as exc:
        gates.add("real_receiver_annexb", False, "path=%s error=%s" % (h264_path, exc))

    gates.add("mali_gpu",
              any("[GpuBackend]" in line and "glVendor=ARM" in line and
                  "glRenderer=Mali-LODX" in line and "hardwareGpu=1" in line
                  for line in hwa_lines), "required=ARM/Mali-LODX hardwareGpu=1")
    gates.add("mpp_h264_encode",
              any("[H264EncodeSuccess]" in line and "backend=mpp" in line and
                  "codec=h264_annexb" in line and
                  "resolution=%dx%d" % (args.width, args.height) in line and
                  "keyFrame=true" in line and "spsPps=true" in line
                  for line in hwa_lines), "required=mpp h264_annexb 800x800 keyframe SPS/PPS")
    gates.add("ffmpeg_h264_decode",
              any("[H264DecodeSuccess]" in line and "backend=ffmpeg" in line and
                  "codec=h264_annexb" in line and
                  "resolution=%dx%d" % (args.width, args.height) in line
                  for line in receiver_lines), "required=real receiver ffmpeg decode")
    dump_lines = [line for line in receiver_lines if "[DdsFrameDump]" in line]
    exact_dump = [line for line in dump_lines if metric(line, "saved") == "1" and
                  int(metric(line, "sample") or -1) == args.receiver_dump_frame_index]
    gates.add("receiver_frame_dump", bool(exact_dump),
              "expectedSample=%d actual=%s path=%s" %
              (args.receiver_dump_frame_index,
               [int(metric(line, "sample") or -1) for line in dump_lines], decoded_png))
    diag_lines = [line for line in receiver_lines if "[DdsFrameDiag]" in line]
    diag_varied = any(numeric(metric(line, "max")) > numeric(metric(line, "min")) and
                      numeric(metric(line, "stddev"), 0.0) > 0.0 for line in diag_lines)
    gates.add("decoded_frame_nonconstant", diag_varied,
              "diagnosticLines=%d" % len(diag_lines))

    conservation = last_line(hwa_lines, "[SyncRoundConservation]")
    conservation_ok = bool(conservation and metric(conservation, "inputMinusCaptured") == "0" and
                           metric(conservation, "queueDepth") == "0" and
                           metric(conservation, "staleFramePublished") == "0" and
                           int(metric(conservation, "acceptedRealtime") or -1) == accepted.get("count", 0) and
                           int(metric(conservation, "lastCapturedSourceSeq") or -1) == accepted.get("last_seq", -2))
    gates.add("round_conservation", conservation_ok, conservation or "missing")
    output_drain = last_line(hwa_lines, "[OutputRoundDrain] reason=stop round=")
    output_drain_ok = bool(output_drain and
                           metric(output_drain, "targetFrames") == metric(output_drain, "completedFrames") and
                           int(metric(output_drain, "targetFrames") or -1) == output.get("count", 0))
    gates.add("output_drain", output_drain_ok, output_drain or "missing")
    stop_result = last_line(hwa_lines, "[ControlStopResult]")
    gates.add("control_stop_result",
              bool(stop_result and metric(stop_result, "outputDrainOk") == "1" and
                   metric(stop_result, "stopForwardOk") == "1" and
                   metric(stop_result, "acceptance") == "pass"),
              stop_result or "missing")

    def read_scalar_int(path: Path) -> Any:
        try:
            return int(path.read_text(encoding="utf-8", errors="replace").strip())
        except (OSError, ValueError):
            return None

    hwa_exit = read_scalar_int(board_dir / "hwa_exit.txt")
    runner_exit = read_scalar_int(board_dir / "board_runner_exit.txt")
    clock_end_lines = load_lines(board_dir / "board_clock_end.txt", gates,
                                 "file.board_clock_end")
    clock_end = {}
    for line in clock_end_lines:
        if "=" in line:
            key, value = line.split("=", 1)
            clock_end[key.strip()] = value.strip()
    gates.add("board_process_exit",
              hwa_exit == 0 and runner_exit == 0 and
              clock_end.get("exitStatus") == "0" and
              clock_end.get("hwasimirStillRunning") == "0",
              "hwaExit=%s runnerExit=%s trapExit=%s hwasimirStillRunning=%s" %
              (hwa_exit, runner_exit, clock_end.get("exitStatus"),
               clock_end.get("hwasimirStillRunning")))

    control_rows: List[Dict[str, Any]] = []
    for line in hwa_lines:
        if "[ControlResponseV3]" not in line:
            continue
        command = int(metric(line, "command") or -1)
        control_rows.append({
            "command": command,
            "round": int(metric(line, "round") or -1),
            "receiveNs": int(metric(line, "receiveNs") or 0),
            "executeNs": int(metric(line, "executeNs") or 0),
            "responseMs": numeric(metric(line, "responseMs")),
        })
    commands = [row["command"] for row in control_rows]
    gates.add("control_responses", commands == [1, 2, 3] and
              all(row["executeNs"] >= row["receiveNs"] > 0 for row in control_rows),
              "commands=%s responseMs=%s" %
              (commands, [row["responseMs"] for row in control_rows]))

    dds_perf = last_line(hwa_lines, "[DdsVideoPerf]")
    dds_perf_ok = bool(dds_perf and int(metric(dds_perf, "sentSamples") or -1) == output.get("count", 0) and
                       metric(dds_perf, "writeErrors") == "0" and
                       metric(dds_perf, "droppedSamples") == "0")
    gates.add("dds_publisher_counts", dds_perf_ok, dds_perf or "missing")
    receiver_perf = last_line(receiver_lines, "[DdsVideoReceiverPerf]")
    receiver_perf_ok = bool(receiver_perf and
                            int(metric(receiver_perf, "receivedSamples") or -1) == received.get("count", 0) and
                            metric(receiver_perf, "ddsErrors") == "0")
    gates.add("dds_receiver_counts", receiver_perf_ok, receiver_perf or "missing")
    stim_final = last_line(stim_lines, "[StimFinal]")
    gates.add("stimulus_counts",
              bool(stim_final and int(metric(stim_final, "successfulRealtimeWrites") or -1) ==
                   sender.get("count", 0)), stim_final or "missing")
    stim_stop = last_line(stim_lines, "[StimStopStatus]")
    stim_drain = last_line(stim_lines, "[StimDrain]")
    gates.add("stimulus_stop_and_ack_drain",
              bool(stim_stop and metric(stim_stop, "observed") == "1" and stim_drain and
                   metric(stim_drain, "ackControl") == "0" and
                   metric(stim_drain, "ackInit") == "0" and
                   metric(stim_drain, "ackRealtime") == "0"),
              "%s | %s" % (stim_stop or "missing_stop", stim_drain or "missing_drain"))

    fatal_patterns = [
        r"\[RunPreflight\]\[FATAL\]", r"\[StartupFatal\]", r"hardwareGpu=0",
        r"llvmpipe", r"GL_INVALID_OPERATION", r"GL error 0x502",
        r"Failed to convert image",
		r"\[Stage6 RawRadianceBuffer\]\[ERROR\]",
		r"\[Stage6 RawFramebufferCompat\]\[ERROR\]",
		r"\[Stage6 RawAttachment\]\[ERROR\]",
        r"\[M1 FormalFailClosed\]\[ERROR\]", r"\[OutputRoundDrain\]\[ERROR\]",
        r"\[DdsVideo\]\[ERROR\]", r"\[DdsFrameProducts\]\[(?:ERROR|FATAL)\]",
        r"\[InputAuditV1\]\[ERROR\]", r"\[StageAuditV1\]\[ERROR\]",
        r"\[P6LinearCapture\]\[ERROR\]", r"\[CodecFallback\]",
        r"mpp_(?:init|cfg|encode).*failed",
    ]
    hwa_failures = [pattern for pattern in fatal_patterns if re.search(pattern, hwa_text, re.I)]
    receiver_failures = re.findall(r"\[DdsVideoReceiver\]\[ERROR\]|\[H264Decoder\]\[ERROR\]",
                                   receiver_text, re.I)
    stim_failures = re.findall(r"\[StimDrain\]\[ERROR\]|\[StimDDS\]\[ERROR\]|"
                               r"\[StimInitAck\]\[FATAL\]|DDS start failed",
                               stim_text, re.I)
    gates.add("no_formal_chain_fatal", not hwa_failures,
              "matchedPatterns=%s" % hwa_failures)
    gates.add("no_receiver_errors", not receiver_failures,
              "errorCount=%d" % len(receiver_failures))
    gates.add("no_stimulus_errors", not stim_failures,
              "errorCount=%d" % len(stim_failures))

    perf = parse_perf(board_dir / "performance.csv")
    gates.add("performance_sampler", perf.get("rows", 0) > 0,
              "rows=%d" % perf.get("rows", 0))
    for field in ("input_overwritten", "input_queue_overflow", "source_seq_gap"):
        maximum = ((perf.get(field) or {}).get("maximum"))
        gates.add("performance_" + field, maximum == 0.0,
                  "maximum=%s" % maximum)

    summary: Dict[str, Any] = {
        "schema": "hwasimir.p11.rk3588.band-acceptance.v1",
        "result": "PASS" if gates.passed else "FAIL",
        "band": args.band,
        "protocol_band": args.protocol_band,
        "resolution": "%dx%d" % (args.width, args.height),
        "duration_sec": args.duration_sec,
        "frame_counts": {
            "sender": sender.get("count", 0), "accepted": accepted.get("count", 0),
            "execute": execute.get("count", 0), "render": render.get("count", 0),
            "output": output.get("count", 0), "received": received.get("count", 0),
        },
        "output_fps": output_fps,
        "output_fps_window": {
            "start_index": active_start_index, "end_index_exclusive": active_end_index,
            "min_accepted_fps": args.min_fps, "max_accepted_fps": args.max_fps,
        },
        "latency": latency_summary,
        "control_response": control_rows,
        "queue": {
            "accepted_max_depth": accepted.get("max_queue_depth"),
            "execute_max_depth": execute.get("max_queue_depth"),
        },
        "raw_radiance": {
            "unit": "W/(m^2_sr_um)", "captures": raw_rows,
            "unique_sha256": unique_raw_hashes,
            "storage": formal_raw_storage_rows,
            "attachment": raw_attachment_lines,
            "cpu_binary16_quantization_reference": quantization_reference_summary,
        },
        "performance_sampler": perf,
        "gates": gates.rows,
    }
    write_json(case_dir / "acceptance_summary.json", summary)
    with (case_dir / "acceptance_gates.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=["gate", "pass", "detail"])
        writer.writeheader()
        writer.writerows(gates.rows)
    markdown = [
        "# P11 RK3588 %s acceptance" % args.band, "",
        "- Result: **%s**" % summary["result"],
        "- Protocol band: `%d`" % args.protocol_band,
        "- Resolution: `%dx%d`" % (args.width, args.height),
        "- Ordered frames (sender/accepted/execute/render/output/receiver): " +
        "`%d/%d/%d/%d/%d/%d`" % tuple(summary["frame_counts"].values()),
        "- Measured output FPS: `%s`" %
        ("%.6f" % output_fps if output_fps is not None else "unavailable"),
        "- Board-steady end-to-end latency mean/p95/max ms: `%s / %s / %s`" %
        tuple("%.6f" % latency_summary[name] if latency_summary[name] is not None else "unavailable"
              for name in ("mean_ms", "p95_ms", "maximum_ms")),
        "- Frames over %.3f ms: `%d`" %
        (args.latency_threshold_ms, latency_summary["over_threshold_count"]),
        "- Raw radiance unit: `W/(m^2_sr_um)`; captures/unique hashes: `%d/%d`" %
        (len(raw_rows), unique_raw_hashes),
        "- Formal raw storage: `RGBA16F_SI`; binary16 theoretical relative "
        "rounding bound: `%.12g`" % (2.0 ** -11), "", "## Failed gates", "",
    ]
    failed = [row for row in gates.rows if not row["pass"]]
    markdown.extend(["- `%s`: %s" % (row["gate"], row["detail"]) for row in failed] or
                    ["- None"])
    (case_dir / "acceptance_summary.md").write_text("\n".join(markdown) + "\n", encoding="utf-8")

    manifest_lines: List[str] = []
    manifest_path = case_dir / "artifact_manifest.sha256"
    for path in sorted(p for p in case_dir.rglob("*") if p.is_file() and p != manifest_path):
        relative = path.relative_to(case_dir).as_posix()
        manifest_lines.append("%s  %s" % (sha256_file(path), relative))
    manifest_path.write_text("\n".join(manifest_lines) + "\n", encoding="utf-8")
    print("[P11 RK3588 Analyze] result=%s band=%s frames=%d fps=%s over80ms=%d output=%s" %
          (summary["result"], args.band, accepted.get("count", 0),
           "%.6f" % output_fps if output_fps is not None else "unavailable",
           latency_summary["over_threshold_count"], case_dir))
    return 0 if gates.passed else 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--case-dir", type=Path, required=True)
    parser.add_argument("--band", choices=["SWIR", "MWIR"], required=True)
    parser.add_argument("--protocol-band", type=int, required=True)
    parser.add_argument("--duration-sec", type=int, default=60)
    parser.add_argument("--width", type=int, default=800)
    parser.add_argument("--height", type=int, default=800)
    parser.add_argument("--min-fps", type=float, default=59.0)
    parser.add_argument("--max-fps", type=float, default=61.5)
    parser.add_argument("--warmup-frames", type=int, default=300)
    parser.add_argument("--tail-trim-frames", type=int, default=60)
    parser.add_argument("--latency-threshold-ms", type=float, default=80.0)
    parser.add_argument("--raw-seqs", default="180,900,1800,2700,3540")
    parser.add_argument("--receiver-dump-frame-index", type=int, default=1800)
    return analyze(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
