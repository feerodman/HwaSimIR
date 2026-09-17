#!/usr/bin/env python3
"""Build the P11 evidence delivery from executed artifacts.

This tool is deliberately evidence-driven.  It never runs HwaSimIR, edits a
production configuration, or turns a failing/missing source result into PASS.
Dynamic evidence roots must be supplied explicitly so an older timestamped
run cannot silently become the final result.
"""

from __future__ import annotations

import argparse
import array
import csv
import hashlib
import html
import json
import math
import os
import re
import shutil
import struct
import sys
import tempfile
import zipfile
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Any, Iterable, Mapping, Sequence


ROOT = Path(__file__).resolve().parents[1]

PASS = "PASS"
FAIL = "FAIL"
PARTIAL = "PARTIAL"
NOT_RUN = "NOT_RUN"
WINDOWS_AERO_SCHEMA = "hwasimir_p11_windows_aero_matrix_check_1"
RK_ACCEPTANCE_OVERALL_SCHEMA = "hwasimir.p11.rk3588.acceptance-summary.v1"
RK_ACCEPTANCE_BAND_SCHEMA = "hwasimir.p11.rk3588.band-acceptance.v1"
RK_BUILD_RECEIPT_SCHEMA = "hwasimir.p11.rk3588.build-receipt.v1"
RK_DEPLOYMENT_RECEIPT_SCHEMA = "hwasimir.p11.rk3588.deployment-receipt.v1"
RK_ROLLBACK_RECEIPT_SCHEMA = "hwasimir.p11.rk3588.rollback-receipt.v2"
RK_ROLLBACK_LOOP_CONTRACT = "hwasimir.p11.rk3588.rollback-loop.v1"
RK_DDS_LIFECYCLE_OVERALL_SCHEMA = "hwasimir.p11.rk3588.dds-lifecycle-summary.v1"
RK_DDS_LIFECYCLE_CASE_SCHEMA = "hwasimir.p11.rk3588.dds-lifecycle-case.v1"
RK_DDS_IMAGE_MATRIX_SCHEMA = "hwasimir.p11.rk3588.dds-image-matrix-summary.v1"
RK_P10_ROLLBACK_SUFFIX = ".before_p11-20260916-053343"
RK_P10_ROLLBACK_ELF_SHA256 = "840b649b7ed21f3b4a8b16a83bba25aee17319efa2932f8ff6705d14d0fe00ba"
RK_P10_ROLLBACK_CONFIG_MANIFEST_SHA256 = "38f6a89d49826bc00d1555c391a27538933e1c15d5dc03656a117cd350b7e9ca"
RK_BAND_PROTOCOL = {"SWIR": 0, "MWIR": 2}
WINDOWS_BAND_CONTRACT = {
    "SWIR": {"protocolBand": 0, "rangeUm": (1.1, 2.5)},
    "MWIR": {"protocolBand": 2, "rangeUm": (3.0, 5.0)},
}
WINDOWS_CASE_MANIFEST_FILES = (
    "fixed_clean.png",
    "auto_clean.png",
    "annotated.png",
    "received.png",
    "raw_radiance.pfm",
    "raw_radiance.json",
    "physical_components.csv",
    "frame_identity.json",
    "case_request.json",
)
WINDOWS_CASE_PACKAGE_FILES = (
    "case.json",
    "fixed_clean.png",
    "auto_clean.png",
    "annotated.png",
    "received.png",
    "raw_radiance.pfm",
    "raw_radiance.json",
    "physical_components.csv",
    "frame_identity.json",
)
RK_DDS_CASE_PACKAGE_FILES = (
    "case.json",
    "fixed_clean.png",
    "auto_clean.png",
    "annotated.png",
    "received.png",
    "raw_radiance.pfm",
    "raw_radiance.json",
    "physical_components.json",
    "frame_identity.json",
)
GALLERY_IMAGE_FILES = {
    "fixed": "fixed_clean.png",
    "agc": "auto_clean.png",
    "annotated": "annotated.png",
    "received": "received.png",
}
GALLERY_ATTACHMENT_FILES = {
    "case": "case.json",
    "rawMeta": "raw_radiance.json",
    "rawPfm": "raw_radiance.pfm",
    "components": "physical_components.json",
    "identity": "frame_identity.json",
}
WINDOWS_FRAME_IDENTITY_SCHEMA = "hwasimir_p11_frame_identity_1"
WINDOWS_H264_DUAL_SCHEMA = "hwasimir_p11_windows_h264_dual_band_acceptance_1"
WINDOWS_H264_BAND_SCHEMA = "hwasimir_p11_windows_h264_acceptance_1"
WINDOWS_H264_PREFLIGHT_SCHEMA = "hwasimir_p11_windows_h264_binary_preflight_1"
WINDOWS_PROGRAM_ROLES = (
    "HwaSim_IR",
    "HwaSim_IR_VideoDisplay",
    "DataDrivenTestQT",
)
MODTRAN_BUNDLE_TOOLS = (
    # These are the non-p11 entry points used to reproduce the delivered SI
    # LUTs.  The latter three import modtran_convert_to_si directly, so this
    # list is also the complete local import closure for these entry points.
    "modtran_convert_to_si.py",
    "modtran_build_lut.py",
    "modtran_qc.py",
    "modtran_build_solar_heating_lut.py",
    "test_modtran_units.py",
)
WINDOWS_PREFLIGHT_ROLE_KEYS = {
    "HwaSim_IR": "hwaExe",
    "HwaSim_IR_VideoDisplay": "videoDisplayExe",
    "DataDrivenTestQT": "stimulusExe",
}
WINDOWS_CORE_COMPONENT_TAGS = (
    "M1 ResourceP0",
    "M1 ModtranSiSelfCheck",
    "L1 MaterialOptics",
    "L1 MaterialThermal",
    "M1 Compare",
    "Stage5 RadianceComponents",
    "P6LinearCapture",
    "DisplayFrameMapping",
    "Stage6 AGC",
)
RK_REQUIRED_GATES = (
    "case_directory",
    "band_protocol_contract",
    "file.accepted",
    "file.execute",
    "file.render",
    "file.output",
    "file.sender",
    "file.received",
    "accepted.header",
    "accepted.rows",
    "execute.header",
    "execute.rows",
    "render.header",
    "render.rows",
    "output.header",
    "output.rows",
    "sender.header",
    "sender.rows",
    "received.header",
    "received.rows",
    "accepted.numeric",
    "accepted.ordinal_contiguous",
    "accepted.source_sequence",
    "accepted.success",
    "accepted.timestamps",
    "accepted.digests",
    "execute.numeric",
    "execute.ordinal_contiguous",
    "execute.source_sequence",
    "execute.success",
    "execute.timestamps",
    "execute.digests",
    "sender.numeric",
    "sender.ordinal_contiguous",
    "sender.success",
    "sender.timestamps",
    "sender.digests",
    "received.numeric",
    "received.ordinal_contiguous",
    "received.source_sequence",
    "received.success",
    "received.timestamps",
    "received.digests",
    "render.sequence",
    "render.steady_monotonic",
    "output.sequence",
    "output.steady_monotonic",
    "minimum_frame_count",
    "maximum_frame_count",
    "accepted_execute_identity",
    "accepted_render_identity",
    "accepted_output_identity",
    "sender_accepted_digest_identity",
    "accepted_receiver_identity",
    "output_fps_about_60",
    "board_latency_domain_valid",
    "log.hwa",
    "log.receiver_out",
    "log.receiver_err",
    "log.stim_out",
    "log.stim_err",
    "formal_band_selection",
    "sensor_profile_file",
    "ordered_input_policy",
    "formal_raw_storage",
    "formal_raw_gles_compatibility",
    "formal_raw_attachment",
    "formal_dual_pass_route",
    "formal_m1_components",
    "formal_raw_cpu_binary16_quantization",
    "physical_capture_log",
    "requested_raw_frames",
    "real_receiver_decoded_png",
    "real_receiver_annexb",
    "mali_gpu",
    "mpp_h264_encode",
    "ffmpeg_h264_decode",
    "receiver_frame_dump",
    "decoded_frame_nonconstant",
    "round_conservation",
    "output_drain",
    "control_stop_result",
    "file.board_clock_end",
    "board_process_exit",
    "control_responses",
    "dds_publisher_counts",
    "dds_receiver_counts",
    "stimulus_counts",
    "stimulus_stop_and_ack_drain",
    "no_formal_chain_fatal",
    "no_receiver_errors",
    "no_stimulus_errors",
    "performance_sampler",
    "performance_input_overwritten",
    "performance_input_queue_overflow",
    "performance_source_seq_gap",
)

RK_DDS_LIFECYCLE_COMMON_GATES = (
    "board_log",
    "stimulus_logs",
    "receiver_logs",
    "runtime_status",
    "dds_domain_150_board",
    "dds_only_protocol_ingress",
    "no_udp_ingress",
    "dds_domain_150_stimulus",
    "dds_domain_150_receiver",
    "tcp_payload_all_disabled",
    "no_dds_producer_errors",
    "no_dds_stimulus_errors",
    "no_dds_decode_errors",
    "dds_error_counters_zero",
    "control_sequence_1_2_3",
    "identity_conservation_and_empty_input_queue",
    "output_drain",
    "stop_ack_and_drain",
)

RK_DDS_LIFECYCLE_CASE_SCENARIOS: Mapping[str, str] = {
    "pause_swir": "pause_resume",
    "pause_mwir": "pause_resume",
    "receiver_restart": "receiver_restart",
    "retained_reinit": "retained_reinit",
}
RK_DDS_LIFECYCLE_CASE_BANDS: Mapping[str, tuple[str, int]] = {
    "pause_swir": ("SWIR", 0),
    "pause_mwir": ("MWIR", 2),
    "receiver_restart": ("MWIR", 2),
    "retained_reinit": ("MULTI", -1),
}

RK_DDS_LIFECYCLE_REQUIRED_GATES: Mapping[str, tuple[str, ...]] = {
    "pause_swir": RK_DDS_LIFECYCLE_COMMON_GATES + (
        "pause_sent_frames_unchanged",
        "catch_up_burst_zero",
        "dds_output_emission_gap",
        "emission_gap",
        "no_gap_emission",
        "digest_identity_conservation",
    ),
    "pause_mwir": RK_DDS_LIFECYCLE_COMMON_GATES + (
        "pause_sent_frames_unchanged",
        "catch_up_burst_zero",
        "dds_output_emission_gap",
        "emission_gap",
        "no_gap_emission",
        "digest_identity_conservation",
    ),
    "receiver_restart": RK_DDS_LIFECYCLE_COMMON_GATES + (
        "receiver_pid_changed",
        "producer_session_unchanged",
        "decoded_before_restart",
        "decoded_after_restart",
        "recovered_sps_pps_idr",
        "recovered_segment_decodable",
    ),
    "retained_reinit": RK_DDS_LIFECYCLE_COMMON_GATES + (
        "each_reset_init_start_stop",
        "retained_renderer",
        "band_sequence_swir_mwir_swir",
        "profile_refresh",
        "lut_refresh",
        "no_stale_band_frame",
        "digest_identity_conservation",
        "product_identity_refreshed_without_session_change",
    ),
}

STATUS_POLICY = {
    PASS: "Executed evidence satisfies every bounded gate rechecked by this finalizer.",
    PARTIAL: "Some executed evidence is valid, but the complete acceptance gate is not satisfied.",
    FAIL: "Executed evidence failed a declared gate, or a PASS claim failed artifact/integrity checks.",
    NOT_RUN: "No complete execution evidence was supplied.",
    "BLOCKED_DATA": "Required auditable data do not exist; no interpolation or relabeling is accepted.",
    "BLOCKED_METADATA": "Required stable scene identity or placement metadata do not exist.",
    "BLOCKED_IMPLEMENTATION": "The production implementation required for the claim is absent.",
    "NOT_VERIFIED_CALIBRATION": "No traceable measurement comparison exists.",
    "UNSUPPORTED_REJECTED": "The unsupported mode is explicitly rejected rather than aliased.",
    "HISTORICAL_DIAGNOSTIC": "Preserved diagnostic evidence from a non-DDS transport; it is not an acceptance gate in DDS-only mode.",
}

RESULT_KEYS = ("result", "overallStatus", "status")
PASS_WORDS = {"PASS", "PASSED", "OK", "SUCCESS"}
FAIL_WORDS = {"FAIL", "FAILED", "ERROR"}
INCOMPLETE_WORDS = {"PARTIAL", "PENDING", "IN_PROGRESS", "NOT_RUN", "NOT RUN", "INCOMPLETE"}

# A preflight/GPU banner is not proof that the formal renderer survived its
# first frame.  This pattern is intentionally shared, semantically, with the
# board rollback runner.  In particular, Panda assertions and the fail-closed
# formal Stage6 attachment error invalidate otherwise plausible startup logs.
RK_ROLLBACK_FATAL_PATTERN = re.compile(
    r"\[RunPreflight\]\[FATAL\]"
    r"|\[StartupFatal\]"
    r"|Assertion failed:"
    r"|\[Stage6 RawAttachment\]\[ERROR\]"
    r"|\[Stage6 [^\]]*\]\[ERROR\]"
    r"|\[P6LinearCapture\]\[ERROR\]"
    r"|hardwareGpu=0"
    r"|llvmpipe"
    r"|GL_INVALID_OPERATION"
    r"|GL error 0x502"
    r"|Could not bind framebuffer"
    r"|raw_buffer_unavailable"
    r"|missing_float_buffer",
    re.IGNORECASE,
)


@dataclass
class Evidence:
    evidence_id: str
    title: str
    status: str
    source: str | None
    source_sha256: str | None = None
    summary: str = ""
    issues: list[str] = field(default_factory=list)
    details: dict[str, Any] = field(default_factory=dict)

    def as_dict(self) -> dict[str, Any]:
        return {
            "id": self.evidence_id,
            "title": self.title,
            "status": self.status,
            "source": self.source,
            "sourceSha256": self.source_sha256,
            "summary": self.summary,
            "issues": self.issues,
            "details": self.details,
        }


@dataclass
class GalleryItem:
    band: str
    category: str
    case_id: str
    case_status: str
    images: dict[str, Path]
    attachments: dict[str, Path]


_SHA256_CACHE: dict[tuple[str, int, int, int], str] = {}


def now_local() -> str:
    return datetime.now().astimezone().isoformat(timespec="seconds")


def sha256_file(path: Path) -> str:
    stat = path.stat()
    key = (os.path.normcase(str(path.resolve())), stat.st_size, stat.st_mtime_ns, stat.st_ctime_ns)
    cached = _SHA256_CACHE.get(key)
    if cached is not None:
        return cached
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    value = digest.hexdigest()
    _SHA256_CACHE[key] = value
    return value


def workspace_path(workspace: Path, value: str | Path | None) -> Path | None:
    if value is None:
        return None
    path = Path(value)
    if not path.is_absolute():
        path = workspace / path
    return path.resolve()


def relative_path(workspace: Path, path: Path | None) -> str | None:
    if path is None:
        return None
    try:
        return path.resolve().relative_to(workspace.resolve()).as_posix()
    except ValueError:
        return str(path.resolve())


def inside_workspace(workspace: Path, path: Path) -> bool:
    try:
        path.resolve().relative_to(workspace.resolve())
        return True
    except ValueError:
        return False


def read_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def normalize_status(value: Any) -> str:
    if isinstance(value, bool):
        return PASS if value else FAIL
    if value is None:
        return NOT_RUN
    text = str(value).strip().upper().replace("-", "_")
    if text in PASS_WORDS:
        return PASS
    if text in FAIL_WORDS:
        return FAIL
    if text in INCOMPLETE_WORDS:
        return PARTIAL if text not in {"NOT_RUN", "NOT RUN"} else NOT_RUN
    if text.startswith("BLOCKED"):
        return text
    if text in {"NOT_VERIFIED_CALIBRATION", "UNSUPPORTED_REJECTED"}:
        return text
    return PARTIAL


def as_number(value: Any) -> float | None:
    if isinstance(value, bool):
        return None
    if isinstance(value, (int, float)):
        return float(value)
    if isinstance(value, str):
        try:
            return float(value.strip())
        except ValueError:
            return None
    return None


def object_result(value: Mapping[str, Any]) -> str:
    for key in RESULT_KEYS:
        if key in value:
            return normalize_status(value[key])
    if isinstance(value.get("pass"), bool):
        return PASS if value["pass"] else FAIL
    return NOT_RUN


def nested_failures(value: Mapping[str, Any]) -> list[str]:
    failures: list[str] = []
    for key in ("checks", "tests", "gates"):
        rows = value.get(key)
        if not isinstance(rows, list):
            continue
        for index, row in enumerate(rows):
            if not isinstance(row, Mapping):
                continue
            passed = row.get("pass")
            if passed is None:
                passed = row.get("passed")
            result = row.get("result")
            if passed is False or (result is not None and normalize_status(result) == FAIL):
                name = row.get("name") or row.get("case") or row.get("gate") or row.get("check") or str(index)
                failures.append(f"{key}:{name}")
    for key in ("failures", "errors"):
        rows = value.get(key)
        if isinstance(rows, list) and rows:
            failures.extend(f"{key}:{item}" for item in rows[:100])
    return failures


def evaluate_json(
    workspace: Path,
    evidence_id: str,
    title: str,
    path: Path | None,
    *,
    require_result: bool = True,
) -> Evidence:
    source = relative_path(workspace, path)
    if path is None or not path.is_file():
        return Evidence(evidence_id, title, NOT_RUN, source, summary="evidence file is missing", issues=["missing_evidence"])
    try:
        value = read_json(path)
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        return Evidence(
            evidence_id,
            title,
            FAIL,
            source,
            sha256_file(path),
            summary="evidence JSON is unreadable",
            issues=[f"invalid_json:{exc}"],
        )
    if not isinstance(value, Mapping):
        return Evidence(evidence_id, title, FAIL, source, sha256_file(path), issues=["root_not_object"])
    status = object_result(value)
    issues = nested_failures(value)
    if require_result and not any(key in value for key in RESULT_KEYS) and not isinstance(value.get("pass"), bool):
        issues.append("explicit_result_missing")
        status = FAIL
    if status == PASS and issues:
        status = FAIL
    count = None
    for key in ("checks", "tests", "gates"):
        if isinstance(value.get(key), list):
            count = len(value[key])
            break
    summary = f"sourceResult={object_result(value)}"
    if count is not None:
        summary += f"; checks={count}"
    return Evidence(
        evidence_id,
        title,
        status,
        source,
        sha256_file(path),
        summary=summary,
        issues=issues,
        details={"schema": value.get("schema"), "sourceResult": object_result(value), "checkCount": count},
    )


def evaluate_pass_csv(workspace: Path, evidence_id: str, title: str, path: Path | None) -> Evidence:
    source = relative_path(workspace, path)
    if path is None or not path.is_file():
        return Evidence(evidence_id, title, NOT_RUN, source, issues=["missing_evidence"])
    issues: list[str] = []
    rows: list[dict[str, str]] = []
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as stream:
            rows = list(csv.DictReader(stream))
    except (OSError, csv.Error) as exc:
        issues.append(f"csv_read_failed:{exc}")
    if not rows:
        issues.append("csv_has_no_data_rows")
    status_column = next((name for name in ("status", "result", "pass") if rows and name in rows[0]), None)
    if status_column is None:
        issues.append("csv_status_column_missing")
    else:
        for index, row in enumerate(rows, 2):
            if normalize_status(row.get(status_column)) != PASS:
                issues.append(f"row_{index}:{status_column}={row.get(status_column)}")
    return Evidence(
        evidence_id,
        title,
        PASS if not issues else FAIL,
        source,
        sha256_file(path),
        summary=f"rows={len(rows)}; failed={len(issues)}",
        issues=issues,
        details={"rows": len(rows), "statusColumn": status_column},
    )


def evaluate_publish_manifest(
    workspace: Path,
    evidence_id: str,
    title: str,
    path: Path,
    expected_formal: Path,
) -> Evidence:
    if not path.is_file():
        return Evidence(evidence_id, title, NOT_RUN, relative_path(workspace, path), issues=["publish_manifest_missing"])
    try:
        value = read_json(path)
    except (OSError, json.JSONDecodeError) as exc:
        return Evidence(evidence_id, title, FAIL, relative_path(workspace, path), issues=[f"publish_manifest_invalid:{exc}"])
    issues: list[str] = []
    publish_status = str(value.get("publish_status", ""))
    if not publish_status.startswith("SUCCESS_"):
        issues.append(f"publish_status:{publish_status or 'missing'}")
    if not expected_formal.is_file():
        issues.append("formal_file_missing")
    else:
        actual_hash = sha256_file(expected_formal)
        if actual_hash.lower() != str(value.get("after_sha256", "")).lower():
            issues.append("published_hash_no_longer_matches_formal_file")
        try:
            rows = csv_summary(expected_formal)["dataRows"]
        except (OSError, csv.Error) as exc:
            issues.append(f"formal_csv_invalid:{exc}")
            rows = None
        if rows != value.get("after_rows"):
            issues.append(f"published_row_count_mismatch:{rows}!={value.get('after_rows')}")
    return Evidence(
        evidence_id, title, PASS if not issues else FAIL,
        relative_path(workspace, path), sha256_file(path),
        summary=f"publishStatus={publish_status}; rows={value.get('after_rows')}; sha256={value.get('after_sha256')}",
        issues=issues,
        details={"publishStatus": publish_status, "rows": value.get("after_rows"), "formalSha256": value.get("after_sha256"), "formalPath": relative_path(workspace, expected_formal)},
    )


def newest(workspace: Path, pattern: str) -> Path | None:
    candidates = [path for path in workspace.glob(pattern) if path.is_file()]
    return max(candidates, key=lambda item: item.stat().st_mtime_ns) if candidates else None


def png_geometry(path: Path) -> tuple[int, int] | None:
    try:
        with path.open("rb") as stream:
            header = stream.read(24)
        if header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
            return None
        return struct.unpack(">II", header[16:24])
    except OSError:
        return None


def valid_sha256(value: Any) -> bool:
    return isinstance(value, str) and re.fullmatch(r"[0-9a-fA-F]{64}", value) is not None


def canonical_program_identities(
    workspace: Path,
    value: Any,
    *,
    verify_files: bool,
) -> tuple[list[dict[str, Any]], list[str]]:
    """Validate and canonicalize the three formal Windows executables.

    SHA-256 is the cross-evidence identity.  Path and byte length are retained
    and checked as independent guards against accidentally selecting an older
    acceptance root or a different executable copy.
    """
    if not isinstance(value, list):
        return [], ["formal_program_identities_missing"]
    issues: list[str] = []
    by_role: dict[str, dict[str, Any]] = {}
    for index, row in enumerate(value):
        if not isinstance(row, Mapping):
            issues.append(f"formal_program_identity_not_object:{index}")
            continue
        role = row.get("role")
        if role not in WINDOWS_PROGRAM_ROLES:
            issues.append(f"formal_program_identity_unexpected_role:{role}")
            continue
        role = str(role)
        if role in by_role:
            issues.append(f"formal_program_identity_duplicate_role:{role}")
            continue
        path_value = row.get("path")
        if not isinstance(path_value, str) or not path_value.strip():
            issues.append(f"formal_program_identity_path_missing:{role}")
            continue
        path = Path(path_value)
        if not path.is_absolute():
            path = workspace / path
        path = path.resolve()
        byte_count = row.get("bytes")
        if not isinstance(byte_count, int) or isinstance(byte_count, bool) or byte_count <= 0:
            issues.append(f"formal_program_identity_bytes_invalid:{role}")
        digest = row.get("sha256")
        if not valid_sha256(digest):
            issues.append(f"formal_program_identity_sha256_invalid:{role}")
        if verify_files:
            if not path.is_file():
                issues.append(f"formal_program_identity_file_missing:{role}")
            else:
                if isinstance(byte_count, int) and not isinstance(byte_count, bool) and path.stat().st_size != byte_count:
                    issues.append(f"formal_program_identity_file_size_mismatch:{role}")
                if valid_sha256(digest) and sha256_file(path).lower() != str(digest).lower():
                    issues.append(f"formal_program_identity_file_sha256_mismatch:{role}")
        by_role[role] = {
            "role": role,
            "path": str(path),
            "bytes": byte_count,
            "sha256": str(digest).lower() if valid_sha256(digest) else digest,
        }
    for role in WINDOWS_PROGRAM_ROLES:
        if role not in by_role:
            issues.append(f"formal_program_identity_role_missing:{role}")
    return [by_role[role] for role in WINDOWS_PROGRAM_ROLES if role in by_role], issues


def program_identity_signature(rows: Sequence[Mapping[str, Any]]) -> tuple[tuple[Any, ...], ...]:
    return tuple(
        (
            str(row.get("role")),
            os.path.normcase(str(row.get("path", ""))),
            row.get("bytes"),
            str(row.get("sha256", "")).lower(),
        )
        for row in rows
    )


def evidence_path(case_dir: Path, value: Any) -> Path | None:
    if not isinstance(value, str) or not value.strip():
        return None
    path = Path(value)
    if not path.is_absolute():
        path = case_dir / path
    return path.resolve()


def same_path(left: Path, right: Path) -> bool:
    return os.path.normcase(str(left.resolve())) == os.path.normcase(str(right.resolve()))


def values_equal(left: Any, right: Any) -> bool:
    left_number = as_number(left)
    right_number = as_number(right)
    if left_number is not None and right_number is not None:
        return math.isclose(left_number, right_number, rel_tol=0.0, abs_tol=1e-9)
    return left == right


def inspect_pfm(path: Path) -> tuple[dict[str, Any] | None, list[str]]:
    """Parse every PFM sample; no metadata-only or sampled validation is accepted."""
    issues: list[str] = []
    try:
        with path.open("rb") as stream:
            def header_line() -> bytes:
                while True:
                    line = stream.readline()
                    if not line:
                        raise ValueError("unexpected_eof")
                    value = line.strip()
                    if value and not value.startswith(b"#"):
                        return value

            magic = header_line()
            if magic not in {b"PF", b"Pf"}:
                raise ValueError(f"magic={magic!r}")
            dimensions = header_line().split()
            if len(dimensions) != 2:
                raise ValueError("dimensions")
            width, height = (int(value) for value in dimensions)
            scale = float(header_line())
            if width <= 0 or height <= 0 or scale == 0.0 or not math.isfinite(scale):
                raise ValueError("header_values")
            channels = 3 if magic == b"PF" else 1
            expected_values = width * height * channels
            payload = stream.read()
        if len(payload) != expected_values * 4:
            issues.append(f"pfm_payload_size:{len(payload)}!={expected_values * 4}")
            return None, issues
        samples = array.array("f")
        samples.frombytes(payload)
        file_little_endian = scale < 0
        if file_little_endian != (sys.byteorder == "little"):
            samples.byteswap()
        finite_count = 0
        nonfinite_count = 0
        minimum = math.inf
        maximum = -math.inf
        total = 0.0
        for sample in samples:
            if not math.isfinite(sample):
                nonfinite_count += 1
                continue
            finite_count += 1
            minimum = min(minimum, sample)
            maximum = max(maximum, sample)
            total += sample
        if nonfinite_count:
            issues.append(f"pfm_nonfinite:{nonfinite_count}")
        if finite_count == 0:
            issues.append("pfm_no_finite_values")
        elif minimum == maximum:
            issues.append(f"pfm_constant:{minimum}")
        return {
            "width": width,
            "height": height,
            "channels": channels,
            "scale": scale,
            "finiteValues": finite_count,
            "nonfiniteValues": nonfinite_count,
            "minimum": minimum,
            "maximum": maximum,
            "mean": total / finite_count if finite_count else math.nan,
        }, issues
    except (OSError, ValueError, OverflowError) as exc:
        return None, [f"pfm_invalid:{exc}"]


