#!/usr/bin/env python3
"""Offline static and adversarial tests for the P11 DDS lifecycle gate."""

from __future__ import annotations

import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import List


ROOT = Path(__file__).resolve().parents[1]
ANALYZER = ROOT / "tools" / "p11_rk3588_dds_lifecycle_analyze.py"
RUNNER = ROOT / "tools" / "p11_rk3588_dds_lifecycle.ps1"
BOARD = ROOT / "tools" / "p11_rk3588_dds_lifecycle_board_run.sh"


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def ledger(path: Path, digests: List[str], gap_after: int = -1) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    now = 1_000_000_000
    rows = []
    for index, digest in enumerate(digests, 1):
        if index > 1:
            now += 3_100_000_000 if index - 1 == gap_after else 16_666_667
        rows.append({"ordinal": index, "sourceSeq": index, "beginNs": now,
                     "endNs": now + 1000, "success": 1, "queueDepth": 0,
                     "digestFNV1a64": digest})
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def output_ledger(path: Path, count: int, gap_after: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    now = 2_000_000_000
    fields = ["sourceSeq", "steadyNs", "outputWorkBeginNs", "readbackMs", "convertMs",
              "encodeMs", "ddsEnqueueMs", "ddsBackpressureMs", "metaAnnotationMs", "totalMs"]
    rows = []
    for index in range(1, count + 1):
        if index > 1:
            now += 3_050_000_000 if index - 1 == gap_after else 16_666_667
        rows.append({"sourceSeq": index, "steadyNs": now, "outputWorkBeginNs": now - 1000,
                     "readbackMs": 1, "convertMs": 1, "encodeMs": 1, "ddsEnqueueMs": 0,
                     "ddsBackpressureMs": 0, "metaAnnotationMs": 0, "totalMs": 3})
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def controls(rounds: int) -> str:
    lines = []
    for round_id in range(1, rounds + 1):
        for command in (1, 2, 3):
            lines.append(f"[Stage0] Control command received: command={command}, round={round_id}/1")
            lines.append(f"[ControlResponseV3] command={command} round={round_id} receiveNs=1 executeNs=2 responseMs=0.1")
        lines.extend([
            f"[OutputRoundDrain] reason=stop round={round_id} targetFrames=2 completedFrames=2",
            "[ControlStopResult] outputDrainOk=1 stopForwardOk=1 acceptance=pass stopTotalMs=1",
            "[SyncRoundConservation] mode=sync acceptedRealtime=2 lastCapturedSourceSeq=2 inputMinusCaptured=0 queueDepth=0 staleFramePublished=0",
        ])
    return "\n".join(lines)


def board_header() -> str:
    return "\n".join([
        "[RuntimeInstance] component=HwaSimIR pid=777",
        "[RunPreflight] result=PASS commandTransport=dds",
        "[DdsVideo] initialized=1 domain=150",
        "[TcpPayloadConfig] SendVideo=0 SendAnnotation=0 SendRealtimeData=0 ForwardInitControl=0",
        "[ProtocolIngress] transport=dds type=control accepted=1",
        "[ProtocolIngress] transport=dds type=init accepted=1",
        "[ProtocolIngress] transport=dds type=realtime accepted=1",
    ])


def stim_text(band: int, pause: bool = False) -> str:
    lines = [
        "[StimTransportConfig] ControlTransport=dds source=cli",
        "[StimTransport] mode=dds ddsRuntimeInitCount=1 domain=150",
        "[StimDDS] type=control command=1 sent=1",
        f"[StimDDS] type=init sent=1 sensorBand={band}",
        "[StimDDS] type=control command=2 sent=1",
    ]
    if pause:
        lines.extend(["[StimPause] state=paused startSec=4 durationSec=3 sentFrames=4",
                      "[StimPause] state=resumed elapsedSec=7 sentFrames=4 catchUpBurst=0"])
    lines.extend(["[StimFinal] transport=dds successfulRealtimeWrites=10",
                  "[StimDDS] type=control command=3 sent=1"])
    return "\n".join(lines) + "\n"


def receiver_text() -> str:
    return "\n".join([
        "[DdsVideoReceiver] ready=1 initCount=1 domain=150",
        "[H264DecodeSuccess] backend=ffmpeg codec=h264_annexb resolution=800x800 keyFrame=true",
        "[DdsVideoReceiverSample] sample=1 bytes=30 codec=h264 ddsErrors=0",
        "[DdsVideoReceiverPerf] receivedSamples=2 ddsErrors=0",
        '[RuntimeMetricsV2] {"server":"abcdef0123456789","receivedSamples":"2"}',
    ]) + "\n"


def base_case(root: Path, scenario: str) -> None:
    runtime = {"process_result": "PASS"}
    if scenario == "receiver_restart":
        runtime.update({"receiver_before_pid": 101, "receiver_after_pid": 202,
                        "producer_pids": [777, 777]})
    elif scenario == "retained_reinit":
        runtime.update({"receiver_before_pid": 101, "producer_pids": [777, 777, 777, 777]})
    write(root / "runtime_status.json", json.dumps(runtime))


def make_pause(root: Path, band: str, protocol: int) -> None:
    base_case(root, "pause_resume")
    write(root / "board" / "hwa.log", board_header() + "\n" + controls(1) + "\n")
    write(root / "round_1" / "stim.err.log", stim_text(protocol, pause=True))
    write(root / "round_1" / "stim.out.log", "")
    write(root / "receiver" / "receiver.err.log", receiver_text())
    write(root / "receiver" / "receiver.out.log", "")
    values = [f"{index:016x}" for index in range(1, 11)]
    ledger(root / "round_1" / "audit" / "input_sender_a.csv", values, gap_after=4)
    ledger(root / "board" / "input_audit" / "input_accepted_a.csv", values)
    ledger(root / "board" / "input_audit" / "input_execute_a.csv", values)
    output_ledger(root / "board" / "input_audit" / "stage_output_a.csv", len(values), gap_after=4)
    ledger(root / "receiver" / "audit" / "input_received_a.csv", values)


def make_restart(root: Path) -> None:
    base_case(root, "receiver_restart")
    board = board_header() + "\n" + controls(1) + "\n" + "\n".join([
        "[ProductWriterTiming] session=0 generation=0 run=0 frameSeq=0",
        "[ProductWriterTiming] session=abcdef0123456789 generation=1 run=1 frameSeq=1",
        "[ProductWriterTiming] session=abcdef0123456789 generation=1 run=1 frameSeq=120",
    ]) + "\n"
    write(root / "board" / "hwa.log", board)
    write(root / "round_1" / "stim.err.log", stim_text(2))
    write(root / "round_1" / "stim.out.log", "")
    for name in ("receiver_before", "receiver_after"):
        write(root / name / "receiver.err.log", receiver_text())
        write(root / name / "receiver.out.log", "")
    # Annex-B SPS, PPS and IDR in recovery order.
    payload = b"\x00\x00\x00\x01\x67\x01\x00\x00\x01\x68\x02\x00\x00\x01\x65\x03"
    (root / "receiver_after" / "received_annexb.h264").write_bytes(payload)


def make_reinit(root: Path, stale: bool = False) -> None:
    base_case(root, "retained_reinit")
    segments = []
    for index, (protocol, band) in enumerate(((0, "SWIR"), (2, "MWIR"), (0, "SWIR")), 1):
        physical = "MWIR" if stale and index == 3 else band
        profile = "default_SWIR.json" if band == "SWIR" else "default_MWIR.json"
        segments.extend([
            f"[Stage1] Sensor profile (init-command): protocolBand={protocol}, band={band}, sensorRange=1-2um, source=/userdata/HwaSimIR/Config/SensorWave/{profile}",
            f"[M1 Compare] sourceSeq=1 band={physical} valid=1 fallbackReason=none responseMode=RectangularBand sourceFiles=interpolated_multiple_cases",
            f"[Stage5 RadianceComponents] sourceSeq=1 band={physical} finalOutput=M1",
            f"[ProductWriterTiming] session=abcdef0123456789 generation={index} run={index} frameSeq=1",
        ])
    write(root / "board" / "hwa.log", board_header() + "\n" + controls(3) + "\n" + "\n".join(segments) + "\n")
    all_values: List[str] = []
    for index, protocol in enumerate((0, 2, 0), 1):
        write(root / f"round_{index}" / "stim.err.log", stim_text(protocol))
        write(root / f"round_{index}" / "stim.out.log", "")
        values = [f"{index * 100 + value:016x}" for value in (1, 2)]
        all_values.extend(values)
        ledger(root / f"round_{index}" / "audit" / f"input_sender_{index}.csv", values)
    ledger(root / "board" / "input_audit" / "input_accepted_a.csv", all_values)
    ledger(root / "board" / "input_audit" / "input_execute_a.csv", all_values)
    write(root / "receiver" / "receiver.err.log", receiver_text())
    write(root / "receiver" / "receiver.out.log", "")
    ledger(root / "receiver" / "audit" / "input_received_a.csv", all_values)


def run_analyzer(case: Path, scenario: str, band: str, protocol: int) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [sys.executable, str(ANALYZER), "--case-dir", str(case), "--scenario", scenario,
         "--band", band, "--protocol-band", str(protocol), "--pause-duration-sec", "3"],
        text=True, capture_output=True, check=False,
    )


