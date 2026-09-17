#!/usr/bin/env python3
"""Finalize one RK3588 DDS-only P11 image case without fabricating evidence."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import shutil
import struct
from pathlib import Path
from typing import Any, Dict, Iterable, List, Optional, Sequence, Tuple


VARIANT_IMAGES = {
    "fixed": "fixed_clean.png",
    "agc": "auto_clean.png",
    "annotated": "annotated.png",
}


def read_json(path: Path) -> Dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(value, dict):
        raise ValueError("JSON root is not an object: %s" % path)
    return value


def write_json(path: Path, value: Any) -> None:
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def png_size(path: Path) -> Tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("invalid PNG: %s" % path)
    return struct.unpack(">II", header[16:24])


def one_glob(directory: Path, pattern: str) -> Path:
    matches = list(directory.glob(pattern))
    if len(matches) != 1:
        raise ValueError("expected one %s under %s, found %d" %
                         (pattern, directory, len(matches)))
    return matches[0]


def read_csv_rows(path: Path) -> List[Dict[str, str]]:
    with path.open("r", encoding="utf-8-sig", newline="") as stream:
        return list(csv.DictReader(stream))


def index_by_seq(rows: Sequence[Dict[str, str]], path: Path) -> Dict[int, Dict[str, str]]:
    result: Dict[int, Dict[str, str]] = {}
    for row in rows:
        seq = int(row["sourceSeq"])
        if seq in result:
            raise ValueError("duplicate sourceSeq %d in %s" % (seq, path))
        result[seq] = row
    return result


def bool_text(value: bool) -> str:
    return "1" if value else "0"


def build_frame_identity(case_dir: Path, acceptance: Path) -> Dict[str, Any]:
    board_audit = acceptance / "board" / "input_audit"
    windows_audit = acceptance / "windows_audit"
    paths = {
        "sender": one_glob(windows_audit, "input_sender_*.csv"),
        "accepted": one_glob(board_audit, "input_accepted_*.csv"),
        "execute": one_glob(board_audit, "input_execute_*.csv"),
        "render": one_glob(board_audit, "stage_render_*.csv"),
        "output": one_glob(board_audit, "stage_output_*.csv"),
        "received": one_glob(windows_audit, "input_received_*.csv"),
    }
    rows_by_stage = {name: read_csv_rows(path) for name, path in paths.items()}
    tables = {name: index_by_seq(rows_by_stage[name], paths[name])
              for name in ("accepted", "execute", "render", "output", "received")}
    sequences = [int(row["sourceSeq"]) for row in rows_by_stage["accepted"]]
    expected = set(sequences)
    stage_sets_equal = (len(sequences) == len(expected) and
                        all(set(table) == expected for table in tables.values()) and
                        len(rows_by_stage["sender"]) == len(sequences))
    rows: List[Dict[str, Any]] = []
    digest_equal = True
    for index, seq in enumerate(sequences):
        digests = [rows_by_stage["sender"][index].get("digestFNV1a64", ""),
                   tables["accepted"][seq].get("digestFNV1a64", ""),
                   tables["execute"][seq].get("digestFNV1a64", ""),
                   tables["received"][seq].get("digestFNV1a64", "")]
        row_digest_equal = len(set(digests)) == 1 and bool(digests[0])
        digest_equal = digest_equal and row_digest_equal
        rows.append({
            "sourceSeq": seq,
            "senderDigestFNV1a64": digests[0],
            "acceptedDigestFNV1a64": digests[1],
            "executeDigestFNV1a64": digests[2],
            "receivedDigestFNV1a64": digests[3],
            "digestIdentity": bool_text(row_digest_equal),
            "renderPresent": bool_text(seq in tables["render"]),
            "outputPresent": bool_text(seq in tables["output"]),
        })
    csv_path = case_dir / "frame_identity.csv"
    with csv_path.open("w", encoding="utf-8", newline="") as stream:
        fields = list(rows[0]) if rows else ["sourceSeq"]
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)
    summary = {
        "schema": "hwasimir.p11.rk3588.dds-frame-identity.v1",
        "result": "PASS" if sequences and stage_sets_equal and digest_equal else "FAIL",
        "row_count": len(rows),
        "first_source_seq": min(sequences) if sequences else None,
        "last_source_seq": max(sequences) if sequences else None,
        "stage_sets_equal": stage_sets_equal,
        "digest_identity": digest_equal,
        "csv": str(csv_path),
        "csv_sha256": sha256(csv_path),
        "source_ledgers": {
            name: {"path": str(path), "sha256": sha256(path),
                   "rows": len(rows_by_stage[name])}
            for name, path in paths.items()
        },
    }
    write_json(case_dir / "frame_identity.json", summary)
    return summary


def select_raw_capture(summary: Dict[str, Any], requested_seq: int) -> Dict[str, Any]:
    captures = summary.get("raw_radiance", {}).get("captures", [])
    exact = [row for row in captures if int(row.get("sourceSeq", -1)) == requested_seq]
    if len(exact) != 1:
        raise ValueError("raw capture for sourceSeq %d is absent or ambiguous" % requested_seq)
    return exact[0]


def collect_component_lines(log_path: Path) -> Dict[str, List[str]]:
    tags = {
        "radiance_components": "[Stage5 RadianceComponents]",
        "m1_compare": "[M1 Compare]",
        "solar_position": "[M1 SolarPosition]",
        "active_illuminator": "[ActiveIlluminator",
        "aero_thermal": "[Aero",
        "engine_plume": "[Plume",
        "final_pipeline": "[Stage6 FinalPipeline]",
        "effective_runtime": "[EffectiveRuntimeConfig]",
    }
    result = {name: [] for name in tags}
    for line in log_path.read_text(encoding="utf-8", errors="replace").splitlines():
        for name, token in tags.items():
            if token in line and len(result[name]) < 200:
                result[name].append(line)
    return result


def agc_numeric_evidence(log_path: Path) -> Dict[str, Any]:
    valid: List[Dict[str, float]] = []
    for line in log_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if "[Perf]" not in line or "agcEnabled=1" not in line:
            continue
        tokens = {}
        for token in line.split():
            if "=" in token:
                key, value = token.split("=", 1)
                tokens[key] = value
        try:
            row = {name: float(tokens[name]) for name in
                   ("agcGain", "agcOffset", "agcLowInput", "agcHighInput", "agcSampleCount")}
        except (KeyError, ValueError):
            continue
        if (all(math.isfinite(value) for value in row.values()) and
                row["agcSampleCount"] > 0 and row["agcHighInput"] > row["agcLowInput"] and
                row["agcGain"] > 0):
            valid.append(row)
    return {
        "result": "PASS" if valid else "FAIL",
        "valid_rows": len(valid),
        "last_valid": valid[-1] if valid else None,
    }


def finalize(args: argparse.Namespace) -> int:
    case_dir = args.case_dir.resolve()
    request_path = case_dir / "case_request.json"
    request = read_json(request_path)
    errors: List[str] = []
    if request.get("schema") != "hwasimir.p11.rk3588.dds-image-case-request.v1":
        errors.append("unexpected case request schema")
    if request.get("transport", {}).get("control") != "DDS" or \
            request.get("transport", {}).get("video") != "DDS":
        errors.append("case request is not DDS-only")
    if request.get("band") != args.band:
        errors.append("case request band mismatch")

    requested_variants = [value for value in args.variants.split(",") if value]
    if any(value not in VARIANT_IMAGES for value in requested_variants):
        errors.append("unknown requested variant")
    variant_rows: List[Dict[str, Any]] = []
    component_variants: Dict[str, Any] = {}
    fixed_acceptance: Optional[Path] = None
    for variant in requested_variants:
        acceptance = case_dir / "variants" / variant / "acceptance" / args.band
        row: Dict[str, Any] = {"variant": variant, "acceptance": str(acceptance), "errors": []}
        try:
            summary = read_json(acceptance / "acceptance_summary.json")
            status = read_json(acceptance / "runtime_status.json")
            band_request = read_json(acceptance / "case_request.json")
            if summary.get("result") != "PASS":
                row["errors"].append("acceptance summary is not PASS")
            if status.get("result") != "PROCESS_PASS":
                row["errors"].append("runtime status is not PROCESS_PASS")
            transport = band_request.get("transport", {})
            if transport.get("kind") != "DDS" or transport.get("codec") != "h264_annexb":
                row["errors"].append("acceptance transport is not DDS/H.264 Annex-B")
            if band_request.get("scenario_id") != request.get("case_id") or \
                    band_request.get("capture_variant") != variant:
                row["errors"].append("scenario/variant binding mismatch")
            if band_request.get("stimulus_extra_args") != request.get("stimulus_extra_args"):
                row["errors"].append("stimulus scenario argument binding mismatch")
            if str(band_request.get("target_fixture_sha256", "")).lower() != \
                    str(request.get("fixture_sha256", "")).lower():
                row["errors"].append("fixture hash binding mismatch")
            network_text = (acceptance / "NetworkConfig_dds.ini").read_text(
                encoding="utf-8", errors="replace")
            if "[DdsProtocol]" not in network_text or "[UDP]" in network_text or "[TCP]" in network_text:
                row["errors"].append("stimulus network config is not DDS-only")
            runtime_text = (acceptance / "board" / "runtime_environment.txt").read_text(
                encoding="utf-8", errors="replace")
            if "TcpSendVideo=false" not in runtime_text or "HwaSimIRDdsVideoEnable=true" not in runtime_text:
                row["errors"].append("board output transport gate is not DDS-only")
            for key, value in (band_request.get("board_environment") or {}).items():
                if "%s=%s" % (key, value) not in runtime_text.splitlines():
                    row["errors"].append("board environment mismatch: %s" % key)
            expected_agc = "false" if variant == "fixed" else "true"
            expected_annotation = "true" if variant == "annotated" else "false"
            if "EnableAGC=" + expected_agc not in runtime_text or \
                    "AnnotationOverlayInSensorImage=" + expected_annotation not in runtime_text:
                row["errors"].append("variant environment was not applied")

            decoded = acceptance / "received_decode.png"
            width, height = png_size(decoded)
            if (width, height) != (800, 800):
                row["errors"].append("decoded PNG is not 800x800")
            canonical = case_dir / VARIANT_IMAGES[variant]
            shutil.copy2(decoded, canonical)
            annexb = acceptance / "received_annexb.h264"
            with annexb.open("rb") as stream:
                prefix = stream.read(4096)
            if annexb.stat().st_size <= 0 or not (b"\x00\x00\x01" in prefix or
                                                  b"\x00\x00\x00\x01" in prefix):
                row["errors"].append("real DDS receive file is not Annex-B")
            row.update({
                "result": "PASS" if not row["errors"] else "FAIL",
                "canonical_png": str(canonical),
                "canonical_png_sha256": sha256(canonical),
                "received_decode_png": str(decoded),
                "received_decode_png_sha256": sha256(decoded),
                "received_annexb": str(annexb),
                "received_annexb_sha256": sha256(annexb),
                "received_annexb_bytes": annexb.stat().st_size,
                "output_fps": summary.get("output_fps"),
                "frame_counts": summary.get("frame_counts"),
                "latency": summary.get("latency"),
                "acceptance_summary_sha256": sha256(acceptance / "acceptance_summary.json"),
            })
            log_path = acceptance / "board" / "hwa.log"
            agc_evidence = agc_numeric_evidence(log_path)
            if variant != "fixed" and agc_evidence["result"] != "PASS":
                row["errors"].append("AGC numeric evidence is absent or invalid")
                row["result"] = "FAIL"
            row["agc_numeric"] = agc_evidence
            component_variants[variant] = {
                "hwa_log": str(log_path),
                "hwa_log_sha256": sha256(log_path),
                "lines": collect_component_lines(log_path),
            }
            if variant == "fixed":
                fixed_acceptance = acceptance
        except Exception as exc:  # keep partial evidence and report exact failure
            row["errors"].append(str(exc))
            row["result"] = "FAIL"
        if row["errors"]:
            errors.extend(["%s: %s" % (variant, value) for value in row["errors"]])
        variant_rows.append(row)

    raw_document: Dict[str, Any] = {}
    frame_identity: Dict[str, Any] = {}
    if fixed_acceptance is None and "fixed" in requested_variants:
        errors.append("fixed variant is required for raw/fixed mapping evidence")
    elif fixed_acceptance is None:
        raw_document = {
            "schema": "hwasimir.p11.rk3588.dds-raw-radiance.v1",
            "result": "NOT_SELECTED",
            "reason": "fixed variant was not part of this bounded run",
        }
        frame_identity = {"result": "NOT_SELECTED"}
    else:
        try:
            summary = read_json(fixed_acceptance / "acceptance_summary.json")
            capture = select_raw_capture(summary, int(request["deterministic_replay_source_seq"]))
            source_pfm = fixed_acceptance / "board" / "linear" / (
                "raw_radiance_seq%d.pfm" % int(capture["sourceSeq"]))
            raw_pfm = case_dir / "raw_radiance.pfm"
            shutil.copy2(source_pfm, raw_pfm)
            shutil.copy2(fixed_acceptance / "raw_radiance_metrics.csv",
                         case_dir / "raw_radiance_metrics.csv")
            raw_document = {
                "schema": "hwasimir.p11.rk3588.dds-raw-radiance.v1",
                "unit": summary.get("raw_radiance", {}).get("unit"),
                "domain": "spectral_radiance",
                "source_seq": capture["sourceSeq"],
                "pfm": str(raw_pfm),
                "pfm_sha256": sha256(raw_pfm),
                "statistics": capture,
                "fixed_mapping": {
                    "agc": False,
                    "source": "formal Stage6 final sensor fixed-window mapping",
                    "same_deployment_config_manifest": request.get("expected_config_manifest_sha256"),
                },
            }
            if raw_document["unit"] != "W/(m^2_sr_um)":
                errors.append("raw radiance unit is not W/(m^2_sr_um)")
            write_json(case_dir / "raw_radiance.json", raw_document)
            frame_identity = build_frame_identity(case_dir, fixed_acceptance)
            if frame_identity.get("result") != "PASS":
                errors.append("frame identity is not PASS")
        except Exception as exc:
            errors.append("raw/frame identity: %s" % exc)

    physical = {
        "schema": "hwasimir.p11.rk3588.dds-physical-components.v1",
        "unit": "W/(m^2_sr_um)",
        "case_id": request.get("case_id"),
        "band": args.band,
        "variants": component_variants,
    }
    write_json(case_dir / "physical_components.json", physical)

    all_variants = set(requested_variants) == set(VARIANT_IMAGES)
    result = "PASS" if not errors and all_variants else ("INCOMPLETE" if not errors else "FAIL")
    case_summary = {
        "schema": "hwasimir.p11.rk3588.dds-image-case.v1",
        "result": result,
        "case_id": request.get("case_id"),
        "factor": request.get("factor"),
        "comparison_group": request.get("comparison_group"),
        "band": args.band,
        "protocol_band": request.get("protocol_band"),
        "transport": {
            "payload_paths": ["DDS control", "DDS H.264 Annex-B video"],
            "udp_payload_tested": False,
            "tcp_payload_tested": False,
            "dds_domain": 150,
            "video_topic": "HwaSimIR.Video.1001.2.H264",
            "encoder": "RK3588 MPP",
            "decoder": "Windows FFmpeg",
        },
        "deployment": {
            "stage_id": request.get("deployment_stage_id"),
            "elf_sha256": request.get("expected_elf_sha256"),
            "config_manifest_sha256": request.get("expected_config_manifest_sha256"),
        },
        "deterministic_replay_source_seq": request.get("deterministic_replay_source_seq"),
        "raw_radiance": raw_document,
        "frame_identity": frame_identity,
        "variants": variant_rows,
        "errors": errors,
    }
    write_json(case_dir / "case.json", case_summary)

    manifest = case_dir / "artifact_manifest.sha256"
    lines = []
    for path in sorted(value for value in case_dir.rglob("*")
                       if value.is_file() and value != manifest):
        lines.append("%s  %s" % (sha256(path), path.relative_to(case_dir).as_posix()))
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("[P11 RK3588 DDS Image Finalize] result=%s band=%s case=%s output=%s" %
          (result, args.band, request.get("case_id"), case_dir))
    return 0 if result in ("PASS", "INCOMPLETE") else 1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--case-dir", type=Path, required=True)
    parser.add_argument("--band", choices=["SWIR", "MWIR"], required=True)
    parser.add_argument("--variants", default="fixed,agc,annotated")
    return finalize(parser.parse_args())


if __name__ == "__main__":
    raise SystemExit(main())