def read_component_rows(path: Path) -> tuple[list[dict[str, Any]], list[str]]:
    issues: list[str] = []
    rows: list[dict[str, Any]] = []
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as stream:
            reader = csv.DictReader(stream)
            required = {"variant", "source_log", "line", "tag", "fields_json"}
            if reader.fieldnames is None or not required.issubset(reader.fieldnames):
                return [], ["physical_components_schema_unrecognized"]
            for index, row in enumerate(reader, 2):
                try:
                    fields = json.loads(row.get("fields_json", ""))
                except json.JSONDecodeError:
                    issues.append(f"physical_components_fields_json_invalid:{index}")
                    continue
                if not isinstance(fields, Mapping):
                    issues.append(f"physical_components_fields_not_object:{index}")
                    continue
                rows.append({**row, "fields": dict(fields)})
    except OSError as exc:
        return [], [f"physical_components_unreadable:{exc}"]
    if not rows:
        issues.append("physical_components_empty")
    return rows, issues


def validate_physical_components(case_dir: Path, band: str, factor: str) -> list[str]:
    rows, issues = read_component_rows(case_dir / "physical_components.csv")
    by_variant: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        by_variant[str(row.get("variant", ""))].append(row)
    factor_tags = {
        "active_illumination": ("L2 ActiveIlluminator",),
        "solar_azimuth_elevation": ("M1 SolarPosition", "L1 SolarHeatingLut"),
        "cloud": ("Stage7 Weather", "WeatherCloud"),
        "rain": ("Stage7 Weather", "Stage7 Precipitation"),
        "snow": ("Stage7 Weather", "Stage7 Precipitation"),
        "combination": ("L2 ActiveIlluminator", "Stage7 Precipitation", "Stage5 AeroThermal"),
        "target_speed": ("Stage5 AeroThermal",),
    }
    required_tags = WINDOWS_CORE_COMPONENT_TAGS + factor_tags.get(factor, ())
    for variant in ("fixed", "agc", "annotated"):
        variant_rows = by_variant.get(variant, [])
        tags = Counter(str(row.get("tag", "")) for row in variant_rows)
        for tag in required_tags:
            if tags[tag] == 0:
                issues.append(f"physical_component_missing:{variant}:{tag}")
        m1_rows = [row["fields"] for row in variant_rows if row.get("tag") == "M1 Compare"]
        if not any(row.get("band") == band and row.get("finalOutput") == "M1" and as_number(row.get("valid")) == 1 for row in m1_rows):
            issues.append(f"physical_component_m1_invalid:{variant}")
        stage5_rows = [row["fields"] for row in variant_rows if row.get("tag") == "Stage5 RadianceComponents"]
        if not any(
            row.get("band") == band
            and row.get("finalOutput") == "M1"
            and as_number(row.get("formalRuntimeAffectsImage")) == 1
            and row.get("radianceUnit") in {"W/(m^2_sr_um)", "W/(m^2 sr um)"}
            for row in stage5_rows
        ):
            issues.append(f"physical_component_stage5_invalid:{variant}")
        capture_rows = [row["fields"] for row in variant_rows if row.get("tag") == "P6LinearCapture"]
        if not any(
            row.get("stage") == "pre_display"
            and row.get("domain") == "spectral_radiance"
            and as_number(row.get("physicalRadiance")) == 1
            for row in capture_rows
        ):
            issues.append(f"physical_component_capture_invalid:{variant}")
    return issues


def fixed_mapping_signature(case_dir: Path, band: str) -> tuple[tuple[tuple[str, Any], ...] | None, list[str]]:
    rows, issues = read_component_rows(case_dir / "physical_components.csv")
    fixed_rows = [row for row in rows if row.get("variant") == "fixed"]
    display = next((row["fields"] for row in fixed_rows if row.get("tag") == "M1 RadianceDisplay"), None)
    mapping_rows = [row["fields"] for row in fixed_rows if row.get("tag") == "DisplayFrameMapping"]
    if not isinstance(display, Mapping):
        issues.append("fixed_mapping_missing:M1 RadianceDisplay")
    if not mapping_rows:
        issues.append("fixed_mapping_missing:DisplayFrameMapping")
    if issues:
        return None, issues
    assert display is not None
    mapping = mapping_rows[-1]
    fields = {
        "windowMin": display.get(f"{band}Min"),
        "windowMax": display.get(f"{band}Max"),
        "windowMode": display.get("mode"),
        "perTargetGain": display.get("perTargetGain"),
        "responseMode": display.get("responseMode"),
        "fixedGain": mapping.get("fixedGain"),
        "offsetGray": mapping.get("offsetGray"),
        "gamma": mapping.get("gamma"),
        "reinhard": mapping.get("reinhard"),
        "whiteHot": mapping.get("whiteHot"),
    }
    if any(value is None for value in fields.values()):
        return None, [f"fixed_mapping_field_missing:{name}" for name, value in fields.items() if value is None]
    return tuple(sorted(fields.items())), []


def validate_frame_identity(case_dir: Path, case: Mapping[str, Any]) -> list[str]:
    issues: list[str] = []
    try:
        identity = read_json(case_dir / "frame_identity.json")
    except (OSError, json.JSONDecodeError) as exc:
        return [f"frame_identity_invalid:{exc}"]
    if not isinstance(identity, Mapping) or identity.get("schema") != WINDOWS_FRAME_IDENTITY_SCHEMA:
        return [f"frame_identity_schema_unrecognized:{identity.get('schema') if isinstance(identity, Mapping) else None}"]
    evidence = identity.get("evidence")
    if not isinstance(evidence, list) or not evidence or not all(isinstance(row, Mapping) for row in evidence):
        return ["frame_identity_evidence_invalid"]
    for row in evidence:
        for key, value in row.items():
            key_lower = str(key).lower()
            if "latest" in key_lower and value not in (None, False, 0, "0", "false", "none", ""):
                issues.append(f"frame_identity_latest:{key}")
            if isinstance(value, str) and value.strip().lower() in {"latest", "latest_only", "latestonly"}:
                issues.append(f"frame_identity_latest_value:{key}")
            if ("drop" in key_lower or "overwrite" in key_lower) and as_number(value) not in (None, 0.0):
                issues.append(f"frame_identity_loss:{key}={value}")
    for variant in ("fixed", "agc", "annotated"):
        marker = f"variants\\{variant}\\"
        rows = [row for row in evidence if marker in str(row.get("sourceLog", "")).lower().replace("/", "\\")]
        tcp = sorted((row for row in rows if row.get("tag") == "TcpPerf"), key=lambda row: int(as_number(row.get("line")) or 0))
        video = [row for row in rows if row.get("tag") == "VideoPerf"]
        if not tcp:
            issues.append(f"frame_identity_tcp_missing:{variant}")
        if not video:
            issues.append(f"frame_identity_video_missing:{variant}")
        previous = 0
        for row in tcp:
            source_seq = int(as_number(row.get("sourceSeq")) or -1)
            output_ordinal = int(as_number(row.get("outputOrdinal")) or -2)
            if source_seq <= previous or source_seq != output_ordinal or as_number(row.get("overwritten")) != 0:
                issues.append(f"frame_identity_tcp_not_ordered:{variant}")
                break
            previous = source_seq
        for row in video:
            if (
                as_number(row.get("sourceSeqContinuous")) != 1
                or as_number(row.get("discontinuities")) != 0
                or as_number(row.get("h264DecodeErrors")) != 0
            ):
                issues.append(f"frame_identity_video_not_continuous:{variant}")
                break
    chain = case.get("formalChainGates")
    if not isinstance(chain, Mapping):
        issues.append("frame_identity_formal_chain_missing")
    else:
        for variant in ("fixed", "agc", "annotated"):
            gate = chain.get(variant)
            if not isinstance(gate, Mapping) or gate.get("policy") != "OrderedQueue":
                issues.append(f"frame_identity_policy_invalid:{variant}")
                continue
            counts = gate.get("counts")
            if not isinstance(counts, Mapping) or not counts:
                issues.append(f"frame_identity_counts_missing:{variant}")
            else:
                count_values = [int(value) for value in counts.values() if isinstance(value, int) and not isinstance(value, bool)]
                if len(count_values) != len(counts) or not count_values or min(count_values) <= 0 or len(set(count_values)) != 1:
                    issues.append(f"frame_identity_counts_not_conserved:{variant}")
            zero = gate.get("zeroCounters")
            if not isinstance(zero, Mapping) or not zero or any(as_number(value) != 0 for value in zero.values()):
                issues.append(f"frame_identity_zero_counters_invalid:{variant}")
            if not isinstance(gate.get("sourceSeqContinuousEvidenceRows"), int) or gate["sourceSeqContinuousEvidenceRows"] <= 0:
                issues.append(f"frame_identity_continuity_evidence_missing:{variant}")
    return issues


def validate_request_against_plan(
    workspace: Path,
    case_dir: Path,
    request: Mapping[str, Any],
    band: str,
    scenario: Mapping[str, Any],
    plan: Mapping[str, Any],
) -> list[str]:
    issues: list[str] = []
    contract = WINDOWS_BAND_CONTRACT.get(band)
    if contract is None:
        return [f"request_unknown_band:{band}"]
    if request.get("protocolBand") != contract["protocolBand"]:
        issues.append(f"request_protocol_band_mismatch:{request.get('protocolBand')}!={contract['protocolBand']}")
    actual_range = request.get("rangeUm")
    expected_range = contract["rangeUm"]
    if not isinstance(actual_range, list) or len(actual_range) != 2 or not all(values_equal(a, b) for a, b in zip(actual_range, expected_range)):
        issues.append(f"request_range_um_mismatch:{actual_range}!={list(expected_range)}")
    for key in ("id", "factor", "comparisonGroup", "factorDelta"):
        request_key = "caseId" if key == "id" else key
        if key in scenario and request.get(request_key) != scenario.get(key):
            issues.append(f"request_plan_mismatch:{request_key}")
    request_scenario = request.get("scenario")
    if not isinstance(request_scenario, Mapping):
        issues.append("request_scenario_missing")
    else:
        # The runner may add normalized/default fields, but every field from the
        # current plan must survive unchanged in the executed request.
        for key, expected_value in scenario.items():
            if key not in request_scenario or not values_equal(request_scenario.get(key), expected_value):
                issues.append(f"request_scenario_plan_mismatch:{key}")
    request_seconds = request.get("seconds")
    if "seconds" in scenario:
        expected_seconds = scenario.get("seconds")
        if request_seconds != expected_seconds:
            issues.append(f"request_seconds_mismatch:{request_seconds}!={expected_seconds}")
    elif (
        not isinstance(request_seconds, int)
        or isinstance(request_seconds, bool)
        or request_seconds < 3
        or request_seconds > 3600
    ):
        issues.append(f"request_seconds_invalid:{request_seconds}")
    expected_fixture = scenario.get("fixture", plan.get("baseFixture"))
    if isinstance(expected_fixture, str):
        expected_base = workspace_path(workspace, expected_fixture)
        actual_base = evidence_path(case_dir, request.get("baseFixture"))
        if expected_base is None or actual_base is None or not same_path(expected_base, actual_base):
            issues.append("request_base_fixture_plan_mismatch")
    for path_key, hash_key in (("fixture", "fixtureSha256"), ("baseFixture", "baseFixtureSha256"), ("profile", "profileSha256")):
        source = evidence_path(case_dir, request.get(path_key))
        expected_hash = request.get(hash_key)
        if source is None or not source.is_file():
            issues.append(f"request_file_missing:{path_key}")
        elif not valid_sha256(expected_hash):
            issues.append(f"request_sha256_missing:{hash_key}")
        elif sha256_file(source).lower() != str(expected_hash).lower():
            issues.append(f"request_sha256_mismatch:{path_key}")
    _program_identities, program_identity_issues = canonical_program_identities(
        workspace,
        request.get("formalProgramIdentities"),
        verify_files=True,
    )
    issues.extend(program_identity_issues)
    try:
        request_file = read_json(case_dir / "case_request.json")
        if request_file != request:
            issues.append("case_request_not_equal_request")
    except (OSError, json.JSONDecodeError) as exc:
        issues.append(f"case_request_invalid:{exc}")
    return issues


def validate_case_artifacts(
    workspace: Path,
    case_dir: Path,
    case: Mapping[str, Any],
    expected_band: str,
    scenario: Mapping[str, Any],
    plan: Mapping[str, Any],
) -> list[str]:
    issues: list[str] = []
    expected_case_id = str(scenario.get("id"))
    request = case.get("request")
    if isinstance(request, Mapping):
        if request.get("band") != expected_band:
            issues.append(f"request_band_mismatch:{request.get('band')}!={expected_band}")
        if request.get("caseId") != expected_case_id:
            issues.append(f"request_case_id_mismatch:{request.get('caseId')}!={expected_case_id}")
        if request.get("resolution") != "800x800":
            issues.append(f"request_resolution_invalid:{request.get('resolution')}")
        issues.extend(validate_request_against_plan(workspace, case_dir, request, expected_band, scenario, plan))
    else:
        issues.append("request_missing")
    if case.get("transport") != "TCP":
        issues.append(f"transport_invalid:{case.get('transport')}")
    for name in ("fixed_clean.png", "auto_clean.png", "annotated.png", "received.png"):
        path = case_dir / name
        geometry = png_geometry(path) if path.is_file() else None
        if geometry is None:
            issues.append(f"missing_or_invalid_png:{name}")
        elif geometry != (800, 800):
            issues.append(f"png_not_800x800:{name}:{geometry[0]}x{geometry[1]}")
    for name in ("raw_radiance.pfm", "raw_radiance.json", "physical_components.csv", "frame_identity.json"):
        path = case_dir / name
        if not path.is_file() or path.stat().st_size == 0:
            issues.append(f"missing_or_empty:{name}")
    raw = case.get("rawRadiance")
    local_pfm = case_dir / "raw_radiance.pfm"
    pfm, pfm_issues = inspect_pfm(local_pfm)
    issues.extend(pfm_issues)
    if not isinstance(raw, Mapping):
        issues.append("rawRadiance_missing")
    else:
        required_raw = (
            "width", "height", "channels", "scale", "finiteValues", "nonfiniteValues",
            "minimum", "maximum", "mean", "path", "sha256", "source", "producerLogLine",
            "stage", "domain", "unit", "bandQuantity",
        )
        for key in required_raw:
            if key not in raw or raw.get(key) is None or raw.get(key) == "":
                issues.append(f"raw_field_missing:{key}")
        if pfm is not None:
            for key in ("width", "height", "channels", "scale", "finiteValues", "nonfiniteValues", "minimum", "maximum", "mean"):
                if not values_equal(raw.get(key), pfm.get(key)):
                    issues.append(f"raw_pfm_metadata_mismatch:{key}")
        if (raw.get("width"), raw.get("height"), raw.get("channels")) != (800, 800, 3):
            issues.append(f"raw_geometry_not_800x800x3:{raw.get('width')}x{raw.get('height')}x{raw.get('channels')}")
        if raw.get("nonfiniteValues") != 0 or as_number(raw.get("maximum")) is None or values_equal(raw.get("minimum"), raw.get("maximum")):
            issues.append("raw_not_finite_nonconstant")
        if raw.get("unit") not in {"W/(m^2 sr um)", "W/(m^2_sr_um)"}:
            issues.append(f"raw_unit_invalid:{raw.get('unit')}")
        if raw.get("stage") != "pre_display" or raw.get("domain") != "spectral_radiance":
            issues.append(f"raw_stage_domain_invalid:{raw.get('stage')}:{raw.get('domain')}")
        if not isinstance(raw.get("bandQuantity"), str) or not raw.get("bandQuantity", "").strip():
            issues.append("raw_band_quantity_missing")
        expected = raw.get("sha256")
        if not valid_sha256(expected):
            issues.append("raw_radiance_sha256_missing")
        elif local_pfm.is_file() and sha256_file(local_pfm).lower() != str(expected).lower():
            issues.append("raw_radiance_sha256_mismatch")
        declared_path = evidence_path(case_dir, raw.get("path"))
        if declared_path is None or not same_path(declared_path, local_pfm):
            issues.append("raw_radiance_path_mismatch")
        source_path = evidence_path(case_dir, raw.get("source"))
        if source_path is None or not source_path.is_file():
            issues.append("raw_radiance_source_missing")
        try:
            raw_file = read_json(case_dir / "raw_radiance.json")
            if raw_file != raw:
                issues.append("raw_radiance_json_mismatch")
        except (OSError, json.JSONDecodeError) as exc:
            issues.append(f"raw_radiance_json_invalid:{exc}")
    decoded = case.get("decodedEvidence")
    if not isinstance(decoded, Mapping):
        issues.append("decodedEvidence_missing")
    else:
        canonical = {"fixed": "fixed_clean.png", "agc": "auto_clean.png", "annotated": "annotated.png", "received": "received.png"}
        identities: dict[str, tuple[Any, Any, Any]] = {}
        for key, filename in canonical.items():
            row = decoded.get(key)
            if not isinstance(row, Mapping):
                issues.append(f"decodedEvidence_missing:{key}")
                continue
            for field_name in ("source", "sourceSha256", "sourceSeq", "frameSeq", "storageIndex", "mp4PtsUs", "output", "outputSha256", "width", "height", "meaning"):
                if field_name not in row or row.get(field_name) is None or row.get(field_name) == "":
                    issues.append(f"decoded_field_missing:{key}:{field_name}")
            if (row.get("width"), row.get("height")) != (800, 800):
                issues.append(f"decoded_geometry_invalid:{key}")
            identities[key] = (row.get("sourceSeq"), row.get("frameSeq"), row.get("storageIndex"))
            expected = row.get("outputSha256")
            local = case_dir / filename
            if not valid_sha256(expected):
                issues.append(f"decoded_output_sha256_missing:{key}")
            elif local.is_file() and sha256_file(local).lower() != str(expected).lower():
                issues.append(f"decoded_sha256_mismatch:{key}")
            output_path = evidence_path(case_dir, row.get("output"))
            if output_path is None or not same_path(output_path, local):
                issues.append(f"decoded_output_path_mismatch:{key}")
            source_path = evidence_path(case_dir, row.get("source"))
            source_hash = row.get("sourceSha256")
            if source_path is None or not source_path.is_file():
                issues.append(f"decoded_source_missing:{key}")
            elif not valid_sha256(source_hash):
                issues.append(f"decoded_source_sha256_missing:{key}")
            elif sha256_file(source_path).lower() != str(source_hash).lower():
                issues.append(f"decoded_source_sha256_mismatch:{key}")
        if len(identities) == 4 and len(set(identities.values())) != 1:
            issues.append("decoded_identity_mismatch")
        agc = decoded.get("agc")
        received = decoded.get("received")
        if isinstance(agc, Mapping) and isinstance(received, Mapping):
            agc_source = evidence_path(case_dir, agc.get("source"))
            received_source = evidence_path(case_dir, received.get("source"))
            if agc_source is None or received_source is None or not same_path(agc_source, received_source) or agc.get("sourceSha256") != received.get("sourceSha256"):
                issues.append("decoded_received_not_agc_source")
        selected = case.get("selectedFrame")
        if not isinstance(selected, Mapping) or not isinstance(selected.get("variants"), Mapping):
            issues.append("selected_frame_missing")
        else:
            variants = selected["variants"]
            requested_seq = selected.get("requestedSourceSeq")
            for key in ("fixed", "agc", "annotated"):
                row = variants.get(key)
                decoded_row = decoded.get(key)
                if not isinstance(row, Mapping) or not isinstance(decoded_row, Mapping):
                    issues.append(f"selected_frame_variant_missing:{key}")
                    continue
                for field_name in ("sourceSeq", "frameSeq", "storageIndex", "mp4PtsUs"):
                    if row.get(field_name) != decoded_row.get(field_name):
                        issues.append(f"selected_frame_mismatch:{key}:{field_name}")
                if row.get("sourceSeq") != requested_seq:
                    issues.append(f"selected_frame_requested_seq_mismatch:{key}")
    for group_name in ("formalChainGates", "scenarioGates"):
        group = case.get(group_name)
        if not isinstance(group, Mapping):
            issues.append(f"{group_name}_missing")
            continue
        for variant in ("fixed", "agc", "annotated"):
            row = group.get(variant)
            if not isinstance(row, Mapping):
                issues.append(f"{group_name}:{variant}:missing")
                continue
            if object_result(row) != PASS:
                issues.append(f"{group_name}:{variant}:result={object_result(row)}")
            errors = row.get("errors")
            if isinstance(errors, list) and errors:
                issues.append(f"{group_name}:{variant}:errors={len(errors)}")
    files = case.get("files")
    if not isinstance(files, Mapping):
        issues.append("files_manifest_missing")
    else:
        for name in WINDOWS_CASE_MANIFEST_FILES:
            if name not in files:
                issues.append(f"files_manifest_required_missing:{name}")
        for name, record in files.items():
            if not isinstance(record, Mapping):
                issues.append(f"files_manifest_invalid:{name}")
                continue
            local = case_dir / str(name)
            if not local.is_file():
                issues.append(f"files_manifest_missing_file:{name}")
                continue
            if not isinstance(record.get("bytes"), int) or isinstance(record.get("bytes"), bool) or record["bytes"] <= 0:
                issues.append(f"files_manifest_bytes_missing:{name}")
            elif local.stat().st_size != record["bytes"]:
                issues.append(f"files_manifest_size_mismatch:{name}")
            expected_hash = record.get("sha256")
            if not valid_sha256(expected_hash):
                issues.append(f"files_manifest_sha256_missing:{name}")
            elif sha256_file(local).lower() != str(expected_hash).lower():
                issues.append(f"files_manifest_sha256_mismatch:{name}")
    issues.extend(validate_frame_identity(case_dir, case))
    factor = str(scenario.get("factor", ""))
    issues.extend(validate_physical_components(case_dir, expected_band, factor))
    return issues


REPRESENTATIVE_CASES: Sequence[tuple[str, Sequence[str]]] = (
    ("target", ("target_near_100m",)),
    ("controlled_sample", ("controlled_sample_solar_off", "controlled_sample_solar_on")),
    ("exhaust_plume", ("exhaust_plume_on", "plume_side_on")),
    ("cloud", ("cloud",)),
    ("rain", ("rain",)),
    ("snow", ("snow",)),
    ("visibility", ("visibility_6km", "humidity_85pct")),
    ("active_illumination", ("active_in_band_on",)),
    ("annotation", ("target_near_100m",)),
    ("solar", ("solar_az_180_el_45", "solar_noon")),
    ("target_altitude", ("target_altitude_high",)),
    ("target_speed", ("speed_dynamic_0_15_30mps", "speed_static_30mps")),
)


