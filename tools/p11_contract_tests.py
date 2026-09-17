#!/usr/bin/env python3
"""Short P11 protocol/profile and formal-evidence contract tests.

These tests do not launch a rendering matrix.  They combine source-level
mapping checks, the production C++ profile loader executable, the actual Qt
stimulus CLI's early-rejection path, and synthetic log rows for the evidence
hard gate.
"""

from __future__ import annotations

import copy
import importlib.util
import json
import os
import re
import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROFILE_EXE = ROOT / "logs/p10/bin/p10_profile_check.exe"
STIMULUS_EXE = ROOT / "build-DataDrivenTestQT-codex-mingw73_64-Release/release/DataDrivenTestQT.exe"
PROFILE_ROOT = ROOT / "HwaSim_IR/Bin/Config/SensorWave"
REPORT = ROOT / "logs/p11/tests/contracts.json"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def load_finalizer():
    path = ROOT / "tools/p11_windows_evidence_finalize.py"
    spec = importlib.util.spec_from_file_location("p11_windows_evidence_finalize", path)
    require(spec is not None and spec.loader is not None, "cannot load P11 finalizer")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def source_contract_tests(rows: list[dict[str, object]]) -> None:
    ir_types = (ROOT / "HwaSim_IR/HwaSim_IR/IR/IRTypes.cpp").read_text(encoding="utf-8")
    mapping_block = re.search(
        r"IRBand\s+IRBandFromProtocol\s*\([^)]*\)\s*\{(.*?)\n\}", ir_types, re.DOTALL
    )
    require(mapping_block is not None, "IRBandFromProtocol body missing")
    mapping = {
        int(value): band
        for value, band in re.findall(
            r"case\s+(-?\d+)\s*:\s*return\s+IRBand::([A-Za-z]+)", mapping_block.group(1)
        )
    }
    expected = {0: "ShortWaveInfrared", 1: "NearInfrared", 2: "MidWaveInfrared"}
    require(all(mapping.get(value) == band for value, band in expected.items()), str(mapping))
    require(mapping.get(4) == "Visible", "protocol 4 changed from VIS or aliases VIS-SWIR")
    require('case IRBand::Visible: return "default_LLLTV.json";' in ir_types, "VIS profile mapping")
    require("default_VIS-SWIR.json" not in ir_types, "VIS-SWIR gained a protocol mapping")

    ui = (ROOT / "DataDrivenTestQT/p7_sensor_form.inl").read_text(encoding="utf-8")
    ui_items = {
        int(value): statement
        for statement, value in re.findall(r"(box->addItem\(.*?,\s*([0-4])\s*\);)", ui)
    }
    require(sorted(ui_items) == [0, 1, 2, 3, 4], f"UI itemData changed: {ui_items}")
    for value, band in ((0, "SWIR"), (1, "NIR"), (2, "MWIR")):
        require(band in ui_items[value], f"UI mapping {value}!={band}")
    require("unsupported" in ui_items[3], "LWIR unsupported label missing")
    require("unsupported" in ui_items[4] and "not VIS-SWIR" in ui_items[4], "VIS disclaimer missing")

    main = (ROOT / "DataDrivenTestQT/main.cpp").read_text(encoding="utf-8")
    cli = re.search(
        r"const QString sensorBandPrefix.*?(?=const QString sensorPixelAnglePrefix)", main, re.DOTALL
    )
    require(cli is not None, "sensor-band CLI block missing")
    for token in (
        "toInt(&ok, 10)", "QString::number(requested) != sensorBandText",
        "requested < 0 || requested > 2", "return 64;", "VIS-SWIR are unsupported",
    ):
        require(token in cli.group(0), f"strict CLI token missing: {token}")
    require("qBound" not in cli.group(0), "sensor-band still clamps invalid values")
    rows.append({"case": "static_protocol_ui_cli_contract", "result": "PASS"})

    evidence_runner = (ROOT / "tools/p11_windows_evidence.ps1").read_text(encoding="utf-8")
    require(
        "@('illuminatorOnStartSec','--illuminator-on-start-sec=', [bool]$scenario.illuminator)"
        in evidence_runner,
        "OFF evidence cases can still pass an enabling illuminator schedule",
    )
    require(
        "@('illuminatorOnEndSec','--illuminator-on-end-sec=', [bool]$scenario.illuminator)"
        in evidence_runner,
        "OFF evidence cases can still pass an illuminator end schedule",
    )
    rows.append({"case": "evidence_off_case_omits_illuminator_schedule", "result": "PASS"})

    for token in (
        "EnableAeroThermalModel='true'",
        "ApplyAeroToRadiance='true'",
        "AeroApplyOnlyBand='SWIR_MWIR'",
        "formalProgramIdentities=$formalProgramIdentities",
    ):
        require(token in evidence_runner, f"formal image matrix aero gate missing: {token}")
    rows.append({"case": "evidence_matrix_applies_local_aero_to_formal_bands", "result": "PASS"})

    phase2a_runner = (
        ROOT / "tools/phase2a_sync60_save_smoke.ps1"
    ).read_text(encoding="utf-8")
    runtime_config = (
        ROOT / "HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini"
    ).read_text(encoding="utf-8")
    require(
        "[int]$OrderedInputQueueMaxFrames = 256" in phase2a_runner
        and '"RenderControl" "AsyncInputQueueMaxFrames" ([string]$OrderedInputQueueMaxFrames)'
        in phase2a_runner,
        "formal UDP round does not provision its ordered transient reservoir",
    )
    require(
        "[int]$InitAckTimeoutSeconds = 30" in phase2a_runner
        and "[int]$StopCompletionTimeoutSeconds = 60" in phase2a_runner,
        "formal UDP round timeout defaults are not InitAck=30s/StopDrain=60s",
    )
    h264_runner = (
        ROOT / "tools/p11_windows_h264_60s_acceptance.ps1"
    ).read_text(encoding="utf-8")
    for runner_name, runner_source in (
        ("matrix", evidence_runner),
        ("h264", h264_runner),
    ):
        require(
            "InitAckTimeoutSeconds=30;StopCompletionTimeoutSeconds=60" in runner_source,
            f"{runner_name} runner does not explicitly bind InitAck=30s/StopDrain=60s",
        )
    require(
        re.search(r"(?m)^AsyncInputPolicy=OrderedQueue$", runtime_config) is not None
        and re.search(r"(?m)^AsyncInputQueueMaxFrames=4096$", runtime_config) is not None,
        "deployed runtime does not preserve a 4096-frame OrderedQueue reservoir",
    )
    rows.append({"case": "ordered_dds_round_reservoir_and_timeout_contract", "result": "PASS"})

    finalizer_source = (
        ROOT / "tools/p11_windows_evidence_finalize.py"
    ).read_text(encoding="utf-8")
    require(
        "stop_total_over_80ms_or_missing" not in finalizer_source,
        "STOP+drain wall time is still misclassified as per-frame >80 ms latency",
    )
    require(
        '"stopTotalOver80Ms"' in finalizer_source
        and '"outputDrainTimeout"' in finalizer_source,
        "STOP/drain timing is not preserved as separate evidence",
    )
    rows.append({"case": "stop_drain_separate_from_frame_latency", "result": "PASS"})
    require(
        '"finalizer": {' in finalizer_source
        and '"sha256": sha256(Path(__file__).resolve())' in finalizer_source,
        "case evidence does not bind the exact finalizer identity",
    )
    rows.append({"case": "case_manifest_binds_finalizer_sha256", "result": "PASS"})

    matrix = json.loads(
        (ROOT / "tools/p11_windows_evidence_matrix.json").read_text(encoding="utf-8")
    )
    checked_dynamic_fixtures: set[str] = set()
    for scenario in matrix.get("scenarios", []):
        fixture_name = scenario.get("fixture", matrix.get("baseFixture"))
        fixture = json.loads((ROOT / fixture_name).read_text(encoding="utf-8-sig"))
        state_rows = fixture.get("SyntheticTelemetryKeyframes", [])
        if state_rows:
            require(
                all(isinstance(row, list) and len(row) == 9 for row in state_rows),
                f"invalid dynamic telemetry row width: {fixture_name}",
            )
            require(
                state_rows[0][0] == 0
                and all(state_rows[index][0] < state_rows[index + 1][0] for index in range(len(state_rows) - 1)),
                f"invalid dynamic telemetry time order: {fixture_name}",
            )
            require(
                all(row[8] in (0, 1) for row in state_rows),
                f"invalid dynamic engine state: {fixture_name}",
            )
            checked_dynamic_fixtures.add(str(fixture_name))
        if scenario.get("dynamic") is True:
            require(bool(state_rows), f"dynamic scenario has no state rows: {scenario.get('id')}")
    require(len(checked_dynamic_fixtures) >= 2, "expected solar and speed dynamic fixtures")
    rows.append({"case": "dynamic_fixture_rows_are_full_width", "result": "PASS"})


