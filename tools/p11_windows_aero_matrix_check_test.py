#!/usr/bin/env python3
"""Contract tests for the read-only P11 Windows aero matrix gate."""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np

import p11_windows_aero_matrix_check as gate


STATIC_SPEEDS = {
    "target_speed_static": 0.0,
    "speed_static_15mps": 15.0,
    "target_speed_25mps": 25.0,
    "speed_static_30mps": 30.0,
}


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")


def write_pfm(path: Path, values: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as stream:
        stream.write(f"PF\n{values.shape[1]} {values.shape[0]}\n-1.0\n".encode("ascii"))
        stream.write(values.astype("<f4").tobytes())


def file_hash(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class AeroMatrixFixture:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.matrix_path = root / "matrix.json"
        scenarios = [
            {"id": case_id, "factor": "target_speed", "targetSpeedMps": speed}
            for case_id, speed in STATIC_SPEEDS.items()
        ]
        scenarios.append({"id": gate.DYNAMIC_CASE, "factor": "target_speed", "targetSpeedMps": 0, "dynamic": True})
        write_json(self.matrix_path, {"bands": [{"name": "SWIR"}, {"name": "MWIR"}], "scenarios": scenarios})
        for band in gate.BANDS:
            for case_id in gate.SPEED_CASES:
                self._make_case(band, case_id)

    @staticmethod
    def identities() -> list[dict[str, object]]:
        return [
            {"role": role, "path": f"C:/formal/{role}.exe", "bytes": 1000 + index, "sha256": f"{index + 1:064x}"}
            for index, role in enumerate(sorted(gate.PROGRAM_ROLES))
        ]

    def _make_case(self, band: str, case_id: str) -> None:
        case_dir = self.root / band / case_id
        dynamic = case_id == gate.DYNAMIC_CASE
        speed = STATIC_SPEEDS.get(case_id, 0.0)
        fixture: dict[str, object] = {
            "Schema": "ordinary_weather_camera_1",
            "SimulationEpochMs": 1,
            "Description": f"synthetic {band} {case_id}",
            "Keyframes": [[0.0, 39.0, 116.0, 500.0, 0.0, 0.0, 0.0]],
            "SyntheticTelemetryTargets": [[85, 3101, 5501, 40.0, 116.0, 500.0, 90.0, 0.0, 0.0, speed * 3.6, 1]],
            "LookAtTargetKey": [85, 3101, 5501],
            "InitializationWeather": {"envVisibility": 23000, "envHumidity": 60},
        }
        if dynamic:
            fixture["SyntheticTelemetryKeyframes"] = [
                [0.0, 40.0, 116.0, 500.0, 0.0, 0.0, 0.0, 0.0, 0],
                [20.0, 40.0, 116.0, 500.0, 0.0, 0.0, 0.0, 54.0, 0],
                [30.0, 40.0, 116.0, 500.0, 0.0, 0.0, 0.0, 108.0, 0],
            ]
        write_json(case_dir / "input_fixture.json", fixture)

        request = {
            "caseId": case_id,
            "band": band,
            "formalProgramIdentities": self.identities(),
            "scenario": {"id": case_id, "targetSpeedMps": speed},
            "deterministicReplaySourceSeq": 1,
        }
        write_json(case_dir / "case_request.json", request)

        raw = np.ones((4, 4, 3), dtype=np.float32)
        if not dynamic and speed > 0.0:
            raw[1:3, 1:3, :] += np.float32(speed * 0.001)
        raw_path = case_dir / "raw_radiance.pfm"
        write_pfm(raw_path, raw)
        write_json(case_dir / "case.json", {
            "result": "PASS",
            "selectedFrame": {"requestedSourceSeq": 1},
            "rawRadiance": {"sha256": file_hash(raw_path)},
        })

        for variant in gate.VARIANTS:
            run_dir = case_dir / "variants" / variant / "formal_tcp_run"
            write_json(run_dir / "phase2a_sync60_save_summary.json", {
                "enableAeroThermalModel": "true",
                "applyAeroToRadiance": "true",
                "aeroApplyOnlyBand": "SWIR_MWIR",
            })
            config = (
                "[Stage5 AeroThermalConfig] EnableAeroThermalModel=1 ApplyAeroToRadiance=1 "
                "AeroApplyOnlyBand=SWIR_MWIR distribution=gpu_local_bounds_normalized wholeBodyHeating=forbidden"
            )
            runtime_rows = []
            speeds = (0.0, 15.0, 25.1) if dynamic else (speed,)
            for index, runtime_speed in enumerate(speeds, 1):
                local = 0.0 if runtime_speed == 0.0 else runtime_speed * 0.001
                runtime_rows.append(
                    "[Stage5 AeroThermal] "
                    f"sourceSeq={index} speedMps={runtime_speed} valid=1 fallbackReason=none "
                    "aeroDistribution=gpu_local_bounds_normalized wholeBodyAeroDeltaK=0 "
                    "bodyAeroDeltaKEffective=0 aeroAppliedToRadiance=1 "
                    f"noseAeroDeltaKEffective={local} edgeAeroDeltaKEffective={local} rearAeroDeltaKEffective={local}"
                )
            (run_dir / "hwa.out.log").write_text(config + "\n" + "\n".join(runtime_rows) + "\n", encoding="utf-8")

        recording = case_dir / "variants/fixed/receiver_recording"
        recording.mkdir(parents=True, exist_ok=True)
        annotation = {
            "sourceSeq": 1,
            "frameSeq": 1,
            "targets": [{
                "targetType": 85,
                "targetPlatID": 3101,
                "targetID": 5501,
                "bboxCorners": [{"x": 1, "y": 1}, {"x": 2, "y": 1}, {"x": 2, "y": 2}, {"x": 1, "y": 2}],
            }],
        }
        payload = json.dumps(annotation, separators=(",", ":")).encode("utf-8")
        (recording / "producer_annotations.jsonl").write_bytes(payload)
        frame_index = {
            "sourceSeq": 1,
            "frameSeq": 1,
            "annotationBodyOffset": "0",
            "annotationBodyBytes": len(payload),
            "annotationBodySha256": hashlib.sha256(payload).hexdigest(),
        }
        (recording / "frame_index.jsonl").write_text(json.dumps(frame_index, separators=(",", ":")) + "\n", encoding="utf-8")

    def check(self) -> dict[str, object]:
        return gate.check(self.root, self.matrix_path)

    def rewrite_raw(self, band: str, case_id: str, raw: np.ndarray) -> None:
        case_dir = self.root / band / case_id
        raw_path = case_dir / "raw_radiance.pfm"
        write_pfm(raw_path, raw)
        case_summary = json.loads((case_dir / "case.json").read_text(encoding="utf-8"))
        case_summary["rawRadiance"]["sha256"] = file_hash(raw_path)
        write_json(case_dir / "case.json", case_summary)


class AeroMatrixGateTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.fixture = AeroMatrixFixture(Path(self.temp.name))

    def tearDown(self) -> None:
        self.temp.cleanup()

    def assert_failed_with(self, needle: str) -> None:
        result = self.fixture.check()
        self.assertEqual("FAIL", result["result"])
        self.assertTrue(any(needle in error for error in result["errors"]), result["errors"])

    def test_passes_complete_contract(self) -> None:
        result = self.fixture.check()
        self.assertEqual("PASS", result["result"], result["errors"])

    def test_cli_output_file_matches_stdout(self) -> None:
        output = self.fixture.root / "aero_matrix_check.json"
        completed = subprocess.run(
            [
                sys.executable,
                str(Path(gate.__file__).resolve()),
                str(self.fixture.root),
                "--matrix",
                str(self.fixture.matrix_path),
                "--output",
                str(output),
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(0, completed.returncode, completed.stderr)
        self.assertTrue(output.is_file())
        self.assertEqual(json.loads(completed.stdout), json.loads(output.read_text(encoding="utf-8")))
        self.assertEqual("PASS", json.loads(completed.stdout)["result"])

    def test_rejects_apply_aero_false(self) -> None:
        path = self.fixture.root / "SWIR/speed_static_15mps/variants/fixed/formal_tcp_run/phase2a_sync60_save_summary.json"
        summary = json.loads(path.read_text(encoding="utf-8"))
        summary["applyAeroToRadiance"] = "false"
        write_json(path, summary)
        self.assert_failed_with("summary_applyAeroToRadiance_not_true")

    def test_rejects_band_scope_other_than_swir_mwir(self) -> None:
        path = self.fixture.root / "MWIR/target_speed_25mps/variants/agc/formal_tcp_run/hwa.out.log"
        path.write_text(path.read_text(encoding="utf-8").replace("AeroApplyOnlyBand=SWIR_MWIR", "AeroApplyOnlyBand=MWIR"), encoding="utf-8")
        self.assert_failed_with("config_AeroApplyOnlyBand")

    def test_rejects_whole_body_heating(self) -> None:
        path = self.fixture.root / "SWIR/speed_static_30mps/variants/annotated/formal_tcp_run/hwa.out.log"
        path.write_text(path.read_text(encoding="utf-8").replace("wholeBodyAeroDeltaK=0", "wholeBodyAeroDeltaK=0.1"), encoding="utf-8")
        self.assert_failed_with("wholeBodyAeroDeltaK_not_zero")

    def test_rejects_zero_local_effective_at_nonzero_speed(self) -> None:
        path = self.fixture.root / "MWIR/speed_static_15mps/variants/fixed/formal_tcp_run/hwa.out.log"
        text = path.read_text(encoding="utf-8")
        text = text.replace("noseAeroDeltaKEffective=0.015", "noseAeroDeltaKEffective=0")
        text = text.replace("edgeAeroDeltaKEffective=0.015", "edgeAeroDeltaKEffective=0")
        text = text.replace("rearAeroDeltaKEffective=0.015", "rearAeroDeltaKEffective=0")
        path.write_text(text, encoding="utf-8")
        self.assert_failed_with("nonzero_speed_local_effective_not_positive")

    def test_rejects_unchanged_target_roi_raw(self) -> None:
        self.fixture.rewrite_raw("SWIR", "speed_static_15mps", np.ones((4, 4, 3), dtype=np.float32))
        self.assert_failed_with("target_roi_raw_not_increased_from_zero_speed")

    def test_rejects_camera_fixture_confound(self) -> None:
        path = self.fixture.root / "MWIR/speed_static_30mps/input_fixture.json"
        fixture = json.loads(path.read_text(encoding="utf-8"))
        fixture["Keyframes"][0][1] += 0.001
        write_json(path, fixture)
        self.assert_failed_with("static_fixture_not_identical_after_speed_normalization")

    def test_rejects_missing_binary_hash_binding(self) -> None:
        path = self.fixture.root / "SWIR/target_speed_static/case_request.json"
        request = json.loads(path.read_text(encoding="utf-8"))
        request.pop("formalProgramIdentities")
        write_json(path, request)
        self.assert_failed_with("formal_program_identities_missing")


if __name__ == "__main__":
    unittest.main(verbosity=2)