def main() -> int:
    checks = []
    runner = RUNNER.read_text(encoding="utf-8")
    board = BOARD.read_text(encoding="utf-8")
    checks.extend([
        ("runner_explicit_dds", "--control-transport=dds" in runner and "DomainId=150" in runner),
        ("runner_no_udp_transport", "--control-transport=udp" not in runner),
        ("board_command_dds", "HwaSimIRCommandTransportInput=dds" in board),
        ("board_tcp_payload_disabled", all(f"export {key}=false" in board for key in
                                           ("TcpSendVideo", "TcpSendAnnotation", "TcpSendRealtimeData", "TcpForwardInitControl"))),
        ("overall_schema", "hwasimir.p11.rk3588.dds-lifecycle-summary.v1" in runner),
        ("four_lifecycle_cases", all(name in runner for name in
                                      ("pause_swir", "pause_mwir", "receiver_restart", "retained_reinit"))),
    ])
    with tempfile.TemporaryDirectory(prefix="p11-dds-lifecycle-") as temp:
        temp_root = Path(temp)
        pause = temp_root / "pause"
        make_pause(pause, "SWIR", 0)
        result = run_analyzer(pause, "pause_resume", "SWIR", 0)
        checks.append(("pause_positive", result.returncode == 0))

        udp = temp_root / "pause_udp"
        make_pause(udp, "MWIR", 2)
        path = udp / "board" / "hwa.log"
        path.write_text(path.read_text(encoding="utf-8") + "[ProtocolIngress] transport=udp type=realtime accepted=1\n", encoding="utf-8")
        result = run_analyzer(udp, "pause_resume", "MWIR", 2)
        checks.append(("udp_ingress_rejected", result.returncode != 0))

        restart = temp_root / "restart"
        make_restart(restart)
        result = run_analyzer(restart, "receiver_restart", "MWIR", 2)
        checks.append(("restart_positive", result.returncode == 0))
        runtime_path = restart / "runtime_status.json"
        runtime = json.loads(runtime_path.read_text(encoding="utf-8"))
        runtime["receiver_after_pid"] = runtime["receiver_before_pid"]
        runtime_path.write_text(json.dumps(runtime), encoding="utf-8")
        result = run_analyzer(restart, "receiver_restart", "MWIR", 2)
        checks.append(("unchanged_receiver_pid_rejected", result.returncode != 0))

        reinit = temp_root / "reinit"
        make_reinit(reinit)
        result = run_analyzer(reinit, "retained_reinit", "MULTI", -1)
        checks.append(("reinit_positive", result.returncode == 0))
        stale = temp_root / "reinit_stale"
        make_reinit(stale, stale=True)
        result = run_analyzer(stale, "retained_reinit", "MULTI", -1)
        checks.append(("stale_band_rejected", result.returncode != 0))

    failed = [name for name, passed in checks if not passed]
    for name, passed in checks:
        print(f"[P11 DDS LifecycleTest] name={name} result={'PASS' if passed else 'FAIL'}")
    print(f"[P11 DDS LifecycleTest] result={'PASS' if not failed else 'FAIL'} passed={len(checks)-len(failed)}/{len(checks)}")
    return 0 if not failed else 1


if __name__ == "__main__":
    raise SystemExit(main())