def evaluate_matrix(workspace: Path, plan_path: Path, matrix_root: Path | None) -> tuple[Evidence, list[dict[str, Any]], list[GalleryItem]]:
    source = relative_path(workspace, matrix_root)
    if matrix_root is None:
        return Evidence("windows-image-matrix", "Windows SWIR/MWIR image matrix", NOT_RUN, source, issues=["matrix_root_not_supplied"]), [], []
    if not matrix_root.is_dir():
        return Evidence("windows-image-matrix", "Windows SWIR/MWIR image matrix", NOT_RUN, source, issues=["matrix_root_missing"]), [], []
    try:
        plan = read_json(plan_path)
    except (OSError, json.JSONDecodeError) as exc:
        return Evidence("windows-image-matrix", "Windows SWIR/MWIR image matrix", FAIL, source, issues=[f"matrix_plan_invalid:{exc}"]), [], []
    bands = [row.get("name") for row in plan.get("bands", []) if isinstance(row, Mapping)]
    plan_issues: list[str] = []
    for row in plan.get("bands", []):
        if not isinstance(row, Mapping):
            plan_issues.append("plan_band_not_object")
            continue
        name = row.get("name")
        contract = WINDOWS_BAND_CONTRACT.get(str(name))
        if contract is None:
            plan_issues.append(f"plan_unknown_band:{name}")
            continue
        if "protocolValue" in row and row.get("protocolValue") != contract["protocolBand"]:
            plan_issues.append(f"plan_protocol_mismatch:{name}")
        if "rangeUm" in row:
            actual_range = row.get("rangeUm")
            if not isinstance(actual_range, list) or len(actual_range) != 2 or not all(values_equal(a, b) for a, b in zip(actual_range, contract["rangeUm"])):
                plan_issues.append(f"plan_range_mismatch:{name}")
    scenarios = [row for row in plan.get("scenarios", []) if isinstance(row, Mapping)]
    runnable = [row for row in scenarios if row.get("runnable") is True]
    blocked: list[dict[str, Any]] = []
    blocked_ids: set[str] = set()
    for row in plan.get("blockedCoverage", []):
        if isinstance(row, Mapping):
            value = dict(row)
            coverage_id = str(value.get("id", ""))
            if coverage_id and coverage_id not in blocked_ids:
                blocked.append(value)
                blocked_ids.add(coverage_id)
    for row in scenarios:
        if row.get("runnable") is not True:
            coverage_id = str(row.get("coverageId") or row.get("id") or "")
            if coverage_id not in blocked_ids:
                blocked.append({"id": coverage_id, "status": str(row.get("blockedReason", "BLOCKED")).split(":", 1)[0], "reason": row.get("blockedReason")})
                blocked_ids.add(coverage_id)

    cases: list[dict[str, Any]] = []
    by_key: dict[tuple[str, str], dict[str, Any]] = {}
    identity_sets: dict[tuple[tuple[Any, ...], ...], list[dict[str, Any]]] = {}
    observed_default_seconds: set[int] = set()
    for band in bands:
        for scenario in runnable:
            case_id = str(scenario.get("id"))
            case_dir = matrix_root / str(band) / case_id
            case_path = case_dir / "case.json"
            row: dict[str, Any] = {
                "band": band,
                "caseId": case_id,
                "factor": scenario.get("factor"),
                "comparisonGroup": scenario.get("comparisonGroup"),
                "factorDelta": scenario.get("factorDelta"),
                "source": relative_path(workspace, case_path),
                "status": NOT_RUN,
                "issues": [],
            }
            if not case_path.is_file():
                row["issues"].append("case_json_missing")
            else:
                try:
                    case = read_json(case_path)
                except (OSError, json.JSONDecodeError) as exc:
                    case = {}
                    row["issues"].append(f"case_json_invalid:{exc}")
                source_status = object_result(case) if isinstance(case, Mapping) else FAIL
                row["sourceResult"] = source_status
                row["status"] = source_status
                if isinstance(case, Mapping):
                    row["issues"].extend(nested_failures(case))
                    row["issues"].extend(validate_case_artifacts(workspace, case_dir, case, str(band), scenario, plan))
                    request = case.get("request")
                    if isinstance(request, Mapping):
                        request_seconds = request.get("seconds")
                        if (
                            "seconds" not in scenario
                            and isinstance(request_seconds, int)
                            and not isinstance(request_seconds, bool)
                            and 3 <= request_seconds <= 3600
                        ):
                            observed_default_seconds.add(request_seconds)
                        identities, identity_issues = canonical_program_identities(
                            workspace,
                            request.get("formalProgramIdentities"),
                            verify_files=False,
                        )
                        if not identity_issues:
                            identity_sets.setdefault(program_identity_signature(identities), identities)
                if source_status == PASS and row["issues"]:
                    row["status"] = FAIL
            row["sourceSha256"] = sha256_file(case_path) if case_path.is_file() else None
            cases.append(row)
            by_key[(str(band), case_id)] = row

    # Fixed display mapping is a comparison-group contract, not a per-case claim.
    grouped: dict[tuple[str, str], list[tuple[dict[str, Any], tuple[tuple[str, Any], ...] | None]]] = defaultdict(list)
    scenario_by_id = {str(row.get("id")): row for row in runnable}
    for row in cases:
        case_id = str(row["caseId"])
        scenario = scenario_by_id[case_id]
        signature, signature_issues = fixed_mapping_signature(matrix_root / str(row["band"]) / case_id, str(row["band"]))
        row["issues"].extend(signature_issues)
        if signature_issues and row["status"] == PASS:
            row["status"] = FAIL
        grouped[(str(row["band"]), str(scenario.get("comparisonGroup", "")))].append((row, signature))
    for (band, group), members in grouped.items():
        signatures = {signature for _row, signature in members if signature is not None}
        if len(signatures) > 1:
            for row, _signature in members:
                row["issues"].append(f"fixed_mapping_drift:{band}:{group}")
                row["status"] = FAIL

    if len(identity_sets) > 1:
        plan_issues.append("matrix_formal_program_identity_drift")
    formal_program_identities = next(iter(identity_sets.values())) if len(identity_sets) == 1 else []
    if len(observed_default_seconds) > 1:
        plan_issues.append(f"matrix_default_seconds_drift:{sorted(observed_default_seconds)}")

    passes = sum(row["status"] == PASS for row in cases)
    missing = sum("case_json_missing" in row["issues"] for row in cases)
    failures = len(cases) - passes - missing

    aborted_path = matrix_root / "ABORTED_RUN.json"
    aborted: Any = None
    if aborted_path.is_file():
        try:
            aborted = read_json(aborted_path)
        except (OSError, json.JSONDecodeError) as exc:
            aborted = {"invalid": str(exc)}
    if plan_issues:
        status = FAIL
    elif aborted is not None:
        status = FAIL
    elif failures:
        status = FAIL
    elif missing:
        status = PARTIAL if passes else NOT_RUN
    elif blocked:
        status = PARTIAL
    else:
        status = PASS
    summary = f"expectedCases={len(bands) * len(runnable)}; pass={passes}; fail={failures}; missing={missing}; blockedCoverage={len(blocked)}"
    issues: list[str] = list(plan_issues)
    if missing:
        issues.append(f"missing_cases:{missing}")
    if failures:
        issues.append(f"failed_cases:{failures}")
    if aborted is not None:
        issues.append("matrix_root_marked_aborted")

    gallery: list[GalleryItem] = []
    for band in bands:
        for category, candidates in REPRESENTATIVE_CASES:
            selected = next((candidate for candidate in candidates if (str(band), candidate) in by_key and (matrix_root / str(band) / candidate / "case.json").is_file()), None)
            if selected is None:
                continue
            case_dir = matrix_root / str(band) / selected
            images = {name: case_dir / filename for name, filename in {
                "fixed": "fixed_clean.png", "agc": "auto_clean.png", "annotated": "annotated.png", "received": "received.png"
            }.items() if (case_dir / filename).is_file()}
            attachments = {name: case_dir / filename for name, filename in {
                "case": "case.json", "rawMeta": "raw_radiance.json", "rawPfm": "raw_radiance.pfm",
                "components": "physical_components.csv", "identity": "frame_identity.json",
            }.items() if (case_dir / filename).is_file()}
            gallery.append(GalleryItem(str(band), category, selected, by_key[(str(band), selected)]["status"], images, attachments))

    plan_sha256 = sha256_file(plan_path)
    matrix_digest_rows = [
        {
            "band": row["band"],
            "caseId": row["caseId"],
            "sourceSha256": row.get("sourceSha256"),
            "status": row["status"],
        }
        for row in cases
    ]
    matrix_evidence_sha256 = hashlib.sha256(json.dumps(
        {"planSha256": plan_sha256, "cases": matrix_digest_rows},
        ensure_ascii=False, sort_keys=True, separators=(",", ":"),
    ).encode("utf-8")).hexdigest()
    detail = {
        "plan": relative_path(workspace, plan_path),
        "planSha256": plan_sha256,
        "matrixEvidenceSha256": matrix_evidence_sha256,
        "formalProgramIdentities": formal_program_identities,
        "observedDefaultDurationSeconds": (
            next(iter(observed_default_seconds)) if len(observed_default_seconds) == 1 else None
        ),
        "observedDefaultDurationValuesSeconds": sorted(observed_default_seconds),
        "explicitScenarioDurationsSeconds": {
            str(row.get("id")): row.get("seconds")
            for row in runnable
            if "seconds" in row
        },
        "bands": bands,
        "scenarioCount": len(scenarios),
        "runnableScenarioCount": len(runnable),
        "expectedCaseCount": len(bands) * len(runnable),
        "plannedVariantRuns": len(bands) * len(scenarios) * len(plan.get("captureVariants", [])),
        "runnableVariantRuns": len(bands) * len(runnable) * len(plan.get("captureVariants", [])),
        "counts": {"pass": passes, "fail": failures, "missing": missing},
        "blockedCoverage": blocked,
        "abortedMarker": relative_path(workspace, aborted_path) if aborted_path.is_file() else None,
    }
    return Evidence(
        "windows-image-matrix", "Windows SWIR/MWIR image matrix", status, source,
        source_sha256=matrix_evidence_sha256, summary=summary, issues=issues, details=detail,
    ), cases, gallery


def evaluate_windows_aero_summary(workspace: Path, matrix_root: Path | None, summary_path: Path | None) -> Evidence:
    """Validate the read-only aero post-run gate and bind it to this matrix."""
    source = relative_path(workspace, summary_path)
    if matrix_root is None:
        return Evidence(
            "windows-aero-matrix", "Windows aerodynamic-heating matrix gate", NOT_RUN, source,
            summary="Windows matrix root was not supplied", issues=["matrix_root_not_supplied"],
        )
    expected_root = matrix_root.resolve()
    if summary_path is None:
        return Evidence(
            "windows-aero-matrix", "Windows aerodynamic-heating matrix gate", FAIL, source,
            summary="Aero summary is mandatory when --windows-matrix-root is supplied",
            issues=["windows_aero_summary_not_supplied"],
            details={"expectedMatrixRoot": str(expected_root)},
        )
    if not summary_path.is_file():
        return Evidence(
            "windows-aero-matrix", "Windows aerodynamic-heating matrix gate", FAIL, source,
            summary="Aero summary file is missing", issues=["windows_aero_summary_missing"],
            details={"expectedMatrixRoot": str(expected_root)},
        )
    digest = sha256_file(summary_path)
    try:
        value = read_json(summary_path)
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        return Evidence(
            "windows-aero-matrix", "Windows aerodynamic-heating matrix gate", FAIL, source, digest,
            summary="Aero summary JSON is unreadable", issues=[f"windows_aero_summary_invalid:{exc}"],
            details={"expectedMatrixRoot": str(expected_root)},
        )
    if not isinstance(value, Mapping):
        return Evidence(
            "windows-aero-matrix", "Windows aerodynamic-heating matrix gate", FAIL, source, digest,
            summary="Aero summary root is not an object", issues=["windows_aero_summary_root_not_object"],
            details={"expectedMatrixRoot": str(expected_root)},
        )

    issues: list[str] = []
    schema = value.get("schema")
    source_result = value.get("result")
    reported_root_value = value.get("matrixRoot")
    resolved_reported_root: Path | None = None
    if schema != WINDOWS_AERO_SCHEMA:
        issues.append(f"windows_aero_schema_mismatch:{schema!r}")
    if source_result != PASS:
        issues.append(f"windows_aero_result_not_pass:{source_result!r}")
    if not isinstance(reported_root_value, str) or not reported_root_value.strip():
        issues.append("windows_aero_matrix_root_missing")
    else:
        try:
            reported_root = Path(reported_root_value)
            if not reported_root.is_absolute():
                reported_root = workspace / reported_root
            resolved_reported_root = reported_root.resolve()
            try:
                same_root = expected_root.samefile(resolved_reported_root)
            except OSError:
                same_root = os.path.normcase(str(expected_root)) == os.path.normcase(str(resolved_reported_root))
            if not same_root:
                issues.append(f"windows_aero_matrix_root_mismatch:{resolved_reported_root}!={expected_root}")
        except (OSError, ValueError) as exc:
            issues.append(f"windows_aero_matrix_root_invalid:{exc}")

    return Evidence(
        "windows-aero-matrix",
        "Windows aerodynamic-heating matrix gate",
        PASS if not issues else FAIL,
        source,
        digest,
        summary=f"sourceResult={source_result}; matrixRoot={resolved_reported_root or reported_root_value}",
        issues=issues,
        details={
            "schema": schema,
            "sourceResult": source_result,
            "reportedMatrixRoot": reported_root_value,
            "resolvedMatrixRoot": str(resolved_reported_root) if resolved_reported_root is not None else None,
            "expectedMatrixRoot": str(expected_root),
            "summarySha256": digest,
        },
    )


def referenced_summary(root: Path, value: Any) -> Path | None:
    if not isinstance(value, str) or not value.strip():
        return None
    candidate = Path(value)
    if not candidate.is_absolute():
        candidate = root / candidate
    if candidate.is_file():
        return candidate.resolve()
    fallback = root / Path(value).name
    return fallback.resolve() if fallback.is_file() else None


def _validate_windows_acceptance_identities(
    workspace: Path,
    root: Path,
    overall: Mapping[str, Any],
    expected_program_identities: Any,
) -> tuple[list[str], list[dict[str, Any]], str | None]:
    issues: list[str] = []
    expected, expected_issues = canonical_program_identities(
        workspace, expected_program_identities, verify_files=True,
    )
    if expected_issues:
        issues.extend(f"matrix_{issue}" for issue in expected_issues)

    preflight = overall.get("preflight")
    if not isinstance(preflight, Mapping):
        return issues + ["binary_preflight_missing"], [], None
    if preflight.get("schema") != WINDOWS_H264_PREFLIGHT_SCHEMA:
        issues.append(f"binary_preflight_schema_mismatch:{preflight.get('schema')!r}")
    if object_result(preflight) != PASS:
        issues.append(f"binary_preflight_result:{object_result(preflight)}")
    preflight_errors = preflight.get("errors")
    if not isinstance(preflight_errors, list) or preflight_errors:
        issues.append("binary_preflight_errors_not_empty")

    preflight_path = root / "binary_preflight.json"
    preflight_sha256: str | None = None
    if not preflight_path.is_file():
        issues.append("binary_preflight_file_missing")
    else:
        preflight_sha256 = sha256_file(preflight_path)
        try:
            persisted = read_json(preflight_path)
            if persisted != preflight:
                issues.append("binary_preflight_file_mismatch")
        except (OSError, UnicodeError, json.JSONDecodeError) as exc:
            issues.append(f"binary_preflight_file_invalid:{exc}")

    actual_rows = []
    for role in WINDOWS_PROGRAM_ROLES:
        key = WINDOWS_PREFLIGHT_ROLE_KEYS[role]
        row = preflight.get(key)
        if not isinstance(row, Mapping):
            issues.append(f"binary_preflight_identity_missing:{role}")
            continue
        actual_rows.append({
            "role": role,
            "path": row.get("path"),
            "bytes": row.get("bytes"),
            "sha256": row.get("sha256"),
        })
    actual, actual_issues = canonical_program_identities(
        workspace, actual_rows, verify_files=True,
    )
    issues.extend(f"binary_preflight_{issue}" for issue in actual_issues)

    expected_by_role = {row["role"]: row for row in expected}
    actual_by_role = {row["role"]: row for row in actual}
    for role in WINDOWS_PROGRAM_ROLES:
        expected_row = expected_by_role.get(role)
        actual_row = actual_by_role.get(role)
        if expected_row is None or actual_row is None:
            continue
        for key in ("path", "bytes", "sha256"):
            left = expected_row.get(key)
            right = actual_row.get(key)
            if key == "path":
                equal = os.path.normcase(str(left)) == os.path.normcase(str(right))
            elif key == "sha256":
                equal = str(left).lower() == str(right).lower()
            else:
                equal = left == right
            if not equal:
                issues.append(f"formal_program_identity_mismatch:{role}:{key}")
    return issues, actual, preflight_sha256


def _evaluate_windows_acceptance(
    workspace: Path,
    *,
    evidence_id: str,
    title: str,
    summary_path: Path | None,
    required_bands: set[str],
    requested_band: str,
    minimum_seconds: float,
    expected_program_identities: Any,
    missing_issue: str,
) -> Evidence:
    source = relative_path(workspace, summary_path)
    if summary_path is None or not summary_path.is_file():
        return Evidence(evidence_id, title, NOT_RUN, source, issues=[missing_issue])
    root = summary_path.parent
    try:
        loaded_overall = read_json(summary_path)
        if not isinstance(loaded_overall, Mapping):
            raise ValueError("root_not_object")
        overall = loaded_overall
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as exc:
        return Evidence(evidence_id, title, FAIL, source, issues=[f"summary_invalid:{exc}"])

    issues: list[str] = []
    bands: dict[str, Any] = {}
    if overall.get("schema") != WINDOWS_H264_DUAL_SCHEMA:
        issues.append(f"overall_schema_mismatch:{overall.get('schema')!r}")
    if object_result(overall) != PASS:
        issues.append(f"overall_source_result:{object_result(overall)}")
    if overall.get("requestedBand") != requested_band:
        issues.append(f"requested_band_mismatch:{overall.get('requestedBand')!r}")
    seconds = overall.get("seconds")
    seconds_numeric = as_number(seconds)
    if seconds_numeric is None or seconds_numeric < minimum_seconds:
        issues.append(f"duration_below_{minimum_seconds:g}s:{seconds}")
    if overall.get("configurationRestored") is not True:
        issues.append("configuration_not_restored")
    before = overall.get("configHashesBefore")
    after = overall.get("configHashesAfter")
    if not isinstance(before, Mapping) or not isinstance(after, Mapping) or before != after:
        issues.append("configuration_hashes_not_restored")

    identity_issues, actual_identities, preflight_sha256 = _validate_windows_acceptance_identities(
        workspace, root, overall, expected_program_identities,
    )
    issues.extend(identity_issues)

    rows = overall.get("bands")
    if not isinstance(rows, list):
        issues.append("band_rows_missing")
        rows = []
    for row in rows:
        if not isinstance(row, Mapping):
            issues.append("band_row_not_object")
            continue
        band = str(row.get("band"))
        if band in bands:
            issues.append(f"duplicate_band:{band}")
            continue
        if object_result(row) != PASS or row.get("analyzerExitCode") != 0:
            issues.append(f"{band}:overall_band_row_failed")
        detail_path = referenced_summary(root / band, row.get("summary")) or (root / band / "p11_windows_h264_acceptance_summary.json")
        detail: Mapping[str, Any] = {}
        detail_sha256: str | None = None
        if not detail_path.is_file():
            issues.append(f"{band}:band_summary_missing")
        else:
            detail_sha256 = sha256_file(detail_path)
            try:
                loaded = read_json(detail_path)
                detail = loaded if isinstance(loaded, Mapping) else {}
            except (OSError, UnicodeError, json.JSONDecodeError) as exc:
                issues.append(f"{band}:band_summary_invalid:{exc}")
        if detail.get("schema") != WINDOWS_H264_BAND_SCHEMA:
            issues.append(f"{band}:band_schema_mismatch:{detail.get('schema')!r}")
        if detail.get("band") != band:
            issues.append(f"{band}:declared_band={detail.get('band')!r}")
        if object_result(detail) != PASS:
            issues.append(f"{band}:source_result={object_result(detail)}")
        expected = detail.get("expected", {}) if isinstance(detail.get("expected"), Mapping) else {}
        expected_seconds = as_number(expected.get("seconds"))
        if expected_seconds is None or expected_seconds < minimum_seconds:
            issues.append(f"{band}:expected_seconds_below_{minimum_seconds:g}")
        if seconds_numeric is not None and expected_seconds is not None and not values_equal(seconds_numeric, expected_seconds):
            issues.append(f"{band}:expected_seconds_mismatch")
        if expected.get("resolution") != "800x800":
            issues.append(f"{band}:resolution={expected.get('resolution')}")
        counts = detail.get("counts") if isinstance(detail.get("counts"), Mapping) else {}
        count_values = [value for value in counts.values() if isinstance(value, int) and not isinstance(value, bool)]
        if not count_values or len(set(count_values)) != 1 or detail.get("countConservation") is not True:
            issues.append(f"{band}:frame_count_not_conserved")
        independent_decode = detail.get("independentDecode")
        if not isinstance(independent_decode, Mapping) or independent_decode.get("pass") is not True:
            issues.append(f"{band}:independent_decode_failed")
        bands[band] = {
            "summary": relative_path(workspace, detail_path),
            "summarySha256": detail_sha256,
            "sourceResult": object_result(detail),
            "counts": counts,
            "rate": detail.get("rate"),
            "latency": detail.get("latency"),
            "controlAndDrain": detail.get("controlAndDrain"),
            "codec": detail.get("codec"),
        }
    if set(bands) != required_bands:
        issues.append(f"required_bands_mismatch:expected={sorted(required_bands)}:actual={sorted(bands)}")
    status = PASS if not issues else FAIL
    return Evidence(
        evidence_id,
        title,
        status,
        relative_path(workspace, summary_path),
        sha256_file(summary_path),
        summary=f"seconds={seconds}; bands={','.join(sorted(bands))}; sourceResult={object_result(overall)}",
        issues=issues,
        details={
            "seconds": seconds,
            "bands": bands,
            "configurationRestored": overall.get("configurationRestored"),
            "formalProgramIdentities": actual_identities,
            "binaryPreflightSha256": preflight_sha256,
        },
    )


def evaluate_windows_h264(
    workspace: Path,
    root: Path | None,
    expected_program_identities: Any = None,
) -> Evidence:
    summary_path = root / "p11_windows_h264_dual_band_summary.json" if root is not None else None
    return _evaluate_windows_acceptance(
        workspace,
        evidence_id="windows-h264-60s",
        title="Windows dual-band H.264 60 s",
        summary_path=summary_path,
        required_bands={"SWIR", "MWIR"},
        requested_band="Both",
        minimum_seconds=60.0,
        expected_program_identities=expected_program_identities,
        missing_issue="windows_h264_root_or_summary_not_supplied",
    )


def evaluate_nir_compatibility(
    workspace: Path,
    summary_path: Path | None,
    expected_program_identities: Any = None,
) -> Evidence:
    return _evaluate_windows_acceptance(
        workspace,
        evidence_id="nir-compatibility",
        title="NIR compatibility regression",
        summary_path=summary_path,
        required_bands={"NIR"},
        requested_band="NIR",
        minimum_seconds=3.0,
        expected_program_identities=expected_program_identities,
        missing_issue="nir_summary_not_supplied",
    )


def as_historical_diagnostic(entry: Evidence, reason: str) -> Evidence:
    """Keep superseded Windows transport evidence without making it a DDS gate."""
    source_status = entry.status
    entry.status = "HISTORICAL_DIAGNOSTIC"
    entry.details = {
        **entry.details,
        "classification": "HISTORICAL_DIAGNOSTIC",
        "blocking": False,
        "validatedSourceStatus": source_status,
        "reason": reason,
    }
    prefix = f"historicalSourceStatus={source_status}; DDS-only nonblocking"
    entry.summary = f"{prefix}; {entry.summary}" if entry.summary else prefix
    return entry


def _case_file(case_dir: Path, relative: str, issues: list[str], label: str) -> Path | None:
    path = (case_dir / relative).resolve()
    if not inside_workspace(case_dir, path) or not path.is_file():
        issues.append(f"dds_only_transport:{label}_missing")
        return None
    return path


def _read_case_logs(case_dir: Path, names: Sequence[str], issues: list[str], label: str) -> str:
    parts: list[str] = []
    for name in names:
        path = _case_file(case_dir, name, issues, f"{label}:{name}")
        if path is not None:
            parts.append(path.read_text(encoding="utf-8-sig", errors="replace"))
    return "\n".join(parts)


def validate_dds_only_transport(
    case_dir: Path,
    request_path_override: Path | None = None,
) -> tuple[list[str], dict[str, Any]]:
    """Bind an RK acceptance case to DDS ingress and DDS video egress only.

    The legacy TCP thread may still be initialized by the executable, so this
    gate does not reject diagnostic words such as ``TCP thread``.  It requires
    the four TCP payload switches to be zero and rejects actual UDP routing,
    transport fallback, or codec fallback.
    """
    issues: list[str] = []
    request_path = request_path_override.resolve() if request_path_override is not None else None
    if request_path is not None and not request_path.is_file():
        issues.append("dds_only_transport:request_missing")
        request_path = None
    if request_path is None and request_path_override is None:
        request_path = _case_file(case_dir, "case_request.json", issues, "request")
    request: Mapping[str, Any] = {}
    if request_path is not None:
        try:
            loaded = read_json(request_path)
            request = loaded if isinstance(loaded, Mapping) else {}
        except (OSError, json.JSONDecodeError) as exc:
            issues.append(f"dds_only_transport:request_invalid:{exc}")
    transport = request.get("transport") if isinstance(request.get("transport"), Mapping) else {}
    request_dds = (
        str(transport.get("kind", "")).upper() == "DDS" or
        (str(transport.get("control", "")).upper() == "DDS" and
         str(transport.get("video", "")).upper() == "DDS")
    )
    if not request_dds:
        issues.append(f"dds_only_transport:request_kind:{transport.get('kind')!r}")
    if transport.get("domain") != 150:
        issues.append(f"dds_only_transport:request_domain:{transport.get('domain')!r}")

    sender_text = _read_case_logs(case_dir, ("stim.out.log", "stim.err.log"), issues, "sender")
    receiver_text = _read_case_logs(case_dir, ("receiver.out.log", "receiver.err.log"), issues, "receiver")
    board_path = _case_file(case_dir, "board/hwa.log", issues, "board_log")
    board_text = (board_path.read_text(encoding="utf-8-sig", errors="replace")
                  if board_path is not None else "")

    if re.search(r"\[StimTransport\][^\r\n]*\bmode=dds\b[^\r\n]*\bdomain=150\b", sender_text, re.I) is None:
        issues.append("dds_only_transport:sender_dds_domain_missing")
    if re.search(r"\[StimFinal\][^\r\n]*\btransport=dds\b", sender_text, re.I) is None:
        issues.append("dds_only_transport:sender_dds_final_missing")
    if re.search(r"\[VideoInput\][^\r\n]*\bTransport=dds\b[^\r\n]*\bdomain=150\b", receiver_text, re.I) is None:
        issues.append("dds_only_transport:receiver_dds_input_missing")
    if re.search(r"\[DdsVideoReceiver\][^\r\n]*\bready=1\b[^\r\n]*\bdomain=150\b[^\r\n]*\bfullTransport=1\b", receiver_text, re.I) is None:
        issues.append("dds_only_transport:receiver_full_dds_missing")
    if re.search(r"\[RunPreflight\][^\r\n]*\bcommandTransport=dds\b", board_text, re.I) is None:
        issues.append("dds_only_transport:board_preflight_dds_missing")
    if re.search(r"\[CommandTransport\][^\r\n]*\binput=dds\b", board_text, re.I) is None:
        issues.append("dds_only_transport:board_command_dds_missing")

    dds_routes = re.findall(r"\[ProtocolRoute\][^\r\n]*\btransport=dds\b", board_text, re.I)
    udp_routes = re.findall(r"\[ProtocolRoute\][^\r\n]*\btransport=udp\b", board_text, re.I)
    if not dds_routes:
        issues.append("dds_only_transport:dds_protocol_route_missing")
    if udp_routes:
        issues.append(f"dds_only_transport:udp_protocol_route_count:{len(udp_routes)}")

    tcp_rows = re.findall(r"\[TcpPayloadConfig\][^\r\n]*", board_text)
    tcp_fields = ("SendVideo", "SendAnnotation", "SendRealtimeData", "ForwardInitControl")
    invalid_tcp_rows = 0
    for line in tcp_rows:
        if any(re.search(rf"\b{field}=0(?:\s|$)", line) is None for field in tcp_fields):
            invalid_tcp_rows += 1
    if not tcp_rows:
        issues.append("dds_only_transport:tcp_payload_config_missing")
    elif invalid_tcp_rows:
        issues.append(f"dds_only_transport:tcp_payload_nonzero_or_missing_rows:{invalid_tcp_rows}")

    combined = "\n".join((sender_text, receiver_text, board_text))
    if re.search(r"\[CodecFallback\]", combined, re.I):
        issues.append("dds_only_transport:codec_fallback_observed")
    if re.search(r"\bH264FallbackToJpeg=(?!0(?:\s|$))\S+", board_text, re.I):
        issues.append("dds_only_transport:h264_fallback_enabled")
    final_video_rows = re.findall(r"\[VideoPerf\][^\r\n]*\bactiveCodec=h264_annexb\b[^\r\n]*", receiver_text, re.I)
    if not final_video_rows:
        issues.append("dds_only_transport:receiver_h264_active_rows_missing")
    elif any(re.search(r"\bcodecFallbackReason=none(?:\s|$)", row, re.I) is None
             for row in final_video_rows):
        issues.append("dds_only_transport:receiver_codec_fallback_non_none")

    return issues, {
        "result": PASS if not issues else FAIL,
        "request": relative_path(case_dir, request_path),
        "requestTransport": dict(transport),
        "tcpPayloadRows": len(tcp_rows),
        "tcpPayloadInvalidRows": invalid_tcp_rows,
        "ddsProtocolRouteCount": len(dds_routes),
        "udpProtocolRouteCount": len(udp_routes),
        "receiverH264ActiveRows": len(final_video_rows),
    }