def executable_profile_tests(rows: list[dict[str, object]]) -> None:
    require(PROFILE_EXE.is_file(), f"missing {PROFILE_EXE}")
    environment = os.environ.copy()
    opencv_bin = ROOT / "HwaSim_IR/HwaSim_IR/opencv2-440/opencv_x64/vc14/bin"
    environment["PATH"] = str(opencv_bin) + os.pathsep + environment.get("PATH", "")
    cases = (
        (0, "default_SWIR.json", "SWIR", 0),
        (1, "default_NVG.json", "NIR", 0),
        (2, "default_MWIR.json", "MWIR", 0),
        (3, "default_LWIR.json", "LWIR", 2),
        (4, "default_LLLTV.json", "VIS", 2),
        (4, "default_VIS-SWIR.json", "VIS-SWIR", 2),
        (-1, "default_MWIR.json", "invalid-negative", 2),
        (5, "default_MWIR.json", "invalid-high", 2),
    )
    with tempfile.TemporaryDirectory(prefix="p11-profile-contract-") as directory:
        for protocol, filename, name, expected_exit in cases:
            output = Path(directory) / f"{protocol}-{name}.json"
            process = subprocess.run(
                [str(PROFILE_EXE), str(PROFILE_ROOT / filename), str(protocol), str(output)],
                capture_output=True, text=True, timeout=10, env=environment,
            )
            require(
                process.returncode == expected_exit,
                f"profile executable {name}: {process.returncode}; {process.stdout}; {process.stderr}",
            )
            result = json.loads(output.read_text(encoding="utf-8-sig"))
            if expected_exit == 0:
                require(result.get("fixedBand") == name, f"profile executable mapping {protocol}: {result}")
                require(int(result.get("productionSupported", 0)) == 1, f"production gate {protocol}")
            else:
                require(int(result.get("productionSupported", 1)) == 0, f"unsupported protocol {protocol}")
            rows.append({
                "case": f"profile_executable_protocol_{protocol}_{name}",
                "exit": process.returncode, "result": "PASS",
            })


