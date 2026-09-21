#!/usr/bin/env python3
"""Check the independent P14 first-valid-position DDS fixture evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_log(path: Path) -> str:
    payload = path.read_bytes()
    if payload.startswith((b"\xff\xfe", b"\xfe\xff")):
        return payload.decode("utf-16", errors="replace")
    if payload[:4096].count(b"\x00") > 16:
        return payload.decode("utf-16-le", errors="replace")
    return payload.decode("utf-8", errors="replace")


def require(condition: bool, name: str, checks: list[dict]) -> None:
    checks.append({"check": name, "passed": bool(condition)})


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sender", type=Path, required=True)
    parser.add_argument("--board", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    sender = read_log(args.sender)
    board = read_log(args.board)
    checks: list[dict] = []

    require(
        "[P14FixtureSummary] scenarios=8 fileReads=0 inputGenerated=1 "
        "strictIdentity=1001/2 resolution=800x800 wireLayoutChanged=0 "
        "result=INPUT_COMPLETE" in sender,
        "fixture_generated_without_1_txt",
        checks,
    )
    require(
        "[P14 AtmosphereIdentity] status=PASS "
        "schema=HwaSimIR.P14.SharedAtmosphereCoverage.1" in board
        and "originalInputBusinessDependency=0" in board,
        "p14_shared_atmosphere_identity_external_sender_independent",
        checks,
    )

    expected = {
        2: (1000.0, "30.010,110.010,1.000", 1),
        4: (1500.0, "30.020,110.020,1.000", 1),
        6: (2000.0, "30.030,110.030,1.000", 1),
        8: (3000.0, "30.040,110.040,1.000", 0),
        10: (4000.0, "30.050,110.050,1.000", 0),
        12: (5000.0, "30.060,110.060,1.000", 1),
        14: (7000.0, "30.070,110.070,1.000", 1),
        16: (6000.0, "0.000,0.000,0.000", 0),
    }
    commits = re.findall(r"^\[FormalReferenceCommit\].*$", board, re.MULTILINE)
    require(len(commits) == 8, "exactly_one_commit_per_eight_generations", checks)
    commit_by_generation: dict[int, str] = {}
    for line in commits:
        match = re.search(r"referenceGeneration=(\d+)", line)
        if match:
            commit_by_generation[int(match.group(1))] = line
    require(set(commit_by_generation) == set(expected), "commit_generations_exact", checks)

    for generation, (time_ms, platform, view_valid) in expected.items():
        line = commit_by_generation.get(generation, "")
        require(
            f"commitCount=1" in line
            and f"sourceTimeMs={time_ms:.3f}" in line
            and f"platform={platform}" in line
            and f"inputViewValid={view_valid}" in line
            and "viewValidUsedForPosition=0" in line
            and "initTemporaryDifferent=1" in line
            and "beforeFirstDraw=1" in line,
            f"generation_{generation}_commit_contract",
            checks,
        )

    starts = {
        int(match.group(1)): match.start()
        for match in re.finditer(
            r"^\[FormalReferenceState\] event=start referenceGeneration=(\d+) "
            r"committed=0 sourceSeq=0 action=wait_first_valid_realtime$",
            board,
            re.MULTILINE,
        )
    }
    require(set(starts) == set(expected), "all_starts_wait_for_first_valid_realtime", checks)
    for generation in expected:
        commit_pos = board.find(commit_by_generation.get(generation, ""))
        start_pos = starts.get(generation, -1)
        between = board[start_pos:commit_pos] if start_pos >= 0 and commit_pos >= 0 else ""
        require(
            "[DdsFrameProducts]" not in between
            and "[H264EncodeSuccess]" not in between,
            f"generation_{generation}_no_frame_before_commit",
            checks,
        )

    require(
        re.search(
            r"\[RealtimeValidity\].*referenceGeneration=6 accepted=0 "
            r"reason=legacy_all_zero_placeholder.*outputFramesCreated=0 "
            r"formalReferenceChanged=0",
            board,
        )
        is not None,
        "placeholder_before_valid_rejected_separately",
        checks,
    )
    require(
        re.search(
            r"\[RealtimeValidity\].*referenceGeneration=12 accepted=0 "
            r"reason=legacy_all_zero_placeholder.*rejectedPlaceholder=1",
            board,
        )
        is not None
        and re.search(
            r"\[RealtimeValidity\].*referenceGeneration=12 accepted=0 "
            r"reason=platform_latitude_out_of_range.*rejectedStructure=1",
            board,
        )
        is not None
        and re.search(
            r"\[RealtimeValidity\].*referenceGeneration=12 accepted=0 "
            r"reason=non_finite_platform_state.*rejectedStructure=2",
            board,
        )
        is not None
        and re.search(
            r"\[ProtocolRoute\] transport=dds type=realtime accepted=0 .*"
            r"packetPlatID=999 packetSensorID=2 reason=plat_mismatch",
            board,
        )
        is not None,
        "invalid_after_valid_rejected_without_rebase_or_route_leak",
        checks,
    )
    require(
        re.search(
            r"\[FormalReferenceState\] event=stop referenceGeneration=12 "
            r"committed=1 sourceSeq=1 acceptedRealtime=2 "
            r"rejectedPlaceholder=1 rejectedStructure=2",
            board,
        )
        is not None,
        "valid_after_invalid_retains_original_reference",
        checks,
    )
    require(
        "sourceSeq=2 targetKey=P11-CIVIL-VAN#plat3#target3" in board,
        "target_late_resolves_full_protocol_key",
        checks,
    )
    require(
        re.search(
            r"\[FormalReferenceState\] event=stop referenceGeneration=10 "
            r"committed=1 sourceSeq=1 acceptedRealtime=2",
            board,
        )
        is not None,
        "viewvalid_zero_row_accepted_then_viewvalid_one",
        checks,
    )
    require(
        "[FormalReferenceCommit] referenceGeneration=16 commitCount=1 "
        "sourceSeq=1 sourceTimeMs=6000.000 platform=0.000,0.000,0.000" in board,
        "legal_zero_geography_not_placeholder",
        checks,
    )
    require(
        "scenario=zero_init_then_valid event=init_prewarm lat=0 lon=0 altM=0 "
        "expectedReceiver=temporary_only_not_formal" in sender,
        "all_zero_init_is_temporary_only",
        checks,
    )
    require(
        re.search(
            r"\[FormalReferenceState\] event=start referenceGeneration=14 "
            r"committed=1 sourceSeq=1 action=retain_same_generation",
            board,
        )
        is not None
        and len(
            re.findall(
                r"\[FormalReferenceCommit\] referenceGeneration=14 ", board
            )
        )
        == 1
        and "scenario=stop_start_same_generation event=same_generation_restart "
        "expectedReceiver=reference_already_committed" in sender,
        "stop_start_same_generation_retains_single_commit",
        checks,
    )

    status = "PASS" if all(item["passed"] for item in checks) else "FAIL"
    result = {
        "schema": "HwaSimIR.P14.FirstValidFixtureCheck.1",
        "status": status,
        "fixtureReads1Txt": False,
        "wireLayoutChanged": False,
        "identity": {"platID": 1001, "sensorID": 2, "width": 800, "height": 800},
        "senderLog": {"path": str(args.sender), "sha256": sha256(args.sender)},
        "boardLog": {"path": str(args.board), "sha256": sha256(args.board)},
        "commitGenerations": sorted(commit_by_generation),
        "checks": checks,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