def _resolve_summary_reference(workspace: Path, parent: Path, value: Any) -> Path | None:
    if not isinstance(value, str) or not value.strip():
        return None
    path = Path(value)
    if not path.is_absolute():
        path = parent / path
    path = path.resolve()
    return path if inside_workspace(workspace, path) else None


def _validate_hashed_artifacts(
    workspace: Path,
    summary_path: Path,
    rows: Any,
    label: str,
) -> list[str]:
    issues: list[str] = []
    artifacts = [row for row in rows if isinstance(row, Mapping)] if isinstance(rows, list) else []
    if not artifacts:
        return [f"{label}:artifacts_missing"]
    seen: set[Path] = set()
    for index, row in enumerate(artifacts):
        path = _resolve_summary_reference(workspace, summary_path.parent, row.get("path"))
        if path is None or not path.is_file():
            issues.append(f"{label}:artifact_missing:{index}")
            continue
        if path in seen:
            issues.append(f"{label}:artifact_duplicate:{index}")
        seen.add(path)
        if row.get("bytes") != path.stat().st_size:
            issues.append(f"{label}:artifact_bytes_mismatch:{index}")
        digest = row.get("sha256")
        if not valid_sha256(digest) or str(digest).lower() != sha256_file(path).lower():
            issues.append(f"{label}:artifact_sha256_mismatch:{index}")
    return issues


def evaluate_rk_dds_lifecycle(workspace: Path, summary_path: Path | None) -> Evidence:
    source = relative_path(workspace, summary_path)
    if summary_path is None or not summary_path.is_file():
        return Evidence("rk3588-dds-lifecycle", "RK3588 DDS lifecycle", NOT_RUN, source,
                        issues=["rk_dds_lifecycle_summary_missing"])
    try:
        overall = read_json(summary_path)
    except (OSError, json.JSONDecodeError) as exc:
        return Evidence("rk3588-dds-lifecycle", "RK3588 DDS lifecycle", FAIL, source,
                        sha256_file(summary_path), issues=[f"lifecycle_overall_invalid:{exc}"])
    issues: list[str] = []
    if not isinstance(overall, Mapping):
        issues.append("lifecycle_overall_not_object")
        overall = {}
    if overall.get("schema") != RK_DDS_LIFECYCLE_OVERALL_SCHEMA:
        issues.append(f"lifecycle_overall_schema:{overall.get('schema')!r}")
    if object_result(overall) != PASS:
        issues.append(f"lifecycle_overall_result:{object_result(overall)}")
    overall_transport = (overall.get("transport")
                         if isinstance(overall.get("transport"), Mapping) else {})
    if (
        str(overall_transport.get("kind", "")).upper() != "DDS"
        or overall_transport.get("domain") != 150
        or overall_transport.get("udp_tested") is not False
        or overall_transport.get("tcp_tested") is not False
        or overall_transport.get("tcp_payload_required_disabled") is not True
    ):
        issues.append("lifecycle_overall_transport_not_dds_only")
    rows = [row for row in overall.get("cases", []) if isinstance(row, Mapping)] \
        if isinstance(overall.get("cases"), list) else []
    by_scenario: dict[str, list[Mapping[str, Any]]] = defaultdict(list)
    for row in rows:
        scenario = row.get("scenario") or row.get("name")
        if isinstance(scenario, str):
            by_scenario[scenario].append(row)
    if set(by_scenario) != set(RK_DDS_LIFECYCLE_REQUIRED_GATES):
        issues.append(f"lifecycle_scenarios:{sorted(by_scenario)}")

    case_details: dict[str, Any] = {}
    for scenario, required_gates in RK_DDS_LIFECYCLE_REQUIRED_GATES.items():
        matches = by_scenario.get(scenario, [])
        if len(matches) != 1:
            issues.append(f"{scenario}:overall_case_count:{len(matches)}")
            continue
        row = matches[0]
        lifecycle_scenario = RK_DDS_LIFECYCLE_CASE_SCENARIOS[scenario]
        expected_band, expected_protocol = RK_DDS_LIFECYCLE_CASE_BANDS[scenario]
        if row.get("lifecycle_scenario") != lifecycle_scenario:
            issues.append(
                f"{scenario}:lifecycle_scenario:{row.get('lifecycle_scenario')!r}")
        if row.get("band") != expected_band or row.get("protocol_band") != expected_protocol:
            issues.append(f"{scenario}:overall_band_protocol_mismatch")
        if object_result(row) != PASS:
            issues.append(f"{scenario}:overall_result:{object_result(row)}")
        case_path = _resolve_summary_reference(workspace, summary_path.parent, row.get("summary"))
        if case_path is None or not case_path.is_file():
            issues.append(f"{scenario}:summary_missing")
            continue
        expected_digest = row.get("summarySha256")
        actual_digest = sha256_file(case_path)
        if not valid_sha256(expected_digest) or str(expected_digest).lower() != actual_digest.lower():
            issues.append(f"{scenario}:summary_sha256_mismatch")
        try:
            case = read_json(case_path)
        except (OSError, json.JSONDecodeError) as exc:
            issues.append(f"{scenario}:summary_invalid:{exc}")
            continue
        if not isinstance(case, Mapping):
            issues.append(f"{scenario}:summary_not_object")
            continue
        if case.get("schema") != RK_DDS_LIFECYCLE_CASE_SCHEMA:
            issues.append(f"{scenario}:schema:{case.get('schema')!r}")
        if object_result(case) != PASS:
            issues.append(f"{scenario}:source_result:{object_result(case)}")
        if (case.get("scenario") or case.get("name")) != lifecycle_scenario:
            issues.append(f"{scenario}:scenario_identity_mismatch")
        if case.get("band") != expected_band or case.get("protocol_band") != expected_protocol:
            issues.append(f"{scenario}:case_band_protocol_mismatch")
        transport = case.get("transport") if isinstance(case.get("transport"), Mapping) else {}
        if str(transport.get("kind", "")).upper() != "DDS" or transport.get("domain") != 150:
            issues.append(f"{scenario}:transport_not_dds_domain150")
        tcp = transport.get("tcp_payload_counts") if isinstance(
            transport.get("tcp_payload_counts"), Mapping) else {}
        tcp_keys = ("SendVideo", "SendAnnotation", "SendRealtimeData", "ForwardInitControl")
        if set(tcp) != set(tcp_keys) or any(tcp.get(key) != 0 for key in tcp_keys):
            issues.append(f"{scenario}:tcp_payload_counts_not_four_zeros")
        if transport.get("udp_ingress_count") != 0:
            issues.append(f"{scenario}:udp_ingress_count:{transport.get('udp_ingress_count')!r}")

        gate_rows = [item for item in case.get("gates", []) if isinstance(item, Mapping)] \
            if isinstance(case.get("gates"), list) else []
        gates_by_name: dict[str, list[Mapping[str, Any]]] = defaultdict(list)
        for gate in gate_rows:
            name = gate.get("gate")
            if isinstance(name, str):
                gates_by_name[name].append(gate)
        if not gate_rows:
            issues.append(f"{scenario}:gates_missing")
        for name, found in gates_by_name.items():
            if len(found) != 1:
                issues.append(f"{scenario}:source_gate_count:{name}={len(found)}")
            if any(gate.get("pass") is not True for gate in found):
                issues.append(f"{scenario}:source_gate_not_true:{name}")
        for name in required_gates:
            found = gates_by_name.get(name, [])
            if len(found) != 1:
                issues.append(f"{scenario}:required_gate_count:{name}={len(found)}")
            elif found[0].get("pass") is not True:
                issues.append(f"{scenario}:required_gate_not_true:{name}")
        publisher_gates = gates_by_name.get("dds_domain_150_board", [])
        if len(publisher_gates) == 1:
            publisher_detail = str(publisher_gates[0].get("detail", ""))
            if re.search(
                    r"(?:^|\s)publisher_initialized=1(?:\s|$)",
                    publisher_detail) is None:
                issues.append(
                    f"{scenario}:dds_domain_150_board_missing_publisher_initialized_1")
        issues.extend(_validate_hashed_artifacts(
            workspace, case_path, case.get("artifacts"), scenario))
        case_details[scenario] = {
            "summary": relative_path(workspace, case_path),
            "summarySha256": actual_digest,
            "transport": dict(transport),
            "lifecycleScenario": lifecycle_scenario,
            "requiredGates": list(required_gates),
        }

    return Evidence(
        "rk3588-dds-lifecycle", "RK3588 DDS lifecycle",
        PASS if not issues else FAIL, source, sha256_file(summary_path),
        summary=f"scenarios={','.join(sorted(case_details))}; sourceResult={object_result(overall)}",
        issues=issues,
        details={
            "runId": overall.get("run_id") or overall.get("runId"),
            "deploymentStageId": overall.get("deployment_stage_id") or overall.get("deploymentStageId"),
            "elfSha256": overall.get("elf_sha256") or overall.get("elfSha256"),
            "configManifestSha256": overall.get("config_manifest_sha256") or overall.get("configManifestSha256"),
            "cases": case_details,
        },
    )


def validate_rk_band_contract(detail: Mapping[str, Any], band: str) -> list[str]:
    """Recheck the non-optional RK physical-chain contract from a band summary."""
    issues: list[str] = []
    expected_protocol = RK_BAND_PROTOCOL[band]
    if detail.get("schema") != RK_ACCEPTANCE_BAND_SCHEMA:
        issues.append(f"{band}:schema={detail.get('schema')!r}")
    if detail.get("band") != band:
        issues.append(f"{band}:declared_band={detail.get('band')!r}")
    if detail.get("protocol_band") != expected_protocol:
        issues.append(f"{band}:protocol_band={detail.get('protocol_band')!r}")

    rows = detail.get("gates")
    gate_rows = [row for row in rows if isinstance(row, Mapping)] if isinstance(rows, list) else []
    by_name: dict[str, list[Mapping[str, Any]]] = defaultdict(list)
    for row in gate_rows:
        name = row.get("gate")
        if isinstance(name, str) and name:
            by_name[name].append(row)
    if not gate_rows:
        issues.append(f"{band}:gates_missing")
    for name, matches in by_name.items():
        if len(matches) != 1:
            issues.append(f"{band}:source_gate_count:{name}={len(matches)}")
        if any(row.get("pass") is not True for row in matches):
            issues.append(f"{band}:source_gate_not_true:{name}")
    for name in RK_REQUIRED_GATES:
        matches = by_name.get(name, [])
        if len(matches) != 1:
            issues.append(f"{band}:required_gate_count:{name}={len(matches)}")
        elif matches[0].get("pass") is not True:
            issues.append(f"{band}:required_gate_not_true:{name}")

    raw = detail.get("raw_radiance")
    if not isinstance(raw, Mapping):
        issues.append(f"{band}:raw_radiance_missing")
        return issues
    if raw.get("unit") != "W/(m^2_sr_um)":
        issues.append(f"{band}:raw_unit={raw.get('unit')!r}")

    storage = raw.get("storage")
    storage_rows = [row for row in storage if isinstance(row, Mapping)] if isinstance(storage, list) else []
    if not storage_rows:
        issues.append(f"{band}:raw_storage_missing")
    for index, row in enumerate(storage_rows):
        rgb_bits = row.get("actual_rgb_bits")
        rgb_values = list(rgb_bits) if isinstance(rgb_bits, (list, tuple)) else []
        relative_bound = as_number(row.get("quantization_relative_error_bound"))
        maximum_finite = as_number(row.get("quantization_max_finite"))
        final_storage_keys = (
            "texture_floating_point",
            "actual_texture_component_type",
            "actual_texture_component_width",
            "actual_texture_components",
        )
        final_storage_schema_present = any(key in row for key in final_storage_keys)
        if final_storage_schema_present:
            texture_storage_valid = (
                str(row.get("texture_floating_point")) == "1"
                and row.get("actual_texture_component_type") == "half_float"
                and row.get("actual_texture_component_width") == 2
                and row.get("actual_texture_components") == 4
            )
        else:
            # Compatibility is limited to summaries emitted before the analyzer
            # exposed the exact texture component contract.  A partially present
            # final schema must never fall back to this coarse legacy flag.
            texture_storage_valid = str(row.get("actual_float")) == "1"
        if (
            row.get("requested") != "RGBA16F_SI"
            or str(row.get("formal_requested")) != "1"
            or row.get("platform_backend") != "linux_gles"
            or row.get("domain") != "W_per_m2_sr_um"
            or row.get("unit") != "W/(m^2_sr_um)"
            or row.get("quantization_model") != "IEEE754_binary16"
            or relative_bound is None
            or not abs(relative_bound - 2.0 ** -11) <= 1.0e-15
            or maximum_finite != 65504.0
            or not texture_storage_valid
            or len(rgb_values) != 3
            or any(not isinstance(value, int) or value < 16 for value in rgb_values)
            or not isinstance(row.get("actual_alpha_bits"), int)
            or row.get("actual_alpha_bits") < 16
        ):
            issues.append(f"{band}:raw_storage_invalid:{index}")

    quantization = raw.get("cpu_binary16_quantization_reference")
    if not isinstance(quantization, Mapping):
        issues.append(f"{band}:binary16_reference_missing")
    else:
        reference_count = quantization.get("cpu_reference_count")
        maximum_relative = as_number(quantization.get("maximum_relative_error"))
        if (
            quantization.get("model") != "IEEE754_binary16_round_to_nearest"
            or quantization.get("unit") != "W/(m^2_sr_um)"
            or not isinstance(reference_count, int)
            or reference_count <= 0
            or quantization.get("overflow_count") != 0
            or quantization.get("bound_failure_count") != 0
            or maximum_relative is None
            or maximum_relative > 0.02
        ):
            issues.append(f"{band}:binary16_reference_invalid")

    captures = raw.get("captures")
    capture_rows = [row for row in captures if isinstance(row, Mapping)] if isinstance(captures, list) else []
    if not capture_rows:
        issues.append(f"{band}:raw_captures_missing")
    seen_sequences: set[int] = set()
    for index, row in enumerate(capture_rows):
        sequence = row.get("sourceSeq")
        finite_count = row.get("finite_count")
        value_count = row.get("value_count")
        checked_count = row.get("half_lattice_checked_count")
        valid_sequence = isinstance(sequence, int) and sequence not in seen_sequences
        if isinstance(sequence, int):
            seen_sequences.add(sequence)
        if (
            not valid_sequence
            or not isinstance(finite_count, int)
            or finite_count <= 0
            or finite_count != value_count
            or checked_count != finite_count
            or row.get("half_lattice_mismatch_count") != 0
            or row.get("half_lattice_overflow_count") != 0
        ):
            issues.append(f"{band}:raw_capture_invalid:{index}")
        if isinstance(sequence, int):
            raw_gate = f"raw_pfm_{sequence}"
            matches = by_name.get(raw_gate, [])
            if len(matches) != 1:
                issues.append(f"{band}:required_gate_count:{raw_gate}={len(matches)}")
            elif matches[0].get("pass") is not True:
                issues.append(f"{band}:required_gate_not_true:{raw_gate}")
    return issues


def evaluate_rk_acceptance(workspace: Path, root: Path | None) -> Evidence:
    source = relative_path(workspace, root)
    if root is None:
        return Evidence("rk3588-dual-band", "RK3588 DDS/MPP dual-band 60 s", NOT_RUN, source, issues=["rk_acceptance_root_not_supplied"])
    overall_path = root / "acceptance_overall.json"
    if not overall_path.is_file():
        return Evidence("rk3588-dual-band", "RK3588 DDS/MPP dual-band 60 s", NOT_RUN, source, issues=["acceptance_overall_missing"])
    try:
        overall = read_json(overall_path)
    except (OSError, json.JSONDecodeError) as exc:
        return Evidence("rk3588-dual-band", "RK3588 DDS/MPP dual-band 60 s", FAIL, source, issues=[f"overall_invalid:{exc}"])
    issues: list[str] = []
    if overall.get("schema") != RK_ACCEPTANCE_OVERALL_SCHEMA:
        issues.append(f"overall_schema={overall.get('schema')!r}")
    if object_result(overall) != PASS:
        issues.append(f"overall_source_result:{object_result(overall)}")
    overall_cases = overall.get("cases")
    case_rows = [row for row in overall_cases if isinstance(row, Mapping)] if isinstance(overall_cases, list) else []
    overall_by_band: dict[str, list[Mapping[str, Any]]] = defaultdict(list)
    for row in case_rows:
        band_name = row.get("band")
        if isinstance(band_name, str):
            overall_by_band[band_name].append(row)
    if set(overall_by_band) != set(RK_BAND_PROTOCOL):
        issues.append(f"overall_required_bands={sorted(overall_by_band)}")
    bands: dict[str, Any] = {}
    for band in ("SWIR", "MWIR"):
        path = root / band / "acceptance_summary.json"
        detail: Mapping[str, Any] = {}
        if not path.is_file():
            issues.append(f"{band}:acceptance_summary_missing")
        else:
            try:
                loaded = read_json(path)
                detail = loaded if isinstance(loaded, Mapping) else {}
            except (OSError, json.JSONDecodeError) as exc:
                issues.append(f"{band}:acceptance_summary_invalid:{exc}")
        if object_result(detail) != PASS:
            issues.append(f"{band}:source_result={object_result(detail)}")
        issues.extend(validate_rk_band_contract(detail, band))
        transport_issues, dds_transport = validate_dds_only_transport(root / band)
        issues.extend(f"{band}:{issue}" for issue in transport_issues)
        overall_matches = overall_by_band.get(band, [])
        if len(overall_matches) != 1:
            issues.append(f"{band}:overall_case_count={len(overall_matches)}")
        else:
            overall_case = overall_matches[0]
            if overall_case.get("protocol_band") != RK_BAND_PROTOCOL[band]:
                issues.append(f"{band}:overall_protocol_band={overall_case.get('protocol_band')!r}")
            if object_result(overall_case) != object_result(detail):
                issues.append(
                    f"{band}:overall_case_result={object_result(overall_case)}"
                    f"!=band_result={object_result(detail)}"
                )
            if overall_case.get("errors") not in (None, []):
                issues.append(f"{band}:overall_case_errors_present")
        if detail.get("resolution") != "800x800":
            issues.append(f"{band}:resolution={detail.get('resolution')}")
        duration = as_number(detail.get("duration_sec"))
        if duration is None or duration < 60.0:
            issues.append(f"{band}:duration_below_60s:{detail.get('duration_sec')}")
        counts = detail.get("frame_counts") if isinstance(detail.get("frame_counts"), Mapping) else {}
        values = [counts.get(key) for key in ("sender", "accepted", "execute", "render", "output", "received")]
        if any(not isinstance(value, int) for value in values) or len(set(values)) != 1:
            issues.append(f"{band}:frame_count_not_conserved:{values}")
        failed_gates = [row.get("gate") for row in detail.get("gates", []) if isinstance(row, Mapping) and row.get("pass") is False]
        if failed_gates:
            issues.append(f"{band}:failed_gates:{','.join(map(str, failed_gates[:30]))}")
        bands[band] = {
            "summary": relative_path(workspace, path),
            "sourceResult": object_result(detail),
            "frameCounts": counts,
            "outputFps": detail.get("output_fps"),
            "latency": detail.get("latency"),
            "controlResponse": detail.get("control_response"),
            "rawRadiance": detail.get("raw_radiance"),
            "ddsOnlyTransport": dds_transport,
            "failedGates": failed_gates,
        }
    return Evidence(
        "rk3588-dual-band",
        "RK3588 DDS/MPP dual-band 60 s",
        PASS if not issues else FAIL,
        relative_path(workspace, overall_path),
        sha256_file(overall_path),
        summary=f"bands={','.join(bands)}; sourceResult={object_result(overall)}",
        issues=issues,
        details={
            "runId": overall.get("run_id"),
            "deploymentStageId": overall.get("deployment_stage_id"),
            "elfSha256": overall.get("elf_sha256"),
            "configManifestSha256": overall.get("config_manifest_sha256"),
            "bands": bands,
        },
    )


def _matrix_blockers(root: Path, summary: Mapping[str, Any], issues: list[str]) -> list[dict[str, Any]]:
    value = summary.get("blockedCoverage") or summary.get("blocked_coverage")
    if value is None:
        path = root / "blocked_coverage.json"
        if not path.is_file():
            issues.append("dds_image_matrix:blocked_coverage_missing")
            return []
        try:
            value = read_json(path)
        except (OSError, json.JSONDecodeError) as exc:
            issues.append(f"dds_image_matrix:blocked_coverage_invalid:{exc}")
            return []
        if isinstance(value, Mapping):
            value = (value.get("blockedCoverage") or value.get("cases") or
                     value.get("blockers") or value.get("entries"))
    rows = [dict(row) for row in value if isinstance(row, Mapping)] if isinstance(value, list) else []
    by_id: dict[str, dict[str, Any]] = {}
    for index, row in enumerate(rows):
        status = normalize_status(row.get("status") or row.get("result"))
        if not status.startswith("BLOCKED"):
            issues.append(f"dds_image_matrix:blocker_status:{index}:{status}")
        identity = row.get("id") or row.get("caseId") or row.get("coverageId")
        if not identity:
            issues.append(f"dds_image_matrix:blocker_identity_missing:{index}")
        elif str(identity) in by_id:
            issues.append(f"dds_image_matrix:blocker_identity_duplicate:{identity}")
        else:
            by_id[str(identity)] = row
        if not (row.get("reason") or row.get("blockedReason")):
            issues.append(f"dds_image_matrix:blocker_reason_missing:{index}")
    if set(by_id) != set(RK_DDS_IMAGE_REQUIRED_BLOCKERS):
        issues.append(f"dds_image_matrix:blocker_ids:{sorted(by_id)}")
    for identity, expected_status in RK_DDS_IMAGE_REQUIRED_BLOCKERS.items():
        row = by_id.get(identity)
        if row is None:
            continue
        actual_status = normalize_status(row.get("status") or row.get("result"))
        if actual_status != expected_status:
            issues.append(
                f"dds_image_matrix:blocker_status:{identity}:"
                f"{actual_status}!={expected_status}")
    return rows


RK_DDS_IMAGE_PROGRAM_ROLES = {
    "DDS H.264 receiver/FFmpeg decoder",
    "DDS control stimulus",
}
RK_DDS_IMAGE_REQUIRED_BLOCKERS = {
    "cloud_target_front_behind": "BLOCKED_METADATA",
    "cloud_individual_id_disable": "BLOCKED_METADATA",
    "target_range_2km_physical_imagery": "BLOCKED_DATA",
}


def _validate_dds_image_program_identities(
    workspace: Path,
    value: Any,
    label: str,
) -> tuple[tuple[tuple[str, str, int, str], ...], list[str]]:
    issues: list[str] = []
    rows = [row for row in value if isinstance(row, Mapping)] if isinstance(value, list) else []
    by_role: dict[str, tuple[str, str, int, str]] = {}
    for index, row in enumerate(rows):
        role = row.get("role")
        if role not in RK_DDS_IMAGE_PROGRAM_ROLES or str(role) in by_role:
            issues.append(f"{label}:program_identity_role:{index}:{role!r}")
            continue
        path = _resolve_summary_reference(workspace, workspace, row.get("path"))
        byte_count = row.get("bytes")
        digest = row.get("sha256")
        if path is None or not path.is_file():
            issues.append(f"{label}:program_identity_file_missing:{role}")
            continue
        if not isinstance(byte_count, int) or byte_count <= 0 or path.stat().st_size != byte_count:
            issues.append(f"{label}:program_identity_bytes:{role}")
        if not valid_sha256(digest) or str(digest).lower() != sha256_file(path).lower():
            issues.append(f"{label}:program_identity_sha256:{role}")
        by_role[str(role)] = (
            str(role), os.path.normcase(str(path)), int(byte_count or 0), str(digest).lower())
    if set(by_role) != RK_DDS_IMAGE_PROGRAM_ROLES:
        issues.append(f"{label}:program_identity_roles:{sorted(by_role)}")
    return tuple(by_role[role] for role in sorted(by_role)), issues


