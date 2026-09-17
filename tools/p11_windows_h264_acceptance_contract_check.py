#!/usr/bin/env python3
"""Static/unit contract checks for the P11 Windows H.264 acceptance tools."""

from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
from types import SimpleNamespace


ROOT = Path(__file__).resolve().parents[1]


def load_analyzer():
    path = ROOT / "tools/p11_windows_h264_acceptance_analyze.py"
    spec = importlib.util.spec_from_file_location("p11_h264_analyzer", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot import {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        type=Path,
        default=ROOT / "logs/p11/contract/p11_windows_h264_acceptance_contract.json",
    )
    args = parser.parse_args()
    analyzer = load_analyzer()
    wrapper_text = (ROOT / "tools/p11_windows_h264_60s_acceptance.ps1").read_text(encoding="utf-8")
    phase2a_text = (ROOT / "tools/phase2a_sync60_save_smoke.ps1").read_text(encoding="utf-8")
    analyzer_text = (ROOT / "tools/p11_windows_h264_acceptance_analyze.py").read_text(encoding="utf-8")
    tcp_receiver_text = (
        ROOT / "HwaSim_IR_VideoDisplay/HwaSim_IR_VideoDisplay/TcpServerWorker.cpp"
    ).read_text(encoding="utf-8")

    checks: dict[str, bool] = {
        "wrapper_defaults_both_bands": "[ValidateSet('Both','SWIR','NIR','MWIR')][string]$Band = 'Both'" in wrapper_text,
        "wrapper_defaults_60_seconds": "[ValidateRange(3, 120)][int]$Seconds = 60" in wrapper_text,
        "wrapper_requires_explicit_run": "[switch]$Run" in wrapper_text and "if (!$Run)" in wrapper_text,
        "wrapper_forces_ffmpeg": "H264Encoder='ffmpeg'" in wrapper_text,
        "wrapper_forbids_jpeg_fallback": "H264FallbackToJpeg='false'" in wrapper_text,
        "wrapper_requests_h264": "StimH264En='1'" in wrapper_text,
        "wrapper_runs_swir": "name='SWIR';protocol=0" in wrapper_text,
        "wrapper_runs_nir": "name='NIR';protocol=1" in wrapper_text,
        "wrapper_runs_mwir": "name='MWIR';protocol=2" in wrapper_text,
        "wrapper_checks_configuration_restore": "configurationRestored=" in wrapper_text,
        "phase2a_records_init_ack": "stimInitAckWaitMs" in phase2a_text and "stimInitAckObserved" in phase2a_text,
        "phase2a_records_sent": "sentFrames =" in phase2a_text,
        "phase2a_records_accepted": "acceptedFrames =" in phase2a_text,
        "phase2a_records_executed": "executedFrames =" in phase2a_text,
        "phase2a_records_rendered": "renderedFrames =" in phase2a_text,
        "phase2a_records_output": "outputFrames =" in phase2a_text,
        "phase2a_records_drain": "outputDrainCompletedFrames =" in phase2a_text,
        "analyzer_requires_800x800": "mp4_geometry_not_800x800" in analyzer_text,
        "analyzer_requires_h264": "mp4_codec_not_h264" in analyzer_text,
        "analyzer_requires_ffmpeg_encoder": "producer_not_ffmpeg_encoder" in analyzer_text,
        "analyzer_requires_ffmpeg_decoder": "ffmpeg_h264_decoder_configuration_missing" in analyzer_text,
        "analyzer_gates_count_conservation": "count_mismatch:" in analyzer_text,
        "analyzer_gates_source_frame_identity": "frame_identity_not_one_to_one" in analyzer_text,
        "analyzer_gates_keyframes": "first_h264_keyframe_missing" in analyzer_text,
        "analyzer_runs_independent_decode": '"-f", "null", "-"' in analyzer_text,
        "analyzer_reports_p95": '"p95Ms"' in analyzer_text,
        "analyzer_reports_p99": '"p99Ms"' in analyzer_text,
        "analyzer_preserves_over_80_list": "p11_windows_h264_over_80ms_frames.csv" in analyzer_text,
        "analyzer_maps_all_production_protocol_bands": (
            '{"SWIR": 0, "NIR": 1, "MWIR": 2}' in analyzer_text
            and 'choices=("SWIR", "NIR", "MWIR")' in analyzer_text
        ),
        "tcp_receiver_forwards_received_h264_au": (
            tcp_receiver_text.count("? parsed.encodedPayload : QByteArray()") >= 2
        ),
    }

    parsed = analyzer.parse_fields(
        "[Perf] renderFrames=3601 outputFrames=3601 latencyMaxMs=81.25 activeCodec=h264_annexb"
    )
    checks["parse_fields_exact"] = (
        parsed.get("renderFrames") == 3601
        and parsed.get("outputFrames") == 3601
        and parsed.get("latencyMaxMs") == 81.25
        and parsed.get("activeCodec") == "h264_annexb"
    )
    checks["percentile_interpolates"] = analyzer.percentile([0.0, 10.0, 20.0], 0.95) == 19.0

    missing_root = ROOT / "logs/p11/contract/nonexistent_h264_evidence"
    negative_args = SimpleNamespace(
        evidence_dir=missing_root,
        band="SWIR",
        expected_seconds=60.0,
        expected_fps=60.0,
        fps_tolerance=2.0,
        duration_tolerance=0.5,
        latency_threshold_ms=80.0,
        ffmpeg=ROOT / "missing-ffmpeg.exe",
        ffprobe=ROOT / "missing-ffprobe.exe",
    )
    negative_result, _ = analyzer.analyze(negative_args)
    checks["missing_evidence_fails_closed"] = (
        negative_result.get("result") == "FAIL" and len(negative_result.get("errors", [])) > 5
    )

    failures = sorted(name for name, passed in checks.items() if not passed)
    result = {
        "schema": "hwasimir_p11_windows_h264_acceptance_contract_1",
        "result": "PASS" if not failures else "FAIL",
        "passed": sum(1 for passed in checks.values() if passed),
        "total": len(checks),
        "checks": checks,
        "failures": failures,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, separators=(",", ":")))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
