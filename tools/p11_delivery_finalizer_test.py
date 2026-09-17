#!/usr/bin/env python3
"""Unit tests for the P11 delivery finalizer; uses temporary synthetic evidence only."""

from __future__ import annotations

import array
import csv
import hashlib
import json
import math
import shutil
import struct
import tempfile
import unittest
import zipfile
from pathlib import Path

import p11_finalize_delivery as finalizer


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value), encoding="utf-8")


def read_json(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def rewrite_pfm(path: Path, first: float, rest: float) -> None:
    count = 800 * 800 * 3
    values = array.array("f", [rest]) * count
    values[0] = first
    path.write_bytes(b"PF\n800 800\n-1.0\n" + values.tobytes())


def minimal_png(path: Path) -> str:
    path.parent.mkdir(parents=True, exist_ok=True)
    # The finalizer only reads the PNG signature and IHDR geometry.  This is a
    # deliberately tiny header fixture, not image evidence.
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 13) + b"IHDR" + struct.pack(">II", 800, 800))
    return hashlib.sha256(path.read_bytes()).hexdigest()


def make_windows_program_identities(workspace: Path) -> list[dict[str, object]]:
    paths = {
        "HwaSim_IR": workspace / "HwaSim_IR/Bin/HwaSim_IR.exe",
        "HwaSim_IR_VideoDisplay": workspace / "HwaSim_IR_VideoDisplay/x64/Release/HwaSim_IR_VideoDisplay.exe",
        "DataDrivenTestQT": workspace / "build-DataDrivenTestQT-codex-mingw73_64-Release/release/DataDrivenTestQT.exe",
    }
    rows = []
    for role in finalizer.WINDOWS_PROGRAM_ROLES:
        path = paths[role]
        path.parent.mkdir(parents=True, exist_ok=True)
        if not path.is_file():
            path.write_bytes((role + " synthetic executable fixture").encode("ascii"))
        rows.append({
            "role": role,
            "path": str(path.resolve()),
            "bytes": path.stat().st_size,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        })
    return rows