def evaluate_rk_dds_image_matrix(workspace: Path, root: Path | None) -> Evidence:
    source = relative_path(workspace, root)
    if root is None or not root.is_dir():
        return Evidence("rk3588-dds-image-matrix", "RK3588 DDS SWIR/MWIR image matrix",
                        NOT_RUN, source, issues=["rk_dds_image_root_missing"])
    summary_path = root / "dds_image_matrix_summary.json"
    if not summary_path.is_file():
        return Evidence("rk3588-dds-image-matrix", "RK3588 DDS SWIR/MWIR image matrix",
                        NOT_RUN, source, issues=["rk_dds_image_summary_missing"])
    try:
        summary = read_json(summary_path)
    except (OSError, json.JSONDecodeError) as exc:
        return Evidence("rk3588-dds-image-matrix", "RK3588 DDS SWIR/MWIR image matrix",
                        FAIL, source, sha256_file(summary_path),
                        issues=[f"rk_dds_image_summary_invalid:{exc}"])
    issues: list[str] = []
    if not isinstance(summary, Mapping):
        issues.append("dds_image_matrix:summary_not_object")
        summary = {}
    if summary.get("schema") != RK_DDS_IMAGE_MATRIX_SCHEMA:
        issues.append(f"dds_image_matrix:schema:{summary.get('schema')!r}")
    source_result = object_result(summary)
    if source_result not in {PASS, PARTIAL}:
        issues.append(f"dds_image_matrix:source_result:{source_result}")
    transport = summary.get("transport") if isinstance(summary.get("transport"), Mapping) else {}
    if (str(transport.get("kind", "")).upper() != "DDS" or
            transport.get("domain") != 150 or
            transport.get("codec") != "h264_annexb"):
        issues.append("dds_image_matrix:transport_not_dds_domain150_h264")
    for key in ("elf_sha256", "config_manifest_sha256"):
        camel = "elfSha256" if key == "elf_sha256" else "configManifestSha256"
        if not valid_sha256(summary.get(key) or summary.get(camel)):
            issues.append(f"dds_image_matrix:{key}_invalid")
    if not (summary.get("deployment_stage_id") or summary.get("deploymentStageId")):
        issues.append("dds_image_matrix:deployment_stage_id_missing")
    program_signature, program_issues = _validate_dds_image_program_identities(
        workspace, summary.get("program_identities"), "dds_image_matrix")
    issues.extend(program_issues)
    plan_path = root / "matrix_plan.json"
    declared_plan_sha256 = summary.get("plan_sha256") or summary.get("planSha256")
    if not plan_path.is_file() or plan_path.stat().st_size == 0:
        issues.append("dds_image_matrix:matrix_plan_missing")
    elif not valid_sha256(declared_plan_sha256):
        issues.append("dds_image_matrix:plan_sha256_invalid")
    elif str(declared_plan_sha256).lower() != sha256_file(plan_path).lower():
        issues.append("dds_image_matrix:plan_sha256_mismatch")

    blockers = _matrix_blockers(root, summary, issues)
    rows = [row for row in summary.get("cases", []) if isinstance(row, Mapping)] \
        if isinstance(summary.get("cases"), list) else []
    if not rows:
        issues.append("dds_image_matrix:cases_missing")
    seen: set[tuple[str, str]] = set()
    bands_seen: set[str] = set()
    cases: list[dict[str, Any]] = []
    for index, row in enumerate(rows):
        band = str(row.get("band", ""))
        case_id = str(row.get("caseId") or row.get("case_id") or "")
        key = (band, case_id)
        if band not in RK_BAND_PROTOCOL or not case_id or key in seen:
            issues.append(f"dds_image_matrix:case_identity:{index}:{key}")
            continue
        seen.add(key)
        bands_seen.add(band)
        row_result = object_result(row)
        if row_result.startswith("BLOCKED") or row_result == "BLOCKED":
            cases.append({
                "band": band,
                "caseId": case_id,
                "source": relative_path(
                    workspace,
                    _resolve_summary_reference(workspace, root, row.get("path"))),
                "sourceResult": row_result,
                "blocking": True,
            })
            continue
        if row_result != PASS:
            issues.append(f"dds_image_matrix:{band}/{case_id}:overall_result:{row_result}")
        raw_case_path = row.get("summary") or row.get("path") or row.get("source")
        case_path = (_resolve_summary_reference(workspace, root, raw_case_path)
                     if raw_case_path else (root / band / case_id / "case.json").resolve())
        if case_path is not None and case_path.is_dir():
            case_path = case_path / "case.json"
        if case_path is None or not case_path.is_file():
            issues.append(f"dds_image_matrix:{band}/{case_id}:case_json_missing")
            continue
        expected_digest = row.get("caseSha256") or row.get("sha256") or row.get("summarySha256")
        if expected_digest is not None and (
                not valid_sha256(expected_digest) or
                str(expected_digest).lower() != sha256_file(case_path).lower()):
            issues.append(f"dds_image_matrix:{band}/{case_id}:case_sha256_mismatch")
        try:
            case = read_json(case_path)
        except (OSError, json.JSONDecodeError) as exc:
            issues.append(f"dds_image_matrix:{band}/{case_id}:case_invalid:{exc}")
            continue
        if not isinstance(case, Mapping) or object_result(case) != PASS:
            issues.append(f"dds_image_matrix:{band}/{case_id}:case_result:{object_result(case) if isinstance(case, Mapping) else 'INVALID'}")
            continue
        if case.get("schema") != "hwasimir.p11.rk3588.dds-image-case.v1":
            issues.append(f"dds_image_matrix:{band}/{case_id}:case_schema:{case.get('schema')!r}")
        if (case.get("case_id") or case.get("caseId")) != case_id:
            issues.append(f"dds_image_matrix:{band}/{case_id}:case_id_mismatch")
        if case.get("band") != band or case.get("protocol_band") != RK_BAND_PROTOCOL[band]:
            issues.append(f"dds_image_matrix:{band}/{case_id}:band_protocol_mismatch")
        case_transport = case.get("transport") if isinstance(case.get("transport"), Mapping) else {}
        payload_paths = case_transport.get("payload_paths")
        case_payloads = set(str(value) for value in payload_paths) if isinstance(payload_paths, list) else set()
        if (
            case_payloads != {"DDS control", "DDS H.264 Annex-B video"}
            or case_transport.get("dds_domain") != 150
            or case_transport.get("udp_payload_tested") is not False
            or case_transport.get("tcp_payload_tested") is not False
            or case_transport.get("encoder") != "RK3588 MPP"
            or case_transport.get("decoder") != "Windows FFmpeg"
        ):
            issues.append(f"dds_image_matrix:{band}/{case_id}:case_transport_not_dds")
        case_dir = case_path.parent
        required = (
            "case_request.json", "input_fixture.json", "artifact_manifest.sha256",
            "physical_components.json", "frame_identity.csv", "raw_radiance_metrics.csv",
            "frame_identity.json", "raw_radiance.pfm", "raw_radiance.json",
            "fixed_clean.png", "auto_clean.png", "annotated.png",
        )
        for name in required:
            path = case_dir / name
            if not path.is_file() or path.stat().st_size == 0:
                issues.append(f"dds_image_matrix:{band}/{case_id}:artifact_missing:{name}")
            elif name.endswith(".png") and png_geometry(path) != (800, 800):
                issues.append(f"dds_image_matrix:{band}/{case_id}:png_geometry:{name}")
        request_path = case_dir / "case_request.json"
        try:
            request_value = read_json(request_path)
        except (OSError, json.JSONDecodeError) as exc:
            request_value = {}
            issues.append(f"dds_image_matrix:{band}/{case_id}:request_invalid:{exc}")
        request_signature, request_identity_issues = _validate_dds_image_program_identities(
            workspace,
            request_value.get("program_identities") if isinstance(request_value, Mapping) else None,
            f"dds_image_matrix:{band}/{case_id}:request",
        )
        issues.extend(request_identity_issues)
        if request_signature != program_signature:
            issues.append(f"dds_image_matrix:{band}/{case_id}:program_identity_mismatch")
        variant_rows = [item for item in case.get("variants", [])
                        if isinstance(item, Mapping)] \
            if isinstance(case.get("variants"), list) else []
        variants_by_name: dict[str, Mapping[str, Any]] = {}
        for variant_index, variant_row in enumerate(variant_rows):
            variant_name = variant_row.get("variant")
            if variant_name not in {"fixed", "agc", "annotated"}:
                issues.append(
                    f"dds_image_matrix:{band}/{case_id}:variant_identity:"
                    f"{variant_index}:{variant_name!r}")
                continue
            if str(variant_name) in variants_by_name:
                issues.append(
                    f"dds_image_matrix:{band}/{case_id}:variant_duplicate:"
                    f"{variant_name}")
                continue
            variants_by_name[str(variant_name)] = variant_row
        if set(variants_by_name) != {"fixed", "agc", "annotated"}:
            issues.append(
                f"dds_image_matrix:{band}/{case_id}:variant_set:"
                f"{sorted(variants_by_name)}")
        variants: dict[str, Any] = {}
        received_decode_paths: dict[str, Path] = {}
        for variant in ("fixed", "agc", "annotated"):
            acceptance_dir = case_dir / "variants" / variant / "acceptance" / band
            acceptance_summary = acceptance_dir / "acceptance_summary.json"
            received_decode = acceptance_dir / "received_decode.png"
            if not received_decode.is_file() or received_decode.stat().st_size == 0:
                issues.append(
                    f"dds_image_matrix:{band}/{case_id}:{variant}:received_decode_missing")
            elif png_geometry(received_decode) != (800, 800):
                issues.append(
                    f"dds_image_matrix:{band}/{case_id}:{variant}:received_decode_geometry")
            else:
                received_decode_paths[variant] = received_decode
            variant_record = variants_by_name.get(variant)
            canonical_png = case_dir / GALLERY_IMAGE_FILES[variant]
            if variant_record is not None:
                if (object_result(variant_record) != PASS or
                        variant_record.get("errors") != []):
                    issues.append(
                        f"dds_image_matrix:{band}/{case_id}:{variant}:"
                        "variant_record_not_pass")
                declared_canonical = _resolve_summary_reference(
                    workspace, case_dir, variant_record.get("canonical_png"))
                declared_received = _resolve_summary_reference(
                    workspace, case_dir, variant_record.get("received_decode_png"))
                if declared_canonical != canonical_png.resolve():
                    issues.append(
                        f"dds_image_matrix:{band}/{case_id}:{variant}:"
                        "canonical_png_path_mismatch")
                if declared_received != received_decode.resolve():
                    issues.append(
                        f"dds_image_matrix:{band}/{case_id}:{variant}:"
                        "received_decode_path_mismatch")
                canonical_digest = variant_record.get("canonical_png_sha256")
                received_digest = variant_record.get("received_decode_png_sha256")
                if (not valid_sha256(canonical_digest) or
                        not canonical_png.is_file() or
                        str(canonical_digest).lower() !=
                        sha256_file(canonical_png).lower()):
                    issues.append(
                        f"dds_image_matrix:{band}/{case_id}:{variant}:"
                        "canonical_png_sha256_mismatch")
                if (not valid_sha256(received_digest) or
                        not received_decode.is_file() or
                        str(received_digest).lower() !=
                        sha256_file(received_decode).lower()):
                    issues.append(
                        f"dds_image_matrix:{band}/{case_id}:{variant}:"
                        "received_decode_png_sha256_mismatch")
                if (valid_sha256(canonical_digest) and
                        valid_sha256(received_digest) and
                        str(canonical_digest).lower() !=
                        str(received_digest).lower()):
                    issues.append(
                        f"dds_image_matrix:{band}/{case_id}:{variant}:"
                        "canonical_received_digest_mismatch")
            detail: Mapping[str, Any] = {}
            if not acceptance_summary.is_file():
                issues.append(f"dds_image_matrix:{band}/{case_id}:{variant}:acceptance_summary_missing")
            else:
                try:
                    loaded = read_json(acceptance_summary)
                    detail = loaded if isinstance(loaded, Mapping) else {}
                except (OSError, json.JSONDecodeError) as exc:
                    issues.append(f"dds_image_matrix:{band}/{case_id}:{variant}:acceptance_summary_invalid:{exc}")
            if object_result(detail) != PASS:
                issues.append(f"dds_image_matrix:{band}/{case_id}:{variant}:acceptance_result:{object_result(detail)}")
            band_issues = validate_rk_band_contract(detail, band)
            issues.extend(
                f"dds_image_matrix:{band}/{case_id}:{variant}:{issue}"
                for issue in band_issues
            )
            transport_issues, transport_details = validate_dds_only_transport(
                acceptance_dir, request_path)
            issues.extend(
                f"dds_image_matrix:{band}/{case_id}:{variant}:{issue}"
                for issue in transport_issues
            )
            variants[variant] = {
                "summary": relative_path(workspace, acceptance_summary),
                "receivedDecodePng": relative_path(workspace, received_decode),
                "sourceResult": object_result(detail),
                "ddsOnlyTransport": transport_details,
            }
        # The normalized delivery gallery uses the AGC receiver decode for its
        # explicit "received" view.  The three canonical display views and all
        # three per-variant receiver decodes remain preserved in the full DDS
        # evidence tree copied into the delivery.
        received_for_gallery = received_decode_paths.get(
            "agc", case_dir / "variants" / "agc" / "acceptance" / band /
            "received_decode.png")
        delivery_artifacts = {
            "case.json": case_path,
            "fixed_clean.png": case_dir / "fixed_clean.png",
            "auto_clean.png": case_dir / "auto_clean.png",
            "annotated.png": case_dir / "annotated.png",
            "received.png": received_for_gallery,
            "raw_radiance.pfm": case_dir / "raw_radiance.pfm",
            "raw_radiance.json": case_dir / "raw_radiance.json",
            "physical_components.json": case_dir / "physical_components.json",
            "frame_identity.json": case_dir / "frame_identity.json",
        }
        cases.append({
            "band": band,
            "caseId": case_id,
            "source": relative_path(workspace, case_path),
            "sourceSha256": sha256_file(case_path),
            "sourceResult": row_result,
            "status": PASS,
            "factor": str(case.get("factor") or row.get("factor") or "unclassified"),
            "comparisonGroup": str(
                case.get("comparison_group") or case.get("comparisonGroup") or
                row.get("comparisonGroup") or row.get("comparison_group") or
                "ungrouped"),
            "factorDelta": str(
                case.get("factor_delta") or case.get("factorDelta") or
                row.get("factorDelta") or row.get("factor_delta") or ""),
            "issues": [],
            "deliveryArtifacts": {
                name: relative_path(workspace, path)
                for name, path in delivery_artifacts.items()
            },
            "variants": variants,
        })
    if bands_seen != set(RK_BAND_PROTOCOL):
        issues.append(f"dds_image_matrix:bands:{sorted(bands_seen)}")

    status = FAIL if issues else (PARTIAL if blockers else PASS)
    return Evidence(
        "rk3588-dds-image-matrix", "RK3588 DDS SWIR/MWIR image matrix",
        status, relative_path(workspace, summary_path), sha256_file(summary_path),
        summary=(f"cases={len(cases)}; blockers={len(blockers)}; "
                 f"sourceResult={source_result}; transport=DDS-only"),
        issues=issues,
        details={
            "runId": summary.get("run_id") or summary.get("runId"),
            "deploymentStageId": summary.get("deployment_stage_id") or summary.get("deploymentStageId"),
            "elfSha256": summary.get("elf_sha256") or summary.get("elfSha256"),
            "configManifestSha256": summary.get("config_manifest_sha256") or summary.get("configManifestSha256"),
            "transport": dict(transport),
            "blockers": blockers,
            "planSha256": declared_plan_sha256,
            "matrixEvidenceSha256": sha256_file(summary_path),
            "cases": cases,
        },
    )


def dds_image_delivery_views(
    workspace: Path,
    evidence: Evidence,
) -> tuple[list[dict[str, Any]], list[GalleryItem]]:
    """Return the formal DDS-only matrix rows and gallery source bindings.

    Windows matrix rows deliberately do not enter this function.  They remain
    evidence entries marked HISTORICAL_DIAGNOSTIC, while the normalized
    ``artifacts/matrix`` tree and every HTML image link are sourced only from
    the audited RK DDS image matrix.
    """
    if evidence.evidence_id != "rk3588-dds-image-matrix":
        return [], []
    raw_rows = evidence.details.get("cases")
    rows = [dict(row) for row in raw_rows if isinstance(row, Mapping)] \
        if isinstance(raw_rows, list) else []
    formal_rows: list[dict[str, Any]] = []
    gallery: list[GalleryItem] = []
    expected_artifacts = set(RK_DDS_CASE_PACKAGE_FILES)
    for row in rows:
        if row.get("blocking") is True:
            continue
        artifacts = row.get("deliveryArtifacts")
        if not isinstance(artifacts, Mapping) or set(artifacts) != expected_artifacts:
            continue
        resolved: dict[str, Path] = {}
        for name in RK_DDS_CASE_PACKAGE_FILES:
            source = workspace_path(workspace, artifacts.get(name))
            if source is not None:
                resolved[name] = source
        if set(resolved) != expected_artifacts:
            continue
        row["deliveryArtifacts"] = {
            name: relative_path(workspace, resolved[name])
            for name in RK_DDS_CASE_PACKAGE_FILES
        }
        issue_prefix = f"dds_image_matrix:{row.get('band')}/{row.get('caseId')}:"
        case_issues = [
            issue for issue in evidence.issues
            if str(issue).startswith(issue_prefix)
        ]
        source_status = normalize_status(row.get("sourceResult") or row.get("status"))
        row["status"] = FAIL if case_issues or source_status != PASS else PASS
        row.setdefault("factor", "unclassified")
        row.setdefault("comparisonGroup", "ungrouped")
        row.setdefault("factorDelta", "")
        row["issues"] = case_issues
        formal_rows.append(row)
        gallery.append(GalleryItem(
            str(row.get("band")),
            str(row.get("factor")),
            str(row.get("caseId")),
            str(row.get("status")),
            {
                variant: resolved[filename]
                for variant, filename in GALLERY_IMAGE_FILES.items()
            },
            {
                label: resolved[filename]
                for label, filename in GALLERY_ATTACHMENT_FILES.items()
            },
        ))
    return formal_rows, gallery


def evaluate_build_receipt(workspace: Path, path: Path | None) -> Evidence:
    if path is None or not path.is_file():
        return Evidence("rk3588-build", "RK3588 cross-build", NOT_RUN, relative_path(workspace, path), issues=["build_receipt_missing"])
    try:
        value = read_json(path)
    except (OSError, json.JSONDecodeError) as exc:
        return Evidence("rk3588-build", "RK3588 cross-build", FAIL, relative_path(workspace, path), issues=[f"receipt_invalid:{exc}"])
    issues = []
    if value.get("schema") != RK_BUILD_RECEIPT_SCHEMA:
        issues.append(f"schema:{value.get('schema')!r}")
    for key in ("stage_id", "source_manifest_sha256", "source_archive_sha256", "elf_sha256", "build_id"):
        if not value.get(key):
            issues.append(f"missing:{key}")
    if value.get("rkmpp") is not True:
        issues.append("rkmpp_not_linked")
    if value.get("zrdds") is not True:
        issues.append("zrdds_not_linked")
    elf = path.parent / "HwaSim_IR.aarch64"
    if not elf.is_file():
        issues.append("elf_missing")
    elif str(value.get("elf_sha256", "")).lower() != sha256_file(elf).lower():
        issues.append("elf_hash_mismatch")
    source_manifest = path.parent / "source_manifest.sha256"
    if not source_manifest.is_file():
        issues.append("source_manifest_missing")
    elif str(value.get("source_manifest_sha256", "")).lower() != sha256_file(source_manifest).lower():
        issues.append("source_manifest_hash_mismatch")
    source_archives = sorted(path.parent.glob("*.source.tgz"))
    if len(source_archives) != 1:
        issues.append(f"source_archive_count:{len(source_archives)}")
    else:
        source_archive = source_archives[0]
        archive_digest = sha256_file(source_archive)
        if str(value.get("source_archive_sha256", "")).lower() != archive_digest.lower():
            issues.append("source_archive_hash_mismatch")
        archive_sidecar = source_archive.with_suffix(source_archive.suffix + ".sha256")
        if not archive_sidecar.is_file():
            issues.append("source_archive_hash_sidecar_missing")
        else:
            sidecar_digest = archive_sidecar.read_text(encoding="ascii", errors="replace").split()
            if not sidecar_digest or sidecar_digest[0].lower() != archive_digest.lower():
                issues.append("source_archive_hash_sidecar_mismatch")
    return Evidence(
        "rk3588-build", "RK3588 cross-build", PASS if not issues else FAIL,
        relative_path(workspace, path), sha256_file(path),
        summary=f"stage={value.get('stage_id')}; elf={value.get('elf_sha256')}", issues=issues,
        details={key: value.get(key) for key in ("stage_id", "source_manifest_sha256", "source_archive_sha256", "elf_sha256", "build_id", "rkmpp", "zrdds")},
    )


def evaluate_deploy_receipt(workspace: Path, path: Path | None) -> Evidence:
    if path is None or not path.is_file():
        return Evidence("rk3588-deployment", "RK3588 atomic deployment", NOT_RUN, relative_path(workspace, path), issues=["deployment_receipt_missing"])
    try:
        value = read_json(path)
    except (OSError, json.JSONDecodeError) as exc:
        return Evidence("rk3588-deployment", "RK3588 atomic deployment", FAIL, relative_path(workspace, path), issues=[f"receipt_invalid:{exc}"])
    issues = []
    if value.get("schema") != RK_DEPLOYMENT_RECEIPT_SCHEMA:
        issues.append(f"schema:{value.get('schema')!r}")
    if value.get("hard_link_snapshot") is not True:
        issues.append("hard_link_snapshot_false")
    if value.get("rollback_retained") is not True:
        issues.append("rollback_not_retained")
    if value.get("removed_file_count") != 0:
        issues.append(f"removed_file_count:{value.get('removed_file_count')}")
    for key in ("stage_id", "elf_sha256", "build_id", "config_manifest_sha256"):
        if not value.get(key):
            issues.append(f"missing:{key}")
    return Evidence(
        "rk3588-deployment", "RK3588 atomic deployment", PASS if not issues else FAIL,
        relative_path(workspace, path), sha256_file(path),
        summary=f"stage={value.get('stage_id')}; changed={value.get('changed_file_count')}; removed={value.get('removed_file_count')}", issues=issues,
        details={key: value.get(key) for key in ("stage_id", "elf_sha256", "build_id", "config_manifest_sha256", "changed_file_count", "removed_file_count", "delta_payload_bytes", "hard_link_snapshot", "rollback_retained")},
    )


def _rollback_evidence_file(
    workspace: Path,
    receipt: Path,
    value: Mapping[str, Any],
    key: str,
    issue_prefix: str,
    issues: list[str],
) -> Path | None:
    raw = value.get(key)
    if not isinstance(raw, str) or not raw.strip():
        issues.append(f"{issue_prefix}:{key}_missing")
        return None
    candidate = Path(raw)
    if not candidate.is_absolute():
        candidate = receipt.parent / candidate
    candidate = candidate.resolve()
    if not inside_workspace(workspace, candidate):
        issues.append(f"{issue_prefix}:{key}_outside_workspace")
        return None
    if not candidate.is_file():
        issues.append(f"{issue_prefix}:{key}_file_missing")
        return None
    expected_hash = str(value.get(f"{key}_sha256", "")).lower()
    if not re.fullmatch(r"[0-9a-f]{64}", expected_hash):
        issues.append(f"{issue_prefix}:{key}_sha256_missing_or_invalid")
    elif sha256_file(candidate).lower() != expected_hash:
        issues.append(f"{issue_prefix}:{key}_sha256_mismatch")
    return candidate


def _numeric_log_fields(line: str, names: Sequence[str]) -> dict[str, int] | None:
    values: dict[str, int] = {}
    for name in names:
        match = re.search(rf"(?:^|\s){re.escape(name)}=(\d+)(?:\s|$)", line)
        if match is None:
            return None
        values[name] = int(match.group(1))
    return values


def _parse_rollback_renderer_log(
    text: str,
    label: str,
    issues: list[str],
    authoritative_loop: Mapping[str, int] | None = None,
) -> dict[str, int]:
    if RK_ROLLBACK_FATAL_PATTERN.search(text):
        issues.append(f"{label}:renderer_fatal_pattern")
    for required, issue in (
        (r"\[RunPreflight\] result=PASS", "run_preflight_pass_missing"),
        (r"\[DeploymentVersion\] result=PASS", "deployment_version_pass_missing"),
        (r"\[GpuBackend\].*glVendor=ARM.*glRenderer=Mali-LODX.*hardwareGpu=1", "hardware_gpu_pass_missing"),
    ):
        if re.search(required, text) is None:
            issues.append(f"{label}:{issue}")

    stop_drain_rows = [
        parsed for line in text.splitlines()
        if "[OutputRoundDrain]" in line and
        re.search(r"(?:^|\s)reason=stop(?:\s|$)", line) is not None
        for parsed in [_numeric_log_fields(line, ("targetFrames", "completedFrames"))]
        if parsed is not None
    ]
    video_rows = [
        parsed for line in text.splitlines()
        if "[DdsVideoPerf]" in line
        for parsed in [_numeric_log_fields(
            line, ("sentSamples", "sentBytes", "writeErrors", "droppedSamples"))]
        if parsed is not None
    ]
    counts = {
        "target_frames": 0,
        "completed_frames": 0,
        "sent_samples": 0,
        "sent_bytes": 0,
        "write_errors": 0,
        "dropped_samples": 0,
    }
    if not stop_drain_rows:
        issues.append(f"{label}:stop_output_drain_counts_missing")
    else:
        matching_drain_rows = stop_drain_rows
        if authoritative_loop is not None:
            matching_drain_rows = [
                row for row in stop_drain_rows
                if row["targetFrames"] == authoritative_loop.get("target_frames")
                and row["completedFrames"] == authoritative_loop.get("completed_frames")
            ]
            if not matching_drain_rows:
                issues.append(f"{label}:stop_output_drain_authority_mismatch")
                matching_drain_rows = stop_drain_rows
        drain = matching_drain_rows[-1]
        counts["target_frames"] = drain["targetFrames"]
        counts["completed_frames"] = drain["completedFrames"]
        if drain["targetFrames"] <= 0:
            issues.append(f"{label}:target_frames_not_positive")
        if drain["completedFrames"] != drain["targetFrames"]:
            issues.append(
                f"{label}:render_count_mismatch:{drain['completedFrames']}!={drain['targetFrames']}")
    if not video_rows:
        issues.append(f"{label}:dds_sender_counts_missing")
    else:
        matching_video_rows = video_rows
        if authoritative_loop is not None:
            matching_video_rows = [
                row for row in video_rows
                if row["sentSamples"] == authoritative_loop.get("sent_samples")
                and row["sentBytes"] == authoritative_loop.get("sent_bytes")
                and row["writeErrors"] == authoritative_loop.get("write_errors")
                and row["droppedSamples"] == authoritative_loop.get("dropped_samples")
            ]
            if not matching_video_rows:
                issues.append(f"{label}:dds_sender_authority_mismatch")
                matching_video_rows = video_rows
        video = matching_video_rows[-1]
        counts.update({
            "sent_samples": video["sentSamples"],
            "sent_bytes": video["sentBytes"],
            "write_errors": video["writeErrors"],
            "dropped_samples": video["droppedSamples"],
        })
        if video["sentSamples"] <= 0 or video["sentBytes"] <= 0:
            issues.append(f"{label}:dds_sender_not_positive")
        if video["writeErrors"] != 0:
            issues.append(f"{label}:dds_write_errors:{video['writeErrors']}")
        if video["droppedSamples"] != 0:
            issues.append(f"{label}:dds_dropped_samples:{video['droppedSamples']}")
    if counts["completed_frames"] != counts["sent_samples"]:
        issues.append(
            f"{label}:render_send_count_mismatch:{counts['completed_frames']}!={counts['sent_samples']}")
    return counts


