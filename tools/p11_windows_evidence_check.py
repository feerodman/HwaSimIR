#!/usr/bin/env python3
"""Read-only validation and expansion of the P11 Windows evidence matrix."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REQUIRED_FACTORS = {
    "target", "exhaust_plume", "cloud", "rain", "snow", "visibility",
    "active_illumination", "solar_azimuth_elevation", "target_altitude",
    "target_speed", "combination",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--matrix", type=Path, default=ROOT / "tools/p11_windows_evidence_matrix.json")
    parser.add_argument("--mode", choices=("dry-run", "quick-check"), default="quick-check")
    parser.add_argument("--band", choices=("ALL", "SWIR", "MWIR"), default="ALL")
    parser.add_argument("--case", default="*")
    parser.add_argument("--variant", choices=("ALL", "fixed", "agc", "annotated"), default="ALL")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    matrix_path = args.matrix.resolve()
    matrix = json.loads(matrix_path.read_text(encoding="utf-8"))
    errors: list[str] = []
    warnings: list[str] = []

    if matrix.get("schema") != "hwasimir_p11_windows_evidence_matrix_1":
        errors.append("matrix_schema")
    serialized = json.dumps(matrix, ensure_ascii=False).lower()
    if "ordinary_sensor_lab" in serialized:
        errors.append("forbidden_ordinary_sensor_lab_reference")

    bands = {item.get("name"): item for item in matrix.get("bands", [])}
    if set(bands) != {"SWIR", "MWIR"}:
        errors.append("band_set")
    if bands.get("SWIR", {}).get("protocolValue") != 0:
        errors.append("swir_protocol_mapping_must_be_0")
    if bands.get("MWIR", {}).get("protocolValue") != 2:
        errors.append("mwir_protocol_mapping_must_be_2")
    if bands.get("SWIR", {}).get("rangeUm") != [1.1, 2.5]:
        errors.append("swir_range")
    if bands.get("MWIR", {}).get("rangeUm") != [3.0, 5.0]:
        errors.append("mwir_range")

    variants = {item.get("name"): item for item in matrix.get("captureVariants", [])}
    if set(variants) != {"fixed", "agc", "annotated"}:
        errors.append("capture_variants")
    else:
        expected = {
            "fixed": (False, False, "fixed_clean.png"),
            "agc": (True, False, "auto_clean.png"),
            "annotated": (True, True, "annotated.png"),
        }
        for name, values in expected.items():
            actual = (variants[name].get("agc"), variants[name].get("annotation"), variants[name].get("canonicalPng"))
            if actual != values:
                errors.append(f"variant_semantics:{name}")

    scenarios = matrix.get("scenarios", [])
    ids = [item.get("id") for item in scenarios]
    if len(ids) != len(set(ids)):
        errors.append("duplicate_case_id")
    missing_factors = sorted(REQUIRED_FACTORS - {item.get("factor") for item in scenarios})
    if missing_factors:
        errors.append("missing_factors:" + ",".join(missing_factors))
    for item in scenarios:
        if item.get("runnable") is False and not item.get("blockedReason"):
            errors.append(f"blocked_without_reason:{item.get('id')}")
        if not 0.05 <= float(item.get("rangeKm", 0)) <= 2.0:
            errors.append(f"non_close_range:{item.get('id')}")
        fixture_relative = item.get("fixture", matrix.get("baseFixture", ""))
        fixture_path = (ROOT / str(fixture_relative)).resolve()
        if not fixture_path.is_file():
            errors.append(f"scenario_fixture_missing:{item.get('id')}:{fixture_relative}")
            continue
        try:
            fixture_data = json.loads(fixture_path.read_text(encoding="utf-8-sig"))
            if fixture_data.get("Schema") != "ordinary_weather_camera_1":
                errors.append(f"scenario_fixture_schema:{item.get('id')}")
            camera_rows = fixture_data.get("Keyframes", [])
            if not camera_rows or any(
                not isinstance(row, list) or len(row) != 7
                or any(isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) for value in row)
                for row in camera_rows
            ):
                errors.append(f"scenario_fixture_camera_rows:{item.get('id')}")
            telemetry_rows = fixture_data.get("SyntheticTelemetryTargets", [])
            if any(not isinstance(row, list) or len(row) != 11 for row in telemetry_rows):
                errors.append(f"scenario_fixture_target_rows:{item.get('id')}")
            state_rows = fixture_data.get("SyntheticTelemetryKeyframes", [])
            if state_rows:
                if len(telemetry_rows) != 1:
                    errors.append(f"scenario_fixture_dynamic_target_count:{item.get('id')}")
                previous_time = -1.0
                for index, row in enumerate(state_rows):
                    if (
                        not isinstance(row, list) or len(row) != 9
                        or any(isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) for value in row)
                    ):
                        errors.append(f"scenario_fixture_dynamic_row:{item.get('id')}:{index}")
                        continue
                    if float(row[0]) <= previous_time or row[8] not in (0, 1):
                        errors.append(f"scenario_fixture_dynamic_order_or_engine:{item.get('id')}:{index}")
                    previous_time = float(row[0])
                if state_rows and float(state_rows[0][0]) != 0.0:
                    errors.append(f"scenario_fixture_dynamic_start:{item.get('id')}")
            if item.get("dynamic") is True and not state_rows:
                errors.append(f"dynamic_scenario_without_state_rows:{item.get('id')}")
        except (OSError, json.JSONDecodeError, TypeError, ValueError) as exc:
            errors.append(f"scenario_fixture_invalid:{item.get('id')}:{exc}")

    blocked_coverage = matrix.get("blockedCoverage", [])
    blocked_by_id = {item.get("id"): item for item in blocked_coverage}
    for blocked_id in ("cloud_target_front_behind", "cloud_individual_id_disable"):
        item = blocked_by_id.get(blocked_id, {})
        if item.get("status") != "BLOCKED_METADATA" or not item.get("reason"):
            errors.append(f"cloud_metadata_gap_must_remain_explicit:{blocked_id}")
        if any(scenario.get("id") == blocked_id and scenario.get("runnable") for scenario in scenarios):
            errors.append(f"cloud_metadata_gap_cannot_be_runnable:{blocked_id}")
    range_2km = next((item for item in scenarios if item.get("id") == "target_range_2km"), None)
    if (
        range_2km is None
        or range_2km.get("runnable") is not False
        or range_2km.get("expectedFormalState") != "fail_closed_out_of_range"
        or range_2km.get("expectedFallbackAxis") != "rangeKm"
        or float(range_2km.get("auditedRangeMaxKm", math.nan)) != 1.0
    ):
        errors.append("target_range_2km_must_be_blocked_fail_closed_out_of_range")
    for visibility_id in ("visibility_6km", "visibility_23km"):
        item = next((scenario for scenario in scenarios if scenario.get("id") == visibility_id), None)
        if item is None or float(item.get("rangeKm", math.nan)) > 1.0:
            errors.append(f"visibility_case_outside_audited_range:{visibility_id}")

    fixture = (ROOT / matrix.get("baseFixture", "")).resolve()
    required = {
        "matrix": matrix_path,
        "base_fixture": fixture,
        "renderer": ROOT / "HwaSim_IR/Bin/HwaSim_IR.exe",
        "receiver": ROOT / "HwaSim_IR_VideoDisplay/x64/Release/HwaSim_IR_VideoDisplay.exe",
        "stimulus": ROOT / "build-DataDrivenTestQT-codex-mingw73_64-Release/release/DataDrivenTestQT.exe",
        "phase2a_runner": ROOT / "tools/phase2a_sync60_save_smoke.ps1",
        "ffmpeg": ROOT / ".deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe",
        "civil_van_bam": ROOT / "HwaSim_IR/Bin/Config/TargetLib/p11/civil_van/p11_civil_van.bam",
        "civil_van_manifest": ROOT / "HwaSim_IR/Bin/Config/TargetLib/p11/civil_van/manifest.json",
        "renderer_band_mapping": ROOT / "HwaSim_IR/HwaSim_IR/IR/IRTypes.cpp",
        "renderer_band_gate": ROOT / "HwaSim_IR/HwaSim_IR/IR/IRConfig.cpp",
        "renderer_main": ROOT / "HwaSim_IR/HwaSim_IR/HwaSimIR.cpp",
        "stimulus_cli": ROOT / "DataDrivenTestQT/main.cpp",
        "stimulus_band_ui": ROOT / "DataDrivenTestQT/p7_sensor_form.inl",
        "runtime_config": ROOT / "HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini",
        "profile_NIR": ROOT / "HwaSim_IR/Bin/Config/SensorWave/default_NVG.json",
        "evidence_finalizer": ROOT / "tools/p11_windows_evidence_finalize.py",
    }
    hashes = {}
    for label, path in required.items():
        if not path.is_file():
            errors.append(f"missing:{label}:{path}")
        else:
            hashes[label] = {"path": str(path), "sha256": sha256(path), "bytes": path.stat().st_size}

    protocol_contract: dict[str, object] = {}
    band_source = required["renderer_band_mapping"].read_text(encoding="utf-8", errors="replace")
    mapping_match = re.search(
        r"IRBand\s+IRBandFromProtocol\s*\([^)]*\)\s*\{(.*?)\n\}",
        band_source, flags=re.DOTALL,
    )
    protocol_mapping = {}
    if mapping_match:
        protocol_mapping = {
            int(value): name
            for value, name in re.findall(
                r"case\s+(-?\d+)\s*:\s*return\s+IRBand::([A-Za-z]+)",
                mapping_match.group(1),
            )
        }
    expected_mapping = {0: "ShortWaveInfrared", 1: "NearInfrared", 2: "MidWaveInfrared"}
    if any(protocol_mapping.get(value) != name for value, name in expected_mapping.items()):
        errors.append("production_protocol_mapping_source_mismatch")
    if protocol_mapping.get(4) != "Visible":
        errors.append("protocol_4_must_remain_visible_not_vis_swir")

    profile_match = re.search(
        r"const char\*\s+IRSensorProfileFileName\s*\([^)]*\)\s*\{(.*?)\n\}",
        band_source, flags=re.DOTALL,
    )
    profile_mapping = {}
    if profile_match:
        profile_mapping = {
            band: filename
            for band, filename in re.findall(
                r"case\s+IRBand::([A-Za-z]+)\s*:\s*return\s+\"([^\"]+)\"",
                profile_match.group(1),
            )
        }
    expected_profiles = {
        "ShortWaveInfrared": "default_SWIR.json",
        "NearInfrared": "default_NVG.json",
        "MidWaveInfrared": "default_MWIR.json",
        "Visible": "default_LLLTV.json",
    }
    if any(profile_mapping.get(name) != filename for name, filename in expected_profiles.items()):
        errors.append("production_profile_filename_mapping_mismatch")
    if "default_VIS-SWIR.json" in profile_mapping.values():
        errors.append("vis_swir_must_not_have_protocol_mapping")

    gate_source = required["renderer_band_gate"].read_text(encoding="utf-8", errors="replace")
    compact_gate = re.sub(r"\s+", "", gate_source)
    if "if(b!=0&&b!=1&&b!=2)returnfalse;" not in compact_gate:
        errors.append("production_band_allowlist_gate_missing")

    cli_source = required["stimulus_cli"].read_text(encoding="utf-8", errors="replace")
    cli_match = re.search(
        r"const QString sensorBandPrefix.*?(?=const QString sensorPixelAnglePrefix)",
        cli_source, flags=re.DOTALL,
    )
    cli_block = cli_match.group(0) if cli_match else ""
    cli_requirements = (
        "toInt(&ok, 10)",
        "QString::number(requested) != sensorBandText",
        "requested < 0 || requested > 2",
        "return 64;",
        "VIS-SWIR are unsupported",
    )
    if not cli_block or any(token not in cli_block for token in cli_requirements) or "qBound" in cli_block:
        errors.append("stimulus_sensor_band_cli_not_strict_0_1_2")

    ui_source = required["stimulus_band_ui"].read_text(encoding="utf-8", errors="replace")
    ui_items = {
        int(value): statement
        for statement, value in re.findall(
            r"(box->addItem\(.*?,\s*([0-4])\s*\);)", ui_source
        )
    }
    for value, label in ((0, "SWIR"), (1, "NIR"), (2, "MWIR")):
        if label not in ui_items.get(value, ""):
            errors.append(f"stimulus_ui_protocol_mapping:{value}:{label}")
    for value in (3, 4):
        if "unsupported" not in ui_items.get(value, ""):
            errors.append(f"stimulus_ui_unsupported_label_missing:{value}")
    if "not VIS-SWIR" not in ui_items.get(4, ""):
        errors.append("stimulus_ui_vis_swir_disclaimer_missing")
    newest_stimulus_contract_source = max(
        required["stimulus_cli"].stat().st_mtime_ns,
        required["stimulus_band_ui"].stat().st_mtime_ns,
    )
    if required["stimulus"].stat().st_mtime_ns < newest_stimulus_contract_source:
        errors.append("stimulus_binary_older_than_band_contract_source")

    runtime_text = required["runtime_config"].read_text(encoding="utf-8-sig", errors="replace")
    runtime_policy = re.findall(r"(?m)^\s*AsyncInputPolicy\s*=\s*([^;#\r\n]+)", runtime_text)
    if not runtime_policy or runtime_policy[-1].strip() != "OrderedQueue":
        errors.append("runtime_async_input_policy_not_ordered_queue")

    phase2a_text = required["phase2a_runner"].read_text(encoding="utf-8-sig", errors="replace")
    if not re.search(r"\[int\]\$StimSimMode\s*=\s*1\b", phase2a_text):
        errors.append("phase2a_formal_default_not_sync")
    runner_text_contract = (ROOT / "tools/p11_windows_evidence.ps1").read_text(
        encoding="utf-8-sig", errors="replace"
    )
    if "transport='TCP'" not in runner_text_contract:
        errors.append("p11_formal_transport_not_tcp")
    # Keep the runner/phase2a splat contract executable.  Stage6 diagnostics is
    # deliberately an inherited process environment alias, not a phase2a
    # parameter, and is checked separately below.
    invoke_parameters = {
        "Seconds", "PostStimulusWaitSeconds", "StimSensorBand", "StimUtcHour",
        "StimExtraArgs", "EnablePerfLog", "Stage5LogComponents",
        "Stage5ComponentLogEveryFrames", "M1CompareOnly", "M1EnableRuntime",
        "M1EnableNIRRuntime", "M1EnableSWIRRuntime", "M1EnableMWIRRuntime",
        "NaturalSolarEnable", "NaturalSolarEnableOpticalShadow",
        "NaturalSolarEnableSolarThermal", "NaturalSolarDebugLog",
        "M1SolarOverrideEnable", "M1SolarOverrideAzimuthDeg",
        "M1SolarOverrideElevationDeg", "M1SunVisibility",
        "ActiveIlluminatorEnable", "ActiveIlluminatorBand",
        "ActiveIlluminatorCenterWavelengthUm", "ActiveIlluminatorBandwidthUm",
        "ActiveIlluminatorIntensityMode", "ActiveIlluminatorDebugLog",
        "EnableAGC", "AGCDebugLog", "AnnotationOverlayInSensorImage",
        "PostprocessAA",
    }
    phase2a_param_block = phase2a_text.split("$ErrorActionPreference", 1)[0]
    phase2a_parameters = set(re.findall(r"\$([A-Za-z][A-Za-z0-9_]*)", phase2a_param_block))
    missing_invoke_parameters = sorted(invoke_parameters - phase2a_parameters)
    if missing_invoke_parameters:
        errors.append("phase2a_missing_runner_parameters:" + ",".join(missing_invoke_parameters))
    if not re.search(r"\$env:Stage6DiagnosticsEnable\s*=\s*['\"]true['\"]", runner_text_contract):
        errors.append("runner_stage6_diagnostics_environment_not_enabled")
    renderer_main = required["renderer_main"].read_text(encoding="utf-8", errors="replace")
    if '"Stage6DiagnosticsEnable"' not in renderer_main:
        errors.append("renderer_stage6_diagnostics_environment_alias_missing")
    finalizer_source = required["evidence_finalizer"].read_text(encoding="utf-8", errors="replace")
    for token in (
        "validate_formal_chain", "ordered_queue_not_observed", "sync_mode_not_observed",
        "source_sequence_not_continuous", "count_mismatch:", "zero_counter_nonzero:",
    ):
        if token not in finalizer_source:
            errors.append(f"formal_chain_hard_gate_missing:{token}")

    profile_contract = {}
    for protocol, band_name, filename, expected_range in (
        (0, "SWIR", "default_SWIR.json", [1.1, 2.5]),
        (1, "NIR", "default_NVG.json", [0.7, 1.1]),
        (2, "MWIR", "default_MWIR.json", [3.0, 5.0]),
    ):
        path = ROOT / "HwaSim_IR/Bin/Config/SensorWave" / filename
        data = json.loads(path.read_text(encoding="utf-8-sig"))
        sensor = data.get("Systems", {}).get("SensorConfigurationSystem", {})
        observed_range = [
            float(sensor.get("SpectralResponseRangeLow", math.nan)),
            float(sensor.get("SpectralResponseRangeHigh", math.nan)),
        ]
        observed_band = data.get("HwaSimIR", {}).get("Band")
        valid = (
            observed_band == band_name
            and all(abs(actual - expected) <= 1.0e-4 for actual, expected in zip(observed_range, expected_range))
        )
        if not valid:
            errors.append(f"production_profile_contract:{protocol}:{band_name}")
        profile_contract[str(protocol)] = {
            "band": band_name, "profile": filename, "rangeUm": observed_range, "valid": valid,
        }
    protocol_contract = {
        "mapping": {str(key): value for key, value in sorted(protocol_mapping.items())},
        "profileMapping": profile_mapping,
        "profiles": profile_contract,
        "cliAllowedValues": [0, 1, 2],
        "uiItemDataPreserved": sorted(ui_items),
        "runtimeInputPolicy": runtime_policy[-1].strip() if runtime_policy else "missing",
        "formalSimMode": 1,
        "visSwirProtocolSupported": False,
    }

    for name, band in bands.items():
        profile = (ROOT / band["profile"]).resolve()
        if not profile.is_file():
            errors.append(f"missing_profile:{name}")
        else:
            profile_data = json.loads(profile.read_text(encoding="utf-8-sig"))
            profile_text = json.dumps(profile_data)
            if name not in profile_text:
                errors.append(f"wrong_profile_band:{name}")
            hashes[f"profile_{name}"] = {"path": str(profile), "sha256": sha256(profile), "bytes": profile.stat().st_size}

    source = ROOT / "DataDrivenTestQT/mainwindow.cpp"
    if source.is_file():
        text = source.read_text(encoding="utf-8", errors="replace")
        fixed_visibility = bool(re.search(r"envVisibility\s*=\s*6000\s*;", text))
        weather_header = (ROOT / "DataDrivenTestQT/OrdinaryWeatherInput.h").read_text(
            encoding="utf-8", errors="replace"
        )
        runner_text = (ROOT / "tools/p11_windows_evidence.ps1").read_text(
            encoding="utf-8", errors="replace"
        )
        fixture_visibility = (
            '"envVisibility",&BYHWICD::InitObjectTrackingParam::envVisibility' in weather_header
            and "InitializationWeather" in runner_text
            and "expectedVisibilityKm" in runner_text
        )
        blocked = [item for item in scenarios if item.get("id") == "visibility_23km" and not item.get("runnable")]
        if fixed_visibility and not fixture_visibility and not blocked:
            errors.append("visibility_23km_not_blocked_despite_fixed_stimulus")
        if fixed_visibility and not fixture_visibility:
            warnings.append("visibility_23km_blocked_by_formal_stimulus_fixed_6000m")
        if fixture_visibility and blocked:
            errors.append("visibility_23km_still_blocked_despite_fixture_override")
        if "targetSpeedMps*3.6" not in runner_text.replace(" ", ""):
            errors.append("protocol_speed_requires_mps_to_kmh_conversion")
        if "const bool engineState=t.engineState" not in weather_header:
            errors.append("fixture_must_preserve_engine_state_protocol_control")

    selected_bands = [args.band] if args.band != "ALL" else ["SWIR", "MWIR"]
    selected_variants = [args.variant] if args.variant != "ALL" else ["fixed", "agc", "annotated"]
    case_pattern = re.compile("^" + re.escape(args.case).replace(r"\*", ".*") + "$")
    plan = []
    for scenario in scenarios:
        if not case_pattern.match(scenario["id"]):
            continue
        for band_name in selected_bands:
            for variant_name in selected_variants:
                plan.append({
                    "case": scenario["id"],
                    "factor": scenario["factor"],
                    "band": band_name,
                    "protocolBand": bands[band_name]["protocolValue"],
                    "variant": variant_name,
                    "runnable": bool(scenario.get("runnable")),
                    "blockedReason": scenario.get("blockedReason", ""),
                    "transport": "TCP",
                    "programs": ["HwaSim_IR", "DataDrivenTestQT", "HwaSim_IR_VideoDisplay"],
                })
    if not plan:
        errors.append("empty_selection")

    result = {
        "schema": "hwasimir_p11_windows_evidence_check_1",
        "result": "PASS" if not errors else "FAIL",
        "mode": args.mode,
        "readOnly": True,
        "matrix": str(matrix_path),
        "selectedRuns": len(plan),
        "runnableRuns": sum(1 for item in plan if item["runnable"]),
        "blockedRuns": sum(1 for item in plan if not item["runnable"]),
        "blockedCoverage": blocked_coverage,
        "blockedCoverageItems": len(blocked_coverage),
        "errors": errors,
        "warnings": warnings,
        "protocolContract": protocol_contract,
        "hashes": hashes,
        "plan": plan,
    }
    encoded = json.dumps(result, ensure_ascii=False, indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded + "\n", encoding="utf-8")
    print(encoded)
    return 0 if not errors else 2


if __name__ == "__main__":
    sys.exit(main())