def stimulus_invalid_cli_tests(rows: list[dict[str, object]]) -> None:
    require(STIMULUS_EXE.is_file(), f"missing {STIMULUS_EXE}")
    environment = os.environ.copy()
    environment["QT_QPA_PLATFORM"] = "windows"
    cases = [
        ([f"--sensor-band={value}"], value or "empty")
        for value in ("", "SWIR", "VIS-SWIR", "3", "4", "-1", "5", "1.0", "01", "+1")
    ] + [
        (["--sensor-band"], "missing_separate_value"),
        (["--sensor-band", "VIS-SWIR"], "separate_VIS-SWIR"),
        (["--sensor-band", "4"], "separate_4"),
    ]
    for arguments, name in cases:
        process = subprocess.run(
            [str(STIMULUS_EXE), *arguments],
            cwd=str(STIMULUS_EXE.parent), env=environment,
            capture_output=True, text=True, timeout=8,
        )
        require(process.returncode == 64, f"Qt CLI accepted {arguments!r}: exit={process.returncode}")
        rows.append({"case": f"stimulus_reject_{name}", "exit": process.returncode, "result": "PASS"})


def evidence_gate_tests(rows: list[dict[str, object]]) -> None:
    finalizer = load_finalizer()
    duration = finalizer.duration_boundary_values(
        [{"producerPtsMs": 0}, {"producerPtsMs": 59016}],
        {"simTimeMs": 1782043200000},
        {"simTimeMs": 1782043261995},
        62000,
    )
    require(duration["encodedTimelineDurationMs"] == 59016, str(duration))
    require(duration["scenarioDurationMs"] == 61995, str(duration))
    require(duration["durationDeltaMs"] == -5, str(duration))
    require(
        duration["durationMetric"] == "producer_annotation.simTimeMs",
        str(duration),
    )
    rows.append({"case": "duration_boundary_uses_protocol_sim_time", "result": "PASS"})

    summary = {
        "stimSimMode": 1, "stimSensorBand": 0,
        "sourceSeqContinuous": 1, "frameSeqContinuousWritten": 1,
        "sourceSeqContinuousWritten": 1,
        "inputQueueOverflow": 0, "tcpOverwritten": 0,
        "recorderDroppedFrames": 0, "h264DecodeErrors": 0,
        "recorderInputFrames": 3, "writtenFrames": 3,
        "frameSeqWritten": 3, "sourceSeqWritten": 3, "mp4Frames": 3,
    }
    hwa = "\n".join((
        "[RenderControl] effectiveSimMode=1 asyncInputPolicy=OrderedQueue",
        "[Perf] mode=sync renderFrames=3 outputFrames=3 inputQueueOverflowCount=0 dropped=0 inputOverwritten=0 outputOverwritten=0 overwritten=0",
        "[RealtimeIngress] appRealtimeQueued=3 appRealtimeConsumed=3 inputQueueOverflow=0 inputOverwritten=0 sourceSeqGapCount=0",
        "[SyncFrame] sourceSeq=3 sourceSeqContinuous=1",
        "[TcpPerf] sourceSeq=3 overwritten=0",
        "[SyncRoundConservation] mode=sync acceptedRealtime=3 lastCapturedSourceSeq=3 inputMinusCaptured=0 queueDepth=0 staleFramePublished=0",
    ))
    video = "\n".join((
        "[TcpFramePacketRx] frameSeq=3 outputOrdinal=3",
        "[VideoPerf] sourceSeq=3 sourceSeqContinuous=1 discontinuities=0 h264DecodeErrors=0",
        "[RecorderPerf] droppedFrames=0 frameSeqWritten=3 sourceSeqWritten=3 frameSeqContinuousWritten=1 sourceSeqContinuousWritten=1",
        "[RecorderFlush] inputFrames=3 writtenFrames=3 frameSeqWritten=3 sourceSeqWritten=3 droppedFrames=0 frameSeqContinuousWritten=1 sourceSeqContinuousWritten=1 completed=1",
    ))
    stim = "[StimFinal] successfulRealtimeWrites=3"
    good = finalizer.validate_formal_chain(summary, hwa, video, stim, 0)
    require(good["result"] == "PASS", f"valid evidence gate rejected: {good}")
    rows.append({"case": "formal_chain_gate_valid", "result": "PASS"})

    mutations = (
        ("latest", summary, hwa.replace("OrderedQueue", "Latest"), video, stim, "ordered_queue_not_observed"),
        ("async", {**summary, "stimSimMode": 2}, hwa, video, stim, "stim_sim_mode_not_sync"),
        ("renderer_async", summary, hwa.replace("[Perf] mode=sync", "[Perf] mode=async"), video, stim, "renderer_perf_not_sync"),
        ("gap", {**summary, "sourceSeqContinuous": 0}, hwa, video, stim, "summary_not_continuous"),
        ("count", {**summary, "mp4Frames": 2}, hwa, video, stim, "count_mismatch:receivedMp4"),
        ("overflow", {**summary, "inputQueueOverflow": 1}, hwa, video, stim, "zero_counter_nonzero:summary.inputQueueOverflow"),
        ("overwrite", summary, hwa.replace("outputOverwritten=0", "outputOverwritten=1"), video, stim, "zero_counter_nonzero:perf.outputOverwritten"),
        ("drop", {**summary, "recorderDroppedFrames": 1}, hwa, video, stim, "zero_counter_nonzero:summary.recorderDroppedFrames"),
    )
    for name, test_summary, test_hwa, test_video, test_stim, expected_error in mutations:
        result = finalizer.validate_formal_chain(
            copy.deepcopy(test_summary), test_hwa, test_video, test_stim, 0
        )
        require(result["result"] == "FAIL", f"invalid evidence passed: {name}")
        require(any(expected_error in error for error in result["errors"]), f"{name}: {result}")
        rows.append({"case": f"formal_chain_gate_reject_{name}", "result": "PASS"})