def _parse_rollback_receiver_log(text: str, label: str, issues: list[str]) -> dict[str, int]:
    perf_rows = [
        parsed for line in text.splitlines()
        if "[DdsVideoReceiverPerf]" in line
        for parsed in [_numeric_log_fields(line, ("receivedSamples", "receivedBytes", "ddsErrors"))]
        if parsed is not None
    ]
    decoded_frames = 0
    for line in text.splitlines():
        if not line.startswith("[RuntimeMetricsV2] "):
            continue
        try:
            row = json.loads(line.split(" ", 1)[1])
            decoded_frames = max(decoded_frames, int(row.get("decodedFrames", 0)))
        except (TypeError, ValueError, json.JSONDecodeError):
            issues.append(f"{label}:runtime_metrics_invalid")
            break
    decode_error_values = [int(value) for value in re.findall(r"h264DecodeErrors=(\d+)", text)]
    h264_decode_lines = [
        line for line in text.splitlines()
        if "[VideoPerf]" in line and "decodeCodec=h264_annexb" in line
        and "h264KeyFrameSeen=1" in line
    ]
    counts = {
        "received_samples": 0,
        "received_bytes": 0,
        "decoded_frames": decoded_frames,
        "dds_errors": 0,
        "decode_errors": max(decode_error_values, default=0),
    }
    if not perf_rows:
        issues.append(f"{label}:dds_receiver_counts_missing")
    else:
        perf = perf_rows[-1]
        counts.update({
            "received_samples": perf["receivedSamples"],
            "received_bytes": perf["receivedBytes"],
            "dds_errors": perf["ddsErrors"],
        })
        if perf["receivedSamples"] <= 0 or perf["receivedBytes"] <= 0:
            issues.append(f"{label}:dds_receiver_not_positive")
        if perf["ddsErrors"] != 0:
            issues.append(f"{label}:dds_receiver_errors:{perf['ddsErrors']}")
    if decoded_frames <= 0:
        issues.append(f"{label}:decoded_frames_not_positive")
    if not h264_decode_lines:
        issues.append(f"{label}:h264_decode_success_missing")
    if not decode_error_values:
        issues.append(f"{label}:h264_decode_error_count_missing")
    elif counts["decode_errors"] != 0:
        issues.append(f"{label}:h264_decode_errors:{counts['decode_errors']}")
    if counts["received_samples"] != decoded_frames:
        issues.append(
            f"{label}:receive_decode_count_mismatch:{counts['received_samples']}!={decoded_frames}")
    return counts


def _parse_rollback_stimulus_log(text: str, label: str, issues: list[str]) -> int | None:
    band_values = [
        int(value) for value in re.findall(
            r"\[StimWeather\][^\r\n]*\bsensorBand=(\d+)(?:\s|$)", text)
    ]
    if not band_values:
        issues.append(f"{label}:stimulus_protocol_band_missing")
        protocol_band = None
    else:
        protocol_band = band_values[-1]
    if re.search(r"\[StimDDS\] type=init sent=1(?:\s|$)", text) is None:
        issues.append(f"{label}:stimulus_dds_init_missing")
    if re.search(r"\[StimInitAck\][^\r\n]*\bready=1(?:\s|$)", text) is None:
        issues.append(f"{label}:stimulus_init_ack_missing")
    return protocol_band


def evaluate_rollback_receipt(workspace: Path, path: Path | None) -> Evidence:
    if path is None or not path.is_file():
        return Evidence("rk3588-rollback", "RK3588 rollback and return", NOT_RUN, relative_path(workspace, path), issues=["rollback_receipt_missing"])
    try:
        value = read_json(path)
    except (OSError, json.JSONDecodeError) as exc:
        return Evidence("rk3588-rollback", "RK3588 rollback and return", FAIL, relative_path(workspace, path), issues=[f"receipt_invalid:{exc}"])
    issues = []
    if value.get("schema") != RK_ROLLBACK_RECEIPT_SCHEMA:
        issues.append(f"schema:{value.get('schema')!r}")
    for key in ("rollback_started", "p11_restored_and_started", "rollback_snapshot_retained"):
        if value.get(key) is not True:
            issues.append(f"{key}=false_or_missing")
    if not value.get("stage_id") or not value.get("active_elf_sha256"):
        issues.append("stage_or_active_elf_missing")
    if value.get("validation_contract") != RK_ROLLBACK_LOOP_CONTRACT:
        issues.append(f"validation_contract:{value.get('validation_contract')!r}")
    rollback_suffix = value.get("rollback_suffix")
    if not isinstance(rollback_suffix, str) or re.fullmatch(
            r"\.before_p11-\d{8}-\d{6}", rollback_suffix) is None:
        issues.append(f"rollback_suffix:{rollback_suffix!r}")
    elif rollback_suffix != RK_P10_ROLLBACK_SUFFIX:
        issues.append(f"rollback_suffix_not_audited_p10:{rollback_suffix!r}")
    for key in (
        "active_elf_sha256", "active_config_manifest_sha256",
        "rollback_elf_sha256", "rollback_config_manifest_sha256",
    ):
        if not valid_sha256(value.get(key)):
            issues.append(f"{key}_missing_or_invalid")
    if value.get("rollback_elf_sha256") != RK_P10_ROLLBACK_ELF_SHA256:
        issues.append("rollback_elf_sha256_not_audited_p10")
    if (value.get("rollback_config_manifest_sha256") !=
            RK_P10_ROLLBACK_CONFIG_MANIFEST_SHA256):
        issues.append("rollback_config_manifest_sha256_not_audited_p10")

    deployment_receipt = _rollback_evidence_file(
        workspace, path, value, "deployment_receipt", "deployment", issues)
    if deployment_receipt is not None:
        try:
            deployment_value = read_json(deployment_receipt)
            for key, receipt_key in (
                ("stage_id", "stage_id"),
                ("active_elf_sha256", "elf_sha256"),
                ("active_config_manifest_sha256", "config_manifest_sha256"),
            ):
                if value.get(key) != deployment_value.get(receipt_key):
                    issues.append(
                        f"deployment:{receipt_key}_mismatch:"
                        f"{deployment_value.get(receipt_key)!r}!={value.get(key)!r}")
        except (OSError, json.JSONDecodeError) as exc:
            issues.append(f"deployment:receipt_invalid:{exc}")

    tools = value.get("tool_identities")
    if not isinstance(tools, Mapping):
        issues.append("tool_identities_missing")
    else:
        if set(tools) != {"controller", "remote_runner"}:
            issues.append(f"tool_identity_keys:{sorted(str(key) for key in tools)}")
        for role in ("controller", "remote_runner"):
            tool = tools.get(role)
            if not isinstance(tool, Mapping):
                issues.append(f"tool:{role}:record_missing")
                continue
            _rollback_evidence_file(
                workspace,
                path,
                {"file": tool.get("path"), "file_sha256": tool.get("sha256")},
                "file",
                f"tool:{role}",
                issues,
            )

    exercise_log = _rollback_evidence_file(
        workspace, path, value, "exercise_log", "exercise", issues)
    exercise_error_log = _rollback_evidence_file(
        workspace, path, value, "exercise_error_log", "exercise_error", issues)
    exercise_loop_counts: dict[str, dict[str, int]] = {}
    if exercise_log is not None:
        exercise_text = exercise_log.read_text(encoding="utf-8-sig", errors="replace")
        if RK_ROLLBACK_FATAL_PATTERN.search(exercise_text):
            issues.append("exercise:fatal_pattern")
        if "[P11RollbackFinal] result=PASS" not in exercise_text:
            issues.append("exercise:final_pass_missing")
        loop_pattern = re.compile(
            r"\[P11RollbackLoop\] release=(rollback|p11_restored) result=PASS "
            r"protocolBand=(\d+) targetFrames=(\d+) completedFrames=(\d+) "
            r"sentSamples=(\d+) sentBytes=(\d+) writeErrors=(\d+) droppedSamples=(\d+)"
        )
        for match in loop_pattern.finditer(exercise_text):
            exercise_loop_counts[match.group(1)] = {
                "protocol_band": int(match.group(2)),
                "target_frames": int(match.group(3)),
                "completed_frames": int(match.group(4)),
                "sent_samples": int(match.group(5)),
                "sent_bytes": int(match.group(6)),
                "write_errors": int(match.group(7)),
                "dropped_samples": int(match.group(8)),
            }
        for release in ("rollback", "p11_restored"):
            loop = exercise_loop_counts.get(release)
            if loop is None or any(loop[key] <= 0 for key in (
                    "target_frames", "completed_frames", "sent_samples", "sent_bytes")):
                issues.append(f"exercise:{release}_positive_loop_missing")
    if exercise_error_log is not None:
        exercise_error_text = exercise_error_log.read_text(
            encoding="utf-8-sig", errors="replace")
        if RK_ROLLBACK_FATAL_PATTERN.search(exercise_error_text):
            issues.append("exercise_error:fatal_pattern")

    releases = value.get("release_evidence")
    parsed_release_details: dict[str, dict[str, Any]] = {}
    if not isinstance(releases, Mapping):
        issues.append("release_evidence_missing")
    else:
        if set(releases) != {"rollback", "p11_restored"}:
            issues.append(f"release_evidence_keys:{sorted(str(key) for key in releases)}")
        for release in ("rollback", "p11_restored"):
            record = releases.get(release)
            label = f"release:{release}"
            if not isinstance(record, Mapping):
                issues.append(f"{label}:record_missing")
                continue
            renderer_path = _rollback_evidence_file(
                workspace, path, record, "renderer_log", label, issues)
            stimulus_path = _rollback_evidence_file(
                workspace, path, record, "stimulus_log", label, issues)
            receiver_path = _rollback_evidence_file(
                workspace, path, record, "receiver_log", label, issues)
            decoded_png = _rollback_evidence_file(
                workspace, path, record, "decoded_png", label, issues)
            if decoded_png is not None and png_geometry(decoded_png) != (800, 800):
                issues.append(f"{label}:decoded_png_geometry_invalid")
            renderer_counts = {
                key: 0 for key in (
                    "target_frames", "completed_frames", "sent_samples", "sent_bytes",
                    "write_errors", "dropped_samples",
                )
            }
            receiver_counts = {
                key: 0 for key in (
                    "received_samples", "received_bytes", "decoded_frames",
                    "dds_errors", "decode_errors",
                )
            }
            if renderer_path is not None:
                renderer_counts = _parse_rollback_renderer_log(
                    renderer_path.read_text(encoding="utf-8-sig", errors="replace"),
                    label,
                    issues,
                    exercise_loop_counts.get(release),
                )
            if receiver_path is not None:
                receiver_counts = _parse_rollback_receiver_log(
                    receiver_path.read_text(encoding="utf-8-sig", errors="replace"),
                    label,
                    issues,
                )
            observed_protocol_band = None
            if stimulus_path is not None:
                observed_protocol_band = _parse_rollback_stimulus_log(
                    stimulus_path.read_text(encoding="utf-8-sig", errors="replace"),
                    label,
                    issues,
                )
            declared_protocol_band = record.get("protocol_band")
            if release == "rollback" and declared_protocol_band != 1:
                issues.append(f"{label}:p10_protocol_band_must_be_nir_1:{declared_protocol_band!r}")
            if release == "p11_restored" and declared_protocol_band not in (0, 1):
                issues.append(f"{label}:restored_protocol_band_not_swir_or_nir:{declared_protocol_band!r}")
            if declared_protocol_band != observed_protocol_band:
                issues.append(
                    f"{label}:stimulus_protocol_band_mismatch:"
                    f"{declared_protocol_band!r}!={observed_protocol_band!r}")
            observed = {**renderer_counts, **receiver_counts}
            for key, observed_value in observed.items():
                if record.get(key) != observed_value:
                    issues.append(f"{label}:declared_{key}_mismatch:{record.get(key)!r}!={observed_value}")
            if observed["sent_samples"] != observed["received_samples"]:
                issues.append(
                    f"{label}:send_receive_count_mismatch:"
                    f"{observed['sent_samples']}!={observed['received_samples']}")
            exercise_loop = exercise_loop_counts.get(release)
            if exercise_loop is not None:
                expected_exercise = {
                    "protocol_band": declared_protocol_band,
                    **{key: observed[key] for key in (
                        "target_frames", "completed_frames", "sent_samples", "sent_bytes",
                        "write_errors", "dropped_samples",
                    )},
                }
                if exercise_loop != expected_exercise:
                    issues.append(
                        f"{label}:exercise_loop_counts_mismatch:"
                        f"{exercise_loop!r}!={expected_exercise!r}")
            parsed_release_details[release] = {
                "protocol_band": observed_protocol_band,
                **observed,
            }
    return Evidence(
        "rk3588-rollback", "RK3588 rollback and return", PASS if not issues else FAIL,
        relative_path(workspace, path), sha256_file(path),
        summary=f"stage={value.get('stage_id')}; run={value.get('run_id')}", issues=issues,
        details={
            **{key: value.get(key) for key in (
                "stage_id", "run_id", "duration_sec_each", "rollback_started",
                "p11_restored_and_started", "rollback_snapshot_retained", "active_elf_sha256",
                "active_config_manifest_sha256", "rollback_suffix", "rollback_elf_sha256",
                "rollback_config_manifest_sha256", "validation_contract",
            )},
            "release_evidence": parsed_release_details,
        },
    )


def cross_check_rk(entries: Sequence[Evidence]) -> list[str]:
    by_id = {entry.evidence_id: entry for entry in entries}
    build = by_id.get("rk3588-build")
    deploy = by_id.get("rk3588-deployment")
    acceptance = by_id.get("rk3588-dual-band")
    lifecycle = by_id.get("rk3588-dds-lifecycle")
    image_matrix = by_id.get("rk3588-dds-image-matrix")
    rollback = by_id.get("rk3588-rollback")
    issues: list[str] = []
    if build and deploy and build.status == PASS and deploy.status == PASS:
        if build.details.get("stage_id") != deploy.details.get("stage_id"):
            issues.append("build_deploy_stage_mismatch")
        if build.details.get("elf_sha256") != deploy.details.get("elf_sha256"):
            issues.append("build_deploy_elf_hash_mismatch")
        if build.details.get("build_id") != deploy.details.get("build_id"):
            issues.append("build_deploy_build_id_mismatch")
    if deploy and acceptance and deploy.status == PASS and acceptance.status == PASS:
        if deploy.details.get("stage_id") != acceptance.details.get("deploymentStageId"):
            issues.append("deploy_acceptance_stage_mismatch")
        if deploy.details.get("elf_sha256") != acceptance.details.get("elfSha256"):
            issues.append("deploy_acceptance_elf_hash_mismatch")
        if deploy.details.get("config_manifest_sha256") != acceptance.details.get("configManifestSha256"):
            issues.append("deploy_acceptance_config_hash_mismatch")
    for suffix, evidence in (("lifecycle", lifecycle), ("image_matrix", image_matrix)):
        if (deploy and evidence and deploy.status == PASS and
                evidence.status in {PASS, PARTIAL}):
            if deploy.details.get("stage_id") != evidence.details.get("deploymentStageId"):
                issues.append(f"deploy_{suffix}_stage_mismatch")
            if deploy.details.get("elf_sha256") != evidence.details.get("elfSha256"):
                issues.append(f"deploy_{suffix}_elf_hash_mismatch")
            if (deploy.details.get("config_manifest_sha256") !=
                    evidence.details.get("configManifestSha256")):
                issues.append(f"deploy_{suffix}_config_hash_mismatch")
    if deploy and rollback and deploy.status == PASS and rollback.status == PASS:
        if deploy.details.get("stage_id") != rollback.details.get("stage_id"):
            issues.append("deploy_rollback_stage_mismatch")
        if deploy.details.get("elf_sha256") != rollback.details.get("active_elf_sha256"):
            issues.append("rollback_returned_elf_hash_mismatch")
    return issues


CONFIG_FILES: Sequence[tuple[str, str]] = (
    ("HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini", "formal runtime gates and display windows"),
    ("HwaSim_IR/Bin/Config/SensorWave/default_SWIR.json", "SWIR 1.1-2.5 um profile"),
    ("HwaSim_IR/Bin/Config/SensorWave/default_MWIR.json", "MWIR 3-5 um profile"),
    ("HwaSim_IR/Bin/Config/SensorWave/default_NVG.json", "NIR compatibility profile"),
    ("HwaSim_IR/Bin/Config/SensorWave/Archive/P11/default_SWIR_legacy_1p5_2p5.json", "legacy SWIR rollback/reference"),
    ("HwaSim_IR/Bin/Config/Materials/MaterialBandOptics.csv", "band optics and temperature assumptions"),
    ("HwaSim_IR/Bin/Config/IRPlume/engine_plume_profiles.json", "localized plume profiles"),
    ("HwaSim_IR/Bin/Config/TargetLib/Targets.json", "target catalog and full protocol keys"),
    ("HwaSim_IR/Bin/Config/Annotation/annotation_profiles.json", "annotation identities"),
    ("HwaSim_IR/Bin/Config/NetworkConfig.ini", "runtime network endpoint snapshot"),
    ("HwaSim_IR_VideoDisplay/x64/Release/NetworkConfig.ini", "receiver network endpoint snapshot"),
    ("build-DataDrivenTestQT-codex-mingw73_64-Release/release/NetworkConfig.ini", "stimulus network endpoint snapshot"),
)

BINARY_FILES: Sequence[tuple[str, str]] = (
    ("HwaSim_IR/Bin/HwaSim_IR.exe", "Windows renderer"),
    ("HwaSim_IR_VideoDisplay/x64/Release/HwaSim_IR_VideoDisplay.exe", "Windows receiver"),
    ("build-DataDrivenTestQT-codex-mingw73_64-Release/release/DataDrivenTestQT.exe", "Windows stimulus"),
)


def file_record(workspace: Path, relative: str, role: str) -> dict[str, Any]:
    path = workspace / relative
    return {
        "path": relative.replace("\\", "/"),
        "role": role,
        "present": path.is_file(),
        "bytes": path.stat().st_size if path.is_file() else None,
        "sha256": sha256_file(path) if path.is_file() else None,
    }


def build_config_manifest(workspace: Path) -> dict[str, Any]:
    ffmpeg_dlls: list[dict[str, Any]] = []
    for directory in (workspace / "HwaSim_IR/Bin", workspace / "HwaSim_IR_VideoDisplay/x64/Release"):
        for name in ("avcodec-62.dll", "avformat-62.dll", "avutil-60.dll", "swscale-9.dll"):
            path = directory / name
            if path.is_file():
                ffmpeg_dlls.append(file_record(workspace, path.relative_to(workspace).as_posix(), "FFmpeg runtime dependency"))
    files = [file_record(workspace, path, role) for path, role in CONFIG_FILES]
    binaries = [file_record(workspace, path, role) for path, role in BINARY_FILES]
    missing = [row["path"] for row in files + binaries if not row["present"]]
    return {
        "schema": "HwaSimIR.P11.ConfigManifest.v2",
        "generatedLocal": now_local(),
        "hashAlgorithm": "SHA-256",
        "bandProtocol": {"0": "SWIR", "1": "NIR compatibility only", "2": "MWIR", "3": "unsupported", "4": "unsupported VIS/VIS-SWIR"},
        "files": files,
        "windowsBinaries": binaries,
        "ffmpegRuntimeDependencies": ffmpeg_dlls,
        "missingFiles": missing,
    }


def csv_summary(path: Path, band_column: str = "band") -> dict[str, Any]:
    rows: list[dict[str, str]] = []
    if path.is_file():
        with path.open("r", encoding="utf-8-sig", newline="") as stream:
            rows = list(csv.DictReader(stream))
    counts = Counter(row.get(band_column, "") for row in rows)
    return {"dataRows": len(rows), "countsByBand": dict(sorted(counts.items()))}


def json_file_evidence(workspace: Path, relative: str, role: str) -> dict[str, Any]:
    """Return a hashed JSON evidence pointer plus its small provenance payload."""
    record = file_record(workspace, relative, role)
    path = workspace / relative
    if not path.is_file():
        return record
    try:
        value = read_json(path)
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        record["readError"] = str(exc)
        return record
    if isinstance(value, Mapping):
        record["details"] = dict(value)
    else:
        record["readError"] = "root_not_object"
    return record


def selected_qc_checks(evidence: Mapping[str, Any], names: Sequence[str]) -> list[dict[str, Any]]:
    requested = set(names)
    selected: list[dict[str, Any]] = []
    details = evidence.get("details")
    if not isinstance(details, Mapping):
        return selected
    rows = details.get("checks")
    if not isinstance(rows, list):
        return selected
    for row in rows:
        if not isinstance(row, Mapping) or row.get("check") not in requested:
            continue
        selected.append({
            key: row.get(key)
            for key in ("check", "passed", "measured", "expected")
            if key in row
        })
    return selected


def build_data_manifest(workspace: Path) -> dict[str, Any]:
    band = workspace / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv"
    solar = workspace / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/solar_heating_lut_si.csv"
    materials = workspace / "HwaSim_IR/Bin/Config/Materials/MaterialBandOptics.csv"
    humidity_qc = json_file_evidence(
        workspace, "logs/p11/modtran/humidity_grid/humidity_qc_results.json",
        "real MODTRAN humidity-grid spectral and physical QC",
    )
    humidity_publish = json_file_evidence(
        workspace, "logs/p11/modtran/humidity_grid/formal_publish/publish_manifest.json",
        "atomic publish record for formal camera-band LUT",
    )
    humidity_generation = json_file_evidence(
        workspace, "logs/p11/modtran/humidity_grid/generation_provenance.json",
        "humidity-grid generation axes and generator identity",
    )
    humidity_engine = json_file_evidence(
        workspace, "logs/p11/modtran/humidity_grid/engine_and_license_evidence.json",
        "MODTRAN executable identity for humidity-grid runs",
    )
    solar_qc = json_file_evidence(
        workspace, "logs/p11/modtran/solar_heating_ground_grid/solar_heating_qc_results.json",
        "real MODTRAN broadband solar-heating spectral and physical QC",
    )
    solar_publish = json_file_evidence(
        workspace, "logs/p11/modtran/solar_heating_ground_grid/formal_publish/publish_manifest.json",
        "atomic publish record for formal broadband solar-heating LUT",
    )
    solar_generation = json_file_evidence(
        workspace, "logs/p11/modtran/solar_heating_ground_grid/generation_provenance.json",
        "broadband solar-heating generation axes and generator identity",
    )
    solar_engine = json_file_evidence(
        workspace, "logs/p11/modtran/solar_heating_ground_grid/engine_and_license_evidence.json",
        "MODTRAN executable identity for broadband solar-heating runs",
    )

    band_record = file_record(
        workspace, band.relative_to(workspace).as_posix(),
        "formal camera-band atmosphere LUT",
    )
    if band.is_file():
        band_record.update(csv_summary(band))
        band_record["formalCounts"] = csv_summary(band, "band")["countsByBand"]
        band_record["profileCounts"] = csv_summary(band, "humidity_profile")["countsByBand"]
    band_record.update({
        "source": "real MODTRAN 5.2.1 outputs plus preserved audited legacy rows",
        "units": {
            "tau_up": "dimensionless",
            "path_thermal": "W/(m^2 sr um)",
            "direct_solar_irradiance_at_target": "W/(m^2 um)",
            "downward_sky_diffuse_irradiance": "W/(m^2 um)",
            "los_path_scattering_radiance": "W/(m^2 sr um)",
        },
        "responseMode": "RectangularBand",
        "modtran": {
            "runtimePolicy": "offline generation only; RK3588 never executes MODTRAN per frame",
            "nativeSourceFields": ["COMBIN TRANS", "PTH_THRML", "SOL TR", "GRND RFLT", "TOTAL RAD"],
            "engineEvidence": humidity_engine,
            "generationEvidence": humidity_generation,
            "publishEvidence": humidity_publish,
            "qcEvidence": {
                **humidity_qc,
                "selectedChecks": selected_qc_checks(humidity_qc, (
                    "real_modtran_case_count", "all_jacobian_integrals_within_tolerance",
                    "pointwise_unit_conversions_exact", "formal_vertex_count",
                )),
            },
        },
        "jacobian": {
            "nativeRadianceUnit": "W/(cm^2 sr cm^-1)",
            "deliveredRadianceUnit": "W/(m^2 sr um)",
            "formula": "L_lambda = L_sigma * 1e4(area) * 1e4/lambda_um^2",
            "integration": "wavelength-domain trapezoidal response mean with exact interpolated band endpoints",
            "arbitraryScale": False,
        },
        "p11NumericHumidityCoverage": {
            "profilesPercent": [30, 60, 85],
            "observerTargetEqualAltitudeKm": [0.001, 1.0],
            "rangeKm": [0.1, 0.5, 1.0],
            "visibilityKm": [6, 23],
            "solarZenithDeg": [20, 45, 70],
            "components": [
                "tau", "path thermal radiance", "direct solar irradiance",
                "downward sky diffuse irradiance", "LOS solar-scattering radiance",
            ],
        },
        "interpolation": "equal-altitude spatial interpolation; numeric RH bracketing; tau in optical depth and radiance/irradiance linearly",
        "outOfDomain": "fail closed",
        "knownBoundary": "No formal 2 km range cell; a 2 km request must fail closed unless new real MODTRAN data are published.",
    })

    solar_record = file_record(
        workspace, solar.relative_to(workspace).as_posix(),
        "broadband solar heating LUT",
    )
    if solar.is_file():
        solar_record.update(csv_summary(solar))
        solar_record["profileCounts"] = csv_summary(solar, "humidity_profile")["countsByBand"]
    solar_record.update({
        "source": "real MODTRAN 5.2.1 broadband 0.30-2.50 um outputs plus 45 preserved legacy rows",
        "units": {
            "direct_shortwave_solar_irradiance": "W/m^2",
            "diffuse_shortwave_down_irradiance": "W/m^2",
            "flux_direct_horizontal_qc": "W/m^2 (QC only)",
        },
        "spectralRangeUm": [0.30, 2.50],
        "responseMode": "BroadbandIntegral",
        "composition": "45 preserved legacy rows plus 48 P11 direct+diffuse vertices from 96 real component runs",
        "modtran": {
            "runtimePolicy": "offline generation only; RK3588 never executes MODTRAN per frame",
            "directSource": "target-level MODOUT2 SOL TR",
            "diffuseSource": ".flx DOWNWARD",
            "fluxDirectPolicy": ".flx DIRECT is retained for QC only and is not added to diffuse",
            "engineEvidence": solar_engine,
            "generationEvidence": solar_generation,
            "publishEvidence": solar_publish,
            "qcEvidence": {
                **solar_qc,
                "selectedChecks": selected_qc_checks(solar_qc, (
                    "real_modtran_case_count", "all_real_spectra_bracket_0p30_2p50",
                    "jacobian_integrals_within_tolerance", "pointwise_unit_conversions_exact",
                    "formal_vertex_count",
                )),
            },
        },
        "jacobian": {
            "nativeDirectUnit": "W/(cm^2 cm^-1)",
            "nativeDiffuseFluxUnit": "W/(cm^2 nm)",
            "deliveredUnit": "W/m^2",
            "directFormula": "E_lambda = E_sigma * 1e4(area) * 1e4/lambda_um^2",
            "diffuseFormula": "E_lambda = E_nm * 1e4(area) * 1e3(nm per um)",
            "integration": "wavelength-domain trapezoidal integral from 0.30 through 2.50 um using real bracketing samples",
            "arbitraryScale": False,
        },
    })

    material_record = file_record(
        workspace, materials.relative_to(workspace).as_posix(),
        "explicit material band optics",
    )
    if materials.is_file():
        material_summary = csv_summary(materials, "Source")
        material_record["dataRows"] = material_summary["dataRows"]
        material_record["countsBySource"] = material_summary["countsByBand"]
    material_record.update({
        "source": "explicit ideal references and engineering assumptions",
        "units": "dimensionless reflectance, emissivity, and transmissivity",
        "energyBalance": "opaque rows explicitly maintain per-band reflectance + emissivity = 1 where transmissivity is zero",
        "prohibition": "Visible RGB is not used as SWIR reflectance or MWIR emissivity.",
        "calibrationStatus": "NOT_VERIFIED_CALIBRATION",
    })
    datasets = [band_record, solar_record, material_record]

    archives: list[dict[str, Any]] = []
    archive_root = workspace / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/Archive/P11"
    if archive_root.is_dir():
        for path in sorted(item for item in archive_root.rglob("*") if item.is_file()):
            record = file_record(workspace, path.relative_to(workspace).as_posix(), "pre-P11 data rollback/reference")
            record["purpose"] = (
                "pre-humidity formal LUT rollback" if path.name.startswith("band_lut_si_pre_p11_humidity_")
                else "pre-P11-ground solar-heating LUT rollback" if path.name.startswith("solar_heating_lut_si_pre_p11_ground_")
                else "pre-P11 data rollback/reference"
            )
            archives.append(record)
    legacy_swir = workspace / "HwaSim_IR/Bin/Config/SensorWave/Archive/P11/default_SWIR_legacy_1p5_2p5.json"
    if legacy_swir.is_file():
        record = file_record(workspace, legacy_swir.relative_to(workspace).as_posix(), "legacy SWIR profile rollback/reference")
        record["purpose"] = "preserve the conflicting legacy 1.5-2.5 um profile"
        archives.append(record)

    engine_details = humidity_engine.get("details") if isinstance(humidity_engine.get("details"), Mapping) else {}
    return {
        "schema": "HwaSimIR.P11.SpectralAndPhysicsDataManifest.v3",
        "generatedLocal": now_local(),
        "hashAlgorithm": "SHA-256",
        "radianceSemantics": "response-weighted rectangular-band mean spectral radiance",
        "radianceUnit": "W/(m^2 sr um)",
        "irradianceUnit": "W/(m^2 um) for camera-band terms; W/m^2 for broadband 0.30-2.50 um solar heating",
        "bands": {
            "SWIR": {"lowUm": 1.1, "highUm": 2.5, "response": "ideal rectangular reference", "measuredSensorResponse": False},
            "MWIR": {"lowUm": 3.0, "highUm": 5.0, "response": "ideal rectangular reference", "measuredSensorResponse": False},
            "NIR": {"lowUm": 0.7, "highUm": 1.1, "role": "compatibility only"},
            "VIS-SWIR": {"supported": False, "policy": "reject; never alias to SWIR"},
        },
        "datasets": datasets,
        "archives": archives,
        "modtranRuntime": {
            "executable": engine_details.get("executable"),
            "version": engine_details.get("file_version"),
            "sha256": engine_details.get("sha256"),
            "runtimePolicy": "offline generation only; RK3588 never executes MODTRAN per frame",
            "columnAndUnitEvidence": "logs/p11/modtran/P11_MODTRAN_HUMIDITY_AND_SOLAR_DELIVERY.md",
        },
        "conversion": {
            "wavenumberToWavelength": "Includes area factor 1e4 and Jacobian abs(d sigma/d lambda)=1e4/lambda_um^2 only when source is per cm^2 per cm^-1.",
            "arbitraryScalingAllowed": False,
            "totalRadTargetContributionReused": False,
        },
        "outOfDomainPolicy": "fail closed; no relabeling, clamping, or unaudited extrapolation",
        "calibrationStatus": "NOT_VERIFIED_CALIBRATION",
    }


