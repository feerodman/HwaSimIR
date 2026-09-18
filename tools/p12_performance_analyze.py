#!/usr/bin/env python3
"""Analyze one P12D ordinary DDS performance case without mixing host clocks.

The production frame index contains two independent monotonic-clock domains:
producer timestamps are from RK3588 and receiver timestamps are from Windows.
Only ``outputLatencyMs`` is a cross-host estimate; it is reported together with
``clockUncertaintyMs`` and is never reconstructed by subtracting steady clocks.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import re
from pathlib import Path
from typing import Any, Iterable


def percentile(values: list[float], q: float) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * q
    low = int(math.floor(position))
    high = int(math.ceil(position))
    if low == high:
        return ordered[low]
    fraction = position - low
    return ordered[low] * (1.0 - fraction) + ordered[high] * fraction


def distribution(values: Iterable[float], *, threshold_ms: float | None = None) -> dict[str, Any]:
    data = [float(value) for value in values if math.isfinite(float(value))]
    if not data:
        return {"count": 0, "mean": None, "p95": None, "p99": None, "max": None,
                "over80Count": 0 if threshold_ms is not None else None}
    result: dict[str, Any] = {
        "count": len(data),
        "mean": sum(data) / len(data),
        "p95": percentile(data, 0.95),
        "p99": percentile(data, 0.99),
        "max": max(data),
    }
    if threshold_ms is not None:
        result["over80Count"] = sum(value > threshold_ms for value in data)
    return result


def number(row: dict[str, Any], key: str) -> int:
    return int(row[key])


def delta_ms(row: dict[str, Any], begin: str, end: str) -> float:
    return (number(row, end) - number(row, begin)) / 1_000_000.0


def load_json_lines(path: Path) -> list[dict[str, Any]]:
    result = []
    with path.open("r", encoding="utf-8-sig") as stream:
        for line_number, line in enumerate(stream, 1):
            if not line.strip():
                continue
            try:
                result.append(json.loads(line))
            except json.JSONDecodeError as exc:
                raise RuntimeError(f"Invalid JSON in {path}:{line_number}: {exc}") from exc
    return result


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def kv(text: str) -> dict[str, str]:
    return dict(re.findall(r"([A-Za-z][A-Za-z0-9_]*)=([^\s]+)", text))


def fps(rows: list[dict[str, Any]], field: str) -> dict[str, Any]:
    timestamps = [number(row, field) for row in rows if field in row]
    if len(timestamps) < 2 or timestamps[-1] <= timestamps[0]:
        return {"frames": len(timestamps), "fps": None, "elapsedSeconds": None}
    elapsed = (timestamps[-1] - timestamps[0]) / 1_000_000_000.0
    return {"frames": len(timestamps), "fps": (len(timestamps) - 1) / elapsed,
            "elapsedSeconds": elapsed}


def sequence_audit(rows: list[dict[str, Any]], field: str) -> dict[str, Any]:
    values = [int(row[field]) for row in rows]
    gaps: list[dict[str, int]] = []
    duplicates = 0
    for left, right in zip(values, values[1:]):
        if right == left:
            duplicates += 1
        elif right != left + 1:
            gaps.append({"after": left, "next": right, "delta": right - left})
    return {
        "count": len(values), "first": values[0] if values else None,
        "last": values[-1] if values else None, "continuous": not gaps and duplicates == 0,
        "duplicates": duplicates, "gaps": gaps[:100], "gapCount": len(gaps),
    }


def parse_last_json_log(log: str, tag: str) -> dict[str, Any] | None:
    matches = re.findall(rf"(?m)^\[{re.escape(tag)}\]\s+(\{{.*\}})\s*$", log)
    if not matches:
        return None
    try:
        return json.loads(matches[-1])
    except json.JSONDecodeError:
        return None


def last_tag_values(log: str, tag: str) -> dict[str, str] | None:
    matches = re.findall(rf"(?m)^\[{re.escape(tag)}\]\s+([^\r\n]+)", log)
    return kv(matches[-1]) if matches else None


def all_tag_values(log: str, tag: str) -> list[dict[str, str]]:
    return [kv(value) for value in re.findall(rf"(?m)^\[{re.escape(tag)}\]\s+([^\r\n]+)", log)]


def analyze(case: Path, requested_seconds: int, band: str, weather: str) -> dict[str, Any]:
    recording_root = case / "recording"
    index_paths = sorted(recording_root.glob("round_*/frame_index.jsonl"))
    if len(index_paths) != 1:
        raise RuntimeError(f"Expected exactly one recording index below {recording_root}, got {len(index_paths)}")
    index_path = index_paths[0]
    round_dir = index_path.parent
    rows = load_json_lines(index_path)
    if not rows:
        raise RuntimeError(f"No products in {index_path}")

    ordinary = case / "ordinary_ui"
    board_path = case / "board.log"
    receiver_path = ordinary / "receiver.err.log"
    board_log = board_path.read_text(encoding="utf-8", errors="replace")
    receiver_log = receiver_path.read_text(encoding="utf-8", errors="replace")
    status_path = round_dir / "recording_status.json"
    status = json.loads(status_path.read_text(encoding="utf-8-sig"))

    max_source = max(int(row["sourceSeq"]) for row in rows)
    cold = [row for row in rows if int(row["sourceSeq"]) <= 180]
    steady = [row for row in rows if int(row["sourceSeq"]) > 180]
    rate_window = [row for row in rows if 180 < int(row["sourceSeq"]) <= max_source - 60]
    if len(rate_window) < 2:
        rate_window = steady

    stages = {
        "acceptedToExecuteMs": ("acceptedSteadyNs", "executeSteadyNs", "board"),
        "executeToCaptureMs": ("executeSteadyNs", "captureSteadyNs", "board"),
        "captureToEncodeMs": ("captureSteadyNs", "encodeSteadyNs", "board"),
        "encodeToWriterSubmitMs": ("encodeSteadyNs", "writerSubmitSteadyNs", "board"),
        "acceptedToWriterSubmitMs": ("acceptedSteadyNs", "writerSubmitSteadyNs", "board"),
        "receiveToDecodeMs": ("receiveSteadyNs", "decodeSteadyNs", "windows"),
        "decodeToGuiBeginMs": ("decodeSteadyNs", "guiBeginSteadyNs", "windows"),
        "guiProcessingSubmitMs": ("guiBeginSteadyNs", "guiSubmitSteadyNs", "windows"),
        "receiveToGuiSubmitMs": ("receiveSteadyNs", "guiSubmitSteadyNs", "windows"),
    }
    stage_report: dict[str, Any] = {}
    csv_rows: list[dict[str, Any]] = []
    for row in rows:
        output: dict[str, Any] = {key: row.get(key) for key in (
            "session", "generation", "run", "sourceSeq", "frameSeq", "outputLatencyEstimated",
            "outputLatencyMs", "clockUncertaintyMs", "clockOffsetNs")}
        for name, (begin, end, _host) in stages.items():
            output[name] = delta_ms(row, begin, end)
        csv_rows.append(output)
    for name, (begin, end, host) in stages.items():
        stage_report[name] = {
            "clockDomain": host,
            "coldSourceSeq1To180": distribution((delta_ms(row, begin, end) for row in cold), threshold_ms=80.0),
            "steadyAfterSourceSeq180": distribution((delta_ms(row, begin, end) for row in steady), threshold_ms=80.0),
            "wholeCase": distribution((delta_ms(row, begin, end) for row in rows), threshold_ms=80.0),
        }

    estimated = [row for row in rows if bool(row.get("outputLatencyEstimated"))]
    estimated_cold = [row for row in cold if bool(row.get("outputLatencyEstimated"))]
    estimated_steady = [row for row in steady if bool(row.get("outputLatencyEstimated"))]
    offsets_ms = [int(row["clockOffsetNs"]) / 1_000_000.0 for row in estimated]
    cross_host = {
        "method": "product_outputLatencyMs_clock_offset_estimate_only",
        "warning": "Estimated cross-host latency; board and Windows steady clocks were not directly subtracted.",
        "validSamples": len(estimated), "totalSamples": len(rows),
        "coldSourceSeq1To180": distribution((float(row["outputLatencyMs"]) for row in estimated_cold), threshold_ms=80.0),
        "steadyAfterSourceSeq180": distribution((float(row["outputLatencyMs"]) for row in estimated_steady), threshold_ms=80.0),
        "wholeCase": distribution((float(row["outputLatencyMs"]) for row in estimated), threshold_ms=80.0),
        "clockUncertaintyMs": distribution((float(row["clockUncertaintyMs"]) for row in estimated)),
        "clockOffsetMsRange": {"min": min(offsets_ms) if offsets_ms else None,
                               "max": max(offsets_ms) if offsets_ms else None},
    }

    paint_rows = all_tag_values(receiver_log, "GuiPaintPerf")
    shutdown_paint = next((row for row in reversed(paint_rows) if row.get("reason") == "shutdown"),
                          paint_rows[-1] if paint_rows else None)
    final_metrics = parse_last_json_log(receiver_log, "RuntimeMetricsV2") or {}
    flush = last_tag_values(receiver_log, "RecorderFlush") or {}
    stop_control = next((row for row in reversed(all_tag_values(board_log, "ControlResponseV3"))
                         if row.get("command") == "3"), {})
    finalizing = next((row for row in reversed(all_tag_values(board_log, "ControlStopPhase"))
                       if row.get("phase") == "finalizing"), {})
    drain = last_tag_values(board_log, "DdsVideoDrain") or {}
    stop_result = last_tag_values(board_log, "ControlStopResult") or {}
    output_drain = last_tag_values(board_log, "OutputRoundDrain") or {}
    cloud_audit = last_tag_values(board_log, "CloudRenderCallAudit") or {}
    weather_rows = all_tag_values(board_log, "Stage7 Weather")
    precipitation_rows = all_tag_values(board_log, "Stage7 Precipitation")
    overlay_rows = all_tag_values(board_log, "Stage7 PrecipitationOverlay")
    weather_perf_rows = all_tag_values(board_log, "Stage7 Perf")
    weather_config = last_tag_values(board_log, "Stage7 WeatherConfig") or {}
    precipitation_mode = weather_config.get("precipitationMode", "unknown")
    batch_draw_proof = any(
        row.get("precipitationMode") == "Batch" and int(float(row.get("precipitationNodeCount", "0"))) > 0
        for row in weather_perf_rows)
    overlay_draw_proof = any(
        row.get("mode") == "ScreenOverlay" and row.get("active") == "1"
        for row in overlay_rows)

    startup: dict[str, Any] = {
        "note": "first_valid_video_frame includes ordinary process wait before sender INIT; it is not render-only latency"
    }
    for match in re.finditer(r"(?m)^\[StartupTiming\]\s+milestone=([^\s]+)\s+elapsedMs=([0-9.]+)([^\r\n]*)", board_log):
        startup[match.group(1)] = {"elapsedMs": float(match.group(2)), **kv(match.group(3))}

    ingress_rows = all_tag_values(board_log, "RealtimeIngress")
    ingress_peak = {}
    for field in ("maxQueueDepth", "inputBackpressureCount", "inputBackpressureWaitMs",
                  "maxInputBackpressureWaitMs", "inputQueueOverflow", "inputOverwritten",
                  "sourceSeqGapCount"):
        ingress_peak[field] = max((float(row.get(field, "0")) for row in ingress_rows), default=0.0)

    codec_match = re.search(
        r"(?m)^\[H264EncodeSuccess\][^\r\n]*backend=mpp[^\r\n]*codec=h264_annexb[^\r\n]*resolution=800x800",
        board_log)
    hard80_cold = stage_report["acceptedToWriterSubmitMs"]["coldSourceSeq1To180"]["over80Count"]
    hard80_steady = stage_report["acceptedToWriterSubmitMs"]["steadyAfterSourceSeq180"]["over80Count"]
    decoded = int(final_metrics.get("decodedFrames", 0))
    decode_errors = int(final_metrics.get("decodeErrors", 0))
    rejected = int(final_metrics.get("statusIdentityRejected", 0))
    all_800 = all(int(row.get("width", 0)) == 800 and int(row.get("height", 0)) == 800 for row in rows)
    diagnostics_off = "[P6LinearCapturePerf]" not in board_log and "[P6LinearWriterPerf]" not in board_log
    # VideoEncoder is emitted concurrently with scene diagnostics and may be
    # textually interleaved on stdout.  H264EncodeSuccess is the per-frame,
    # post-encode proof and therefore the authoritative backend/resolution row.
    encoder_ok = bool(codec_match)
    recorder_ok = (str(status.get("fileError", "true")).lower() == "false" and
                   str(status.get("muxerFinalized", "false")).lower() == "true" and
                   str(status.get("sourceSeqContinuous", "false")).lower() == "true" and
                   str(status.get("frameSeqContinuous", "false")).lower() == "true")
    continuity_ok = sequence_audit(rows, "sourceSeq")["continuous"] and sequence_audit(rows, "frameSeq")["continuous"]

    checks = {
        "ordinaryDiagnosticsOff": diagnostics_off,
        "hardwareMppH264At800x800": encoder_ok,
        "allProducts800x800": all_800,
        "decodeErrorsZero": decode_errors == 0,
        "identityRejectedZero": rejected == 0,
        "recordingFinalizedAndContinuous": recorder_ok,
        "indexSequencesContinuous": continuity_ok,
        "decodedMatchesProducts": decoded == len(rows),
        "boardInputNotDroppedOrOverwritten": ingress_peak["inputQueueOverflow"] == 0 and ingress_peak["inputOverwritten"] == 0,
        "hard80ColdAcceptedToWriterSubmit": hard80_cold == 0,
        "hard80SteadyAcceptedToWriterSubmit": hard80_steady == 0,
    }

    artifacts = {}
    for path in [index_path, status_path, round_dir / "output.mp4", board_path, receiver_path,
                 ordinary / "receiver_actual_widget.png", ordinary / "DataDrivenTestQT.NetworkConfig.ini",
                 ordinary / "VideoDisplay.NetworkConfig.ini"]:
        if path.is_file():
            artifacts[str(path.relative_to(case)).replace("\\", "/")] = {
                "bytes": path.stat().st_size, "sha256": sha256(path)}

    result: dict[str, Any] = {
        "schema": "hwasimir.p12d.ordinary-performance.v1",
        "case": case.name, "requestedSeconds": requested_seconds, "band": band, "weather": weather,
        "clockPolicy": {
            "boardStages": "RK3588 steady clock only",
            "windowsStages": "Windows QPC steady clock only",
            "crossHost": "production outputLatencyMs estimate with uncertainty",
        },
        "windowsExeSha256": sha256(Path(r"D:\HwaSimIR\HwaSim_IR_VideoDisplay\x64\Release\HwaSim_IR_VideoDisplay.exe")),
        "products": len(rows),
        "sequences": {"sourceSeq": sequence_audit(rows, "sourceSeq"),
                      "frameSeq": sequence_audit(rows, "frameSeq")},
        "rateWindows": {
            "wholeCaseBoardWriterSubmit": fps(rows, "writerSubmitSteadyNs"),
            "wholeCaseWindowsGuiSubmit": fps(rows, "guiSubmitSteadyNs"),
            "steadyBoardWriterSubmit": {**fps(rate_window, "writerSubmitSteadyNs"),
                "definition": f"sourceSeq 181..{max_source - 60}; first 180 and final 60 excluded"},
            "steadyWindowsGuiSubmit": {**fps(rate_window, "guiSubmitSteadyNs"),
                "definition": f"sourceSeq 181..{max_source - 60}; first 180 and final 60 excluded"},
            "actualQtPaint": shutdown_paint,
        },
        "sameHostStageLatency": stage_report,
        "crossHostEstimatedLatency": cross_host,
        "startup": startup,
        "stopAndDrain": {
            "boardControlStopResponseMs": float(stop_control["responseMs"]) if "responseMs" in stop_control else None,
            "renderDrainWaitMs": float(finalizing["drainWaitMs"]) if "drainWaitMs" in finalizing else None,
            "ddsQueueDrainMs": float(drain["queueDrainMs"]) if "queueDrainMs" in drain else None,
            "ddsTotalDrainMs": float(drain["totalDrainMs"]) if "totalDrainMs" in drain else None,
            "stopTotalMs": float(stop_result["stopTotalMs"]) if "stopTotalMs" in stop_result else None,
            "outputRound": output_drain,
            "windowsRecorderFlush": flush,
        },
        "loadEvidence": {"cloudRenderCallAudit": cloud_audit, "ingressPeak": ingress_peak,
                         "finalRuntimeMetrics": final_metrics},
        "weatherDrawEvidence": {
            "configuredMode": precipitation_mode,
            "lastWeather": weather_rows[-1] if weather_rows else None,
            "lastPrecipitation": precipitation_rows[-1] if precipitation_rows else None,
            "lastOverlayLog": overlay_rows[-1] if overlay_rows else None,
            "lastWeatherPerf": weather_perf_rows[-1] if weather_perf_rows else None,
            "batchActualDrawProvedByPositiveNodeCount": batch_draw_proof,
            "screenOverlayActualDrawProvedByActiveFlag": overlay_draw_proof,
            "interpretation": "Batch uses precipitation nodes and keeps the overlay inactive; ScreenOverlay uses the final shader overlay. These are not interchangeable.",
        },
        "recordingStatus": status, "checks": checks,
        "hard80Policy": {
            "definition": "acceptedSteadyNs to writerSubmitSteadyNs on RK3588; cold and steady are independently visible",
            "coldOver80Count": hard80_cold, "steadyOver80Count": hard80_steady,
            "result": "PASS" if hard80_cold == 0 and hard80_steady == 0 else "FAIL",
        },
        "artifacts": artifacts,
    }
    non_hard_checks = [value for key, value in checks.items() if not key.startswith("hard80")]
    result["functionalResult"] = "PASS" if all(non_hard_checks) else "FAIL"
    result["performanceResult"] = "PASS" if all(checks.values()) else "FAIL"

    json_path = case / "performance.json"
    json_path.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    if csv_rows:
        with (case / "latency_all_frames.csv").open("w", newline="", encoding="utf-8-sig") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(csv_rows[0]))
            writer.writeheader(); writer.writerows(csv_rows)

    accepted = stage_report["acceptedToWriterSubmitMs"]
    paint = shutdown_paint or {}
    md = [
        f"# {case.name} P12D 性能摘要", "",
        f"- 功能链结果：**{result['functionalResult']}**；含 80 ms 硬门槛的性能结果：**{result['performanceResult']}**。",
        f"- 产品：{len(rows)} 帧；实际解码：{decoded}；identity 拒收：{rejected}；解码错误：{decode_errors}。",
        f"- 板端稳态输出：{result['rateWindows']['steadyBoardWriterSubmit']['fps']:.3f} fps；Windows GUI submit：{result['rateWindows']['steadyWindowsGuiSubmit']['fps']:.3f} fps。",
        f"- Qt 真正 Paint：{paint.get('steadyPaintedFrames', 'NA')} 帧，{paint.get('steadyPaintFps', 'NA')} fps，稳态最大绘制间隔 {paint.get('steadyMaxFrameIntervalMs', 'NA')} ms。",
        f"- RK3588 accepted→writer：冷启动 p99={accepted['coldSourceSeq1To180']['p99']:.3f} ms / max={accepted['coldSourceSeq1To180']['max']:.3f} ms / >80={hard80_cold}；稳态 p99={accepted['steadyAfterSourceSeq180']['p99']:.3f} ms / max={accepted['steadyAfterSourceSeq180']['max']:.3f} ms / >80={hard80_steady}。",
        f"- 跨机估计仅采用产品 clock-offset：稳态 p99={cross_host['steadyAfterSourceSeq180']['p99']:.3f} ms / max={cross_host['steadyAfterSourceSeq180']['max']:.3f} ms；不确定度 max={cross_host['clockUncertaintyMs']['max']:.3f} ms。",
        f"- STOP：控制响应 {result['stopAndDrain']['boardControlStopResponseMs']} ms，渲染排空 {result['stopAndDrain']['renderDrainWaitMs']} ms，DDS 总排空 {result['stopAndDrain']['ddsTotalDrainMs']} ms，完整停止 {result['stopAndDrain']['stopTotalMs']} ms。",
        "- 时钟边界：板端阶段、Windows 阶段分别在本机单调时钟内计算；跨机结果是带不确定度的估计，不混用 steady clock。",
    ]
    (case / "performance_summary.md").write_text("\n".join(md) + "\n", encoding="utf-8")
    print(json.dumps({"case": case.name, "functionalResult": result["functionalResult"],
                      "performanceResult": result["performanceResult"], "products": len(rows),
                      "coldOver80": hard80_cold, "steadyOver80": hard80_steady}))
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("case", type=Path)
    parser.add_argument("--requested-seconds", type=int, required=True)
    parser.add_argument("--band", choices=("SWIR", "MWIR"), required=True)
    parser.add_argument("--weather", required=True)
    args = parser.parse_args()
    analyze(args.case.resolve(), args.requested_seconds, args.band, args.weather)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
