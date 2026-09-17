#!/usr/bin/env python3
"""Strict analyzer for P11 RK3588 DDS-only lifecycle evidence.

The runner deliberately separates evidence collection from judgement.  This
module never contacts the board and uses only standard-library Python, so the
same checks can be rerun from the offline delivery bundle.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import sys
from collections import Counter
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8-sig", errors="replace")
    except OSError:
        return ""


def metric(line: str, name: str) -> Optional[str]:
    match = re.search(r"(?:^|[\s,])" + re.escape(name) + r"=([^\s,]+)", line)
    return match.group(1) if match else None


def tagged_lines(text: str, tag: str) -> List[str]:
    return [line for line in text.splitlines() if tag in line]


class Gates:
    def __init__(self) -> None:
        self.rows: List[Dict[str, Any]] = []

    def add(self, name: str, passed: bool, detail: str) -> bool:
        self.rows.append({"gate": name, "pass": bool(passed), "detail": detail})
        return bool(passed)

    @property
    def passed(self) -> bool:
        return all(row["pass"] for row in self.rows)


def csv_rows(paths: Sequence[Path]) -> List[Dict[str, str]]:
    rows: List[Dict[str, str]] = []
    for path in sorted(paths):
        with path.open("r", encoding="utf-8-sig", newline="") as stream:
            rows.extend(csv.DictReader(stream))
    return rows


def ledger(case_dir: Path, pattern: str, gates: Gates, name: str) -> List[Dict[str, str]]:
    paths = sorted(case_dir.glob(pattern))
    try:
        rows = csv_rows(paths)
    except Exception as exc:
        gates.add(name, False, f"read_error={exc} paths={len(paths)}")
        return []
    valid = bool(paths) and bool(rows) and all(
        row.get("digestFNV1a64") and re.fullmatch(r"[0-9a-fA-F]{16}", row["digestFNV1a64"])
        for row in rows
    )
    gates.add(name, valid, f"files={len(paths)} rows={len(rows)}")
    return rows


def digests(rows: Sequence[Dict[str, str]]) -> List[str]:
    return [row["digestFNV1a64"].lower() for row in rows if row.get("digestFNV1a64")]


def int_metric(line: str, name: str, default: int = -1) -> int:
    try:
        value = metric(line, name)
        return int(value) if value is not None else default
    except ValueError:
        return default


def nal_types(payload: bytes) -> List[int]:
    starts: List[Tuple[int, int]] = []
    index = 0
    while index + 3 < len(payload):
        if payload[index:index + 4] == b"\x00\x00\x00\x01":
            starts.append((index, 4))
            index += 4
        elif payload[index:index + 3] == b"\x00\x00\x01":
            starts.append((index, 3))
            index += 3
        else:
            index += 1
    result: List[int] = []
    for offset, width in starts:
        nal = offset + width
        if nal < len(payload):
            result.append(payload[nal] & 0x1F)
    return result


def transport_gates(board_text: str, stim_texts: Sequence[str], receiver_texts: Sequence[str],
                    gates: Gates) -> Dict[str, Any]:
    ingress = tagged_lines(board_text, "[ProtocolIngress]")
    udp = [line for line in ingress if metric(line, "transport") == "udp"]
    non_dds = [line for line in ingress if metric(line, "transport") != "dds"]
    gates.add("dds_domain_150_board",
              "[DdsVideo] initialized=1" in board_text and " domain=150" in board_text,
              "publisher_initialized=%d" % int("[DdsVideo] initialized=1" in board_text))
    gates.add("dds_only_protocol_ingress", bool(ingress) and not non_dds,
              f"ingress={len(ingress)} nonDds={len(non_dds)}")
    gates.add("no_udp_ingress", not udp, f"udpIngress={len(udp)}")
    stim_ok = bool(stim_texts) and all(
        "[StimTransport] mode=dds" in text and " domain=150" in text and
        "[StimTransportConfig] ControlTransport=dds" in text for text in stim_texts
    )
    gates.add("dds_domain_150_stimulus", stim_ok, f"stimulusLogs={len(stim_texts)}")
    receiver_ok = bool(receiver_texts) and all(
        "[DdsVideoReceiver] ready=1" in text and " domain=150" in text for text in receiver_texts
    )
    gates.add("dds_domain_150_receiver", receiver_ok, f"receiverLogs={len(receiver_texts)}")

    tcp_keys = ("SendVideo", "SendAnnotation", "SendRealtimeData", "ForwardInitControl")
    tcp_lines = tagged_lines(board_text, "[TcpPayloadConfig]")
    effective = [line for line in tcp_lines if all(metric(line, key) is not None for key in tcp_keys)]
    tcp_counts = {key: sum(int_metric(line, key, 1) for line in effective) for key in tcp_keys}
    tcp_disabled = bool(effective) and all(value == 0 for value in tcp_counts.values()) and all(
        all(metric(line, key) == "0" for key in tcp_keys) for line in effective
    )
    gates.add("tcp_payload_all_disabled", tcp_disabled,
              "lines=%d %s" % (len(effective), " ".join(f"{k}={v}" for k, v in tcp_counts.items())))
    return {
        "kind": "DDS",
        "domain": 150,
        "udp_ingress_count": len(udp),
        "protocol_ingress_count": len(ingress),
        "tcp_payload_counts": tcp_counts,
    }


def common_lifecycle_gates(board_text: str, expected_rounds: int, gates: Gates) -> Dict[str, Any]:
    # The Stage0 line is emitted by the render-thread command executor and is
    # therefore the authoritative lifecycle sequence.  ControlResponseV3 is
    # emitted from a concurrent timing path and can be byte-interleaved with a
    # ControlStopPhase diagnostic on the RK console; do not turn that harmless
    # logging race into a false lifecycle failure.
    controls = [int_metric(line, "command") for line in
                tagged_lines(board_text, "[Stage0] Control command received:")]
    expected = [value for _ in range(expected_rounds) for value in (1, 2, 3)]
    gates.add("control_sequence_1_2_3", controls == expected,
              f"actual={controls} expected={expected}")
    conservations = tagged_lines(board_text, "[SyncRoundConservation]")
    conservation_ok = len(conservations) == expected_rounds and all(
        metric(line, "inputMinusCaptured") == "0" and metric(line, "queueDepth") == "0" and
        metric(line, "staleFramePublished") == "0" for line in conservations
    )
    gates.add("identity_conservation_and_empty_input_queue", conservation_ok,
              f"rounds={len(conservations)} expected={expected_rounds}")
    drains = tagged_lines(board_text, "[OutputRoundDrain] reason=stop round=")
    drain_ok = len(drains) == expected_rounds and all(
        metric(line, "targetFrames") == metric(line, "completedFrames") and
        int_metric(line, "targetFrames", 0) > 0 for line in drains
    )
    gates.add("output_drain", drain_ok, f"rounds={len(drains)} expected={expected_rounds}")
    stops = tagged_lines(board_text, "[ControlStopResult]")
    stop_ok = len(stops) == expected_rounds and all(
        metric(line, "outputDrainOk") == "1" and metric(line, "stopForwardOk") == "1" and
        metric(line, "acceptance") == "pass" for line in stops
    )
    gates.add("stop_ack_and_drain", stop_ok, f"rounds={len(stops)} expected={expected_rounds}")
    return {"controls": controls, "conservation": conservations, "drains": drains}


def error_gates(board_text: str, stim_texts: Sequence[str], receiver_texts: Sequence[str], gates: Gates) -> None:
    board_patterns = [
        r"\[RunPreflight\]\[FATAL\]", r"\[StartupFatal\]", r"hardwareGpu=0", r"llvmpipe",
        r"\[DdsVideo\]\[(?:ERROR|FATAL)\]", r"\[OutputRoundDrain\]\[ERROR\]",
        r"\[InputAuditV1\]\[ERROR\]", r"\[SensorProfileRequest\]\[ERROR\]",
        r"\[CodecFallback\]", r"DDSIF::[^\n]*failed", r"Send message failed:",
    ]
    stim_patterns = [r"\[StimDrain\]\[ERROR\]", r"\[StimDDS\]\[ERROR\]",
                     r"\[StimInitAck\]\[FATAL\]", r"DDS start failed"]
    receiver_patterns = [r"\[DdsVideoReceiver\]\[(?:ERROR|FATAL)\]",
                         r"\[H264Decoder\]\[ERROR\]"]
    board_bad = [pattern for pattern in board_patterns if re.search(pattern, board_text, re.I)]
    stim_bad = [pattern for pattern in stim_patterns if any(re.search(pattern, text, re.I) for text in stim_texts)]
    receiver_bad = [pattern for pattern in receiver_patterns if any(re.search(pattern, text, re.I) for text in receiver_texts)]
    gates.add("no_dds_producer_errors", not board_bad, f"patterns={board_bad}")
    gates.add("no_dds_stimulus_errors", not stim_bad, f"patterns={stim_bad}")
    gates.add("no_dds_decode_errors", not receiver_bad, f"patterns={receiver_bad}")
    producer_counter_errors = [line for line in board_text.splitlines()
                               if ("[DdsVideoPerf]" in line or "[DdsFrameProducts]" in line) and
                               int_metric(line, "writeErrors", 0) != 0]
    receiver_counter_errors = [line for text in receiver_texts for line in text.splitlines()
                               if "[DdsVideoReceiverPerf]" in line and int_metric(line, "ddsErrors", 0) != 0]
    gates.add("dds_error_counters_zero", not producer_counter_errors and not receiver_counter_errors,
              f"producerNonzero={len(producer_counter_errors)} receiverNonzero={len(receiver_counter_errors)}")


def exact_identity(case_dir: Path, gates: Gates, require_receiver: bool = True) -> Dict[str, int]:
    sender = ledger(case_dir, "round_*/audit/input_sender_*.csv", gates, "ledger_sender")
    accepted = ledger(case_dir, "board/input_audit/input_accepted_*.csv", gates, "ledger_accepted")
    execute = ledger(case_dir, "board/input_audit/input_execute_*.csv", gates, "ledger_execute")
    received = ledger(case_dir, "receiver*/audit/input_received_*.csv", gates, "ledger_received") if require_receiver else []
    sender_d = digests(sender)
    accepted_d = digests(accepted)
    execute_d = digests(execute)
    receiver_d = digests(received)
    equal = bool(sender_d) and sender_d == accepted_d == execute_d
    if require_receiver:
        equal = equal and sender_d == receiver_d
    gates.add("digest_identity_conservation", equal,
              "sender=%d accepted=%d execute=%d receiver=%d" %
              (len(sender_d), len(accepted_d), len(execute_d), len(receiver_d)))
    return {"sender": len(sender_d), "accepted": len(accepted_d),
            "execute": len(execute_d), "receiver": len(receiver_d)}


def analyze_pause(case_dir: Path, board_text: str, stim_texts: Sequence[str],
                  gates: Gates, pause_duration: float) -> Dict[str, Any]:
    common_lifecycle_gates(board_text, 1, gates)
    text = "\n".join(stim_texts)
    paused = tagged_lines(text, "[StimPause] state=paused")
    resumed = tagged_lines(text, "[StimPause] state=resumed")
    paused_count = int_metric(paused[-1], "sentFrames") if paused else -1
    resumed_count = int_metric(resumed[-1], "sentFrames") if resumed else -2
    stable = len(paused) == 1 and len(resumed) == 1 and paused_count >= 1 and paused_count == resumed_count
    gates.add("pause_sent_frames_unchanged", stable,
              f"paused={paused_count} resumed={resumed_count}")
    catch_up = bool(resumed and metric(resumed[-1], "catchUpBurst") == "0")
    gates.add("catch_up_burst_zero", catch_up, resumed[-1] if resumed else "missing")

    sender = ledger(case_dir, "round_*/audit/input_sender_*.csv", gates, "pause_sender_timing_ledger")
    begin_ns: List[int] = []
    try:
        begin_ns = [int(row["beginNs"]) for row in sender]
    except (KeyError, ValueError):
        pass
    split = paused_count
    gap_ns = begin_ns[split] - begin_ns[split - 1] if 0 < split < len(begin_ns) else -1
    gaps = [b - a for a, b in zip(begin_ns, begin_ns[1:])]
    max_index = gaps.index(max(gaps)) + 1 if gaps else -1
    output_paths = sorted(case_dir.glob("board/input_audit/stage_output_*.csv"))
    output_rows: List[Dict[str, str]] = []
    try:
        output_rows = csv_rows(output_paths)
        output_ns = [int(row["steadyNs"]) for row in output_rows]
    except (OSError, KeyError, ValueError):
        output_ns = []
    output_gaps = [b - a for a, b in zip(output_ns, output_ns[1:])]
    output_max_index = output_gaps.index(max(output_gaps)) + 1 if output_gaps else -1
    output_gap_ns = output_gaps[split - 1] if 0 < split <= len(output_gaps) else -1
    output_gap_ok = (len(output_rows) == len(sender) and output_gap_ns >= int(pause_duration * 0.70 * 1e9)
                     and output_max_index == split)
    gates.add("dds_output_emission_gap", output_gap_ok,
              f"rows={len(output_rows)} split={split} gapMs={output_gap_ns / 1e6 if output_gap_ns >= 0 else None} "
              f"maxGapIndex={output_max_index}")
    emission_gap = (gap_ns >= int(pause_duration * 0.80 * 1e9) and max_index == split and
                    output_gap_ok)
    gates.add("emission_gap", emission_gap,
              f"split={split} gapMs={gap_ns / 1e6 if gap_ns >= 0 else None} maxGapIndex={max_index}")
    nominal = sorted(value for index, value in enumerate(gaps, 1) if index != split)
    nominal_ns = nominal[len(nominal) // 2] if nominal else 0
    post = gaps[split:split + 5]
    # A scheduler may place the first regular sample immediately after the
    # explicit resume sample.  Permit that one boundary interval, but reject a
    # multi-sample catch-up burst; the ledgers below still require every digest
    # to be unique, ordered and conserved end to end.
    no_burst = (len(post) == 5 and nominal_ns > 0 and
                sum(value < nominal_ns * 0.45 for value in post) <= 1)
    output_post = output_gaps[split:split + 5]
    output_nominal = sorted(value for index, value in enumerate(output_gaps, 1) if index != split)
    output_nominal_ns = output_nominal[len(output_nominal) // 2] if output_nominal else 0
    output_no_burst = (len(output_post) == 5 and output_nominal_ns > 0 and
                       sum(value < output_nominal_ns * 0.35
                           for value in output_post) <= 1)
    gates.add("no_gap_emission", emission_gap and no_burst and output_no_burst,
              f"pauseSenderRows=0 pauseOutputRows=0 senderPostNs={post} outputPostNs={output_post} "
              f"senderNominalNs={nominal_ns} outputNominalNs={output_nominal_ns}")
    counts = exact_identity(case_dir, gates, require_receiver=True)
    return {"pause_sent_frames": paused_count, "resume_sent_frames": resumed_count,
            "emission_gap_ms": gap_ns / 1e6 if gap_ns >= 0 else None,
            "dds_output_emission_gap_ms": output_gap_ns / 1e6 if output_gap_ns >= 0 else None,
            "identity_counts": counts}


def analyze_restart(case_dir: Path, board_text: str, receiver_texts: Sequence[str],
                    runtime: Dict[str, Any], gates: Gates) -> Dict[str, Any]:
    common_lifecycle_gates(board_text, 1, gates)
    before_pid = runtime.get("receiver_before_pid")
    after_pid = runtime.get("receiver_after_pid")
    gates.add("receiver_pid_changed", isinstance(before_pid, int) and isinstance(after_pid, int) and
              before_pid > 0 and after_pid > 0 and before_pid != after_pid,
              f"before={before_pid} after={after_pid}")
    producer_pids = runtime.get("producer_pids", [])
    # ProductWriter emits an initialization diagnostic with session=0 before
    # the first accepted INIT command.  That sentinel is not a transport
    # session and must not make a stable producer look like it changed session
    # across a receiver-only restart.
    sessions = [
        value for value in re.findall(
            r"\[ProductWriterTiming\].*?\bsession=([0-9a-f]+)", board_text)
        if int(value, 16) != 0
    ]
    receiver_sessions = []
    for text in receiver_texts:
        observed = re.findall(r'"server":"([0-9a-f]+)"', text)
        receiver_sessions.append(sorted(set(observed)))
    producer_unchanged = (
        len(producer_pids) >= 2 and len(set(producer_pids)) == 1 and
        bool(sessions) and len(set(sessions)) == 1 and len(receiver_sessions) == 2 and
        all(values == sorted(set(sessions)) for values in receiver_sessions)
    )
    gates.add("producer_session_unchanged", producer_unchanged,
              f"pids={producer_pids} sessions={sorted(set(sessions))} receiverSessions={receiver_sessions}")
    # Do not infer chronology from lexical sorting ("after" sorts before
    # "before").  These paths are part of the lifecycle evidence contract.
    before = (read_text(case_dir / "receiver_before" / "receiver.err.log") + "\n" +
              read_text(case_dir / "receiver_before" / "receiver.out.log"))
    after = (read_text(case_dir / "receiver_after" / "receiver.err.log") + "\n" +
             read_text(case_dir / "receiver_after" / "receiver.out.log"))
    decoded_before = "[H264DecodeSuccess]" in before and "[DdsVideoReceiverSample]" in before
    decoded_after = "[H264DecodeSuccess]" in after and "[DdsVideoReceiverSample]" in after
    gates.add("decoded_before_restart", decoded_before, f"logPresent={int(bool(before))}")
    gates.add("decoded_after_restart", decoded_after, f"logPresent={int(bool(after))}")
    recovered = case_dir / "receiver_after" / "received_annexb.h264"
    types = nal_types(recovered.read_bytes()) if recovered.exists() else []
    try:
        idr = types.index(5)
        recovered_chain = 7 in types[:idr] and 8 in types[:idr]
    except ValueError:
        recovered_chain = False
    gates.add("recovered_sps_pps_idr", recovered_chain,
              f"nalPrefix={types[:32]} bytes={recovered.stat().st_size if recovered.exists() else 0}")
    gates.add("recovered_segment_decodable", recovered_chain and decoded_after,
              "receiverAfterH264DecodeSuccess=%d" % int(decoded_after))
    return {"receiver_before_pid": before_pid, "receiver_after_pid": after_pid,
            "producer_pids": producer_pids, "producer_sessions": sorted(set(sessions)),
            "recovered_nal_prefix": types[:32]}


def analyze_reinit(case_dir: Path, board_text: str, stim_texts: Sequence[str],
                   runtime: Dict[str, Any], gates: Gates) -> Dict[str, Any]:
    common_lifecycle_gates(board_text, 3, gates)
    expected = [0, 2, 0]
    stim_controls: List[List[int]] = []
    for text in stim_texts:
        stim_controls.append([int_metric(line, "command") for line in tagged_lines(text, "[StimDDS] type=control")])
    gates.add("each_reset_init_start_stop", len(stim_controls) == 3 and all(row == [1, 2, 3] for row in stim_controls),
              f"stimulusControls={stim_controls}")
    producer_pids = runtime.get("producer_pids", [])
    runtime_instances = re.findall(r"\[RuntimeInstance\].*?\bpid=([0-9]+)", board_text)
    retained = len(producer_pids) >= 4 and len(set(producer_pids)) == 1 and len(set(runtime_instances)) == 1
    gates.add("retained_renderer", retained,
              f"probes={producer_pids} runtimeInstances={sorted(set(runtime_instances))}")

    profile_pattern = re.compile(
        r"\[Stage1\] Sensor profile \(init-command\): protocolBand=([0-9]+), band=(SWIR|MWIR).*?source=([^\s]+)")
    profiles = [(match.start(), int(match.group(1)), match.group(2), match.group(3))
                for match in profile_pattern.finditer(board_text)]
    actual_bands = [item[1] for item in profiles]
    gates.add("band_sequence_swir_mwir_swir", actual_bands == expected,
              f"actual={actual_bands} expected={expected}")
    profile_refresh = (
        len(profiles) == 3 and
        [item[2] for item in profiles] == ["SWIR", "MWIR", "SWIR"] and
        all(("default_SWIR.json" in item[3]) if item[2] == "SWIR"
            else ("default_MWIR.json" in item[3]) for item in profiles)
    )
    gates.add("profile_refresh", profile_refresh,
              f"profiles={[(item[1], item[2], item[3]) for item in profiles]}")

    physical_bands: List[List[str]] = []
    lut_sources: List[List[str]] = []
    for index, profile in enumerate(profiles):
        end = profiles[index + 1][0] if index + 1 < len(profiles) else len(board_text)
        segment = board_text[profile[0]:end]
        # Console writes from the DDS publisher and render thread can share a
        # byte stream.  A physically meaningful row must begin with its own
        # tag; a tag embedded inside another diagnostic line is an interleaved
        # fragment, not a stale-band frame.
        segment_lines = segment.splitlines()
        m1_lines = [line for line in segment_lines if line.startswith("[M1 Compare]")]
        physical = [metric(line, "band") or "" for line in m1_lines]
        stage5_bands = [metric(line, "band") or "" for line in
                        segment_lines if line.startswith("[Stage5 RadianceComponents]")]
        physical.extend(stage5_bands)
        sources = [metric(line, "interpolation") or metric(line, "sourceFiles") or "" for line in m1_lines
                   if metric(line, "valid") == "1" and metric(line, "fallbackReason") == "none" and
                   metric(line, "responseMode") == "RectangularBand"]
        physical_bands.append(physical)
        lut_sources.append(sources)
    expected_names = ["SWIR", "MWIR", "SWIR"]
    refresh = (
        len(physical_bands) == 3 and
        all(rows and all(value == expected_names[i] for value in rows)
            for i, rows in enumerate(physical_bands)) and
        all(rows and all(value for value in rows) for rows in lut_sources)
    )
    gates.add("lut_refresh", refresh,
              f"bands={[rows[:5] for rows in physical_bands]} sources={[rows[:2] for rows in lut_sources]}")
    stale = sum(value != expected_names[i] for i, rows in enumerate(physical_bands) for value in rows)
    gates.add("no_stale_band_frame", len(physical_bands) == 3 and stale == 0 and all(physical_bands),
              f"stale={stale} perRoundCounts={[len(rows) for rows in physical_bands]}")
    counts = exact_identity(case_dir, gates, require_receiver=True)

    product_rows = [row for row in re.findall(
        r"\[ProductWriterTiming\].*?\bsession=([0-9a-f]+).*?\bgeneration=([0-9]+).*?\brun=([0-9]+)", board_text)
        if int(row[0], 16) != 0]
    sessions = {row[0] for row in product_rows}
    generations = sorted({int(row[1]) for row in product_rows})
    gates.add("product_identity_refreshed_without_session_change",
              len(sessions) == 1 and len(generations) >= 3,
              f"sessions={sorted(sessions)} generations={generations}")
    return {"protocol_band_sequence": actual_bands, "profile_bands": [item[2] for item in profiles],
            "producer_pids": producer_pids, "product_sessions": sorted(sessions),
            "product_generations": generations, "identity_counts": counts}


def artifact_rows(case_dir: Path) -> List[Dict[str, Any]]:
    # The analyzer stdout/stderr files are still open while this function runs;
    # the root manifest hashes them after process exit instead.
    ignored = {"lifecycle_summary.json", "artifact_manifest.sha256",
               "analyzer.out.log", "analyzer.err.log"}
    rows: List[Dict[str, Any]] = []
    for path in sorted(item for item in case_dir.rglob("*") if item.is_file() and item.name not in ignored):
        rows.append({"path": path.relative_to(case_dir).as_posix(), "bytes": path.stat().st_size,
                     "sha256": sha256_file(path)})
    return rows


def analyze(args: argparse.Namespace) -> int:
    case_dir = args.case_dir.resolve()
    gates = Gates()
    board_path = case_dir / "board" / "hwa.log"
    board_text = read_text(board_path)
    gates.add("board_log", bool(board_text), f"path={board_path} bytes={len(board_text.encode('utf-8'))}")
    stim_paths = sorted(case_dir.glob("round_*/stim.err.log"))
    stim_texts = [read_text(path) + "\n" + read_text(path.with_name("stim.out.log")) for path in stim_paths]
    gates.add("stimulus_logs", bool(stim_texts) and all(text.strip() for text in stim_texts),
              f"count={len(stim_texts)}")
    receiver_paths = sorted(case_dir.glob("receiver*/receiver.err.log"))
    receiver_texts = [read_text(path) + "\n" + read_text(path.with_name("receiver.out.log")) for path in receiver_paths]
    gates.add("receiver_logs", bool(receiver_texts) and all(text.strip() for text in receiver_texts),
              f"count={len(receiver_texts)}")
    runtime_path = case_dir / "runtime_status.json"
    try:
        runtime = json.loads(read_text(runtime_path))
    except (ValueError, OSError):
        runtime = {}
    gates.add("runtime_status", bool(runtime) and runtime.get("process_result") == "PASS",
              f"processResult={runtime.get('process_result')}")

    transport = transport_gates(board_text, stim_texts, receiver_texts, gates)
    error_gates(board_text, stim_texts, receiver_texts, gates)
    if args.scenario == "pause_resume":
        metrics = analyze_pause(case_dir, board_text, stim_texts, gates, args.pause_duration_sec)
    elif args.scenario == "receiver_restart":
        metrics = analyze_restart(case_dir, board_text, receiver_texts, runtime, gates)
    else:
        metrics = analyze_reinit(case_dir, board_text, stim_texts, runtime, gates)

    artifacts = artifact_rows(case_dir)
    summary: Dict[str, Any] = {
        "schema": "hwasimir.p11.rk3588.dds-lifecycle-case.v1",
        "result": "PASS" if gates.passed else "FAIL",
        "scenario": args.scenario,
        "band": args.band,
        "protocol_band": args.protocol_band,
        "transport": transport,
        "metrics": metrics,
        "gates": gates.rows,
        "artifacts": artifacts,
    }
    summary_path = case_dir / "lifecycle_summary.json"
    summary_path.write_text(json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    manifest_rows = []
    ignored_manifest = {"artifact_manifest.sha256", "analyzer.out.log", "analyzer.err.log"}
    for path in sorted(item for item in case_dir.rglob("*") if item.is_file() and item.name not in ignored_manifest):
        manifest_rows.append(f"{sha256_file(path)}  {path.relative_to(case_dir).as_posix()}")
    (case_dir / "artifact_manifest.sha256").write_text("\n".join(manifest_rows) + "\n", encoding="utf-8")
    failed = [row for row in gates.rows if not row["pass"]]
    print(f"[P11 DDS LifecycleAnalyze] scenario={args.scenario} result={summary['result']} "
          f"failed={len(failed)} output={summary_path}")
    return 0 if gates.passed else 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--case-dir", type=Path, required=True)
    parser.add_argument("--scenario", choices=("pause_resume", "receiver_restart", "retained_reinit"), required=True)
    parser.add_argument("--band", choices=("SWIR", "MWIR", "MULTI"), required=True)
    parser.add_argument("--protocol-band", type=int, required=True)
    parser.add_argument("--pause-duration-sec", type=float, default=3.0)
    return analyze(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