def asset_record(workspace: Path, asset_id: str, root: str, origin: str, redistributable: bool) -> dict[str, Any]:
    asset_root = workspace / root
    manifest = asset_root / "manifest.json"
    license_path = next((path for path in (asset_root / "LICENSE", asset_root / "LICENSE.txt", asset_root / "LICENSE.md") if path.is_file()), None)
    value: Mapping[str, Any] = {}
    if manifest.is_file():
        try:
            loaded = read_json(manifest)
            value = loaded if isinstance(loaded, Mapping) else {}
        except (OSError, json.JSONDecodeError):
            value = {}
    return {
        "id": asset_id,
        "root": root,
        "present": asset_root.is_dir(),
        "origin": origin,
        "license": value.get("license"),
        "redistributable": redistributable,
        "productionConnected": True,
        "manifest": relative_path(workspace, manifest) if manifest.is_file() else None,
        "manifestSha256": sha256_file(manifest) if manifest.is_file() else None,
        "licenseFile": relative_path(workspace, license_path) if license_path else None,
        "licenseSha256": sha256_file(license_path) if license_path else None,
        "geometry": value.get("geometry"),
        "materialPolicy": value.get("materialPolicy"),
        "calibrationStatus": "ideal numeric references and engineering assumptions; not measured target calibration",
    }


def build_asset_manifest(workspace: Path, glass: Evidence) -> dict[str, Any]:
    assets = [
        asset_record(workspace, "p11-civil-panel-van", "HwaSim_IR/Bin/Config/TargetLib/p11/civil_van", "project-authored deterministic primitive geometry", True),
        asset_record(workspace, "p11-controlled-ir-sample-rack", "HwaSim_IR/Bin/Config/TargetLib/p11/controlled_samples", "project-authored deterministic geometry and ideal/engineering values", True),
        {
            "id": "gmc-van-white-original-and-bridge",
            "root": "ondulus ir 红外图片示例/模型示例/GMC_Van_White",
            "origin": "locally supplied OpenFlight reference asset",
            "license": "NO_REDISTRIBUTION_GRANT_FOUND_ADJACENT_TO_ASSET",
            "redistributable": False,
            "productionConnected": False,
            "deliveryZipAllowed": False,
        },
        {"id": "li-chenyang-thesis", "license": "reference-only; redistribution not asserted", "redistributable": False, "productionConnected": False},
        {"id": "ondulus-overview-pptx", "license": "reference-only; redistribution not asserted", "redistributable": False, "productionConnected": False},
    ]
    return {
        "schema": "HwaSimIR.P11.AssetLicenseManifest.v2",
        "generatedLocal": now_local(),
        "deliveryPolicy": "Only explicitly redistributable assets enter the ZIP; reference-only and GMC inputs are excluded.",
        "glassImplementationEvidence": {"status": glass.status, "source": glass.source, "limitationsPreserved": True},
        "assets": assets,
    }


DDS_ONLY_BLOCKING_IDS = {
    "independent-reference",
    "protocol-contracts",
    "rk3588-build",
    "rk3588-deployment",
    "rk3588-dual-band",
    "rk3588-dds-lifecycle",
    "rk3588-dds-image-matrix",
    "rk3588-rollback",
    "rk3588-chain-consistency",
}


def aggregate_status(entries: Sequence[Evidence], rk_cross_issues: Sequence[str]) -> str:
    by_id = {entry.evidence_id: entry for entry in entries}
    required = [by_id[evidence_id] for evidence_id in DDS_ONLY_BLOCKING_IDS
                if evidence_id in by_id]
    if set(by_id).isdisjoint(DDS_ONLY_BLOCKING_IDS) or any(
            evidence_id not in by_id for evidence_id in DDS_ONLY_BLOCKING_IDS):
        return PARTIAL
    if rk_cross_issues or any(entry.status == FAIL for entry in required):
        return FAIL
    if any(entry.status in {NOT_RUN, PARTIAL} or entry.status.startswith("BLOCKED") for entry in required):
        return PARTIAL
    return PASS


def copy_file(workspace: Path, source: Path, staging: Path, archive_relative: Path) -> dict[str, Any]:
    if not inside_workspace(workspace, source):
        raise ValueError(f"refusing artifact outside workspace: {source}")
    destination = staging / archive_relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    return {
        "source": relative_path(workspace, source),
        "path": archive_relative.as_posix(),
        "bytes": destination.stat().st_size,
        "sha256": sha256_file(destination),
    }


def copy_matrix_cases(workspace: Path, staging: Path, cases: Sequence[Mapping[str, Any]]) -> list[dict[str, Any]]:
    """Copy every formal case's bounded evidence set into the delivery.

    Legacy callers without ``deliveryArtifacts`` retain the old Windows
    package behavior for diagnostic tooling tests.  DDS-only finalization
    always supplies an explicit canonical-name -> audited-source mapping.
    """
    copied: list[dict[str, Any]] = []
    seen_keys: set[tuple[str, str]] = set()
    for row in cases:
        band = str(row.get("band", ""))
        case_id = str(row.get("caseId", ""))
        key = (band, case_id)
        if not band or not case_id or key in seen_keys:
            raise ValueError(f"matrix case identity invalid or duplicate: {key}")
        seen_keys.add(key)
        destination = Path("artifacts/matrix") / band / case_id
        declared = row.get("deliveryArtifacts")
        if isinstance(declared, Mapping):
            if set(declared) != set(RK_DDS_CASE_PACKAGE_FILES):
                raise ValueError(
                    f"DDS matrix artifact mapping invalid: {band}/{case_id}:"
                    f"{sorted(map(str, declared))}")
            sources = [
                (name, workspace_path(workspace, declared.get(name)))
                for name in RK_DDS_CASE_PACKAGE_FILES
            ]
        else:
            case_path = workspace_path(workspace, row.get("source"))
            if case_path is None or not case_path.is_file():
                raise FileNotFoundError(f"matrix case.json missing: {band}/{case_id}")
            case_dir = case_path.parent
            sources = [(name, case_dir / name) for name in WINDOWS_CASE_PACKAGE_FILES]
        for name, source in sources:
            if source is None:
                raise FileNotFoundError(f"matrix core artifact missing: {band}/{case_id}/{name}")
            if not source.is_file() or source.stat().st_size == 0:
                raise FileNotFoundError(f"matrix core artifact missing: {band}/{case_id}/{name}")
            copied.append(copy_file(workspace, source, staging, destination / name))
    return copied


def copy_gallery(workspace: Path, staging: Path, gallery: Sequence[GalleryItem]) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    copied: list[dict[str, Any]] = []
    rendered: list[dict[str, Any]] = []
    for item in gallery:
        # Gallery is an index over the canonical full-matrix copy.  Do not
        # duplicate PFM payloads or other large evidence under category paths.
        base = Path("artifacts/matrix") / item.band / item.case_id
        images: dict[str, str] = {}
        attachments: dict[str, str] = {}
        for name, source in item.images.items():
            filename = GALLERY_IMAGE_FILES.get(name, source.name)
            destination = staging / base / filename
            if not destination.is_file():
                raise FileNotFoundError(f"gallery canonical image missing: {base / filename}")
            images[name] = (base / filename).as_posix()
        for name, source in item.attachments.items():
            filename = GALLERY_ATTACHMENT_FILES.get(name, source.name)
            destination = staging / base / filename
            if not destination.is_file():
                raise FileNotFoundError(f"gallery canonical attachment missing: {base / filename}")
            attachments[name] = (base / filename).as_posix()
        rendered.append({"band": item.band, "category": item.category, "caseId": item.case_id, "status": item.case_status, "images": images, "attachments": attachments})
    return copied, rendered


def copy_acceptance_artifacts(
    workspace: Path,
    staging: Path,
    root: Path | None,
    family: str,
    patterns: Sequence[str],
    bands: Sequence[str] = ("SWIR", "MWIR"),
) -> list[dict[str, Any]]:
    copied: list[dict[str, Any]] = []
    if root is None or not root.is_dir():
        return copied
    for band in bands:
        band_root = root / band
        if not band_root.is_dir():
            continue
        seen: set[Path] = set()
        for pattern in patterns:
            for source in sorted(band_root.glob(pattern)):
                if not source.is_file() or source in seen:
                    continue
                seen.add(source)
                copied.append(copy_file(workspace, source, staging, Path("artifacts") / family / band / source.name))
    for name in ("p11_windows_h264_dual_band_summary.json", "binary_preflight.json", "acceptance_overall.json"):
        source = root / name
        if source.is_file():
            copied.append(copy_file(workspace, source, staging, Path("artifacts") / family / name))
    return copied


def copy_artifact_tree(
    workspace: Path,
    staging: Path,
    root: Path | None,
    archive_relative: Path,
) -> list[dict[str, Any]]:
    """Copy a bounded evidence root verbatim, retaining paths and log identity."""
    copied: list[dict[str, Any]] = []
    if root is None or not root.is_dir():
        return copied
    if not inside_workspace(workspace, root):
        raise ValueError(f"refusing evidence root outside workspace: {root}")
    for source in sorted(path for path in root.rglob("*") if path.is_file()):
        copied.append(copy_file(
            workspace, source, staging, archive_relative / source.relative_to(root)))
    return copied


def copy_entry_sources(workspace: Path, staging: Path, entries: Sequence[Evidence]) -> list[dict[str, Any]]:
    copied: list[dict[str, Any]] = []
    seen: set[Path] = set()
    for entry in entries:
        if not entry.source:
            continue
        source = workspace_path(workspace, entry.source)
        if source is None or not source.is_file() or source in seen:
            continue
        seen.add(source)
        copied.append(copy_file(workspace, source, staging, Path("artifacts/evidence_sources") / entry.evidence_id / source.name))
    return copied


def copy_rk_receipt_context(workspace: Path, staging: Path, args: argparse.Namespace) -> list[dict[str, Any]]:
    copied: list[dict[str, Any]] = []
    specifications = (
        ("build", workspace_path(workspace, args.rk_build_receipt), (
            "HwaSim_IR.aarch64", "source_manifest.sha256", "*.source.tgz",
            "*.source.tgz.sha256", "vm_build.log",
        )),
        ("deployment", workspace_path(workspace, args.rk_deployment_receipt), ("deployment_final.log", "deployment_final.txt", "deployment_manifest.sha256")),
        ("rollback", workspace_path(workspace, args.rk_rollback_receipt), (
            "rollback_exercise.log", "rollback_exercise.err.log",
            "rollback_startup.log", "p11_restored_startup.log",
            "receiver*.log", "stim*.log", "received_decode.png",
            "rollback_controller_tool.ps1", "rollback_remote_runner_tool.sh",
            "deployment_receipt_input.json",
        )),
    )
    for family, receipt, names in specifications:
        if receipt is None or not receipt.is_file():
            continue
        copied.append(copy_file(workspace, receipt, staging, Path("artifacts/rk3588/receipts") / family / receipt.name))
        seen: set[Path] = set()
        for name in names:
            direct = receipt.parent / name
            candidates = [direct] if direct.is_file() else list(receipt.parent.rglob(name))
            for source in sorted(set(path.resolve() for path in candidates if path.is_file())):
                if source in seen:
                    continue
                seen.add(source)
                destination = Path("artifacts/rk3588/receipts") / family / source.relative_to(receipt.parent)
                copied.append(copy_file(workspace, source, staging, destination))
    return copied


def copy_bundle_inputs(workspace: Path, staging: Path) -> list[dict[str, Any]]:
    copied: list[dict[str, Any]] = []
    for relative, _role in CONFIG_FILES:
        source = workspace / relative
        if source.is_file() and "NetworkConfig.ini" not in source.name:
            copied.append(copy_file(workspace, source, staging, Path("bundle/config") / relative))
    for relative, _role in BINARY_FILES:
        source = workspace / relative
        if source.is_file():
            copied.append(copy_file(workspace, source, staging, Path("bundle/bin") / relative))
    for relative in (
        "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv",
        "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/solar_heating_lut_si.csv",
    ):
        source = workspace / relative
        if source.is_file():
            copied.append(copy_file(workspace, source, staging, Path("bundle/data") / relative))
    for asset_root in (
        workspace / "HwaSim_IR/Bin/Config/TargetLib/p11/civil_van",
        workspace / "HwaSim_IR/Bin/Config/TargetLib/p11/controlled_samples",
    ):
        if not asset_root.is_dir():
            continue
        for source in sorted(path for path in asset_root.rglob("*") if path.is_file()):
            copied.append(copy_file(workspace, source, staging, Path("bundle/assets") / source.relative_to(workspace)))
    tool_patterns = ("p11_*.py", "p11_*.ps1", "p11_*.sh", "p11_*.cpp", "p11_*.json")
    seen: set[Path] = set()
    for pattern in tool_patterns:
        for source in sorted((workspace / "tools").glob(pattern)):
            if source.is_file() and source not in seen:
                seen.add(source)
                copied.append(copy_file(workspace, source, staging, Path("bundle/tools") / source.name))
    for name in MODTRAN_BUNDLE_TOOLS:
        source = workspace / "tools" / name
        if not source.is_file():
            raise FileNotFoundError(f"required MODTRAN reproduction tool missing: tools/{name}")
        if source not in seen:
            seen.add(source)
            copied.append(copy_file(workspace, source, staging, Path("bundle/tools") / source.name))
    input_root = workspace / "tools/p11_inputs"
    if input_root.is_dir():
        for source in sorted(path for path in input_root.rglob("*") if path.is_file()):
            copied.append(copy_file(workspace, source, staging, Path("bundle/tools/p11_inputs") / source.relative_to(input_root)))
    return copied


def html_status_class(status: str) -> str:
    if status == PASS:
        return "pass"
    if status == FAIL:
        return "fail"
    if status.startswith("BLOCKED"):
        return "blocked"
    return "partial"


def html_anchor(prefix: str, value: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "-", value.lower()).strip("-") or "item"
    digest = hashlib.sha256(value.encode("utf-8")).hexdigest()[:8]
    return f"{prefix}-{slug}-{digest}"


def render_html(status_manifest: Mapping[str, Any], matrix_cases: Sequence[Mapping[str, Any]], gallery: Sequence[Mapping[str, Any]]) -> str:
    entries = status_manifest["entries"]
    cards = []
    for entry in entries:
        issues = "" if not entry["issues"] else "<ul>" + "".join(f"<li><code>{html.escape(str(issue))}</code></li>" for issue in entry["issues"][:12]) + "</ul>"
        source = entry.get("source")
        source_text = html.escape(str(source)) if source else "not supplied"
        source_sha256 = entry.get("sourceSha256")
        hash_text = f'<p class="small">SHA-256: <code>{html.escape(str(source_sha256))}</code></p>' if source_sha256 else ""
        cards.append(
            f'<article class="card"><span class="tag {html_status_class(entry["status"])}">{html.escape(entry["status"])}</span>'
            f'<h3>{html.escape(entry["title"])}</h3><p>{html.escape(entry["summary"])}</p>'
            f'<p class="small">Source: <code>{source_text}</code></p>{hash_text}{issues}</article>'
        )
    figures: list[str] = []
    for item in gallery:
        image_cells = []
        for variant in ("fixed", "agc", "annotated", "received"):
            path = item["images"].get(variant)
            if path:
                rel = html.escape("../" + path if not str(path).startswith("artifacts/") else path)
                label = "received (AGC decode alias)" if variant == "received" else variant
                image_cells.append(f'<a href="{rel}"><img src="{rel}" loading="lazy" alt="{html.escape(item["band"] + " " + item["caseId"] + " " + variant)}"><span>{label}</span></a>')
        attachments = " · ".join(f'<a href="{html.escape(path)}">{html.escape(name)}</a>' for name, path in item["attachments"].items())
        figures.append(
            f'<article class="gallery"><h3>{html.escape(item["band"])} / {html.escape(item["category"])} / {html.escape(item["caseId"])}</h3>'
            f'<span class="tag {html_status_class(item["status"])}">{html.escape(item["status"])}</span>'
            f'<div class="images">{"".join(image_cells)}</div><p>{attachments}</p></article>'
        )

    # The shortcut gallery above is intentionally small.  The themed matrix
    # below is the canonical visual index and exposes every runnable case and
    # every delivered view, including the independently decoded receiver PNG.
    themed: dict[str, list[Mapping[str, Any]]] = defaultdict(list)
    for row in matrix_cases:
        themed[str(row.get("factor") or "unclassified")].append(row)
    theme_links: list[str] = []
    comparison_index: list[str] = []
    theme_sections: list[str] = []
    for factor in sorted(themed):
        rows = themed[factor]
        theme_id = html_anchor("theme", factor)
        theme_links.append(f'<a class="chip" href="#{theme_id}">{html.escape(factor)} ({len(rows)})</a>')
        grouped: dict[str, list[Mapping[str, Any]]] = defaultdict(list)
        for row in rows:
            grouped[str(row.get("comparisonGroup") or "ungrouped")].append(row)
        group_sections: list[str] = []
        for group in sorted(grouped):
            group_rows = sorted(grouped[group], key=lambda item: (str(item.get("band")), str(item.get("caseId"))))
            group_id = html_anchor("comparison", group)
            members = ", ".join(f'{row.get("band")}/{row.get("caseId")}' for row in group_rows)
            comparison_index.append(
                f'<li><a href="#{group_id}">{html.escape(group)}</a> '
                f'<span class="small">{html.escape(members)}</span></li>'
            )
            case_cards: list[str] = []
            for row in group_rows:
                band = str(row.get("band"))
                case_id = str(row.get("caseId"))
                base = f"artifacts/matrix/{band}/{case_id}"
                variants = (
                    ("fixed", "fixed_clean.png"),
                    ("agc", "auto_clean.png"),
                    ("annotated", "annotated.png"),
                    ("received", "received.png"),
                )
                image_cells = "".join(
                    f'<a data-variant="{variant}" href="{html.escape(base + "/" + filename)}">'
                    f'<img src="{html.escape(base + "/" + filename)}" loading="lazy" '
                    f'alt="{html.escape(band + " " + case_id + " " + variant)}"><span>'
                    f'{"received (AGC decode alias)" if variant == "received" else variant}'
                    f'</span></a>'
                    for variant, filename in variants
                )
                declared_artifacts = row.get("deliveryArtifacts")
                components_filename = (
                    "physical_components.json"
                    if isinstance(declared_artifacts, Mapping) and
                    "physical_components.json" in declared_artifacts
                    else "physical_components.csv"
                )
                attachments = " · ".join(
                    f'<a href="{html.escape(base + "/" + filename)}">{label}</a>'
                    for label, filename in (
                        ("case", "case.json"), ("raw metadata", "raw_radiance.json"),
                        ("raw PFM", "raw_radiance.pfm"), ("components", components_filename),
                        ("identity", "frame_identity.json"),
                    )
                )
                status = str(row.get("status"))
                delta = str(row.get("factorDelta") or "")
                issues = "; ".join(map(str, row.get("issues", [])))
                case_cards.append(
                    f'<article class="gallery matrix-case" id="{html_anchor("case", band + "-" + case_id)}" '
                    f'data-factor="{html.escape(factor)}" data-comparison-group="{html.escape(group)}">'
                    f'<h4>{html.escape(band)} / {html.escape(case_id)}</h4>'
                    f'<span class="tag {html_status_class(status)}">{html.escape(status)}</span>'
                    f'<p class="delta">{html.escape(delta)}</p><div class="images">{image_cells}</div>'
                    f'<p>{attachments}</p>'
                    + (f'<p class="small"><code>{html.escape(issues)}</code></p>' if issues else "")
                    + '</article>'
                )
            group_sections.append(
                f'<details class="comparison" id="{group_id}" open><summary><strong>{html.escape(group)}</strong> '
                f'— {len(group_rows)} band/case rows</summary><div class="galleries">{"".join(case_cards)}</div></details>'
            )
        theme_sections.append(
            f'<section class="theme" id="{theme_id}"><h3>{html.escape(factor)}</h3>{"".join(group_sections)}</section>'
        )

    case_rows: list[str] = []
    for row in matrix_cases:
        issue_text = "; ".join(map(str, row.get("issues", [])))
        case_link = html_anchor("case", str(row.get("band")) + "-" + str(row.get("caseId")))
        case_rows.append(
            f'<tr><td>{html.escape(str(row.get("band")))}</td><td><a href="#{case_link}">{html.escape(str(row.get("caseId")))}</a></td>'
            f'<td>{html.escape(str(row.get("factor")))}</td><td>{html.escape(str(row.get("comparisonGroup")))}</td>'
            f'<td><span class="tag {html_status_class(str(row.get("status")))}">{html.escape(str(row.get("status")))}</span></td>'
            f'<td><code>{html.escape(issue_text)}</code></td></tr>'
        )
    blocked_rows = []
    matrix = next((entry for entry in entries if entry["id"] == "rk3588-dds-image-matrix"), {})
    for row in matrix.get("details", {}).get("blockers", []):
        blocked_rows.append(f'<tr><td>{html.escape(str(row.get("id")))}</td><td>{html.escape(str(row.get("status")))}</td><td>{html.escape(str(row.get("reason")))}</td></tr>')
    overall = status_manifest["overallStatus"]
    return f'''<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>HwaSimIR P11 SWIR/MWIR 最终证据</title>
<style>
:root{{--bg:#0e1520;--panel:#182333;--text:#e9f1f8;--muted:#a8b8c8;--pass:#68d391;--fail:#ff7b7b;--partial:#ffd166;--blocked:#d9a0ff;--link:#82c7ff}}
*{{box-sizing:border-box}}body{{margin:0;background:var(--bg);color:var(--text);font:15px/1.55 system-ui,"Microsoft YaHei",sans-serif}}main{{max-width:1500px;margin:auto;padding:28px}}h1{{margin-bottom:6px}}h2{{margin-top:34px}}a{{color:var(--link)}}code{{white-space:pre-wrap;overflow-wrap:anywhere}}.lead,.delta{{color:var(--muted)}}.cards{{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:14px}}.card,.gallery,.comparison{{background:var(--panel);padding:16px;border-radius:10px}}.tag,.chip{{display:inline-block;padding:2px 8px;border-radius:999px;font-size:12px;font-weight:700}}.chip{{margin:3px;border:1px solid #496783}}.pass{{color:var(--pass);border:1px solid var(--pass)}}.fail{{color:var(--fail);border:1px solid var(--fail)}}.partial{{color:var(--partial);border:1px solid var(--partial)}}.blocked{{color:var(--blocked);border:1px solid var(--blocked)}}.small{{color:var(--muted);font-size:12px}}.theme{{scroll-margin-top:12px}}.comparison{{margin:12px 0}}.comparison summary{{cursor:pointer}}.galleries{{display:grid;gap:18px}}.comparison .galleries{{grid-template-columns:repeat(auto-fit,minmax(420px,1fr));margin-top:14px}}.images{{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:10px;margin-top:10px}}.images a{{display:grid;gap:4px}}img{{width:100%;height:auto;background:#000;border-radius:6px}}table{{border-collapse:collapse;width:100%;background:var(--panel)}}th,td{{padding:8px;border:1px solid #34475d;text-align:left;vertical-align:top}}th{{position:sticky;top:0;background:#23344a}}
</style></head><body><main>
<h1>HwaSimIR P11 SWIR / MWIR 证据交付</h1>
<p class="lead">Generated {html.escape(status_manifest['generatedLocal'])}. Overall: <span class="tag {html_status_class(overall)}">{html.escape(overall)}</span>. 本页由结构化证据生成；FAIL、NOT_RUN 与 BLOCKED 不会被隐藏或改写。</p>
<p><a href="final_status.json">final_status.json</a> · <a href="final_status.md">final_status.md</a> · <a href="manifests/evidence_manifest.json">evidence manifest</a> · <a href="manifests/data_manifest.json">data provenance</a> · <a href="manifests/delivery_file_manifest.json">file hashes</a></p>
<h2>验收状态</h2><section class="cards">{''.join(cards)}</section>
<h2>关键案例快捷入口</h2><section class="galleries">{''.join(figures) or '<p>No representative matrix images were supplied.</p>'}</section>
<h2>完整矩阵图片（按物理主题与对比组）</h2>
<p class="lead">每个可运行 DDS band/case 列出固定映射、AGC、标注和显式接收解码入口；对比组保持同组入口可发现。</p>
<nav>{''.join(theme_links)}</nav>
<h3>对比组索引</h3><ul>{''.join(comparison_index) or '<li>No comparison groups.</li>'}</ul>
{''.join(theme_sections) or '<p>No matrix cases were supplied.</p>'}
<h2>矩阵逐项结果</h2><table><thead><tr><th>Band</th><th>Case</th><th>Factor</th><th>Comparison group</th><th>Status</th><th>Issues</th></tr></thead><tbody>{''.join(case_rows)}</tbody></table>
<h2>保留的阻塞项</h2><table><thead><tr><th>ID</th><th>Status</th><th>Reason</th></tr></thead><tbody>{''.join(blocked_rows)}</tbody></table>
<h2>边界声明</h2><ul><li>原始浮点量为响应加权波段平均谱辐亮度 W/(m²·sr·μm)。</li><li>NIR 仅兼容回归；VIS-SWIR 不支持并明确拒绝。</li><li>工程材料、温度和羽流参数不是实测目标/传感器标定。</li><li>图片均保留所属 case 状态；失败图片只能作为诊断证据。</li></ul>
</main></body></html>'''