def agc_evidence_gate_tests(rows: list[dict[str, object]]) -> None:
    finalizer = load_finalizer()
    interleaved_perf = finalizer.parse_fields(
        "mode=sync outputFrames=1[Stage6 AGC] sourceSeq=2 mode=Percentile valid=0"
    )
    require(interleaved_perf.get("mode") == "sync", str(interleaved_perf))
    rows.append({"case": "log_parser_keeps_prefix_tag_first_key", "result": "PASS"})
    matrix = json.loads((ROOT / "tools/p11_windows_evidence_matrix.json").read_text(encoding="utf-8"))
    valid_required_ids = {
        scenario["id"] for scenario in matrix["scenarios"]
        if scenario.get("requireAgcValid") is True
    }
    require(
        valid_required_ids == {
            "target_near_100m", "controlled_sample_solar_off", "controlled_sample_solar_on",
        },
        f"unexpected requireAgcValid matrix cases: {sorted(valid_required_ids)}",
    )
    rows.append({"case": "agc_matrix_explicit_valid_required_cases", "result": "PASS"})

    scenarios = {scenario["id"]: scenario for scenario in matrix["scenarios"]}
    controlled_fovs = {
        scenarios[case_id].get("sensorPixelAngleUrad")
        for case_id in ("controlled_sample_solar_off", "controlled_sample_solar_on")
    }
    require(
        controlled_fovs == {100},
        f"controlled sample A/B must share the same near-view FOV: {controlled_fovs}",
    )
    require(
        scenarios["solar_thermal_inertia_cycle"].get("sensorPixelAngleUrad") == 50,
        "dynamic solar target is not large enough for auditable local contrast",
    )
    rows.append({"case": "agc_representative_near_view_fov_contract", "result": "PASS"})

    enabled = (
        "[Stage6 AGC] sourceSeq=20 enabled=1 effective=1 mode=Percentile "
        "statsSource=stratified_pixel_centers_unclipped_linear_order_statistics "
        "updateHz=30.000 sampleCount=4096 lowPercentile=2.000 highPercentile=98.000 "
        "lowInput=0.013 highInput=0.030 gain=7.677 offset=-0.049 "
        "gainSmoothed=7.677 offsetSmoothed=-0.049 valid=1 fallbackReason=none "
        "stage6AgcStatsMs=0.240"
    )
    low_contrast = (
        "[Stage6 AGC] sourceSeq=20 enabled=1 effective=1 mode=Percentile "
        "statsSource=stratified_pixel_centers_unclipped_linear_order_statistics "
        "updateHz=30.000 sampleCount=4096 lowPercentile=2.000 highPercentile=98.000 "
        "lowInput=0.013 highInput=0.030 gain=7.677 offset=-0.049 "
        "gainSmoothed=7.677 offsetSmoothed=-0.049 valid=0 "
        "fallbackReason=continuous_low_contrast stage6AgcStatsMs=0.240"
    )

    representative = finalizer.validate_scenario_expectations(
        {"requireAgcValid": True}, enabled, {}, "MWIR", True, 20
    )
    require(representative["result"] == "PASS", f"representative AGC rejected: {representative}")
    require(representative["observations"]["agcContract"]["validObserved"] is True, str(representative))
    rows.append({"case": "agc_representative_requires_and_observes_valid", "result": "PASS"})

    representative_fallback = finalizer.validate_scenario_expectations(
        {"requireAgcValid": True}, low_contrast, {}, "MWIR", True, 20
    )
    require(representative_fallback["result"] == "FAIL", "required valid accepted fallback-only AGC")
    require(
        "agc_runtime_valid_not_observed_for_required_scenario" in representative_fallback["errors"],
        str(representative_fallback),
    )
    rows.append({"case": "agc_representative_rejects_fallback_only", "result": "PASS"})

    ordinary_fallback = finalizer.validate_scenario_expectations(
        {}, low_contrast, {}, "MWIR", True, 20
    )
    require(ordinary_fallback["result"] == "PASS", f"legal fallback rejected: {ordinary_fallback}")
    fallback_observation = ordinary_fallback["observations"]["agcContract"]
    require(fallback_observation["validObserved"] is False, str(ordinary_fallback))
    require(fallback_observation["acceptedInvalidFallback"] == "continuous_low_contrast", str(ordinary_fallback))
    rows.append({"case": "agc_ordinary_records_low_contrast_fallback", "result": "PASS"})

    missing_rows = finalizer.validate_scenario_expectations({}, "", {}, "MWIR", True, 20)
    require(missing_rows["result"] == "FAIL", "missing AGC rows passed")
    require("agc_runtime_rows_missing" in missing_rows["errors"], str(missing_rows))
    rows.append({"case": "agc_gate_reject_missing_rows", "result": "PASS"})

    interleaved = enabled + "\n" + (
        "[Stage6 AGC] sourceSeq=2 enabled=1 effective=[ProtocolRoute] "
        "transport=udp type=realtime accepted=1"
    )
    interleaved_result = finalizer.validate_scenario_expectations(
        {"requireAgcValid": True}, interleaved, {}, "MWIR", True, 20
    )
    require(interleaved_result["result"] == "PASS", str(interleaved_result))
    require(
        interleaved_result["observations"]["agcMalformedInterleavedRows"] == 1,
        str(interleaved_result),
    )
    rows.append({"case": "agc_gate_records_nonselected_interleaved_log_row", "result": "PASS"})

    selected_interleaved_result = finalizer.validate_scenario_expectations(
        {"requireAgcValid": True}, interleaved, {}, "MWIR", True, 2
    )
    require(selected_interleaved_result["result"] == "FAIL", str(selected_interleaved_result))
    require(
        "agc_requested_source_seq_row_missing" in selected_interleaved_result["errors"],
        str(selected_interleaved_result),
    )
    rows.append({"case": "agc_gate_rejects_interleaved_selected_evidence_row", "result": "PASS"})

    selected_low_contrast_90 = low_contrast.replace("sourceSeq=20", "sourceSeq=90")
    nonselected_interleaved_suffix = low_contrast.replace(
        "sourceSeq=20", "sourceSeq=89"
    ).replace(
        "fallbackReason=continuous_low_contrast",
        "fallbackReason=continuous_low_contrast[TcpFramePacket]",
    )
    suffix_result = finalizer.validate_scenario_expectations(
        {}, selected_low_contrast_90 + "\n" + nonselected_interleaved_suffix,
        {}, "MWIR", True, 90,
    )
    require(suffix_result["result"] == "PASS", str(suffix_result))
    require(suffix_result["observations"]["agcMalformedInterleavedRows"] == 1, str(suffix_result))
    require(suffix_result["observations"]["agcSelectedSourceSeqRows"] == 1, str(suffix_result))
    rows.append({"case": "agc_gate_ignores_nonselected_interleaved_fallback_suffix", "result": "PASS"})

    selected_interleaved_suffix = nonselected_interleaved_suffix.replace("sourceSeq=89", "sourceSeq=90")
    selected_suffix_result = finalizer.validate_scenario_expectations(
        {}, selected_interleaved_suffix, {}, "MWIR", True, 90
    )
    require(selected_suffix_result["result"] == "FAIL", str(selected_suffix_result))
    require(
        "agc_requested_source_seq_row_missing" in selected_suffix_result["errors"],
        str(selected_suffix_result),
    )
    rows.append({"case": "agc_gate_rejects_selected_interleaved_fallback_suffix", "result": "PASS"})

    numeric_interleave = selected_low_contrast_90.replace(
        "sourceSeq=90", "sourceSeq=89"
    ).replace(
        "highPercentile=98.000", "highPercentile=[ProtocolIngress]98.000"
    ).replace(
        "fallbackReason=continuous_low_contrast",
        "fallbackReason=continuous_low_contrast38",
    )
    numeric_interleave_result = finalizer.validate_scenario_expectations(
        {}, selected_low_contrast_90 + "\n" + numeric_interleave,
        {}, "MWIR", True, 90,
    )
    require(numeric_interleave_result["result"] == "PASS", str(numeric_interleave_result))
    require(
        numeric_interleave_result["observations"]["agcMalformedInterleavedRows"] == 1,
        str(numeric_interleave_result),
    )
    require(
        numeric_interleave_result["observations"]["agcSelectedSourceSeqRows"] == 1,
        str(numeric_interleave_result),
    )
    rows.append({"case": "agc_gate_rejects_nonselected_numeric_field_interleave_as_row", "result": "PASS"})

    selected_numeric_interleave = numeric_interleave.replace("sourceSeq=89", "sourceSeq=90")
    selected_numeric_result = finalizer.validate_scenario_expectations(
        {}, selected_numeric_interleave, {}, "MWIR", True, 90,
    )
    require(selected_numeric_result["result"] == "FAIL", str(selected_numeric_result))
    require(
        "agc_requested_source_seq_row_missing" in selected_numeric_result["errors"],
        str(selected_numeric_result),
    )
    rows.append({"case": "agc_gate_rejects_selected_numeric_field_interleave", "result": "PASS"})

    intact_numeric_suffix = selected_low_contrast_90.replace(
        "fallbackReason=continuous_low_contrast",
        "fallbackReason=continuous_low_contrast38",
    )
    intact_numeric_suffix_result = finalizer.validate_scenario_expectations(
        {}, intact_numeric_suffix, {}, "MWIR", True, 90,
    )
    require(intact_numeric_suffix_result["result"] == "FAIL", str(intact_numeric_suffix_result))
    require(
        "agc_runtime_invalid_fallback_not_allowed" in intact_numeric_suffix_result["errors"],
        str(intact_numeric_suffix_result),
    )
    rows.append({"case": "agc_gate_does_not_strip_plain_numeric_fallback_suffix", "result": "PASS"})

    unknown_fallback_90 = selected_low_contrast_90.replace(
        "fallbackReason=continuous_low_contrast", "fallbackReason=unknown_failure"
    )
    unknown_result = finalizer.validate_scenario_expectations(
        {}, unknown_fallback_90, {}, "MWIR", True, 90
    )
    require(unknown_result["result"] == "FAIL", str(unknown_result))
    require("agc_runtime_invalid_fallback_not_allowed" in unknown_result["errors"], str(unknown_result))
    rows.append({"case": "agc_gate_rejects_plain_unknown_fallback", "result": "PASS"})

    rejection_cases = (
        (
            "unknown_fallback",
            low_contrast.replace("continuous_low_contrast", "unknown_failure"),
            "agc_runtime_invalid_fallback_not_allowed",
        ),
        (
            "missing_samples",
            low_contrast.replace("sampleCount=4096", "sampleCount=0"),
            "agc_runtime_enabled_effective_sample_contract_failed",
        ),
        (
            "not_effective",
            low_contrast.replace("effective=1", "effective=0"),
            "agc_runtime_enabled_effective_sample_contract_failed",
        ),
        (
            "valid_with_fallback",
            enabled.replace("fallbackReason=none", "fallbackReason=continuous_low_contrast"),
            "agc_runtime_valid_row_has_fallback",
        ),
    )
    for name, log_text, expected_error in rejection_cases:
        result = finalizer.validate_scenario_expectations({}, log_text, {}, "MWIR", True, 20)
        require(result["result"] == "FAIL", f"invalid AGC evidence passed: {name}")
        require(expected_error in result["errors"], f"{name}: {result}")
        rows.append({"case": f"agc_gate_reject_{name}", "result": "PASS"})