def make_case(
    case_dir: Path,
    result: str = "PASS",
    *,
    factor: str = "target",
    mapping_gain: float = 1.0,
    seconds: int | float | str = 3,
    explicit_scenario_seconds: int | None = None,
) -> None:
    workspace = case_dir.parents[2]
    band = case_dir.parent.name
    case_id = case_dir.name
    contract = finalizer.WINDOWS_BAND_CONTRACT[band]
    hashes = {}
    for name in ("fixed_clean.png", "auto_clean.png", "annotated.png", "received.png"):
        hashes[name] = minimal_png(case_dir / name)
    values = array.array("f", [1.0]) * (800 * 800 * 3)
    values[-1] = 2.0
    pfm = case_dir / "raw_radiance.pfm"
    pfm.write_bytes(b"PF\n800 800\n-1.0\n" + values.tobytes())
    raw_hash = hashlib.sha256(pfm.read_bytes()).hexdigest()
    raw = {
        "width": 800, "height": 800, "channels": 3, "scale": -1.0,
        "finiteValues": len(values), "nonfiniteValues": 0,
        "minimum": 1.0, "maximum": 2.0, "mean": 1.0 + 1.0 / len(values),
        "path": str(pfm.resolve()), "sha256": raw_hash, "source": str(pfm.resolve()),
        "producerLogLine": "[P6LinearCapture] stage=pre_display domain=spectral_radiance physicalRadiance=1",
        "stage": "pre_display", "domain": "spectral_radiance", "unit": "W/(m^2 sr um)",
        "bandQuantity": "response-weighted spectral-radiance band mean",
    }
    write_json(case_dir / "raw_radiance.json", raw)

    base_fixture = workspace / "tools/p11_inputs/base.json"
    profile = workspace / f"profiles/{band}.json"
    input_fixture = case_dir / "input_fixture.json"
    for path, value in ((base_fixture, {"base": True}), (profile, {"band": band}), (input_fixture, {"caseId": case_id})):
        write_json(path, value)
    scenario = {"id": case_id, "factor": factor, "comparisonGroup": "fixture_group", "runnable": True}
    if explicit_scenario_seconds is not None:
        scenario["seconds"] = explicit_scenario_seconds
    request = {
        "schema": "hwasimir_p11_windows_case_request_1", "caseId": case_id,
        "factor": factor, "comparisonGroup": "fixture_group", "band": band,
        "protocolBand": contract["protocolBand"], "rangeUm": list(contract["rangeUm"]),
        "transport": "TCP", "resolution": "800x800", "seconds": seconds,
        "scenario": scenario, "fixture": str(input_fixture.resolve()),
        "fixtureSha256": hashlib.sha256(input_fixture.read_bytes()).hexdigest(),
        "baseFixture": str(base_fixture.resolve()),
        "baseFixtureSha256": hashlib.sha256(base_fixture.read_bytes()).hexdigest(),
        "profile": str(profile.resolve()), "profileSha256": hashlib.sha256(profile.read_bytes()).hexdigest(),
        "deterministicReplaySourceSeq": 90,
        "formalProgramIdentities": make_windows_program_identities(workspace),
    }
    write_json(case_dir / "case_request.json", request)

    component_path = case_dir / "physical_components.csv"
    component_path.parent.mkdir(parents=True, exist_ok=True)
    factor_tags = {
        "active_illumination": ("L2 ActiveIlluminator",),
        "solar_azimuth_elevation": ("M1 SolarPosition", "L1 SolarHeatingLut"),
        "cloud": ("Stage7 Weather", "WeatherCloud"),
        "rain": ("Stage7 Weather", "Stage7 Precipitation"),
        "snow": ("Stage7 Weather", "Stage7 Precipitation"),
        "combination": ("L2 ActiveIlluminator", "Stage7 Precipitation", "Stage5 AeroThermal"),
        "target_speed": ("Stage5 AeroThermal",),
    }
    with component_path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=("variant", "source_log", "line", "tag", "fields_json"))
        writer.writeheader()
        line = 1
        for variant in ("fixed", "agc", "annotated"):
            tags = list(finalizer.WINDOWS_CORE_COMPONENT_TAGS) + list(factor_tags.get(factor, ())) + ["M1 RadianceDisplay"]
            for tag in tags:
                fields: dict[str, object] = {}
                if tag == "M1 Compare":
                    fields = {"band": band, "finalOutput": "M1", "valid": 1}
                elif tag == "Stage5 RadianceComponents":
                    fields = {"band": band, "finalOutput": "M1", "formalRuntimeAffectsImage": 1, "radianceUnit": "W/(m^2_sr_um)"}
                elif tag == "P6LinearCapture":
                    fields = {"stage": "pre_display", "domain": "spectral_radiance", "physicalRadiance": 1}
                elif tag == "DisplayFrameMapping":
                    fields = {"fixedGain": mapping_gain, "offsetGray": 0, "gamma": 2.2, "reinhard": 1, "whiteHot": 1}
                elif tag == "M1 RadianceDisplay":
                    fields = {
                        f"{band}Min": 0, f"{band}Max": 40 if band == "SWIR" else 64,
                        "mode": "scene_wide_physical_window", "perTargetGain": "forbidden", "responseMode": "RectangularBand",
                    }
                writer.writerow({"variant": variant, "source_log": f"variants/{variant}/formal_tcp_run/hwa.out.log", "line": line, "tag": tag, "fields_json": json.dumps(fields)})
                line += 1

    identity_rows = []
    for variant in ("fixed", "agc", "annotated"):
        hwa_log = f"variants\\{variant}\\formal_tcp_run\\hwa.out.log"
        video_log = f"variants\\{variant}\\formal_tcp_run\\video.err.log"
        identity_rows.extend([
            {"tag": "TcpPerf", "sourceLog": hwa_log, "line": 1, "sourceSeq": 1, "outputOrdinal": 1, "overwritten": 0},
            {"tag": "TcpPerf", "sourceLog": hwa_log, "line": 2, "sourceSeq": 90, "outputOrdinal": 90, "overwritten": 0},
            {"tag": "VideoPerf", "sourceLog": video_log, "line": 3, "sourceSeq": 90, "outputOrdinal": 90,
             "sourceSeqContinuous": 1, "discontinuities": 0, "h264DecodeErrors": 0},
        ])
    write_json(case_dir / "frame_identity.json", {
        "schema": finalizer.WINDOWS_FRAME_IDENTITY_SCHEMA, "evidence": identity_rows,
        "note": "Ordered identity fixture.",
    })

    source_paths = {}
    for variant in ("fixed", "agc", "annotated"):
        source = case_dir / f"variants/{variant}/receiver_recording/output.mp4"
        source.parent.mkdir(parents=True, exist_ok=True)
        source.write_bytes((variant + " receiver fixture").encode("ascii"))
        source_paths[variant] = source
    decoded = {}
    canonical = {"fixed": "fixed_clean.png", "agc": "auto_clean.png", "annotated": "annotated.png", "received": "received.png"}
    for variant, filename in canonical.items():
        source_variant = "agc" if variant == "received" else variant
        source = source_paths[source_variant]
        output = case_dir / filename
        decoded[variant] = {
            "source": str(source.resolve()), "sourceSha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            "sourceSeq": 90, "frameSeq": 90, "storageIndex": 90, "mp4PtsUs": 1_483_333,
            "output": str(output.resolve()), "outputSha256": hashes[filename],
            "width": 800, "height": 800, "meaning": "synthetic receiver decode fixture",
        }
    selected = {
        "requestedSourceSeq": 90,
        "variants": {
            variant: {key: decoded[variant][key] for key in ("sourceSeq", "frameSeq", "storageIndex", "mp4PtsUs")}
            for variant in ("fixed", "agc", "annotated")
        },
    }
    chain = {
        variant: {
            "result": "PASS", "errors": [], "policy": "OrderedQueue",
            "counts": {name: 90 for name in ("sent", "accepted", "queued", "executed", "captured", "rendered", "output", "receiverInput", "receiverWritten")},
            "zeroCounters": {"inputQueueOverflow": 0, "tcpOverwritten": 0, "recorderDroppedFrames": 0},
            "sourceSeqContinuousEvidenceRows": 1,
        }
        for variant in ("fixed", "agc", "annotated")
    }
    files = {}
    for name in finalizer.WINDOWS_CASE_MANIFEST_FILES:
        path = case_dir / name
        files[name] = {"bytes": path.stat().st_size, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
    write_json(case_dir / "case.json", {
        "schema": "test",
        "result": result,
        "transport": "TCP",
        "request": request,
        "rawRadiance": raw,
        "decodedEvidence": decoded,
        "selectedFrame": selected,
        "formalChainGates": chain,
        "scenarioGates": {name: {"result": "PASS", "errors": []} for name in ("fixed", "agc", "annotated")},
        "files": files,
    })


def make_windows_acceptance(
    root: Path,
    identities: list[dict[str, object]],
    *,
    bands: tuple[str, ...] = ("SWIR", "MWIR"),
    requested_band: str = "Both",
    seconds: int = 60,
) -> Path:
    by_role = {str(row["role"]): row for row in identities}
    preflight: dict[str, object] = {
        "schema": finalizer.WINDOWS_H264_PREFLIGHT_SCHEMA,
        "result": "PASS",
        "errors": [],
    }
    for role, key in finalizer.WINDOWS_PREFLIGHT_ROLE_KEYS.items():
        source = by_role[role]
        preflight[key] = {
            "path": source["path"],
            "bytes": source["bytes"],
            "sha256": source["sha256"],
        }
    write_json(root / "binary_preflight.json", preflight)
    band_rows = []
    for band in bands:
        detail_path = root / band / "p11_windows_h264_acceptance_summary.json"
        write_json(detail_path, {
            "schema": finalizer.WINDOWS_H264_BAND_SCHEMA,
            "result": "PASS",
            "band": band,
            "expected": {"seconds": seconds, "resolution": "800x800"},
            "counts": {name: seconds * 60 for name in (
                "sent", "accepted", "queued", "executed", "captured", "rendered",
                "output", "receiver", "written", "receiverLastFrameSeq",
                "receiverLastSourceSeq", "frameIndex", "producerAnnotations", "mp4",
            )},
            "countConservation": True,
            "independentDecode": {"pass": True},
        })
        band_rows.append({
            "band": band,
            "result": "PASS",
            "analyzerExitCode": 0,
            "summary": str(detail_path.resolve()),
        })
    summary_path = root / "p11_windows_h264_dual_band_summary.json"
    write_json(summary_path, {
        "schema": finalizer.WINDOWS_H264_DUAL_SCHEMA,
        "result": "PASS",
        "requestedBand": requested_band,
        "seconds": seconds,
        "preflight": preflight,
        "configHashesBefore": {"runtime": "a" * 64},
        "configHashesAfter": {"runtime": "a" * 64},
        "configurationRestored": True,
        "restoreErrors": [],
        "bands": band_rows,
    })
    return summary_path


def make_rk_band_summary(band: str, protocol_band: int) -> dict[str, object]:
    gates = [{"gate": name, "pass": True, "detail": "fixture"} for name in finalizer.RK_REQUIRED_GATES]
    gates.append({"gate": "raw_pfm_120", "pass": True, "detail": "fixture"})
    return {
        "schema": finalizer.RK_ACCEPTANCE_BAND_SCHEMA,
        "result": "PASS",
        "band": band,
        "protocol_band": protocol_band,
        "resolution": "800x800",
        "duration_sec": 60,
        "frame_counts": {name: 3600 for name in ("sender", "accepted", "execute", "render", "output", "received")},
        "raw_radiance": {
            "unit": "W/(m^2_sr_um)",
            "storage": [{
                "requested": "RGBA16F_SI",
                "formal_requested": "1",
                "platform_backend": "linux_gles",
                "domain": "W_per_m2_sr_um",
                "unit": "W/(m^2_sr_um)",
                "quantization_model": "IEEE754_binary16",
                "quantization_relative_error_bound": 2.0 ** -11,
                "quantization_max_finite": 65504.0,
                "texture_floating_point": "1",
                "actual_texture_component_type": "half_float",
                "actual_texture_component_width": 2,
                "actual_texture_components": 4,
                "actual_rgb_bits": [16, 16, 16],
                "actual_alpha_bits": 16,
            }],
            "cpu_binary16_quantization_reference": {
                "model": "IEEE754_binary16_round_to_nearest",
                "unit": "W/(m^2_sr_um)",
                "cpu_reference_count": 2,
                "overflow_count": 0,
                "bound_failure_count": 0,
                "maximum_relative_error": 0.0004,
            },
            "captures": [{
                "sourceSeq": 120,
                "value_count": 1_920_000,
                "finite_count": 1_920_000,
                "half_lattice_checked_count": 1_920_000,
                "half_lattice_mismatch_count": 0,
                "half_lattice_overflow_count": 0,
            }],
        },
        "gates": gates,
    }


def make_rk_acceptance_root(root: Path) -> None:
    cases = []
    for band, protocol in finalizer.RK_BAND_PROTOCOL.items():
        band_root = root / band
        write_json(band_root / "acceptance_summary.json", make_rk_band_summary(band, protocol))
        write_json(band_root / "case_request.json", {
            "schema": "hwasimir.p11.rk3588.band-request.v1",
            "band": band,
            "protocol_band": protocol,
            "transport": {"kind": "DDS", "domain": 150, "codec": "h264_annexb"},
        })
        (band_root / "stim.out.log").write_text("", encoding="utf-8")
        (band_root / "stim.err.log").write_text(
            "[StimTransport] mode=dds ddsRuntimeInitCount=1 domain=150 qos=fixture\n"
            "[StimFinal] transport=dds successfulRealtimeWrites=3600\n",
            encoding="utf-8",
        )
        (band_root / "receiver.out.log").write_text("", encoding="utf-8")
        (band_root / "receiver.err.log").write_text(
            "[VideoInput] Transport=dds streamRole=direct codec=h264 domain=150\n"
            "[DdsVideoReceiver] ready=1 initCount=1 domain=150 topic=fixture codec=h264 "
            "width=800 height=800 fps=60 wireType=DDS::Bytes fullTransport=1\n"
            "[VideoPerf] activeCodec=h264_annexb codecFallbackReason=none\n",
            encoding="utf-8",
        )
        board = band_root / "board"
        board.mkdir(parents=True, exist_ok=True)
        (board / "hwa.log").write_text(
            "[RunPreflight] result=PASS commandTransport=dds\n"
            "[CommandTransport] input=dds ddsProtocolEnable=1 domain=150\n"
            "[TcpPayloadConfig] SendVideo=0 SendAnnotation=0 SendRealtimeData=0 "
            "ForwardInitControl=0\n"
            "[ProtocolRoute] transport=dds type=realtime accepted=1\n"
            "[TcpOutputConfig] H264FallbackToJpeg=0\n",
            encoding="utf-8",
        )
        (board / "performance.csv").write_text(
            "input_overwritten,input_queue_overflow,source_seq_gap\n0,0,0\n",
            encoding="utf-8",
        )
        for name in ("input_accepted_fixture.csv", "input_execute_fixture.csv",
                     "stage_render_fixture.csv", "stage_output_fixture.csv"):
            path = board / "input_audit" / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture\n", encoding="utf-8")
        for name in ("input_sender_fixture.csv", "input_received_fixture.csv"):
            path = band_root / "windows_audit" / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture\n", encoding="utf-8")
        cases.append({"band": band, "protocol_band": protocol, "result": "PASS", "errors": []})
    write_json(root / "acceptance_overall.json", {
        "schema": finalizer.RK_ACCEPTANCE_OVERALL_SCHEMA,
        "result": "PASS",
        "run_id": "fixture",
        "deployment_stage_id": "stage-1",
        "elf_sha256": "a" * 64,
        "config_manifest_sha256": "b" * 64,
        "cases": cases,
    })


def make_rk_dds_lifecycle_summary(root: Path) -> Path:
    cases = []
    for scenario, required in finalizer.RK_DDS_LIFECYCLE_REQUIRED_GATES.items():
        lifecycle_scenario = finalizer.RK_DDS_LIFECYCLE_CASE_SCENARIOS[scenario]
        band, protocol_band = finalizer.RK_DDS_LIFECYCLE_CASE_BANDS[scenario]
        case_dir = root / scenario
        artifact = case_dir / "evidence.log"
        artifact.parent.mkdir(parents=True, exist_ok=True)
        artifact.write_text(f"{scenario} DDS evidence\n", encoding="utf-8")
        case_path = case_dir / "lifecycle_summary.json"
        write_json(case_path, {
            "schema": finalizer.RK_DDS_LIFECYCLE_CASE_SCHEMA,
            "result": "PASS",
            "scenario": lifecycle_scenario,
            "band": band,
            "protocol_band": protocol_band,
            "transport": {
                "kind": "DDS", "domain": 150,
                "tcp_payload_counts": {
                    "SendVideo": 0, "SendAnnotation": 0,
                    "SendRealtimeData": 0, "ForwardInitControl": 0,
                },
                "udp_ingress_count": 0,
            },
            "gates": [{
                "gate": name,
                "pass": True,
                "detail": (
                    "publisher_initialized=1"
                    if name == "dds_domain_150_board" else "fixture"),
            } for name in required],
            "artifacts": [{
                "path": artifact.name,
                "bytes": artifact.stat().st_size,
                "sha256": hashlib.sha256(artifact.read_bytes()).hexdigest(),
            }],
        })
        cases.append({
            "scenario": scenario,
            "lifecycle_scenario": lifecycle_scenario,
            "band": band,
            "protocol_band": protocol_band,
            "result": "PASS",
            "summary": case_path.relative_to(root).as_posix(),
            "summarySha256": hashlib.sha256(case_path.read_bytes()).hexdigest(),
        })
    overall = root / "dds_lifecycle_overall.json"
    write_json(overall, {
        "schema": finalizer.RK_DDS_LIFECYCLE_OVERALL_SCHEMA,
        "result": "PASS",
        "run_id": "lifecycle-fixture",
        "deployment_stage_id": "stage-1",
        "elf_sha256": "a" * 64,
        "config_manifest_sha256": "b" * 64,
        "transport": {
            "kind": "DDS", "domain": 150, "udp_tested": False,
            "tcp_tested": False, "tcp_payload_required_disabled": True,
        },
        "cases": cases,
    })
    return overall


def make_rk_dds_image_root(root: Path) -> Path:
    fixture_acceptance = root / "_fixture_acceptance"
    make_rk_acceptance_root(fixture_acceptance)
    program_identities = []
    for role, name in (
        ("DDS H.264 receiver/FFmpeg decoder", "dds_receiver.exe"),
        ("DDS control stimulus", "dds_stimulus.exe"),
    ):
        path = root.parent / "bin" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(role.encode("utf-8"))
        program_identities.append({
            "role": role, "path": str(path.resolve()), "bytes": path.stat().st_size,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        })
    blockers = [
        {"id": "cloud_target_front_behind", "status": "BLOCKED_METADATA", "reason": "no placement metadata"},
        {"id": "cloud_individual_id_disable", "status": "BLOCKED_METADATA", "reason": "no stable cloud id"},
        {"id": "target_range_2km_physical_imagery", "status": "BLOCKED_DATA", "reason": "audited LUT ends at 1 km"},
    ]
    write_json(root / "blocked_coverage.json", blockers)
    write_json(root / "matrix_plan.json", {"result": "PASS"})
    write_json(root / "quick_check.json", {"result": "PASS"})
    rows = []
    for band, protocol in finalizer.RK_BAND_PROTOCOL.items():
        case_id = "target_near_100m"
        case_dir = root / band / case_id
        write_json(case_dir / "case_request.json", {
            "band": band, "protocol_band": protocol,
            "transport": {
                "control": "DDS", "video": "DDS", "domain": 150,
                "codec": "h264_annexb", "udp_payload_tested": False,
                "tcp_payload_tested": False,
            },
            "program_identities": program_identities,
        })
        write_json(case_dir / "input_fixture.json", {"caseId": case_id})
        write_json(case_dir / "physical_components.json", {"result": "PASS"})
        (case_dir / "frame_identity.csv").write_text("sourceSeq,outputOrdinal\n1,1\n", encoding="utf-8")
        write_json(case_dir / "frame_identity.json", {
            "schema": "hwasimir.p11.rk3588.dds-frame-identity.v1",
            "result": "PASS", "row_count": 1,
        })
        (case_dir / "raw_radiance_metrics.csv").write_text("sourceSeq,mean\n120,1\n", encoding="utf-8")
        (case_dir / "raw_radiance.pfm").write_bytes(
            b"PF\n1 1\n-1.0\n" + struct.pack("<fff", 1.0, 1.0, 1.0))
        write_json(case_dir / "raw_radiance.json", {
            "schema": "hwasimir.p11.rk3588.dds-raw-radiance.v1",
            "unit": "W/(m^2_sr_um)", "source_seq": 120,
        })
        for name in ("fixed_clean.png", "auto_clean.png", "annotated.png"):
            minimal_png(case_dir / name)
        for variant in ("fixed", "agc", "annotated"):
            destination = case_dir / "variants" / variant / "acceptance" / band
            shutil.copytree(fixture_acceptance / band, destination)
            minimal_png(destination / "received_decode.png")
        variant_records = []
        for variant, canonical_name in (
            ("fixed", "fixed_clean.png"),
            ("agc", "auto_clean.png"),
            ("annotated", "annotated.png"),
        ):
            canonical = case_dir / canonical_name
            received = case_dir / "variants" / variant / "acceptance" / band / "received_decode.png"
            # Canonical display evidence is copied from the exact receiver
            # decode selected for that independent DDS run.
            canonical.write_bytes(received.read_bytes())
            digest = hashlib.sha256(received.read_bytes()).hexdigest()
            variant_records.append({
                "variant": variant, "result": "PASS", "errors": [],
                "canonical_png": str(canonical.resolve()),
                "canonical_png_sha256": digest,
                "received_decode_png": str(received.resolve()),
                "received_decode_png_sha256": digest,
            })
        case_path = case_dir / "case.json"
        write_json(case_path, {
            "schema": "hwasimir.p11.rk3588.dds-image-case.v1",
            "result": "PASS", "case_id": case_id, "factor": "target",
            "comparison_group": "near", "band": band, "protocol_band": protocol,
            "transport": {
                "payload_paths": ["DDS control", "DDS H.264 Annex-B video"],
                "udp_payload_tested": False, "tcp_payload_tested": False,
                "dds_domain": 150, "video_topic": "fixture",
                "encoder": "RK3588 MPP", "decoder": "Windows FFmpeg",
            },
            "deployment": {"stage_id": "stage-1"},
            "deterministic_replay_source_seq": 120,
            "variants": variant_records, "errors": [],
        })
        (case_dir / "artifact_manifest.sha256").write_text(
            f"{hashlib.sha256(case_path.read_bytes()).hexdigest()}  case.json\n", encoding="ascii")
        rows.append({
            "caseId": case_id, "band": band, "result": "PASS",
            "path": case_path.relative_to(root).as_posix(),
            "caseSha256": hashlib.sha256(case_path.read_bytes()).hexdigest(),
            "factor": "target", "comparisonGroup": "near",
        })
    shutil.rmtree(fixture_acceptance)
    summary = root / "dds_image_matrix_summary.json"
    write_json(summary, {
        "schema": finalizer.RK_DDS_IMAGE_MATRIX_SCHEMA,
        "result": "PARTIAL",
        "run_id": "image-fixture",
        "deployment_stage_id": "stage-1",
        "elf_sha256": "a" * 64,
        "config_manifest_sha256": "b" * 64,
        "transport": {"kind": "DDS", "domain": 150, "codec": "h264_annexb"},
        "program_identities": program_identities,
        "plan_sha256": hashlib.sha256((root / "matrix_plan.json").read_bytes()).hexdigest(),
        "blockedCoverage": blockers,
        "cases": rows,
    })
    return summary


def make_rollback_receipt(
    workspace: Path,
    *,
    fatal_renderer: bool = False,
    positive_counts: bool = True,
) -> Path:
    """Create a v2 rollback receipt whose claims are bound to raw loop logs."""
    root = workspace / "rollback"
    root.mkdir(parents=True, exist_ok=True)

    def digest(path: Path) -> str:
        return hashlib.sha256(path.read_bytes()).hexdigest()

    active_elf = "a" * 64
    active_manifest = "b" * 64
    deployment = root / "deployment_receipt_input.json"
    write_json(deployment, {
        "schema": finalizer.RK_DEPLOYMENT_RECEIPT_SCHEMA,
        "stage_id": "p11-20260916-201234",
        "elf_sha256": active_elf,
        "config_manifest_sha256": active_manifest,
    })
    controller_tool = root / "rollback_controller_tool.ps1"
    remote_tool = root / "rollback_remote_runner_tool.sh"
    controller_tool.write_text("# synthetic controller\n", encoding="utf-8")
    remote_tool.write_text("#!/bin/sh\n# synthetic remote runner\n", encoding="utf-8")

    count = 3 if positive_counts else 0
    byte_count = 300 if positive_counts else 0
    releases: dict[str, object] = {}
    exercise_lines = []
    for release, band, renderer_name in (
        ("rollback", 1, "rollback_startup.log"),
        ("p11_restored", 0, "p11_restored_startup.log"),
    ):
        phase = root / release
        phase.mkdir()
        renderer = root / "P11_rollback_fixture" / renderer_name
        renderer.parent.mkdir(exist_ok=True)
        fatal_line = (
            "Assertion failed: Failed to convert image\n"
            "[Stage6 RawAttachment][ERROR] action=fail_closed\n"
            if fatal_renderer else ""
        )
        renderer.write_text(
            "[RunPreflight] result=PASS\n"
            "[DeploymentVersion] result=PASS\n"
            "[GpuBackend] glVendor=ARM glRenderer=Mali-LODX hardwareGpu=1\n"
            + fatal_line
            + f"[OutputRoundDrain] reason=stop targetFrames={count} completedFrames={count}\n"
            + f"[DdsVideoPerf] sentSamples={count} sentBytes={byte_count} "
              "writeErrors=0 droppedSamples=0\n"
            + f"[OutputRoundDrain] reason=process_stop active=0 targetFrames=0 "
              f"completedFrames={count}\n",
            encoding="utf-8",
        )
        stimulus = phase / "stim.err.log"
        stimulus.write_text(
            f"[StimWeather] envSky=0 sensorBand={band} source=cli_or_default\n"
            "[StimDDS] type=init sent=1 platID=1001 sensorID=2\n"
            "[StimInitAck] transport=dds received=1 platID=1001 sensorID=2 ready=1\n",
            encoding="utf-8",
        )
        receiver = phase / "receiver.err.log"
        receiver.write_text(
            "[VideoPerf] decodeCodec=h264_annexb h264KeyFrameSeen=1 h264DecodeErrors=0\n"
            + f'[RuntimeMetricsV2] {{"decodedFrames":"{count}"}}\n'
            + f"[DdsVideoReceiverPerf] receivedSamples={count} receivedBytes={byte_count} "
              "ddsErrors=0 finalizeCode=manager\n",
            encoding="utf-8",
        )
        decoded = phase / "received_decode.png"
        minimal_png(decoded)
        releases[release] = {
            "protocol_band": band,
            "renderer_log": renderer.relative_to(root).as_posix(),
            "renderer_log_sha256": digest(renderer),
            "stimulus_log": stimulus.relative_to(root).as_posix(),
            "stimulus_log_sha256": digest(stimulus),
            "receiver_log": receiver.relative_to(root).as_posix(),
            "receiver_log_sha256": digest(receiver),
            "decoded_png": decoded.relative_to(root).as_posix(),
            "decoded_png_sha256": digest(decoded),
            "target_frames": count,
            "completed_frames": count,
            "sent_samples": count,
            "sent_bytes": byte_count,
            "write_errors": 0,
            "dropped_samples": 0,
            "received_samples": count,
            "received_bytes": byte_count,
            "decoded_frames": count,
            "dds_errors": 0,
            "decode_errors": 0,
        }
        # A forged positive controller summary must not override a fatal or
        # zero-count renderer log; the finalizer independently reparses both.
        exercise_lines.append(
            f"[P11RollbackLoop] release={release} result=PASS protocolBand={band} "
            "targetFrames=3 completedFrames=3 sentSamples=3 sentBytes=300 "
            "writeErrors=0 droppedSamples=0"
        )
    exercise_lines.append(
        "[P11RollbackFinal] result=PASS stageId=p11-20260916-201234 "
        "rollbackSnapshotRetained=1 p11Restored=1"
    )
    exercise = root / "rollback_exercise.log"
    exercise.write_text("\n".join(exercise_lines) + "\n", encoding="utf-8")
    exercise_error = root / "rollback_exercise.err.log"
    exercise_error.write_text("", encoding="utf-8")
    receipt = root / "rollback_receipt.json"
    write_json(receipt, {
        "schema": finalizer.RK_ROLLBACK_RECEIPT_SCHEMA,
        "validation_contract": finalizer.RK_ROLLBACK_LOOP_CONTRACT,
        "stage_id": "p11-20260916-201234",
        "run_id": "p11rb-20260916-210000",
        "duration_sec_each": 15,
        "rollback_started": True,
        "p11_restored_and_started": True,
        "rollback_snapshot_retained": True,
        "active_elf_sha256": active_elf,
        "active_config_manifest_sha256": active_manifest,
        "rollback_suffix": finalizer.RK_P10_ROLLBACK_SUFFIX,
        "rollback_elf_sha256": finalizer.RK_P10_ROLLBACK_ELF_SHA256,
        "rollback_config_manifest_sha256": finalizer.RK_P10_ROLLBACK_CONFIG_MANIFEST_SHA256,
        "deployment_receipt": deployment.name,
        "deployment_receipt_sha256": digest(deployment),
        "exercise_log": exercise.name,
        "exercise_log_sha256": digest(exercise),
        "exercise_error_log": exercise_error.name,
        "exercise_error_log_sha256": digest(exercise_error),
        "tool_identities": {
            "controller": {"path": controller_tool.name, "sha256": digest(controller_tool)},
            "remote_runner": {"path": remote_tool.name, "sha256": digest(remote_tool)},
        },
        "release_evidence": releases,
    })
    return receipt


class FinalizerTests(unittest.TestCase):
    def test_false_nested_gate_cannot_be_promoted(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "result.json"
            write_json(path, {"result": "PASS", "checks": [{"name": "physical", "pass": False}]})
            entry = finalizer.evaluate_json(root, "x", "x", path)
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("checks:physical", entry.issues)

    def test_matrix_preserves_source_fail(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan_path = root / "tools/matrix.json"
            write_json(plan_path, {
                "bands": [{"name": "SWIR"}, {"name": "MWIR"}],
                "captureVariants": [{"name": "fixed"}, {"name": "agc"}, {"name": "annotated"}],
                "blockedCoverage": [{"id": "cloud-id", "status": "BLOCKED_METADATA", "reason": "fixture"}],
                "scenarios": [{"id": "target_near_100m", "factor": "target", "runnable": True}],
            })
            matrix = root / "matrix"
            make_case(matrix / "SWIR/target_near_100m", "PASS")
            make_case(matrix / "MWIR/target_near_100m", "FAIL")
            entry, cases, gallery = finalizer.evaluate_matrix(root, plan_path, matrix)
            self.assertEqual(entry.status, "FAIL")
            self.assertEqual([row["status"] for row in cases], ["PASS", "FAIL"])
            self.assertEqual(entry.details["blockedCoverage"][0]["status"], "BLOCKED_METADATA")
            self.assertEqual(len(gallery), 4)  # target + annotation for each band

    def test_pass_with_missing_artifact_is_fail(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan_path = root / "plan.json"
            write_json(plan_path, {
                "bands": [{"name": "SWIR"}], "captureVariants": [{"name": "fixed"}],
                "blockedCoverage": [], "scenarios": [{"id": "target_near_100m", "factor": "target", "runnable": True}],
            })
            case_dir = root / "matrix/SWIR/target_near_100m"
            make_case(case_dir, "PASS")
            (case_dir / "received.png").unlink()
            entry, cases, _gallery = finalizer.evaluate_matrix(root, plan_path, root / "matrix")
            self.assertEqual(entry.status, "FAIL")
            self.assertEqual(cases[0]["status"], "FAIL")
            self.assertIn("missing_or_invalid_png:received.png", cases[0]["issues"])

    def test_matrix_missing_case_is_not_run_without_crashing(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan_path = root / "plan.json"
            write_json(plan_path, {
                "bands": [{"name": "SWIR"}],
                "captureVariants": [{"name": "fixed"}],
                "blockedCoverage": [{
                    "id": "windows_poison_blocker",
                    "status": "BLOCKED_METADATA",
                    "reason": "historical Windows-only poison fixture",
                }],
                "scenarios": [{"id": "target_near_100m", "factor": "target", "runnable": True}],
            })
            (root / "matrix").mkdir()
            entry, cases, _gallery = finalizer.evaluate_matrix(root, plan_path, root / "matrix")
            self.assertEqual(entry.status, "NOT_RUN")
            self.assertIn("missing_cases:1", entry.issues)
            self.assertEqual(cases[0]["status"], "NOT_RUN")
            self.assertIn("case_json_missing", cases[0]["issues"])

    def test_matrix_complete_pass(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan_path = root / "plan.json"
            write_json(plan_path, {
                "bands": [{"name": "SWIR"}, {"name": "MWIR"}],
                "captureVariants": [{"name": "fixed"}, {"name": "agc"}, {"name": "annotated"}],
                "blockedCoverage": [], "scenarios": [{"id": "target_near_100m", "factor": "target", "runnable": True}],
            })
            for band in ("SWIR", "MWIR"):
                make_case(root / f"matrix/{band}/target_near_100m")
            entry, cases, _gallery = finalizer.evaluate_matrix(root, plan_path, root / "matrix")
            self.assertEqual(entry.status, "PASS")
            self.assertTrue(all(row["status"] == "PASS" for row in cases))
            self.assertTrue(finalizer.valid_sha256(entry.source_sha256))
            self.assertTrue(finalizer.valid_sha256(entry.details["planSha256"]))
            self.assertEqual(entry.source_sha256, entry.details["matrixEvidenceSha256"])
            self.assertEqual(
                [row["role"] for row in entry.details["formalProgramIdentities"]],
                list(finalizer.WINDOWS_PROGRAM_ROLES),
            )

    def test_matrix_observes_one_global_default_duration_across_bands(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan_path = root / "plan.json"
            write_json(plan_path, {
                "bands": [{"name": "SWIR"}, {"name": "MWIR"}],
                "captureVariants": [{"name": "fixed"}],
                "blockedCoverage": [],
                "scenarios": [{"id": "target_near_100m", "factor": "target", "runnable": True}],
            })
            for band in ("SWIR", "MWIR"):
                make_case(root / f"matrix/{band}/target_near_100m", seconds=8)
            entry, cases, _gallery = finalizer.evaluate_matrix(root, plan_path, root / "matrix")
            self.assertEqual(entry.status, "PASS", (entry.issues, cases))
            self.assertEqual(entry.details["observedDefaultDurationSeconds"], 8)
            self.assertEqual(entry.details["observedDefaultDurationValuesSeconds"], [8])
            self.assertEqual(entry.details["explicitScenarioDurationsSeconds"], {})

    def test_matrix_rejects_default_duration_drift_and_noninteger(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan_path = root / "plan.json"
            write_json(plan_path, {
                "bands": [{"name": "SWIR"}, {"name": "MWIR"}],
                "captureVariants": [{"name": "fixed"}],
                "blockedCoverage": [],
                "scenarios": [{"id": "target_near_100m", "factor": "target", "runnable": True}],
            })
            make_case(root / "matrix/SWIR/target_near_100m", seconds=8)
            make_case(root / "matrix/MWIR/target_near_100m", seconds=9)
            entry, _cases, _gallery = finalizer.evaluate_matrix(root, plan_path, root / "matrix")
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("matrix_default_seconds_drift:[8, 9]", entry.issues)
            self.assertIsNone(entry.details["observedDefaultDurationSeconds"])
            self.assertEqual(entry.details["observedDefaultDurationValuesSeconds"], [8, 9])

            noninteger_root = root / "noninteger"
            make_case(noninteger_root / "matrix/SWIR/target_near_100m", seconds=8.0)
            write_json(noninteger_root / "plan.json", {
                "bands": [{"name": "SWIR"}],
                "captureVariants": [{"name": "fixed"}],
                "blockedCoverage": [],
                "scenarios": [{"id": "target_near_100m", "factor": "target", "runnable": True}],
            })
            entry, cases, _gallery = finalizer.evaluate_matrix(
                noninteger_root, noninteger_root / "plan.json", noninteger_root / "matrix",
            )
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("request_seconds_invalid:8.0", cases[0]["issues"])

    def test_matrix_explicit_dynamic_duration_requires_exact_match(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan_path = root / "plan.json"
            plan = {
                "bands": [{"name": "SWIR"}],
                "captureVariants": [{"name": "fixed"}],
                "blockedCoverage": [],
                "scenarios": [{
                    "id": "solar_thermal_inertia_cycle", "factor": "solar_azimuth_elevation",
                    "seconds": 62, "runnable": True,
                }],
            }
            write_json(plan_path, plan)
            case_dir = root / "matrix/SWIR/solar_thermal_inertia_cycle"
            make_case(
                case_dir,
                factor="solar_azimuth_elevation",
                seconds=62,
                explicit_scenario_seconds=62,
            )
            entry, cases, _gallery = finalizer.evaluate_matrix(root, plan_path, root / "matrix")
            self.assertEqual(entry.status, "PASS", (entry.issues, cases))
            self.assertIsNone(entry.details["observedDefaultDurationSeconds"])
            self.assertEqual(
                entry.details["explicitScenarioDurationsSeconds"],
                {"solar_thermal_inertia_cycle": 62},
            )

            case = read_json(case_dir / "case.json")
            case["request"]["seconds"] = 61
            request_path = case_dir / "case_request.json"
            write_json(request_path, case["request"])
            case["files"]["case_request.json"] = {
                "bytes": request_path.stat().st_size,
                "sha256": hashlib.sha256(request_path.read_bytes()).hexdigest(),
            }
            write_json(case_dir / "case.json", case)
            entry, cases, _gallery = finalizer.evaluate_matrix(root, plan_path, root / "matrix")
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("request_seconds_mismatch:61!=62", cases[0]["issues"])

    def test_windows_h264_is_bound_to_matrix_program_identities(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            identities = make_windows_program_identities(workspace)
            acceptance_root = workspace / "h264"
            make_windows_acceptance(acceptance_root, identities)
            entry = finalizer.evaluate_windows_h264(workspace, acceptance_root, identities)
            self.assertEqual(entry.status, "PASS", entry.issues)
            self.assertEqual(
                [row["role"] for row in entry.details["formalProgramIdentities"]],
                list(finalizer.WINDOWS_PROGRAM_ROLES),
            )
            self.assertTrue(finalizer.valid_sha256(entry.details["binaryPreflightSha256"]))
            self.assertTrue(all(
                finalizer.valid_sha256(row["summarySha256"])
                for row in entry.details["bands"].values()
            ))

    def test_windows_h264_rejects_old_or_incomplete_binary_identity(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            identities = make_windows_program_identities(workspace)
            acceptance_root = workspace / "h264"
            summary_path = make_windows_acceptance(acceptance_root, identities)

            overall = read_json(summary_path)
            preflight = dict(overall["preflight"])
            hwa = dict(preflight["hwaExe"])
            hwa["sha256"] = "f" * 64
            preflight["hwaExe"] = hwa
            overall["preflight"] = preflight
            write_json(acceptance_root / "binary_preflight.json", preflight)
            write_json(summary_path, overall)
            entry = finalizer.evaluate_windows_h264(workspace, acceptance_root, identities)
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("formal_program_identity_mismatch:HwaSim_IR:sha256", entry.issues)

            preflight.pop("stimulusExe")
            overall["preflight"] = preflight
            write_json(acceptance_root / "binary_preflight.json", preflight)
            write_json(summary_path, overall)
            entry = finalizer.evaluate_windows_h264(workspace, acceptance_root, identities)
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("binary_preflight_identity_missing:DataDrivenTestQT", entry.issues)

    def test_nir_compatibility_is_bound_to_same_matrix_program_identities(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            identities = make_windows_program_identities(workspace)
            acceptance_root = workspace / "nir"
            summary_path = make_windows_acceptance(
                acceptance_root,
                identities,
                bands=("NIR",),
                requested_band="NIR",
                seconds=3,
            )
            entry = finalizer.evaluate_nir_compatibility(workspace, summary_path, identities)
            self.assertEqual(entry.status, "PASS", entry.issues)

            overall = read_json(summary_path)
            preflight = dict(overall["preflight"])
            stimulus = dict(preflight["stimulusExe"])
            stimulus["sha256"] = "e" * 64
            preflight["stimulusExe"] = stimulus
            overall["preflight"] = preflight
            write_json(acceptance_root / "binary_preflight.json", preflight)
            write_json(summary_path, overall)
            entry = finalizer.evaluate_nir_compatibility(workspace, summary_path, identities)
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("formal_program_identity_mismatch:DataDrivenTestQT:sha256", entry.issues)

    def test_matrix_rejects_formal_program_identity_drift(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan_path = root / "plan.json"
            write_json(plan_path, {
                "bands": [{"name": "SWIR"}, {"name": "MWIR"}],
                "captureVariants": [{"name": "fixed"}],
                "blockedCoverage": [],
                "scenarios": [{"id": "target_near_100m", "factor": "target", "runnable": True}],
            })
            swir = root / "matrix/SWIR/target_near_100m"
            mwir = root / "matrix/MWIR/target_near_100m"
            make_case(swir)
            make_case(mwir)
            case_path = mwir / "case.json"
            case = read_json(case_path)
            case["request"]["formalProgramIdentities"][0]["sha256"] = "d" * 64
            write_json(case_path, case)
            request_path = mwir / "case_request.json"
            write_json(request_path, case["request"])
            files = case["files"]
            files["case_request.json"] = {
                "bytes": request_path.stat().st_size,
                "sha256": hashlib.sha256(request_path.read_bytes()).hexdigest(),
            }
            write_json(case_path, case)
            entry, _cases, _gallery = finalizer.evaluate_matrix(root, plan_path, root / "matrix")
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("matrix_formal_program_identity_drift", entry.issues)
            self.assertEqual(entry.details["formalProgramIdentities"], [])

    def test_matrix_rejects_missing_raw_and_file_manifest_hashes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan_path = root / "plan.json"
            write_json(plan_path, {
                "bands": [{"name": "SWIR"}], "captureVariants": [], "blockedCoverage": [],
                "scenarios": [{"id": "target_near_100m", "factor": "target", "runnable": True}],
            })
            case_dir = root / "matrix/SWIR/target_near_100m"
            make_case(case_dir)
            case_path = case_dir / "case.json"
            case = read_json(case_path)
            del case["rawRadiance"]["sha256"]
            del case["rawRadiance"]["bandQuantity"]
            del case["files"]["raw_radiance.json"]
            case["files"]["fixed_clean.png"].pop("sha256")
            write_json(case_path, case)
            _entry, cases, _gallery = finalizer.evaluate_matrix(root, plan_path, root / "matrix")
            issues = cases[0]["issues"]
            self.assertIn("raw_radiance_sha256_missing", issues)
            self.assertIn("raw_field_missing:bandQuantity", issues)
            self.assertIn("files_manifest_required_missing:raw_radiance.json", issues)
            self.assertIn("files_manifest_sha256_missing:fixed_clean.png", issues)

    def test_matrix_rejects_nan_and_constant_pfm(self) -> None:
        for first, rest, expected in (
            (math.nan, 1.0, "pfm_nonfinite:1"),
            (1.0, 1.0, "pfm_constant:1.0"),
        ):
            with self.subTest(expected=expected), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                plan_path = root / "plan.json"
                write_json(plan_path, {
                    "bands": [{"name": "SWIR"}], "captureVariants": [], "blockedCoverage": [],
                    "scenarios": [{"id": "target_near_100m", "factor": "target", "runnable": True}],
                })
                case_dir = root / "matrix/SWIR/target_near_100m"
                make_case(case_dir)
                rewrite_pfm(case_dir / "raw_radiance.pfm", first, rest)
                _entry, cases, _gallery = finalizer.evaluate_matrix(root, plan_path, root / "matrix")
                self.assertIn(expected, cases[0]["issues"])

    def test_matrix_rejects_decoded_missing_fields_wrong_identity_and_source(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan_path = root / "plan.json"
            write_json(plan_path, {
                "bands": [{"name": "SWIR"}], "captureVariants": [], "blockedCoverage": [],
                "scenarios": [{"id": "target_near_100m", "factor": "target", "runnable": True}],
            })
            case_dir = root / "matrix/SWIR/target_near_100m"
            make_case(case_dir)
            case_path = case_dir / "case.json"
            case = read_json(case_path)
            case["decodedEvidence"]["fixed"].pop("sourceSha256")
            case["decodedEvidence"]["annotated"]["sourceSeq"] = 91
            fixed_source = case["decodedEvidence"]["fixed"]["source"]
            case["decodedEvidence"]["received"]["source"] = fixed_source
            case["decodedEvidence"]["received"]["sourceSha256"] = hashlib.sha256(Path(fixed_source).read_bytes()).hexdigest()
            write_json(case_path, case)
            _entry, cases, _gallery = finalizer.evaluate_matrix(root, plan_path, root / "matrix")
            issues = cases[0]["issues"]
            self.assertIn("decoded_field_missing:fixed:sourceSha256", issues)
            self.assertIn("decoded_identity_mismatch", issues)
            self.assertIn("decoded_received_not_agc_source", issues)

    def test_matrix_rejects_identity_latest_drop_and_unordered(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan_path = root / "plan.json"
            write_json(plan_path, {
                "bands": [{"name": "SWIR"}], "captureVariants": [], "blockedCoverage": [],
                "scenarios": [{"id": "target_near_100m", "factor": "target", "runnable": True}],
            })
            case_dir = root / "matrix/SWIR/target_near_100m"
            make_case(case_dir)
            identity_path = case_dir / "frame_identity.json"
            identity = read_json(identity_path)
            identity["evidence"][0]["queuePolicy"] = "Latest"
            identity["evidence"][0]["dropped"] = 1
            identity["evidence"][1]["sourceSeq"] = 1
            write_json(identity_path, identity)
            _entry, cases, _gallery = finalizer.evaluate_matrix(root, plan_path, root / "matrix")
            issues = cases[0]["issues"]
            self.assertIn("frame_identity_latest_value:queuePolicy", issues)
            self.assertIn("frame_identity_loss:dropped=1", issues)
            self.assertIn("frame_identity_tcp_not_ordered:fixed", issues)

    def test_matrix_rejects_missing_physical_component(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan_path = root / "plan.json"
            write_json(plan_path, {
                "bands": [{"name": "SWIR"}], "captureVariants": [], "blockedCoverage": [],
                "scenarios": [{"id": "target_near_100m", "factor": "target", "runnable": True}],
            })
            case_dir = root / "matrix/SWIR/target_near_100m"
            make_case(case_dir)
            components = case_dir / "physical_components.csv"
            with components.open("r", encoding="utf-8", newline="") as stream:
                rows = list(csv.DictReader(stream))
            with components.open("w", encoding="utf-8", newline="") as stream:
                writer = csv.DictWriter(stream, fieldnames=("variant", "source_log", "line", "tag", "fields_json"))
                writer.writeheader()
                writer.writerows(row for row in rows if not (row["variant"] == "fixed" and row["tag"] == "Stage5 RadianceComponents"))
            _entry, cases, _gallery = finalizer.evaluate_matrix(root, plan_path, root / "matrix")
            self.assertIn("physical_component_missing:fixed:Stage5 RadianceComponents", cases[0]["issues"])

    def test_matrix_rejects_fixed_mapping_drift_within_group(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan_path = root / "plan.json"
            write_json(plan_path, {
                "bands": [{"name": "SWIR"}], "captureVariants": [], "blockedCoverage": [],
                "scenarios": [
                    {"id": "case_a", "factor": "target", "comparisonGroup": "same", "runnable": True},
                    {"id": "case_b", "factor": "target", "comparisonGroup": "same", "runnable": True},
                ],
            })
            make_case(root / "matrix/SWIR/case_a", mapping_gain=1.0)
            make_case(root / "matrix/SWIR/case_b", mapping_gain=2.0)
            for name in ("case_a", "case_b"):
                case_path = root / f"matrix/SWIR/{name}/case.json"
                case = read_json(case_path)
                case["request"]["comparisonGroup"] = "same"
                case["request"]["scenario"]["comparisonGroup"] = "same"
                write_json(case_path.parent / "case_request.json", case["request"])
                request_file = case_path.parent / "case_request.json"
                case["files"]["case_request.json"] = {
                    "bytes": request_file.stat().st_size,
                    "sha256": hashlib.sha256(request_file.read_bytes()).hexdigest(),
                }
                write_json(case_path, case)
            entry, cases, _gallery = finalizer.evaluate_matrix(root, plan_path, root / "matrix")
            self.assertEqual(entry.status, "FAIL")
            self.assertTrue(all("fixed_mapping_drift:SWIR:same" in row["issues"] for row in cases))

    def test_matrix_rejects_plan_request_sensor_field_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan_path = root / "plan.json"
            write_json(plan_path, {
                "bands": [{"name": "SWIR", "protocolValue": 0, "rangeUm": [1.1, 2.5]}],
                "captureVariants": [], "blockedCoverage": [], "baseFixture": "tools/p11_inputs/base.json",
                "scenarios": [{
                    "id": "target_near_100m", "factor": "target", "comparisonGroup": "fixture_group",
                    "sensorPixelAngleUrad": 100, "rangeKm": 0.1, "runnable": True,
                }],
            })
            case_dir = root / "matrix/SWIR/target_near_100m"
            make_case(case_dir)
            case_path = case_dir / "case.json"
            case = read_json(case_path)
            case["request"]["protocolBand"] = 2
            case["request"]["rangeUm"] = [1.5, 2.5]
            write_json(case_dir / "case_request.json", case["request"])
            request_file = case_dir / "case_request.json"
            case["files"]["case_request.json"] = {
                "bytes": request_file.stat().st_size,
                "sha256": hashlib.sha256(request_file.read_bytes()).hexdigest(),
            }
            write_json(case_path, case)
            _entry, cases, _gallery = finalizer.evaluate_matrix(root, plan_path, root / "matrix")
            self.assertIn("request_protocol_band_mismatch:2!=0", cases[0]["issues"])
            self.assertIn("request_range_um_mismatch:[1.5, 2.5]!=[1.1, 2.5]", cases[0]["issues"])
            self.assertIn("request_scenario_plan_mismatch:sensorPixelAngleUrad", cases[0]["issues"])
            self.assertIn("request_scenario_plan_mismatch:rangeKm", cases[0]["issues"])

    def test_matrix_rejects_unrecognized_identity_schema(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan_path = root / "plan.json"
            write_json(plan_path, {
                "bands": [{"name": "SWIR"}], "captureVariants": [], "blockedCoverage": [],
                "scenarios": [{"id": "target_near_100m", "factor": "target", "runnable": True}],
            })
            case_dir = root / "matrix/SWIR/target_near_100m"
            make_case(case_dir)
            identity_path = case_dir / "frame_identity.json"
            identity = read_json(identity_path)
            identity["schema"] = "unknown"
            write_json(identity_path, identity)
            _entry, cases, _gallery = finalizer.evaluate_matrix(root, plan_path, root / "matrix")
            self.assertIn("frame_identity_schema_unrecognized:unknown", cases[0]["issues"])

    def test_full_matrix_case_copy_is_self_contained_and_missing_case_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first = root / "matrix/SWIR/case_a"
            second = root / "matrix/MWIR/case_b"
            make_case(first)
            make_case(second)
            cases = [
                {"band": "SWIR", "caseId": "case_a", "source": str(first / "case.json")},
                {"band": "MWIR", "caseId": "case_b", "source": str(second / "case.json")},
            ]
            staging = root / "delivery"
            copied = finalizer.copy_matrix_cases(root, staging, cases)
            self.assertEqual(len(copied), 2 * len(finalizer.WINDOWS_CASE_PACKAGE_FILES))
            for row in cases:
                for name in finalizer.WINDOWS_CASE_PACKAGE_FILES:
                    self.assertTrue((staging / "artifacts/matrix" / row["band"] / row["caseId"] / name).is_file())
            (second / "raw_radiance.json").unlink()
            with self.assertRaises(FileNotFoundError):
                finalizer.copy_matrix_cases(root, root / "delivery2", cases)

    def test_offline_html_indexes_every_case_and_all_four_views_by_theme(self) -> None:
        status = {
            "generatedLocal": "test", "overallStatus": "PASS",
            "entries": [{
                "id": "windows-image-matrix", "title": "matrix", "status": "PASS",
                "summary": "two cases", "source": "matrix", "sourceSha256": None,
                "issues": [], "details": {"blockedCoverage": []},
            }],
        }
        cases = [
            {
                "band": "SWIR", "caseId": "solar_off", "factor": "solar",
                "comparisonGroup": "solar_toggle", "factorDelta": "solar disabled",
                "status": "PASS", "issues": [],
            },
            {
                "band": "MWIR", "caseId": "solar_on", "factor": "solar",
                "comparisonGroup": "solar_toggle", "factorDelta": "solar enabled",
                "status": "PASS", "issues": [],
            },
        ]
        rendered = finalizer.render_html(status, cases, [])
        self.assertIn("完整矩阵图片（按物理主题与对比组）", rendered)
        self.assertIn("solar_toggle", rendered)
        self.assertIn("solar disabled", rendered)
        self.assertIn("solar enabled", rendered)
        for row in cases:
            base = f'artifacts/matrix/{row["band"]}/{row["caseId"]}'
            for variant, filename in (
                ("fixed", "fixed_clean.png"), ("agc", "auto_clean.png"),
                ("annotated", "annotated.png"), ("received", "received.png"),
            ):
                self.assertIn(f'data-variant="{variant}" href="{base}/{filename}"', rendered)
        self.assertEqual(rendered.count('class="gallery matrix-case"'), 2)
        self.assertEqual(rendered.count('data-variant="'), 8)

    def test_data_manifest_preserves_units_sources_modtran_and_jacobian_provenance(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            band = root / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv"
            band.parent.mkdir(parents=True)
            band.write_text(
                "band,humidity_profile\nSWIR,scaled_mls_surface_rh30\nMWIR,scaled_mls_surface_rh60\n",
                encoding="utf-8",
            )
            solar = band.parent / "solar_heating_lut_si.csv"
            solar.write_text(
                "band,humidity_profile\nSOLAR_SHORTWAVE_0.30_2.50_UM,default\n",
                encoding="utf-8",
            )
            materials = root / "HwaSim_IR/Bin/Config/Materials/MaterialBandOptics.csv"
            materials.parent.mkdir(parents=True)
            materials.write_text("Material,Source\npaint,P11_engineering_assumption\n", encoding="utf-8")
            engine = {
                "executable": "F:/Programs/PcModWin5/Bin/Mod5.2.1.0.exe",
                "file_version": "5.2.1.0", "sha256": "a" * 64,
                "run_status": "SUCCESS_REAL_MODTRAN_OUTPUT",
            }
            for family, qc_name, checks in (
                ("humidity_grid", "humidity_qc_results.json", [
                    {"check": "real_modtran_case_count", "passed": True, "measured": 504, "expected": 504},
                    {"check": "all_jacobian_integrals_within_tolerance", "passed": True, "measured": {"max_relative_error": 1e-6}, "expected": "<=5e-6"},
                ]),
                ("solar_heating_ground_grid", "solar_heating_qc_results.json", [
                    {"check": "real_modtran_case_count", "passed": True, "measured": 96, "expected": 96},
                    {"check": "jacobian_integrals_within_tolerance", "passed": True, "measured": 1e-8, "expected": "<=2e-5"},
                ]),
            ):
                evidence_root = root / "logs/p11/modtran" / family
                write_json(evidence_root / qc_name, {"status": "PASS", "checks": checks})
                write_json(evidence_root / "engine_and_license_evidence.json", engine)
                write_json(evidence_root / "generation_provenance.json", {"component_runs": 96})
                write_json(evidence_root / "formal_publish/publish_manifest.json", {"publish_status": "SUCCESS"})
            legacy = root / "HwaSim_IR/Bin/Config/SensorWave/Archive/P11/default_SWIR_legacy_1p5_2p5.json"
            write_json(legacy, {"lowUm": 1.5, "highUm": 2.5})

            manifest = finalizer.build_data_manifest(root)
            self.assertEqual(manifest["schema"], "HwaSimIR.P11.SpectralAndPhysicsDataManifest.v3")
            self.assertIn("irradianceUnit", manifest)
            self.assertEqual(manifest["modtranRuntime"]["sha256"], "a" * 64)
            self.assertFalse(manifest["conversion"]["arbitraryScalingAllowed"])
            self.assertFalse(manifest["conversion"]["totalRadTargetContributionReused"])
            by_role = {row["role"]: row for row in manifest["datasets"]}
            camera = by_role["formal camera-band atmosphere LUT"]
            self.assertIn("real MODTRAN 5.2.1", camera["source"])
            self.assertEqual(camera["units"]["tau_up"], "dimensionless")
            self.assertIn("1e4/lambda_um^2", camera["jacobian"]["formula"])
            self.assertEqual(camera["formalCounts"], {"MWIR": 1, "SWIR": 1})
            self.assertEqual(len(camera["modtran"]["qcEvidence"]["selectedChecks"]), 2)
            heating = by_role["broadband solar heating LUT"]
            self.assertEqual(heating["modtran"]["directSource"], "target-level MODOUT2 SOL TR")
            self.assertIn("not added to diffuse", heating["modtran"]["fluxDirectPolicy"])
            self.assertIn("nativeDiffuseFluxUnit", heating["jacobian"])
            material = by_role["explicit material band optics"]
            self.assertIn("Visible RGB is not used", material["prohibition"])
            self.assertTrue(any(row.get("purpose") == "preserve the conflicting legacy 1.5-2.5 um profile" for row in manifest["archives"]))

    def test_bundle_includes_complete_non_p11_modtran_reproduction_toolset(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in finalizer.MODTRAN_BUNDLE_TOOLS:
                path = root / "tools" / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(f"# {name}\n", encoding="utf-8")
            copied = finalizer.copy_bundle_inputs(root, root / "delivery")
            copied_paths = {row["path"] for row in copied}
            self.assertEqual(set(finalizer.MODTRAN_BUNDLE_TOOLS), {
                "modtran_convert_to_si.py", "modtran_build_lut.py", "modtran_qc.py",
                "modtran_build_solar_heating_lut.py", "test_modtran_units.py",
            })
            for name in finalizer.MODTRAN_BUNDLE_TOOLS:
                self.assertIn(f"bundle/tools/{name}", copied_paths)
                self.assertTrue((root / "delivery/bundle/tools" / name).is_file())

    def test_matrix_deduplicates_declared_blocked_coverage(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan_path = root / "plan.json"
            write_json(plan_path, {
                "bands": [{"name": "SWIR"}], "captureVariants": [{"name": "fixed"}],
                "blockedCoverage": [{"id": "range-2km", "status": "BLOCKED_DATA", "reason": "audited LUT ends at 1 km"}],
                "scenarios": [{
                    "id": "target_range_2km", "coverageId": "range-2km", "factor": "target",
                    "runnable": False, "blockedReason": "BLOCKED_DATA: no audited cell",
                }],
            })
            (root / "matrix").mkdir()
            entry, cases, _gallery = finalizer.evaluate_matrix(root, plan_path, root / "matrix")
            self.assertEqual(cases, [])
            self.assertEqual(len(entry.details["blockedCoverage"]), 1)
            self.assertEqual(entry.details["blockedCoverage"][0]["reason"], "audited LUT ends at 1 km")
            self.assertIn("blockedCoverage=1", entry.summary)

    def test_matrix_adds_undeclared_nonrunnable_coverage(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            plan_path = root / "plan.json"
            write_json(plan_path, {
                "bands": [{"name": "SWIR"}], "captureVariants": [{"name": "fixed"}],
                "blockedCoverage": [],
                "scenarios": [{
                    "id": "missing-fixture", "factor": "cloud", "runnable": False,
                    "blockedReason": "BLOCKED_METADATA: fixture unavailable",
                }],
            })
            (root / "matrix").mkdir()
            entry, cases, _gallery = finalizer.evaluate_matrix(root, plan_path, root / "matrix")
            self.assertEqual(cases, [])
            self.assertEqual(entry.details["blockedCoverage"], [{
                "id": "missing-fixture", "status": "BLOCKED_METADATA",
                "reason": "BLOCKED_METADATA: fixture unavailable",
            }])
            self.assertIn("blockedCoverage=1", entry.summary)

    def test_windows_aero_summary_pass_is_bound_and_hashed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            matrix = root / "matrix"
            matrix.mkdir()
            summary_path = root / "aero_summary.json"
            write_json(summary_path, {
                "schema": finalizer.WINDOWS_AERO_SCHEMA,
                "result": "PASS",
                "matrixRoot": str(matrix.resolve()),
                "errors": [],
            })
            entry = finalizer.evaluate_windows_aero_summary(root, matrix, summary_path)
            expected_hash = hashlib.sha256(summary_path.read_bytes()).hexdigest()
            self.assertEqual(entry.status, "PASS")
            self.assertEqual(entry.source_sha256, expected_hash)
            self.assertEqual(entry.details["summarySha256"], expected_hash)
            self.assertEqual(entry.as_dict()["sourceSha256"], expected_hash)
            rendered = finalizer.render_html({
                "generatedLocal": "test", "overallStatus": "PASS", "entries": [entry.as_dict()],
            }, [], [])
            self.assertIn(expected_hash, rendered)
            args = finalizer.parse_args(["--windows-aero-summary", str(summary_path)])
            self.assertEqual(args.windows_aero_summary, str(summary_path))

    def test_windows_aero_summary_is_mandatory_with_matrix_root(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            matrix = root / "matrix"
            matrix.mkdir()
            entry = finalizer.evaluate_windows_aero_summary(root, matrix, None)
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("windows_aero_summary_not_supplied", entry.issues)
            historical = finalizer.as_historical_diagnostic(entry, "DDS-only")
            self.assertEqual(historical.status, "HISTORICAL_DIAGNOSTIC")
            self.assertEqual(finalizer.aggregate_status([historical], []), "PARTIAL")

    def test_windows_aero_summary_rejects_source_fail(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            matrix = root / "matrix"
            matrix.mkdir()
            summary_path = root / "aero_summary.json"
            write_json(summary_path, {
                "schema": finalizer.WINDOWS_AERO_SCHEMA,
                "result": "FAIL",
                "matrixRoot": str(matrix.resolve()),
            })
            entry = finalizer.evaluate_windows_aero_summary(root, matrix, summary_path)
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("windows_aero_result_not_pass:'FAIL'", entry.issues)

    def test_windows_aero_summary_rejects_schema_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            matrix = root / "matrix"
            matrix.mkdir()
            summary_path = root / "aero_summary.json"
            write_json(summary_path, {
                "schema": "wrong-schema", "result": "PASS", "matrixRoot": str(matrix.resolve()),
            })
            entry = finalizer.evaluate_windows_aero_summary(root, matrix, summary_path)
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("windows_aero_schema_mismatch:'wrong-schema'", entry.issues)

    def test_windows_aero_summary_rejects_matrix_root_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            matrix = root / "matrix"
            other = root / "other-matrix"
            matrix.mkdir()
            other.mkdir()
            summary_path = root / "aero_summary.json"
            write_json(summary_path, {
                "schema": finalizer.WINDOWS_AERO_SCHEMA,
                "result": "PASS",
                "matrixRoot": str(other.resolve()),
            })
            entry = finalizer.evaluate_windows_aero_summary(root, matrix, summary_path)
            self.assertEqual(entry.status, "FAIL")
            self.assertTrue(any(issue.startswith("windows_aero_matrix_root_mismatch:") for issue in entry.issues))

    def test_rk_identity_mismatch_is_explicit(self) -> None:
        entries = [
            finalizer.Evidence("rk3588-build", "build", "PASS", None, details={"elf_sha256": "a", "build_id": "1", "stage_id": "s"}),
            finalizer.Evidence("rk3588-deployment", "deploy", "PASS", None, details={"elf_sha256": "b", "build_id": "1", "stage_id": "s", "config_manifest_sha256": "c"}),
            finalizer.Evidence("rk3588-dual-band", "acceptance", "PASS", None, details={"elfSha256": "b", "deploymentStageId": "s", "configManifestSha256": "c"}),
            finalizer.Evidence("rk3588-rollback", "rollback", "PASS", None, details={"active_elf_sha256": "b", "stage_id": "s"}),
        ]
        self.assertEqual(finalizer.cross_check_rk(entries), ["build_deploy_elf_hash_mismatch"])

    def test_rk_dds_lifecycle_and_image_identities_join_deployment_chain(self) -> None:
        entries = [
            finalizer.Evidence(
                "rk3588-deployment", "deploy", "PASS", None,
                details={"stage_id": "s", "elf_sha256": "a", "config_manifest_sha256": "c"}),
            finalizer.Evidence(
                "rk3588-dds-lifecycle", "lifecycle", "PASS", None,
                details={"deploymentStageId": "wrong", "elfSha256": "a", "configManifestSha256": "c"}),
            finalizer.Evidence(
                "rk3588-dds-image-matrix", "image", "PARTIAL", None,
                details={"deploymentStageId": "s", "elfSha256": "b", "configManifestSha256": "c"}),
        ]
        self.assertEqual(finalizer.cross_check_rk(entries), [
            "deploy_lifecycle_stage_mismatch",
            "deploy_image_matrix_elf_hash_mismatch",
        ])

    def test_rk_acceptance_complete_contract_passes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            root = workspace / "acceptance"
            make_rk_acceptance_root(root)
            entry = finalizer.evaluate_rk_acceptance(workspace, root)
            self.assertEqual(entry.status, "PASS", entry.issues)
            self.assertEqual(entry.details["bands"]["SWIR"]["ddsOnlyTransport"]["result"], "PASS")
            for required in (
                "formal_band_selection", "sensor_profile_file", "ordered_input_policy",
                "mali_gpu", "mpp_h264_encode", "ffmpeg_h264_decode",
                "control_responses", "dds_publisher_counts", "dds_receiver_counts",
                "performance_input_overwritten", "performance_input_queue_overflow",
                "performance_source_seq_gap",
            ):
                self.assertIn(required, finalizer.RK_REQUIRED_GATES)

    def test_rk_acceptance_rejects_udp_route_tcp_payload_and_codec_fallback(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            root = workspace / "acceptance"
            make_rk_acceptance_root(root)
            board = root / "SWIR/board/hwa.log"
            board.write_text(
                board.read_text(encoding="utf-8")
                + "[ProtocolRoute] transport=udp type=realtime accepted=1\n"
                  "[TcpPayloadConfig] SendVideo=1 SendAnnotation=0 SendRealtimeData=0 "
                  "ForwardInitControl=0\n[CodecFallback] requested=h264 actual=jpeg\n",
                encoding="utf-8",
            )
            entry = finalizer.evaluate_rk_acceptance(workspace, root)
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("SWIR:dds_only_transport:udp_protocol_route_count:1", entry.issues)
            self.assertIn(
                "SWIR:dds_only_transport:tcp_payload_nonzero_or_missing_rows:1",
                entry.issues,
            )
            self.assertIn("SWIR:dds_only_transport:codec_fallback_observed", entry.issues)

    def test_rk_dds_lifecycle_complete_contract_and_hashes_pass(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            summary = make_rk_dds_lifecycle_summary(workspace / "lifecycle")
            entry = finalizer.evaluate_rk_dds_lifecycle(workspace, summary)
            self.assertEqual(entry.status, "PASS", entry.issues)
            self.assertEqual(set(entry.details["cases"]), set(finalizer.RK_DDS_LIFECYCLE_REQUIRED_GATES))

            pause = summary.parent / "pause_swir/lifecycle_summary.json"
            value = read_json(pause)
            publisher_gate = next(
                row for row in value["gates"]
                if row["gate"] == "dds_domain_150_board")
            publisher_gate["detail"] = "publisher_initialized=0"
            write_json(pause, value)
            overall = read_json(summary)
            overall["cases"][0]["summarySha256"] = hashlib.sha256(pause.read_bytes()).hexdigest()
            write_json(summary, overall)
            publisher_failed = finalizer.evaluate_rk_dds_lifecycle(workspace, summary)
            self.assertEqual(publisher_failed.status, "FAIL")
            self.assertIn(
                "pause_swir:dds_domain_150_board_missing_publisher_initialized_1",
                publisher_failed.issues,
            )

            publisher_gate["detail"] = "publisher_initialized=1"
            value["gates"].append({"gate": "unexpected_false_gate", "pass": False})
            write_json(pause, value)
            overall["cases"][0]["summarySha256"] = hashlib.sha256(pause.read_bytes()).hexdigest()
            write_json(summary, overall)
            false_gate = finalizer.evaluate_rk_dds_lifecycle(workspace, summary)
            self.assertIn("pause_swir:source_gate_not_true:unexpected_false_gate", false_gate.issues)

            value["gates"].pop()
            value["transport"]["tcp_payload_counts"]["SendVideo"] = 1
            write_json(pause, value)
            overall["cases"][0]["summarySha256"] = hashlib.sha256(pause.read_bytes()).hexdigest()
            write_json(summary, overall)
            failed = finalizer.evaluate_rk_dds_lifecycle(workspace, summary)
            self.assertEqual(failed.status, "FAIL")
            self.assertIn("pause_swir:tcp_payload_counts_not_four_zeros", failed.issues)

    def test_rk_dds_image_matrix_is_partial_only_for_three_declared_blockers(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            root = workspace / "dds-image"
            make_rk_dds_image_root(root)
            entry = finalizer.evaluate_rk_dds_image_matrix(workspace, root)
            self.assertEqual(entry.status, "PARTIAL", entry.issues)
            self.assertEqual(len(entry.details["cases"]), 2)
            self.assertEqual(len(entry.details["blockers"]), 3)
            self.assertEqual(
                {row["id"]: row["status"] for row in entry.details["blockers"]},
                finalizer.RK_DDS_IMAGE_REQUIRED_BLOCKERS,
            )
            for case in entry.details["cases"]:
                self.assertEqual(
                    set(case["deliveryArtifacts"]),
                    set(finalizer.RK_DDS_CASE_PACKAGE_FILES),
                )
                self.assertIn("received_decode.png", case["deliveryArtifacts"]["received.png"])
                self.assertEqual(case["factor"], "target")
                self.assertEqual(case["comparisonGroup"], "near")

            fixed_png = root / "SWIR/target_near_100m/fixed_clean.png"
            fixed_bytes = fixed_png.read_bytes()
            fixed_png.write_bytes(fixed_bytes + b"TAMPER")
            tampered_fixed = finalizer.evaluate_rk_dds_image_matrix(workspace, root)
            self.assertEqual(tampered_fixed.status, "FAIL")
            self.assertIn(
                "dds_image_matrix:SWIR/target_near_100m:fixed:"
                "canonical_png_sha256_mismatch",
                tampered_fixed.issues,
            )
            fixed_png.write_bytes(fixed_bytes)

            received_png = (
                root / "SWIR/target_near_100m/variants/agc/acceptance/SWIR/received_decode.png")
            received_bytes = received_png.read_bytes()
            received_png.write_bytes(received_bytes + b"TAMPER")
            tampered_received = finalizer.evaluate_rk_dds_image_matrix(workspace, root)
            self.assertEqual(tampered_received.status, "FAIL")
            self.assertIn(
                "dds_image_matrix:SWIR/target_near_100m:agc:"
                "received_decode_png_sha256_mismatch",
                tampered_received.issues,
            )
            received_png.write_bytes(received_bytes)

            plan = root / "matrix_plan.json"
            plan_bytes = plan.read_bytes()
            plan.write_bytes(plan_bytes + b"\n")
            tampered_plan = finalizer.evaluate_rk_dds_image_matrix(workspace, root)
            self.assertEqual(tampered_plan.status, "FAIL")
            self.assertIn("dds_image_matrix:plan_sha256_mismatch", tampered_plan.issues)
            plan.write_bytes(plan_bytes)

            summary = root / "dds_image_matrix_summary.json"
            summary_value = read_json(summary)
            summary_value["blockedCoverage"][0]["id"] = "poison_blocker"
            write_json(summary, summary_value)
            tampered_blockers = finalizer.evaluate_rk_dds_image_matrix(workspace, root)
            self.assertEqual(tampered_blockers.status, "FAIL")
            self.assertTrue(any(
                issue.startswith("dds_image_matrix:blocker_ids:")
                for issue in tampered_blockers.issues))
            summary_value["blockedCoverage"][0]["id"] = "cloud_target_front_behind"
            write_json(summary, summary_value)

            board = root / "SWIR/target_near_100m/variants/fixed/acceptance/SWIR/board/hwa.log"
            board.write_text(
                board.read_text(encoding="utf-8")
                + "[ProtocolRoute] transport=udp type=realtime accepted=1\n",
                encoding="utf-8",
            )
            failed = finalizer.evaluate_rk_dds_image_matrix(workspace, root)
            self.assertEqual(failed.status, "FAIL")
            self.assertTrue(any("udp_protocol_route_count:1" in issue for issue in failed.issues))
            rows, _gallery = finalizer.dds_image_delivery_views(workspace, failed)
            by_band = {row["band"]: row for row in rows}
            self.assertEqual(by_band["SWIR"]["status"], "FAIL")
            self.assertTrue(any(
                "udp_protocol_route_count:1" in issue
                for issue in by_band["SWIR"]["issues"]))
            self.assertEqual(by_band["MWIR"]["status"], "PASS")

    def test_dds_only_gallery_stages_rk_cases_and_never_windows_historical_cases(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            plan = workspace / "tools/p11_windows_evidence_matrix.json"
            write_json(plan, {
                "bands": [{"name": "SWIR"}],
                "captureVariants": [
                    {"name": "fixed"}, {"name": "agc"}, {"name": "annotated"},
                ],
                "blockedCoverage": [],
                "scenarios": [{
                    "id": "windows_only", "factor": "target", "runnable": True,
                }],
            })
            windows_root = workspace / "windows-matrix"
            make_case(windows_root / "SWIR/windows_only")
            dds_root = workspace / "dds-image"
            make_rk_dds_image_root(dds_root)

            args = finalizer.parse_args([
                "--workspace", str(workspace),
                "--acceptance-mode", "dds-only",
                "--windows-matrix-root", str(windows_root),
                "--rk-dds-image-root", str(dds_root),
            ])
            status, cases, gallery, entries = finalizer.build_status(args)
            self.assertEqual(len(cases), 2)
            self.assertEqual({row["caseId"] for row in cases}, {"target_near_100m"})
            self.assertEqual({row["band"] for row in cases}, {"SWIR", "MWIR"})
            self.assertEqual(len(gallery), 2)
            windows_entry = next(
                row for row in entries if row.evidence_id == "windows-image-matrix")
            self.assertEqual(windows_entry.status, "HISTORICAL_DIAGNOSTIC")

            staging = workspace / "staging"
            copied = finalizer.copy_matrix_cases(workspace, staging, cases)
            self.assertEqual(
                len(copied), len(cases) * len(finalizer.RK_DDS_CASE_PACKAGE_FILES))
            _gallery_copied, rendered_gallery = finalizer.copy_gallery(
                workspace, staging, gallery)
            for row in cases:
                base = staging / "artifacts/matrix" / row["band"] / row["caseId"]
                for name in finalizer.RK_DDS_CASE_PACKAGE_FILES:
                    self.assertTrue((base / name).is_file(), name)
                source_received = workspace / row["deliveryArtifacts"]["received.png"]
                self.assertFalse((source_received.parents[4] / "received.png").exists())
                self.assertEqual(
                    (base / "received.png").read_bytes(), source_received.read_bytes())
                self.assertEqual(
                    hashlib.sha256((base / "received.png").read_bytes()).hexdigest(),
                    hashlib.sha256((base / "auto_clean.png").read_bytes()).hexdigest(),
                )
                self.assertTrue((base / "physical_components.json").is_file())
                self.assertFalse((base / "physical_components.csv").exists())

            rendered = finalizer.render_html(status, cases, rendered_gallery)
            self.assertNotIn("artifacts/matrix/SWIR/windows_only", rendered)
            self.assertIn(
                "artifacts/matrix/SWIR/target_near_100m/received.png", rendered)
            self.assertIn("physical_components.json", rendered)
            for blocker in (
                "cloud_target_front_behind", "cloud_individual_id_disable",
                "target_range_2km_physical_imagery",
            ):
                self.assertIn(blocker, rendered)
            self.assertNotIn("windows_poison_blocker", rendered)

    def test_dds_only_aggregate_ignores_historical_windows_failures(self) -> None:
        blocking = [
            finalizer.Evidence(evidence_id, evidence_id, "PASS", None)
            for evidence_id in finalizer.DDS_ONLY_BLOCKING_IDS
        ]
        windows = finalizer.as_historical_diagnostic(
            finalizer.Evidence("windows-h264-60s", "Windows", "FAIL", None,
                               issues=["historic_failure"]),
            "DDS-only",
        )
        self.assertEqual(windows.status, "HISTORICAL_DIAGNOSTIC")
        self.assertEqual(windows.details["validatedSourceStatus"], "FAIL")
        self.assertEqual(finalizer.aggregate_status(blocking + [windows], []), "PASS")
        image = next(row for row in blocking
                     if row.evidence_id == "rk3588-dds-image-matrix")
        image.status = "PARTIAL"
        self.assertEqual(finalizer.aggregate_status(blocking + [windows], []), "PARTIAL")

    def test_rk_acceptance_empty_gates_cannot_pass(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            root = workspace / "acceptance"
            make_rk_acceptance_root(root)
            swir = root / "SWIR/acceptance_summary.json"
            value = json.loads(swir.read_text(encoding="utf-8"))
            value["gates"] = []
            write_json(swir, value)
            entry = finalizer.evaluate_rk_acceptance(workspace, root)
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("SWIR:required_gate_count:formal_raw_storage=0", entry.issues)
            self.assertIn("SWIR:required_gate_count:raw_pfm_120=0", entry.issues)

    def test_rk_acceptance_duplicate_required_gate_cannot_pass(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            root = workspace / "acceptance"
            make_rk_acceptance_root(root)
            swir = root / "SWIR/acceptance_summary.json"
            value = json.loads(swir.read_text(encoding="utf-8"))
            value["gates"].append({"gate": "formal_raw_storage", "pass": True, "detail": "duplicate"})
            write_json(swir, value)
            entry = finalizer.evaluate_rk_acceptance(workspace, root)
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("SWIR:required_gate_count:formal_raw_storage=2", entry.issues)

    def test_rk_acceptance_overall_cases_must_match_band_summaries(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            root = workspace / "acceptance"
            make_rk_acceptance_root(root)
            overall_path = root / "acceptance_overall.json"
            overall = json.loads(overall_path.read_text(encoding="utf-8"))
            overall["cases"][0]["protocol_band"] = 2
            overall["cases"][1]["result"] = "FAIL"
            write_json(overall_path, overall)
            entry = finalizer.evaluate_rk_acceptance(workspace, root)
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("SWIR:overall_protocol_band=2", entry.issues)
            self.assertIn("MWIR:overall_case_result=FAIL!=band_result=PASS", entry.issues)

    def test_rk_acceptance_rejects_schema_band_protocol_and_half_lattice(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            root = workspace / "acceptance"
            make_rk_acceptance_root(root)
            swir = root / "SWIR/acceptance_summary.json"
            value = json.loads(swir.read_text(encoding="utf-8"))
            value["schema"] = "wrong"
            value["band"] = "MWIR"
            value["protocol_band"] = 2
            value["raw_radiance"]["captures"][0]["half_lattice_mismatch_count"] = 1
            write_json(swir, value)
            entry = finalizer.evaluate_rk_acceptance(workspace, root)
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("SWIR:schema='wrong'", entry.issues)
            self.assertIn("SWIR:declared_band='MWIR'", entry.issues)
            self.assertIn("SWIR:protocol_band=2", entry.issues)
            self.assertIn("SWIR:raw_capture_invalid:0", entry.issues)

    def test_rk_acceptance_rejects_each_invalid_final_texture_field(self) -> None:
        invalid_values = {
            "texture_floating_point": "0",
            "actual_texture_component_type": "float",
            "actual_texture_component_width": 4,
            "actual_texture_components": 3,
        }
        for field, invalid in invalid_values.items():
            with self.subTest(field=field), tempfile.TemporaryDirectory() as directory:
                workspace = Path(directory)
                root = workspace / "acceptance"
                make_rk_acceptance_root(root)
                swir = root / "SWIR/acceptance_summary.json"
                value = json.loads(swir.read_text(encoding="utf-8"))
                value["raw_radiance"]["storage"][0][field] = invalid
                write_json(swir, value)
                entry = finalizer.evaluate_rk_acceptance(workspace, root)
                self.assertEqual(entry.status, "FAIL")
                self.assertIn("SWIR:raw_storage_invalid:0", entry.issues)

        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            root = workspace / "acceptance"
            make_rk_acceptance_root(root)
            swir = root / "SWIR/acceptance_summary.json"
            value = json.loads(swir.read_text(encoding="utf-8"))
            del value["raw_radiance"]["storage"][0]["actual_texture_components"]
            write_json(swir, value)
            entry = finalizer.evaluate_rk_acceptance(workspace, root)
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("SWIR:raw_storage_invalid:0", entry.issues)

    def test_rk_acceptance_legacy_actual_float_requires_true(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            root = workspace / "acceptance"
            make_rk_acceptance_root(root)
            swir = root / "SWIR/acceptance_summary.json"
            value = json.loads(swir.read_text(encoding="utf-8"))
            storage = value["raw_radiance"]["storage"][0]
            for field in (
                "texture_floating_point", "actual_texture_component_type",
                "actual_texture_component_width", "actual_texture_components",
            ):
                del storage[field]
            storage["actual_float"] = "1"
            write_json(swir, value)
            self.assertEqual(finalizer.evaluate_rk_acceptance(workspace, root).status, "PASS")
            storage["actual_float"] = "0"
            write_json(swir, value)
            entry = finalizer.evaluate_rk_acceptance(workspace, root)
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("SWIR:raw_storage_invalid:0", entry.issues)

    def test_build_receipt_binds_manifest_archive_and_sidecar(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            root = workspace / "build"
            root.mkdir()
            elf = root / "HwaSim_IR.aarch64"
            manifest = root / "source_manifest.sha256"
            archive = root / "fixture.source.tgz"
            elf.write_bytes(b"elf")
            manifest.write_text("source manifest\n", encoding="utf-8")
            archive.write_bytes(b"archive")
            archive_hash = hashlib.sha256(archive.read_bytes()).hexdigest()
            archive.with_suffix(archive.suffix + ".sha256").write_text(
                f"{archive_hash}  {archive.name}\n", encoding="ascii")
            receipt = root / "build_receipt.json"
            write_json(receipt, {
                "schema": finalizer.RK_BUILD_RECEIPT_SCHEMA,
                "stage_id": "stage-1",
                "source_manifest_sha256": hashlib.sha256(manifest.read_bytes()).hexdigest(),
                "source_archive_sha256": archive_hash,
                "elf_sha256": hashlib.sha256(elf.read_bytes()).hexdigest(),
                "build_id": "build-1",
                "rkmpp": True,
                "zrdds": True,
            })
            self.assertEqual(finalizer.evaluate_build_receipt(workspace, receipt).status, "PASS")
            staging = workspace / "staging"
            args = finalizer.parse_args(["--workspace", str(workspace), "--rk-build-receipt", str(receipt)])
            copied = finalizer.copy_rk_receipt_context(workspace, staging, args)
            copied_paths = {row["path"] for row in copied}
            self.assertIn("artifacts/rk3588/receipts/build/fixture.source.tgz", copied_paths)
            self.assertIn("artifacts/rk3588/receipts/build/fixture.source.tgz.sha256", copied_paths)
            manifest.write_text("tampered\n", encoding="utf-8")
            entry = finalizer.evaluate_build_receipt(workspace, receipt)
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("source_manifest_hash_mismatch", entry.issues)

    def test_rk_artifact_tree_preserves_request_logs_six_ledgers_and_runtime_context(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            acceptance = workspace / "acceptance"
            make_rk_acceptance_root(acceptance)
            swir = acceptance / "SWIR"
            extra_files = {
                "artifact_manifest.sha256": "fixture\n",
                "controller_clock_begin.txt": "utc=fixture\n",
                "controller_clock_end.txt": "utc=fixture\n",
                "board/runtime_environment.txt": "DISPLAY=:0\n",
                "board/deployment_version.env": "P11_STAGE_ID=stage-1\n",
                "board/board_clock_begin.txt": "epoch=1\n",
                "board/board_clock_end.txt": "exitStatus=0\n",
                "board/hwa_exit.txt": "0\n",
                "board/board_runner_exit.txt": "0\n",
            }
            for relative, content in extra_files.items():
                path = swir / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content, encoding="utf-8")
            staging = workspace / "staging"
            copied = finalizer.copy_artifact_tree(
                workspace, staging, acceptance, Path("artifacts/rk3588/dds_dual_band"))
            paths = {row["path"] for row in copied}
            expected = {
                "artifacts/rk3588/dds_dual_band/SWIR/case_request.json",
                "artifacts/rk3588/dds_dual_band/SWIR/stim.err.log",
                "artifacts/rk3588/dds_dual_band/SWIR/receiver.err.log",
                "artifacts/rk3588/dds_dual_band/SWIR/board/hwa.log",
                "artifacts/rk3588/dds_dual_band/SWIR/board/performance.csv",
                "artifacts/rk3588/dds_dual_band/SWIR/artifact_manifest.sha256",
                "artifacts/rk3588/dds_dual_band/SWIR/board/runtime_environment.txt",
                "artifacts/rk3588/dds_dual_band/SWIR/controller_clock_begin.txt",
                "artifacts/rk3588/dds_dual_band/SWIR/board/hwa_exit.txt",
                "artifacts/rk3588/dds_dual_band/SWIR/board/input_audit/input_accepted_fixture.csv",
                "artifacts/rk3588/dds_dual_band/SWIR/board/input_audit/input_execute_fixture.csv",
                "artifacts/rk3588/dds_dual_band/SWIR/board/input_audit/stage_render_fixture.csv",
                "artifacts/rk3588/dds_dual_band/SWIR/board/input_audit/stage_output_fixture.csv",
                "artifacts/rk3588/dds_dual_band/SWIR/windows_audit/input_sender_fixture.csv",
                "artifacts/rk3588/dds_dual_band/SWIR/windows_audit/input_received_fixture.csv",
            }
            self.assertTrue(expected.issubset(paths), sorted(expected - paths))

    def test_rollback_v2_positive_render_send_decode_contract_passes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            receipt = make_rollback_receipt(workspace)
            entry = finalizer.evaluate_rollback_receipt(workspace, receipt)
            self.assertEqual(entry.status, "PASS", entry.issues)
            self.assertEqual(entry.details["release_evidence"]["rollback"]["protocol_band"], 1)
            self.assertEqual(entry.details["release_evidence"]["p11_restored"]["protocol_band"], 0)
            self.assertEqual(entry.details["release_evidence"]["rollback"]["sent_samples"], 3)
            self.assertEqual(entry.details["release_evidence"]["rollback"]["target_frames"], 3)
            rollback_renderer = (
                receipt.parent / "P11_rollback_fixture/rollback_startup.log")
            self.assertIn(
                "reason=process_stop active=0 targetFrames=0",
                rollback_renderer.read_text(encoding="utf-8"),
            )
            staging = workspace / "staging"
            args = finalizer.parse_args([
                "--workspace", str(workspace), "--rk-rollback-receipt", str(receipt)])
            copied = finalizer.copy_rk_receipt_context(workspace, staging, args)
            copied_paths = {row["path"] for row in copied}
            for expected in (
                "artifacts/rk3588/receipts/rollback/rollback_exercise.err.log",
                "artifacts/rk3588/receipts/rollback/rollback/receiver.err.log",
                "artifacts/rk3588/receipts/rollback/rollback/stim.err.log",
                "artifacts/rk3588/receipts/rollback/rollback/received_decode.png",
                "artifacts/rk3588/receipts/rollback/rollback_controller_tool.ps1",
                "artifacts/rk3588/receipts/rollback/rollback_remote_runner_tool.sh",
            ):
                self.assertIn(expected, copied_paths)

    def test_rollback_renderer_rejects_process_stop_as_round_authority(self) -> None:
        prefix = (
            "[RunPreflight] result=PASS\n"
            "[DeploymentVersion] result=PASS\n"
            "[GpuBackend] glVendor=ARM glRenderer=Mali-LODX hardwareGpu=1\n"
        )
        sender = (
            "[DdsVideoPerf] sentSamples=3 sentBytes=300 "
            "writeErrors=0 droppedSamples=0\n"
        )
        authority = {
            "target_frames": 3, "completed_frames": 3,
            "sent_samples": 3, "sent_bytes": 300,
            "write_errors": 0, "dropped_samples": 0,
        }

        issues: list[str] = []
        counts = finalizer._parse_rollback_renderer_log(
            prefix
            + "[OutputRoundDrain] reason=stop targetFrames=3 completedFrames=3\n"
            + sender
            + "[OutputRoundDrain] reason=process_stop active=0 targetFrames=0 "
              "completedFrames=3\n",
            "release:test",
            issues,
            authority,
        )
        self.assertEqual(issues, [])
        self.assertEqual(counts["target_frames"], 3)

        issues = []
        finalizer._parse_rollback_renderer_log(
            prefix
            + sender
            + "[OutputRoundDrain] reason=process_stop active=0 targetFrames=0 "
              "completedFrames=3\n",
            "release:test",
            issues,
            authority,
        )
        self.assertIn("release:test:stop_output_drain_counts_missing", issues)

    def test_rollback_rejects_assertion_stage6_error_and_zero_counts(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            receipt = make_rollback_receipt(
                workspace, fatal_renderer=True, positive_counts=False)
            entry = finalizer.evaluate_rollback_receipt(workspace, receipt)
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("release:rollback:renderer_fatal_pattern", entry.issues)
            self.assertIn("release:rollback:target_frames_not_positive", entry.issues)
            self.assertIn("release:rollback:dds_sender_not_positive", entry.issues)
            self.assertIn("release:rollback:decoded_frames_not_positive", entry.issues)
            self.assertTrue(any(
                issue.startswith("release:rollback:exercise_loop_counts_mismatch:")
                for issue in entry.issues))

    def test_rollback_historical_startup_only_v1_receipt_cannot_pass(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            receipt = workspace / "rollback_receipt.json"
            write_json(receipt, {
                "schema": "hwasimir.p11.rk3588.rollback-receipt.v1",
                "stage_id": "p11-20260916-201234",
                "run_id": "p11rb-20260916-202709",
                "rollback_started": True,
                "p11_restored_and_started": True,
                "rollback_snapshot_retained": True,
                "active_elf_sha256": "a" * 64,
            })
            entry = finalizer.evaluate_rollback_receipt(workspace, receipt)
            self.assertEqual(entry.status, "FAIL")
            self.assertIn(
                "schema:'hwasimir.p11.rk3588.rollback-receipt.v1'", entry.issues)
            self.assertIn("release_evidence_missing", entry.issues)
            self.assertTrue(any(issue.startswith("validation_contract:") for issue in entry.issues))

    def test_rollback_tool_hash_tamper_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            receipt = make_rollback_receipt(workspace)
            (receipt.parent / "rollback_controller_tool.ps1").write_text(
                "# changed after exercise\n", encoding="utf-8")
            entry = finalizer.evaluate_rollback_receipt(workspace, receipt)
            self.assertEqual(entry.status, "FAIL")
            self.assertIn("tool:controller:file_sha256_mismatch", entry.issues)

    def test_rollback_scripts_bind_true_p10_and_invoke_both_loops(self) -> None:
        tools = Path(__file__).resolve().parent
        controller = (tools / "p11_rk3588_rollback_exercise.ps1").read_text(
            encoding="utf-8-sig")
        remote = (tools / "p11_rk3588_rollback_exercise.sh").read_text(
            encoding="utf-8-sig")
        self.assertIn(".before_p11-20260916-053343", controller)
        self.assertIn("RollbackProtocolBand = 1", controller)
        self.assertIn("Start-Process -FilePath 'ssh.exe'", controller)
        self.assertIn("Wait-LocalToken", controller)
        self.assertIn(
            "Invoke-RollbackLoopPhase -Release 'rollback' -ProtocolBand $RollbackProtocolBand",
            controller,
        )
        self.assertIn(
            "Invoke-RollbackLoopPhase -Release 'p11_restored' -ProtocolBand $RestoredProtocolBand",
            controller,
        )
        self.assertIn("hwasimir.p11.rk3588.rollback-receipt.v2", controller)
        self.assertIn("backup_suffix=$rollback_suffix", remote)
        self.assertNotIn("backup_suffix=.before_$stage_id", remote)
        self.assertIn("Assertion failed:", remote)
        self.assertIn("[ \"$target_frames\" -gt 0 ]", remote)

    def test_receipt_schemas_and_cross_stage_identity_are_strict(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            deploy_path = workspace / "deployment_receipt.json"
            rollback_path = workspace / "rollback_receipt.json"
            write_json(deploy_path, {
                "schema": "wrong", "stage_id": "stage-1", "elf_sha256": "a",
                "build_id": "build-1", "config_manifest_sha256": "c",
                "hard_link_snapshot": True, "rollback_retained": True, "removed_file_count": 0,
            })
            write_json(rollback_path, {
                "schema": "wrong", "stage_id": "stage-1", "active_elf_sha256": "a",
                "rollback_started": True, "p11_restored_and_started": True,
                "rollback_snapshot_retained": True,
            })
            self.assertEqual(finalizer.evaluate_deploy_receipt(workspace, deploy_path).status, "FAIL")
            self.assertEqual(finalizer.evaluate_rollback_receipt(workspace, rollback_path).status, "FAIL")

        entries = [
            finalizer.Evidence("rk3588-build", "build", "PASS", None,
                               details={"elf_sha256": "a", "build_id": "1", "stage_id": "build-stage"}),
            finalizer.Evidence("rk3588-deployment", "deploy", "PASS", None,
                               details={"elf_sha256": "a", "build_id": "1", "stage_id": "deploy-stage", "config_manifest_sha256": "c"}),
            finalizer.Evidence("rk3588-dual-band", "acceptance", "PASS", None,
                               details={"elfSha256": "a", "deploymentStageId": "deploy-stage", "configManifestSha256": "c"}),
            finalizer.Evidence("rk3588-rollback", "rollback", "PASS", None,
                               details={"active_elf_sha256": "b", "stage_id": "deploy-stage"}),
        ]
        self.assertEqual(finalizer.cross_check_rk(entries), [
            "build_deploy_stage_mismatch", "rollback_returned_elf_hash_mismatch",
        ])

    def test_document_status_block_is_idempotent(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.md"
            path.write_text("# Report\n\nOriginal body.\n", encoding="utf-8")
            finalizer.update_doc(path, "# Status\n\n- FAIL")
            finalizer.update_doc(path, "# Status\n\n- PASS")
            text = path.read_text(encoding="utf-8")
            self.assertEqual(text.count(finalizer.DOC_BEGIN), 1)
            self.assertIn("- PASS", text)
            self.assertNotIn("- FAIL", text)
            self.assertTrue(text.startswith("# Report\n\n" + finalizer.DOC_BEGIN))

    def test_zip_is_verified_and_does_not_hide_files(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            staging = root / "delivery"
            staging.mkdir()
            (staging / "final_status.json").write_text('{"overallStatus":"FAIL"}\n', encoding="utf-8")
            zip_path = root / "delivery.zip"
            receipt = finalizer.build_zip(staging, zip_path)
            self.assertEqual(receipt["verification"], "PASS")
            with zipfile.ZipFile(zip_path) as archive:
                self.assertIn("HwaSimIR_P11_Delivery/final_status.json", archive.namelist())
                value = json.loads(archive.read("HwaSimIR_P11_Delivery/final_status.json"))
                self.assertEqual(value["overallStatus"], "FAIL")


if __name__ == "__main__":
    unittest.main(verbosity=2)