def render_status_markdown(status_manifest: Mapping[str, Any]) -> str:
    lines = [
        "# P11 final evidence status", "",
        f"- Generated: `{status_manifest['generatedLocal']}`",
        f"- Overall: **{status_manifest['overallStatus']}**", "",
        "| Evidence | Status | Summary |", "|---|---:|---|",
    ]
    for entry in status_manifest["entries"]:
        summary = str(entry.get("summary", "")).replace("|", "\\|")
        lines.append(f"| {entry['title']} | `{entry['status']}` | {summary} |")
    lines.extend(["", "## Preserved failures and blockers", ""])
    any_issue = False
    for entry in status_manifest["entries"]:
        if entry["status"] != PASS or entry["issues"]:
            any_issue = True
            lines.append(f"- **{entry['id']} / {entry['status']}**")
            if entry["issues"]:
                lines.extend(f"  - `{issue}`" for issue in entry["issues"])
    if not any_issue:
        lines.append("- None in the supplied, bounded evidence set.")
    lines.extend(["", "This file is generated from the cited JSON/CSV artifacts. It does not override their gates.", ""])
    return "\n".join(lines)


DOC_BEGIN = "<!-- P11_FINALIZER_STATUS_BEGIN -->"
DOC_END = "<!-- P11_FINALIZER_STATUS_END -->"


def update_doc(path: Path, status_markdown: str) -> None:
    current = path.read_text(encoding="utf-8-sig") if path.is_file() else ""
    block = f"{DOC_BEGIN}\n\n{status_markdown.strip()}\n\n{DOC_END}"
    pattern = re.compile(re.escape(DOC_BEGIN) + r".*?" + re.escape(DOC_END), re.DOTALL)
    if pattern.search(current):
        updated = pattern.sub(block, current)
    else:
        first_line, separator, remainder = current.partition("\n")
        if first_line.startswith("# ") and separator:
            updated = first_line + "\n\n" + block + "\n\n" + remainder.lstrip("\r\n")
        else:
            updated = block + "\n\n" + current.lstrip("\r\n")
    path.write_text(updated, encoding="utf-8")


def make_file_manifest(staging: Path) -> dict[str, Any]:
    rows = []
    manifest_path = staging / "manifests/delivery_file_manifest.json"
    for path in sorted(item for item in staging.rglob("*") if item.is_file() and item != manifest_path):
        rows.append({"path": path.relative_to(staging).as_posix(), "bytes": path.stat().st_size, "sha256": sha256_file(path)})
    return {
        "schema": "HwaSimIR.P11.DeliveryFileManifest.v1",
        "generatedLocal": now_local(),
        "hashAlgorithm": "SHA-256",
        "fileCount": len(rows),
        "totalBytes": sum(row["bytes"] for row in rows),
        "files": rows,
    }


def verify_file_manifest(staging: Path, manifest: Mapping[str, Any]) -> list[str]:
    issues: list[str] = []
    for row in manifest.get("files", []):
        path = staging / str(row.get("path"))
        if not path.is_file():
            issues.append(f"missing:{row.get('path')}")
        elif path.stat().st_size != row.get("bytes"):
            issues.append(f"size:{row.get('path')}")
        elif sha256_file(path) != row.get("sha256"):
            issues.append(f"sha256:{row.get('path')}")
    return issues


def build_zip(staging: Path, zip_path: Path) -> dict[str, Any]:
    zip_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = zip_path.with_name(zip_path.name + ".tmp")
    if temporary.exists():
        temporary.unlink()
    files = sorted(path for path in staging.rglob("*") if path.is_file())
    with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in files:
            archive.write(path, Path("HwaSimIR_P11_Delivery") / path.relative_to(staging))
    verification: list[str] = []
    with zipfile.ZipFile(temporary, "r") as archive:
        bad = archive.testzip()
        if bad:
            verification.append(f"crc:{bad}")
        names = archive.namelist()
        if len(names) != len(files):
            verification.append(f"entry_count:{len(names)}!={len(files)}")
    os.replace(temporary, zip_path)
    receipt = {
        "schema": "HwaSimIR.P11.DeliveryZipReceipt.v1",
        "generatedLocal": now_local(),
        "path": str(zip_path),
        "bytes": zip_path.stat().st_size,
        "sha256": sha256_file(zip_path),
        "entryCount": len(files),
        "verification": "PASS" if not verification else "FAIL",
        "issues": verification,
    }
    zip_path.with_suffix(zip_path.suffix + ".sha256").write_text(f"{receipt['sha256']}  {zip_path.name}\n", encoding="ascii")
    write_json(zip_path.with_suffix(zip_path.suffix + ".receipt.json"), receipt)
    return receipt


def static_evidence(workspace: Path) -> list[Evidence]:
    active = newest(workspace, "logs/l2-active-qc-*/l2_active_illuminator_qc.csv")
    band_lut = workspace / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv"
    solar_lut = workspace / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/solar_heating_lut_si.csv"
    return [
        evaluate_json(workspace, "baseline", "Prechange backup", workspace / "logs/p11/baseline/prechange_20260916_0126/baseline_summary.json"),
        evaluate_json(workspace, "independent-reference", "Independent CPU / production CPU / WGL GPU", workspace / "logs/p11/reference/final_gpu/reference_results.json"),
        evaluate_json(workspace, "modtran-humidity-qc", "MODTRAN SWIR/MWIR humidity and five-component QC", workspace / "logs/p11/modtran/humidity_grid/humidity_qc_results.json"),
        evaluate_publish_manifest(workspace, "modtran-band-publish", "MODTRAN camera-band LUT atomic publish", workspace / "logs/p11/modtran/humidity_grid/formal_publish/publish_manifest.json", band_lut),
        evaluate_json(workspace, "modtran-solar-heating-qc", "MODTRAN broadband solar-heating QC", workspace / "logs/p11/modtran/solar_heating_ground_grid/solar_heating_qc_results.json"),
        evaluate_publish_manifest(workspace, "modtran-solar-publish", "MODTRAN solar-heating LUT atomic publish", workspace / "logs/p11/modtran/solar_heating_ground_grid/formal_publish/publish_manifest.json", solar_lut),
        evaluate_json(workspace, "glass-transmission-static", "Glass transmission static/CPU gate", workspace / "logs/p11/reference/glass_transmission/p11_glass_transmission_check.json"),
        evaluate_json(workspace, "glass-transmission-gpu", "Glass transmission WGL GPU probe", workspace / "logs/p11/reference/glass_transmission/gpu/p11_glass_gpu_probe.json"),
        evaluate_json(workspace, "protocol-contracts", "Protocol and formal-chain contracts", workspace / "logs/p11/tests/contracts.json"),
        evaluate_pass_csv(workspace, "active-illumination", "Active illumination component gate", active),
        evaluate_json(workspace, "thermal-inertia", "Thermal inertia component gate", newest(workspace, "logs/p11/thermal_inertia/*/p11_thermal_inertia_summary.json")),
        evaluate_json(workspace, "aero-locality", "Aero/material locality gate", newest(workspace, "logs/p11/aero_material_locality/*/p11_aero_material_locality_summary.json")),
        evaluate_json(workspace, "plume-physics", "Exhaust plume component gate", newest(workspace, "logs/p11/plume_physics/*/p11_plume_physics_summary.json")),
        evaluate_json(workspace, "controlled-pixel-swir", "SWIR CPU-uniform-float pixel triplet", workspace / "logs/p11/windows/validation-controlled-swir-near-20260916/SWIR/controlled_sample_solar_off/cpu_shader_pixel_check.json"),
        evaluate_json(workspace, "controlled-pixel-mwir", "MWIR CPU-uniform-float pixel triplet", workspace / "logs/p11/windows/validation-controlled-mwir-near-20260916/MWIR/controlled_sample_solar_off/cpu_shader_pixel_check.json"),
        evaluate_json(workspace, "band-switch", "SWIR to MWIR to SWIR re-INIT", workspace / "logs/p11/band_switch/final17/p11_band_switch_reinit_summary.json"),
        Evidence("real-calibration", "Traceable real target/sensor calibration", "NOT_VERIFIED_CALIBRATION", None, summary="Engineering assumptions are explicit; no traceable coupon/SRF/QE/optics/exposure truth was supplied."),
        Evidence("vis-swir", "VIS-SWIR", "UNSUPPORTED_REJECTED", "logs/p11/band_switch/final17/p11_band_switch_reinit_summary.json", summary="Unsupported protocol values are rejected and never aliased to SWIR."),
    ]


def add_extra_evidence(workspace: Path, entries: list[Evidence], values: Sequence[str]) -> None:
    for value in values:
        if "=" not in value:
            raise ValueError(f"--extra-evidence requires ID=PATH: {value}")
        evidence_id, raw_path = value.split("=", 1)
        entries.append(evaluate_json(workspace, evidence_id.strip(), evidence_id.strip().replace("_", " "), workspace_path(workspace, raw_path.strip())))


def build_status(args: argparse.Namespace) -> tuple[dict[str, Any], list[dict[str, Any]], list[GalleryItem], list[Evidence]]:
    workspace = args.workspace.resolve()
    entries = static_evidence(workspace)
    windows_matrix_root = workspace_path(workspace, args.windows_matrix_root)
    matrix, _windows_cases, _windows_gallery = evaluate_matrix(
        workspace, workspace / "tools/p11_windows_evidence_matrix.json",
        windows_matrix_root)
    entries.append(as_historical_diagnostic(
        matrix, "Windows TCP image evidence predates the user-selected DDS-only acceptance scope"))
    entries.append(as_historical_diagnostic(evaluate_windows_aero_summary(
        workspace,
        windows_matrix_root,
        workspace_path(workspace, args.windows_aero_summary),
    ), "Windows TCP aero evidence predates the user-selected DDS-only acceptance scope"))
    expected_windows_programs = matrix.details.get("formalProgramIdentities", [])
    entries.append(as_historical_diagnostic(evaluate_windows_h264(
        workspace,
        workspace_path(workspace, args.windows_h264_root),
        expected_windows_programs,
    ), "Windows TCP H.264 evidence is retained for diagnostics only"))
    entries.append(evaluate_build_receipt(workspace, workspace_path(workspace, args.rk_build_receipt)))
    entries.append(evaluate_deploy_receipt(workspace, workspace_path(workspace, args.rk_deployment_receipt)))
    entries.append(evaluate_rollback_receipt(workspace, workspace_path(workspace, args.rk_rollback_receipt)))
    entries.append(evaluate_rk_acceptance(workspace, workspace_path(workspace, args.rk_acceptance_root)))
    entries.append(evaluate_rk_dds_lifecycle(
        workspace, workspace_path(workspace, args.rk_dds_lifecycle_summary)))
    rk_dds_image = evaluate_rk_dds_image_matrix(
        workspace, workspace_path(workspace, args.rk_dds_image_root))
    entries.append(rk_dds_image)
    cases, gallery = dds_image_delivery_views(workspace, rk_dds_image)
    if args.nir_summary:
        entries.append(as_historical_diagnostic(evaluate_nir_compatibility(
            workspace,
            workspace_path(workspace, args.nir_summary),
            expected_windows_programs,
        ), "NIR compatibility remains nonblocking in DDS-only SWIR/MWIR acceptance"))
    else:
        entries.append(as_historical_diagnostic(
            Evidence("nir-compatibility", "NIR compatibility regression", NOT_RUN, None,
                     issues=["nir_summary_not_supplied"]),
            "NIR compatibility remains nonblocking in DDS-only SWIR/MWIR acceptance"))
    add_extra_evidence(workspace, entries, args.extra_evidence)
    cross = cross_check_rk(entries)
    if cross:
        entries.append(Evidence(
            "rk3588-chain-consistency",
            "RK build/deploy/DDS lifecycle/image/rollback identity",
            FAIL, None, issues=cross, summary="; ".join(cross)))
    else:
        relevant = [entry for entry in entries if entry.evidence_id in {
            "rk3588-build", "rk3588-deployment", "rk3588-rollback",
            "rk3588-dual-band", "rk3588-dds-lifecycle", "rk3588-dds-image-matrix",
        }]
        identity_status = PASS if (
            len(relevant) == 6 and
            all(entry.status == PASS for entry in relevant
                if entry.evidence_id != "rk3588-dds-image-matrix") and
            next((entry.status for entry in relevant
                  if entry.evidence_id == "rk3588-dds-image-matrix"), NOT_RUN)
            in {PASS, PARTIAL}
        ) else NOT_RUN
        entries.append(Evidence(
            "rk3588-chain-consistency",
            "RK build/deploy/DDS lifecycle/image/rollback identity",
            identity_status,
            None,
            summary=("ELF/build/config/stage identities agree" if identity_status == PASS
                     else "identity chain is incomplete"),
        ))
    status = {
        "schema": "HwaSimIR.P11.FinalStatus.v1",
        "generatedLocal": now_local(),
        "workspace": str(workspace),
        "acceptanceMode": args.acceptance_mode.replace("-", "_").upper(),
        "blockingEvidenceIds": sorted(DDS_ONLY_BLOCKING_IDS),
        "overallStatus": aggregate_status(entries, cross),
        "statusPolicy": STATUS_POLICY,
        "inputRoots": {
            "windowsMatrix": relative_path(workspace, workspace_path(workspace, args.windows_matrix_root)),
            "windowsAeroSummary": relative_path(workspace, workspace_path(workspace, args.windows_aero_summary)),
            "windowsH264": relative_path(workspace, workspace_path(workspace, args.windows_h264_root)),
            "rkAcceptance": relative_path(workspace, workspace_path(workspace, args.rk_acceptance_root)),
            "rkDdsLifecycleSummary": relative_path(workspace, workspace_path(workspace, args.rk_dds_lifecycle_summary)),
            "rkDdsImageRoot": relative_path(workspace, workspace_path(workspace, args.rk_dds_image_root)),
            "rkBuildReceipt": relative_path(workspace, workspace_path(workspace, args.rk_build_receipt)),
            "rkDeploymentReceipt": relative_path(workspace, workspace_path(workspace, args.rk_deployment_receipt)),
            "rkRollbackReceipt": relative_path(workspace, workspace_path(workspace, args.rk_rollback_receipt)),
            "nirSummary": relative_path(workspace, workspace_path(workspace, args.nir_summary)),
        },
        "entries": [entry.as_dict() for entry in entries],
        "matrixCases": cases,
        "prohibitions": [
            "A source FAIL, false gate, missing artifact, or hash mismatch is never converted to PASS.",
            "DryRun/config parsing is not production image acceptance.",
            "TCP evidence is not DDS evidence; cross-build/startup is not 60-second hardware acceptance.",
            "Windows UDP/TCP artifacts are HISTORICAL_DIAGNOSTIC and cannot satisfy a DDS-only blocking gate.",
            "Missing spectral data are not renamed, clamped, or extrapolated.",
            "Engineering assumptions are not described as measured calibration.",
        ],
    }
    return status, cases, gallery, entries


def prepare_staging(args: argparse.Namespace, status: Mapping[str, Any], cases: Sequence[Mapping[str, Any]], gallery: Sequence[GalleryItem], entries: Sequence[Evidence]) -> tuple[Path, list[dict[str, Any]]]:
    workspace = args.workspace.resolve()
    output = args.output_dir.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=f".{output.name}.staging-", dir=output.parent))
    copied = copy_matrix_cases(workspace, staging, cases)
    gallery_copied, rendered_gallery = copy_gallery(workspace, staging, gallery)
    copied += gallery_copied
    copied += copy_acceptance_artifacts(workspace, staging, workspace_path(workspace, args.windows_h264_root), "windows_h264", (
        "p11_windows_h264_acceptance_summary.json", "p11_windows_h264_over_80ms_frames.csv", "p11_windows_h264_per_frame_latency.csv", "received*.png", "*decode*.png",
    ))
    nir_summary_path = workspace_path(workspace, args.nir_summary)
    nir_root = nir_summary_path.parent if nir_summary_path is not None and nir_summary_path.is_file() else None
    copied += copy_acceptance_artifacts(workspace, staging, nir_root, "nir_compatibility", (
        "p11_windows_h264_acceptance_summary.json", "p11_windows_h264_over_80ms_frames.csv",
        "p11_windows_h264_per_frame_latency.csv", "received*.png", "*decode*.png",
    ), bands=("NIR",))
    copied += copy_artifact_tree(
        workspace, staging, workspace_path(workspace, args.rk_acceptance_root),
        Path("artifacts/rk3588/dds_dual_band"))
    lifecycle_summary = workspace_path(workspace, args.rk_dds_lifecycle_summary)
    lifecycle_root = lifecycle_summary.parent if lifecycle_summary is not None and lifecycle_summary.is_file() else None
    copied += copy_artifact_tree(
        workspace, staging, lifecycle_root, Path("artifacts/rk3588/dds_lifecycle"))
    copied += copy_artifact_tree(
        workspace, staging, workspace_path(workspace, args.rk_dds_image_root),
        Path("artifacts/rk3588/dds_image_matrix"))
    copied += copy_entry_sources(workspace, staging, entries)
    copied += copy_rk_receipt_context(workspace, staging, args)
    copied += copy_bundle_inputs(workspace, staging)
    for value in args.extra_artifact:
        source = workspace_path(workspace, value)
        if source is None or not source.is_file():
            raise FileNotFoundError(f"extra artifact missing: {value}")
        copied.append(copy_file(workspace, source, staging, Path("artifacts/extra") / source.name))

    write_json(staging / "final_status.json", status)
    status_markdown = render_status_markdown(status)
    (staging / "final_status.md").write_text(status_markdown, encoding="utf-8")
    matrix_entry = next((entry for entry in status["entries"]
                         if entry.get("id") == "rk3588-dds-image-matrix"), {})
    matrix_details = matrix_entry.get("details") if isinstance(matrix_entry, Mapping) else {}
    if not isinstance(matrix_details, Mapping):
        matrix_details = {}
    write_json(staging / "manifests/evidence_manifest.json", {
        "schema": "HwaSimIR.P11.DeliveryEvidenceManifest.v2",
        "generatedLocal": status["generatedLocal"],
        "overallStatus": status["overallStatus"],
        "matrixPlanSha256": matrix_details.get("planSha256"),
        "matrixEvidenceSha256": matrix_details.get("matrixEvidenceSha256"),
        "entries": status["entries"],
        "matrixCases": cases,
        "copiedArtifacts": copied,
    })
    config = build_config_manifest(workspace)
    data = build_data_manifest(workspace)
    glass = next(entry for entry in entries if entry.evidence_id == "glass-transmission-static")
    assets = build_asset_manifest(workspace, glass)
    write_json(staging / "manifests/config_manifest.json", config)
    write_json(staging / "manifests/data_manifest.json", data)
    write_json(staging / "manifests/asset_license_manifest.json", assets)
    (staging / "index.html").write_text(render_html(status, cases, rendered_gallery), encoding="utf-8")
    for doc in (args.report, args.runbook):
        source = workspace_path(workspace, doc)
        if source is not None and source.is_file():
            copied.append(copy_file(workspace, source, staging, Path("docs") / source.name))
    manifest = make_file_manifest(staging)
    write_json(staging / "manifests/delivery_file_manifest.json", manifest)
    verification = verify_file_manifest(staging, manifest)
    if verification:
        raise RuntimeError("delivery file verification failed: " + "; ".join(verification))
    return staging, copied


def publish_staging(staging: Path, output: Path, replace: bool) -> Path | None:
    backup: Path | None = None
    if output.exists():
        if not replace:
            raise FileExistsError(f"output exists; use --replace-output: {output}")
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        backup = output.with_name(f"{output.name}.previous-{stamp}")
        counter = 1
        while backup.exists():
            backup = output.with_name(f"{output.name}.previous-{stamp}-{counter}")
            counter += 1
        os.replace(output, backup)
    try:
        os.replace(staging, output)
    except Exception:
        if backup is not None and not output.exists():
            os.replace(backup, output)
        raise
    return backup


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, default=ROOT)
    parser.add_argument(
        "--acceptance-mode", choices=("dds-only",), default="dds-only",
        help="Blocking transport scope. P11 final acceptance is DDS-only; Windows UDP/TCP is historical diagnostic evidence.",
    )
    parser.add_argument("--windows-matrix-root", help="Explicit final matrix root containing SWIR/ and MWIR/.")
    parser.add_argument("--windows-aero-summary", help="JSON stdout saved from p11_windows_aero_matrix_check.py for the exact final matrix root.")
    parser.add_argument("--windows-h264-root", help="Explicit dual-band Windows H.264 acceptance root.")
    parser.add_argument("--rk-acceptance-root", help="Explicit dual-band RK3588 acceptance root.")
    parser.add_argument("--rk-dds-lifecycle-summary", help="Exact RK3588 DDS lifecycle overall summary JSON.")
    parser.add_argument("--rk-dds-image-root", help="Explicit RK3588 DDS image-matrix root.")
    parser.add_argument("--rk-build-receipt", help="Exact final build_receipt.json.")
    parser.add_argument("--rk-deployment-receipt", help="Exact final deployment_receipt.json.")
    parser.add_argument("--rk-rollback-receipt", help="Exact final rollback_receipt.json.")
    parser.add_argument("--nir-summary", help="Executed NIR compatibility summary JSON.")
    parser.add_argument("--extra-evidence", action="append", default=[], metavar="ID=PATH", help="Additional result JSON to preserve.")
    parser.add_argument("--extra-artifact", action="append", default=[], metavar="PATH", help="Additional workspace file to include verbatim.")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "logs/p11/delivery")
    parser.add_argument("--report", default="docs/HwaSimIR_P11_IR_Physics_RootCause_And_Fix.md")
    parser.add_argument("--runbook", default="docs/HwaSimIR_P11_Runbook.md")
    parser.add_argument("--update-docs", action="store_true", help="Replace/append a marked generated status block in report and runbook.")
    parser.add_argument("--validate-only", action="store_true", help="Read and validate sources; do not write delivery files.")
    parser.add_argument("--replace-output", action="store_true", help="Atomically replace output, preserving the previous directory as timestamped backup.")
    parser.add_argument("--build-zip", action="store_true", help="Create and verify the final ZIP after publishing the delivery directory.")
    parser.add_argument("--zip-path", type=Path, default=ROOT / "HwaSimIR_P11_Delivery.zip")
    parser.add_argument("--fail-on-nonpass", action="store_true", help="Return exit 3 when overallStatus is not PASS after writing/validation.")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    args.workspace = args.workspace.resolve()
    args.output_dir = args.output_dir if args.output_dir.is_absolute() else args.workspace / args.output_dir
    args.output_dir = args.output_dir.resolve()
    args.zip_path = args.zip_path if args.zip_path.is_absolute() else args.workspace / args.zip_path
    args.zip_path = args.zip_path.resolve()
    try:
        status, cases, gallery, entries = build_status(args)
        if args.validate_only:
            print(json.dumps({
                "schema": status["schema"], "overallStatus": status["overallStatus"],
                "entries": {row["id"]: row["status"] for row in status["entries"]},
                "matrixCases": len(cases), "galleryItems": len(gallery), "written": False,
            }, ensure_ascii=False, indent=2))
            return 3 if args.fail_on_nonpass and status["overallStatus"] != PASS else 0
        staging, copied = prepare_staging(args, status, cases, gallery, entries)
        backup = publish_staging(staging, args.output_dir, args.replace_output)
        if args.update_docs:
            status_markdown = render_status_markdown(status)
            for value in (args.report, args.runbook):
                path = workspace_path(args.workspace, value)
                if path is not None:
                    update_doc(path, status_markdown)
        result = {
            "schema": "HwaSimIR.P11.DeliveryFinalizerReceipt.v1",
            "overallStatus": status["overallStatus"],
            "output": str(args.output_dir),
            "previousOutputBackup": str(backup) if backup else None,
            "copiedArtifactCount": len(copied),
            "zipRequested": args.build_zip,
        }
        if args.update_docs:
            for value in (args.report, args.runbook):
                source = workspace_path(args.workspace, value)
                if source is not None and source.is_file():
                    destination = args.output_dir / "docs" / source.name
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(source, destination)
        write_json(args.output_dir / "finalizer_receipt.json", result)
        # Receipt changes the directory after the frozen manifest; regenerate it last.
        manifest = make_file_manifest(args.output_dir)
        write_json(args.output_dir / "manifests/delivery_file_manifest.json", manifest)
        manifest_issues = verify_file_manifest(args.output_dir, manifest)
        if manifest_issues:
            raise RuntimeError("post-publish manifest verification failed: " + "; ".join(manifest_issues))
        zip_receipt = build_zip(args.output_dir, args.zip_path) if args.build_zip else None
        result["zip"] = zip_receipt
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 3 if args.fail_on_nonpass and status["overallStatus"] != PASS else 0
    except Exception as exc:
        print(json.dumps({"result": FAIL, "error": str(exc)}, ensure_ascii=False), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