def solar_evidence_gate_tests(rows: list[dict[str, object]]) -> None:
    finalizer = load_finalizer()
    fixed_agc = (
        "[Stage6 AGC] sourceSeq=20 enabled=0 effective=0 mode=Percentile "
        "statsSource=previous_readback updateHz=30.000 sampleCount=0 "
        "lowPercentile=2.000 highPercentile=98.000 lowInput=0.000 highInput=1.000 "
        "gain=1.000 offset=0.000 gainSmoothed=1.000 offsetSmoothed=0.000 "
        "valid=0 fallbackReason=disabled stage6AgcStatsMs=0.000"
    )
    clean_solar = (
        "[M1 SolarPosition] lat=39.996 lon=116.000 altM=500.000 "
        "UTC=2026-09-06T12.000000h solarAz=180.000000 solarEl=45.000000 "
        "solarZenith=45.000000 sunDirection=(0.000000,-0.707107,0.707107) "
        "frame=ENU/Panda(X=East,Y=North,Z=Up) "
        "positionSource=controlled_config_solar_override solarOverride=1 "
        "dateSource=config_fallback protocolTimeSource=time_ms_since_midnight "
        "valid=1 fallbackReason=none"
    )
    interleaved_solar = (
        "[M1 SolarPosition] lat=39.996 lon=116.000 altM=500.000 "
        "UTC=2026-09-06T12.000000h solarAz=180.000000 solarEl=45.000000 "
        "solarZenith=45.000000 sunDirection=(0.000000,-0.707107,0.707107) "
        "frame=ENU/Panda(X=East,Y=North,Z=Up) "
        "positionSource=controlled_config_solar_override[ProtocolIngress] "
        "transport=udp solarOverride=1 dateSource=config_fallback "
        "protocolTimeSource=time_ms_since_midnight type=realtime accepted=1 "
        "valid= duplicate=1 fallbackReason=none0"
    )
    m1_compare = (
        "[M1 Compare] sourceSeq=20 band=MWIR solarZenithDeg=45.000000 "
        "valid=1 fallbackReason=none"
    )
    stage5_radiance = (
        "[Stage5 Radiance] sourceSeq=20 band=MWIR "
        "sunElevation=45.000000 sunAzimuth=180.000000"
    )
    scenario = {"solarAzimuthDeg": 180.0, "solarElevationDeg": 45.0}
    summary = {
        "m1SolarOverrideEnable": True,
        "m1SolarOverrideAzimuthDeg": 180.0,
        "m1SolarOverrideElevationDeg": 45.0,
    }

    clean_result = finalizer.validate_scenario_expectations(
        scenario, fixed_agc + "\n" + clean_solar, summary, "MWIR", False, 20,
    )
    require(clean_result["result"] == "PASS", str(clean_result))
    require(clean_result["observations"]["solarOverride"]["completeRows"] == 1, str(clean_result))
    require(
        clean_result["observations"]["solarOverride"]["recoveredInterleavedRows"] == 0,
        str(clean_result),
    )
    rows.append({"case": "solar_gate_accepts_complete_override_row", "result": "PASS"})

    corroborated_interleave = "\n".join(
        (fixed_agc, interleaved_solar, m1_compare, stage5_radiance)
    )
    recovered_result = finalizer.validate_scenario_expectations(
        scenario, corroborated_interleave, summary, "MWIR", False, 20,
    )
    require(recovered_result["result"] == "PASS", str(recovered_result))
    solar_observation = recovered_result["observations"]["solarOverride"]
    require(solar_observation["completeRows"] == 0, str(recovered_result))
    require(solar_observation["interleavedCandidates"] == 1, str(recovered_result))
    require(solar_observation["recoveredInterleavedRows"] == 1, str(recovered_result))
    require(solar_observation["m1CompareCorroborationRows"] == 1, str(recovered_result))
    require(solar_observation["stage5RadianceCorroborationRows"] == 1, str(recovered_result))
    rows.append({"case": "solar_gate_recovers_exact_corroborated_protocol_ingress_interleave", "result": "PASS"})

    negative_cases = (
        (
            "wrong_angle",
            corroborated_interleave.replace("solarAz=180.000000", "solarAz=181.000000"),
        ),
        (
            "foreign_tag",
            corroborated_interleave.replace("[ProtocolIngress]", "[ProtocolRoute]"),
        ),
        (
            "missing_ingress_witness",
            corroborated_interleave.replace("[ProtocolIngress] transport=udp", "[ProtocolIngress]"),
        ),
        (
            "missing_downstream_corroboration",
            fixed_agc + "\n" + interleaved_solar,
        ),
        (
            "complete_invalid",
            fixed_agc + "\n" + clean_solar.replace("valid=1", "valid=0"),
        ),
        (
            "complete_unknown_fallback",
            fixed_agc + "\n" + clean_solar.replace("fallbackReason=none", "fallbackReason=unknown_failure"),
        ),
    )
    for name, log_text in negative_cases:
        result = finalizer.validate_scenario_expectations(
            scenario, log_text, summary, "MWIR", False, 20,
        )
        require(result["result"] == "FAIL", f"invalid solar evidence passed: {name}: {result}")
        require(
            "solar_override_runtime_angle_not_observed:180.0:45.0" in result["errors"],
            f"{name}: {result}",
        )
        rows.append({"case": f"solar_gate_reject_{name}", "result": "PASS"})


def main() -> int:
    rows: list[dict[str, object]] = []
    source_contract_tests(rows)
    executable_profile_tests(rows)
    stimulus_invalid_cli_tests(rows)
    evidence_gate_tests(rows)
    agc_evidence_gate_tests(rows)
    solar_evidence_gate_tests(rows)
    report = {"schema": "hwasimir_p11_contract_tests_1", "result": "PASS", "tests": rows}
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"result": "PASS", "tests": len(rows), "report": str(REPORT)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
