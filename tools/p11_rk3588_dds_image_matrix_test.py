#!/usr/bin/env python3
"""Offline contract tests for the RK3588 P11 DDS-only image harness."""

from __future__ import annotations

import copy
import importlib.util
import json
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
MATRIX_PATH = TOOLS / "p11_windows_evidence_matrix.json"


def load_checker():
    path = TOOLS / "p11_rk3588_dds_image_matrix_check.py"
    spec = importlib.util.spec_from_file_location("dds_image_check", path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class Args:
    matrix = MATRIX_PATH
    band = "ALL"
    case = "Representative"
    variant = "ALL"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    checks = 0
    checker = load_checker()
    result = checker.check(Args())
    require(result["result"] == "PASS", "representative quick-check must pass")
    checks += 1
    require(result["selection"]["runnable_runs"] == 60,
            "representative profile must expand to 10 cases x 2 bands x 3 variants")
    checks += 1
    require(not result["representative_profile"]["missing_factors"],
            "all requested image factors must be represented")
    checks += 1
    require(len(result["blocked_coverage"]) == 3,
            "the three honest blockers must remain explicit")
    checks += 1

    matrix = json.loads(MATRIX_PATH.read_text(encoding="utf-8-sig"))
    with tempfile.TemporaryDirectory(prefix="p11-dds-image-test-") as temporary:
        temp = Path(temporary)
        bad = copy.deepcopy(matrix)
        bad["blockedCoverage"] = bad["blockedCoverage"][:-1]
        bad_path = temp / "bad_blockers.json"
        bad_path.write_text(json.dumps(bad), encoding="utf-8")
        bad_args = Args()
        bad_args.matrix = bad_path
        bad_result = checker.check(bad_args)
        require(bad_result["result"] == "FAIL" and
                any("blockers changed" in value for value in bad_result["errors"]),
                "removing a blocker must fail closed")
        checks += 1

        bad = copy.deepcopy(matrix)
        bad["scenarios"] = [row for row in bad["scenarios"]
                            if row["id"] != "active_in_band_on"]
        bad_path = temp / "bad_representative.json"
        bad_path.write_text(json.dumps(bad), encoding="utf-8")
        bad_args.matrix = bad_path
        bad_result = checker.check(bad_args)
        require(bad_result["result"] == "FAIL",
                "missing a representative scenario must fail closed")
        checks += 1

    acceptance = (TOOLS / "p11_rk3588_band_acceptance.ps1").read_text(encoding="utf-8")
    board = (TOOLS / "p11_rk3588_band_board_run.sh").read_text(encoding="utf-8")
    harness = (TOOLS / "p11_rk3588_dds_image_matrix.ps1").read_text(encoding="utf-8")
    finalizer = (TOOLS / "p11_rk3588_dds_image_finalize.py").read_text(encoding="utf-8")

    require("--control-transport=dds" in acceptance and
            "--receive-transport=dds" in acceptance,
            "both stimulus and real receiver must be pinned to DDS")
    checks += 1
    require("'[UDP]'" not in acceptance and "'[TCP]'" not in acceptance,
            "acceptance must not generate UDP/TCP configuration sections")
    checks += 1
    require("TcpSendVideo=false" in board and "HwaSimIRDdsVideoEnable=true" in board,
            "board output must disable TCP and enable DDS")
    checks += 1
    require("scenario_environment_file=${9:-}" in board and
            "unsupported_scenario_environment_key" in board and
            "Do not source the file" in board,
            "board override file must be optional, hashed, parsed and allowlisted")
    checks += 1
    require("|ApplyAero|" in board,
            "board runtime evidence must record ApplyAeroToRadiance")
    checks += 1
    require("original eight-argument board runner invocation" in acceptance and
            "if ($scenarioOverridesRequested)" in acceptance,
            "ordinary band acceptance invocation must retain its prior runner contract")
    checks += 1
    require("ValidateSet('All', 'fixed', 'agc', 'annotated')" in harness and
            "ValidateRange(10, 3600)" in harness and
            "$Case = 'Representative'" in harness,
            "matrix selection must be bounded and explicit")
    checks += 1
    require("udp_payload_tested = $false" in harness and
            "tcp_payload_tested = $false" in harness and
            "control = 'DDS'; video = 'DDS'" in harness,
            "matrix evidence contract must state DDS-only payload testing")
    checks += 1
    require("fixed_clean.png" in finalizer and "auto_clean.png" in finalizer and
            "annotated.png" in finalizer and "raw_radiance.pfm" in finalizer,
            "required canonical image/raw artifacts must be finalized")
    checks += 1
    require('"[UDP]" in network_text or "[TCP]" in network_text' in finalizer and
            '"TcpSendVideo=false" not in runtime_text' in finalizer,
            "finalizer must fail closed if a payload path is not DDS-only")
    checks += 1
    require("frame_identity.csv" in finalizer and "digest_identity" in finalizer and
            "physical_components.json" in finalizer,
            "frame identity and physical component evidence must be emitted")
    checks += 1
    require("hwasimir.p11.rk3588.dds-image-matrix-summary.v1" in harness and
            "deployment_stage_id" in harness and "config_manifest_sha256" in harness and
            "program_identities" in harness,
            "root summary must bind the exact deployment")
    checks += 1

    print("[P11 RK3588 DDS Image Matrix Test] result=PASS checks=%d" % checks)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
