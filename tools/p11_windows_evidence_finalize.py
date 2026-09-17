#!/usr/bin/env python3
"""Finalize only genuine formal-chain P11 Windows evidence.

The script decodes receiver MP4 pixels and inventories renderer-produced PFM
data. It never paints, rescales, or synthesizes an infrared image.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FFMPEG = ROOT / ".deps/ffmpeg-n8.1-win64-gpl-shared/ffmpeg-n8.1-latest-win64-gpl-shared-8.1/bin/ffmpeg.exe"
FFPROBE = FFMPEG.with_name("ffprobe.exe")
VARIANTS = {"fixed": "fixed_clean.png", "agc": "auto_clean.png", "annotated": "annotated.png"}
AGC_REQUIRED_NUMERIC_FIELDS = (
    "updateHz", "lowPercentile", "highPercentile", "lowInput", "highInput",
    "gain", "offset", "gainSmoothed", "offsetSmoothed", "stage6AgcStatsMs",
)
AGC_REQUIRED_TOKEN_FIELDS = ("mode", "statsSource", "fallbackReason")
COMPONENT_TAGS = (
    "M1 ", "M1[", "Stage5 RadianceComponents", "Stage5 AeroThermal",
    "L1 ", "L2 ActiveIlluminator", "Weather", "Stage7", "DisplayFrameMapping",
    "Stage6 AGC", "P6LinearCapture", "Perf", "TcpPerf", "VideoPerf", "RecorderPerf",
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def parse_fields(line: str) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in re.findall(r"\b([A-Za-z][A-Za-z0-9_]*)=([^\s]+)", line):
        # A foreign logger can be byte-interleaved after a partially written
        # line and repeat generic keys such as mode/sourceSeq.  The prefix tag
        # owns the first occurrence; never let the appended foreign record
        # overwrite it.  Field-specific validators still reject malformed
        # first values (for example outputFrames=1[Stage6).
        if key in result:
            continue
        try:
            number = float(value)
            result[key] = int(number) if number.is_integer() else number
        except ValueError:
            result[key] = value
    return result


def tagged_entries(tag: str, text: str) -> list[dict[str, object]]:
    prefix = f"[{tag}]"
    return [parse_fields(line) for line in text.splitlines() if line.startswith(prefix)]


def plain_log_token(value: object) -> bool:
    """Accept one logger token while rejecting suffixes from interleaved tags."""
    return isinstance(value, str) and re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", value) is not None


def integer(value: object) -> int | None:
    """Return an exact integer, never silently truncate a malformed counter."""
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, int):
        return value
    if isinstance(value, float) and value.is_integer():
        return int(value)
    if isinstance(value, str) and re.fullmatch(r"[+-]?\d+", value):
        return int(value)
    return None


def number(value: object) -> float | None:
    if isinstance(value, bool):
        return float(value)
    if isinstance(value, (int, float)) and math.isfinite(float(value)):
        return float(value)
    if isinstance(value, str):
        try:
            parsed = float(value)
            return parsed if math.isfinite(parsed) else None
        except ValueError:
            return None
    return None


def boolean(value: object) -> bool | None:
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)) and value in (0, 1):
        return bool(value)
    if isinstance(value, str):
        normalized = value.strip().lower()
        if normalized in ("1", "true", "yes", "on"):
            return True
        if normalized in ("0", "false", "no", "off"):
            return False
    return None


def maximum_field(rows: list[dict[str, object]], key: str) -> int | None:
    values = [integer(row.get(key)) for row in rows if key in row]
    exact = [value for value in values if value is not None]
    return max(exact) if exact else None


def validate_formal_chain(
    summary: dict[str, object],
    hwa_text: str,
    video_text: str,
    stim_text: str,
    expected_protocol_band: int,
) -> dict[str, object]:
    """Apply P11's lossless ordered/synchronous frame-contract hard gate."""
    gate_errors: list[str] = []

    render_controls = tagged_entries("RenderControl", hwa_text)
    active_control = render_controls[-1] if render_controls else {}
    if active_control.get("asyncInputPolicy") != "OrderedQueue":
        gate_errors.append("ordered_queue_not_observed")
    if integer(active_control.get("effectiveSimMode")) != 1:
        gate_errors.append("sync_mode_not_observed")
    if integer(summary.get("stimSimMode")) != 1:
        gate_errors.append("stim_sim_mode_not_sync")
    if integer(summary.get("stimSensorBand")) != expected_protocol_band:
        gate_errors.append("stim_sensor_band_mismatch")

    perf_rows = tagged_entries("Perf", hwa_text)
    if not perf_rows or any(row.get("mode") != "sync" for row in perf_rows if "mode" in row):
        gate_errors.append("renderer_perf_not_sync")

    rounds = tagged_entries("SyncRoundConservation", hwa_text)
    if len(rounds) != 1:
        gate_errors.append(f"sync_round_count:{len(rounds)}")
    round_final = rounds[-1] if rounds else {}
    accepted = integer(round_final.get("acceptedRealtime"))
    captured = integer(round_final.get("lastCapturedSourceSeq"))
    if round_final.get("mode") != "sync":
        gate_errors.append("conservation_mode_not_sync")
    for key in ("inputMinusCaptured", "queueDepth", "staleFramePublished"):
        if integer(round_final.get(key)) != 0:
            gate_errors.append(f"conservation_nonzero:{key}")

    ingress_rows = tagged_entries("RealtimeIngress", hwa_text)
    executed = maximum_field(ingress_rows, "appRealtimeConsumed")
    queued = maximum_field(ingress_rows, "appRealtimeQueued")
    rendered = maximum_field(perf_rows, "renderFrames")
    output = maximum_field(perf_rows, "outputFrames")
    sent = maximum_field(tagged_entries("StimFinal", stim_text), "successfulRealtimeWrites")
    recorder_rows = tagged_entries("RecorderPerf", video_text)
    recorder_flush_rows = tagged_entries("RecorderFlush", video_text)
    receiver_input = maximum_field(recorder_flush_rows, "inputFrames")
    receiver_written = maximum_field(recorder_flush_rows, "writtenFrames")
    receiver_last_frame_seq = maximum_field(recorder_flush_rows, "frameSeqWritten")
    receiver_last_source_seq = maximum_field(recorder_flush_rows, "sourceSeqWritten")
    summary_written = integer(summary.get("writtenFrames"))
    mp4_frames = integer(summary.get("mp4Frames"))
    counts = {
        "sent": sent,
        "accepted": accepted,
        "queued": queued,
        "executed": executed,
        "captured": captured,
        "rendered": rendered,
        "output": output,
        "receiverInput": receiver_input,
        "receiverWritten": receiver_written,
        "receiverLastFrameSeq": receiver_last_frame_seq,
        "receiverLastSourceSeq": receiver_last_source_seq,
        "summaryWritten": summary_written,
        "receivedMp4": mp4_frames,
    }
    if accepted is None or accepted <= 0:
        gate_errors.append("accepted_count_missing_or_zero")
    else:
        for name, value in counts.items():
            if value != accepted:
                gate_errors.append(f"count_mismatch:{name}:{value}!={accepted}")

    continuity_rows = (
        tagged_entries("SyncFrame", hwa_text)
        + tagged_entries("VideoPerf", video_text)
        + recorder_rows
        + recorder_flush_rows
    )
    continuity_values = [
        integer(row[key])
        for row in continuity_rows
        for key in ("sourceSeqContinuous", "frameSeqContinuousWritten", "sourceSeqContinuousWritten")
        if key in row
    ]
    if not continuity_values:
        gate_errors.append("source_sequence_continuity_evidence_missing")
    elif any(value != 1 for value in continuity_values):
        gate_errors.append("source_sequence_not_continuous")
    for key in ("sourceSeqContinuous", "frameSeqContinuousWritten", "sourceSeqContinuousWritten"):
        if integer(summary.get(key)) != 1:
            gate_errors.append(f"summary_not_continuous:{key}")

    if not recorder_flush_rows:
        gate_errors.append("recorder_flush_evidence_missing")
    elif integer(recorder_flush_rows[-1].get("completed")) != 1:
        gate_errors.append("recorder_flush_not_completed")

    zero_sources = {
        "summary": ([summary], (
            "inputQueueOverflow", "tcpOverwritten", "recorderDroppedFrames",
            "h264DecodeErrors",
        )),
        "ingress": (ingress_rows, (
            "inputQueueOverflow", "inputOverwritten", "sourceSeqGapCount",
        )),
        "perf": (perf_rows, (
            "inputQueueOverflowCount", "dropped", "inputOverwritten",
            "outputOverwritten", "overwritten",
        )),
        "tcp": (tagged_entries("TcpPerf", hwa_text), ("overwritten",)),
        "video": (tagged_entries("VideoPerf", video_text), (
            "discontinuities", "h264DecodeErrors",
        )),
        "recorder": (recorder_rows + recorder_flush_rows, ("droppedFrames",)),
    }
    zero_observations: dict[str, int | None] = {}
    for source, (rows, keys) in zero_sources.items():
        for key in keys:
            value = maximum_field(rows, key)
            zero_observations[f"{source}.{key}"] = value
            if value is None:
                gate_errors.append(f"zero_counter_missing:{source}.{key}")
            elif value != 0:
                gate_errors.append(f"zero_counter_nonzero:{source}.{key}:{value}")

    packet_rows = tagged_entries("TcpFramePacketRx", video_text)
    return {
        "result": "PASS" if not gate_errors else "FAIL",
        "policy": active_control.get("asyncInputPolicy", "missing"),
        "mode": active_control.get("effectiveSimMode", "missing"),
        "counts": counts,
        "sourceSeqContinuousEvidenceRows": len(continuity_values),
        "lastLoggedReceiverFrameSeq": maximum_field(packet_rows, "frameSeq"),
        "zeroCounters": zero_observations,
        "errors": sorted(set(gate_errors)),
    }


def validate_scenario_expectations(
    scenario: dict[str, object],
    hwa_text: str,
    summary: dict[str, object],
    band: str,
    expected_agc: bool,
    requested_source_seq: int | None,
) -> dict[str, object]:
    """Verify requested controls against values emitted by the running renderer."""
    gate_errors: list[str] = []
    observations: dict[str, object] = {}

    agc_rows = tagged_entries("Stage6 AGC", hwa_text)
    # stdout is shared with protocol/telemetry threads and an occasional line
    # can be byte-interleaved.  Such a row is logging evidence loss, not an AGC
    # physics fallback.  Keep it visible, but validate the exact requested
    # sourceSeq and the remaining complete rows rather than interpreting a
    # foreign token (for example "effective=[ProtocolRoute]" or a
    # "fallbackReason=continuous_low_contrast[TcpFramePacket]" suffix) as a
    # genuine AGC fallback.  Every field emitted by the production Stage6 AGC
    # record is checked, so an interleave that corrupts highPercentile while
    # leaving a syntactically plain fallbackReason is still evidence loss.
    # Unknown but otherwise complete fallback tokens remain well formed so the
    # semantic allow-list below rejects them.
    well_formed_agc_rows = [
        row for row in agc_rows
        if (integer(row.get("sourceSeq")) or 0) > 0
        and integer(row.get("enabled")) in (0, 1)
        and integer(row.get("effective")) in (0, 1)
        and integer(row.get("sampleCount")) is not None
        and integer(row.get("valid")) in (0, 1)
        and all(number(row.get(key)) is not None for key in AGC_REQUIRED_NUMERIC_FIELDS)
        and all(plain_log_token(row.get(key)) for key in AGC_REQUIRED_TOKEN_FIELDS)
    ]
    malformed_agc_rows = [row for row in agc_rows if row not in well_formed_agc_rows]
    selected_agc_rows = [
        row for row in well_formed_agc_rows
        if integer(row.get("sourceSeq")) == requested_source_seq
    ]
    observations["agcRows"] = len(agc_rows)
    observations["agcWellFormedRows"] = len(well_formed_agc_rows)
    observations["agcMalformedInterleavedRows"] = len(malformed_agc_rows)
    observations["agcSelectedSourceSeqRows"] = len(selected_agc_rows)
    if expected_agc:
        require_agc_valid = boolean(scenario.get("requireAgcValid")) is True
        enabled_effective_sample_rows = [
            row for row in well_formed_agc_rows
            if integer(row.get("enabled")) == 1
            and integer(row.get("effective")) == 1
            and (integer(row.get("sampleCount")) or 0) > 0
        ]
        invalid_rows = [row for row in well_formed_agc_rows if integer(row.get("valid")) != 1]
        valid_rows = [row for row in well_formed_agc_rows if integer(row.get("valid")) == 1]
        invalid_fallback_reasons = sorted({
            str(row.get("fallbackReason", "missing")) for row in invalid_rows
        })
        unexpected_invalid_rows = [
            row for row in invalid_rows
            if row.get("fallbackReason") != "continuous_low_contrast"
        ]
        unexpected_valid_rows = [
            row for row in valid_rows
            if row.get("fallbackReason") not in (None, "none")
        ]
        observations["agcContract"] = {
            "requireAgcValid": require_agc_valid,
            "enabledEffectiveSampleRows": len(enabled_effective_sample_rows),
            "validObserved": bool(valid_rows),
            "validRows": len(valid_rows),
            "invalidRows": len(invalid_rows),
            "invalidFallbackReasons": invalid_fallback_reasons,
            "malformedInterleavedRows": len(malformed_agc_rows),
            "selectedSourceSeq": requested_source_seq,
            "selectedValidRows": sum(1 for row in selected_agc_rows if integer(row.get("valid")) == 1),
            "acceptedInvalidFallback": (
                "continuous_low_contrast"
                if invalid_rows and not unexpected_invalid_rows else None
            ),
        }
        if not well_formed_agc_rows:
            gate_errors.append("agc_runtime_rows_missing")
        if len(enabled_effective_sample_rows) != len(well_formed_agc_rows):
            gate_errors.append("agc_runtime_enabled_effective_sample_contract_failed")
        if unexpected_invalid_rows:
            gate_errors.append("agc_runtime_invalid_fallback_not_allowed")
        if unexpected_valid_rows:
            gate_errors.append("agc_runtime_valid_row_has_fallback")
        if require_agc_valid and not valid_rows:
            gate_errors.append("agc_runtime_valid_not_observed_for_required_scenario")
        if not require_agc_valid and not valid_rows and not invalid_rows:
            gate_errors.append("agc_runtime_valid_or_low_contrast_fallback_not_observed")
        if not selected_agc_rows:
            gate_errors.append("agc_requested_source_seq_row_missing")
        elif require_agc_valid and not any(integer(row.get("valid")) == 1 for row in selected_agc_rows):
            gate_errors.append("agc_requested_source_seq_not_valid")
        elif not require_agc_valid and not any(
            integer(row.get("valid")) == 1 or row.get("fallbackReason") == "continuous_low_contrast"
            for row in selected_agc_rows
        ):
            gate_errors.append("agc_requested_source_seq_has_unaccepted_fallback")
    else:
        agc_matches = [
            row for row in well_formed_agc_rows
            if integer(row.get("enabled")) == 0 and integer(row.get("effective")) == 0
        ]
        if not agc_matches:
            gate_errors.append("agc_runtime_disabled_not_observed")
        if any(integer(row.get("enabled")) == 1 or integer(row.get("effective")) == 1 for row in well_formed_agc_rows):
            gate_errors.append("agc_runtime_unexpectedly_enabled")

    expected_natural_solar = boolean(scenario.get("naturalSolarEnable"))
    if expected_natural_solar is not None:
        natural_rows = tagged_entries("L1 NaturalSolarConfig", hwa_text)
        actual_value = 1 if expected_natural_solar else 0
        observations["naturalSolarEnable"] = [integer(row.get("Enable")) for row in natural_rows]
        if not any(integer(row.get("Enable")) == actual_value for row in natural_rows):
            gate_errors.append(f"natural_solar_runtime_state_not_observed:{actual_value}")
        if boolean(summary.get("naturalSolarEnable")) is not expected_natural_solar:
            gate_errors.append("natural_solar_summary_mismatch")

    expected_humidity = number(scenario.get("relativeHumidityPercent"))
    if expected_humidity is not None:
        humidity_rows = [
            row for row in tagged_entries("Stage5 ModtranRadianceCompare", hwa_text)
            if row.get("band") == band
            and integer(row.get("valid")) == 1
            and row.get("fallbackReason") == "none"
            and number(row.get("relativeHumidityPercent")) is not None
            and abs(float(row["relativeHumidityPercent"]) - expected_humidity) <= 0.01
        ]
        observations["humidity"] = {
            "requestedPercent": expected_humidity,
            "matchedRows": len(humidity_rows),
            "modes": sorted({str(row.get("humidityMode", "missing")) for row in humidity_rows}),
        }
        if not humidity_rows:
            gate_errors.append(f"humidity_runtime_value_not_observed:{expected_humidity}")
        elif not any(str(row.get("humidityMode", "")).startswith("numeric_") for row in humidity_rows):
            gate_errors.append("humidity_runtime_mode_not_numeric")
        expected_mode = scenario.get("expectedHumidityMode")
        if expected_mode is not None and not any(row.get("humidityMode") == expected_mode for row in humidity_rows):
            gate_errors.append(f"humidity_runtime_mode_mismatch:{expected_mode}")
        if scenario.get("comparisonGroup") == "humidity":
            heating_rows = [
                row for row in tagged_entries("L1 SolarHeatingLut", hwa_text)
                if number(row.get("relativeHumidityPercent")) is not None
                and abs(float(row["relativeHumidityPercent"]) - expected_humidity) <= 0.01
                and str(row.get("humidityMode", "")).startswith("numeric_")
            ]
            observations["solarHeatingHumidityRows"] = len(heating_rows)
            if not heating_rows:
                gate_errors.append(f"solar_heating_humidity_value_mode_not_observed:{expected_humidity}")

    expected_azimuth = number(scenario.get("solarAzimuthDeg"))
    expected_elevation = number(scenario.get("solarElevationDeg"))
    if (expected_azimuth is None) != (expected_elevation is None):
        gate_errors.append("solar_override_request_incomplete")
    elif expected_azimuth is not None and expected_elevation is not None:
        tolerance = number(scenario.get("solarToleranceDeg")) or 0.05

        def azimuth_error_degrees(actual: float, expected: float) -> float:
            delta = abs(actual - expected) % 360.0
            return min(delta, 360.0 - delta)

        def solar_geometry_match(row: dict[str, object]) -> bool:
            azimuth = number(row.get("solarAz"))
            elevation = number(row.get("solarEl"))
            zenith = number(row.get("solarZenith"))
            direction = row.get("sunDirection")
            if azimuth is None or elevation is None or zenith is None:
                return False
            if not isinstance(direction, str) or not direction.startswith("(") or not direction.endswith(")"):
                return False
            direction_values = [number(value) for value in direction[1:-1].split(",")]
            if len(direction_values) != 3 or any(value is None for value in direction_values):
                return False
            azimuth_rad = math.radians(azimuth)
            elevation_rad = math.radians(elevation)
            expected_direction = (
                math.cos(elevation_rad) * math.sin(azimuth_rad),
                math.cos(elevation_rad) * math.cos(azimuth_rad),
                math.sin(elevation_rad),
            )
            return (
                row.get("frame") == "ENU/Panda(X=East,Y=North,Z=Up)"
                and azimuth_error_degrees(azimuth, expected_azimuth) <= tolerance
                and abs(elevation - expected_elevation) <= tolerance
                and abs(zenith - (90.0 - elevation)) <= tolerance
                and all(
                    abs(float(actual) - expected) <= 2.0e-5
                    for actual, expected in zip(direction_values, expected_direction)
                )
            )

        solar_line_rows = [
            (line, parse_fields(line))
            for line in hwa_text.splitlines()
            if line.startswith("[M1 SolarPosition]")
        ]
        complete_solar = [
            row for _line, row in solar_line_rows
            if solar_geometry_match(row)
            and integer(row.get("valid")) == 1
            and integer(row.get("solarOverride")) == 1
            and row.get("positionSource") == "controlled_config_solar_override"
            and row.get("dateSource") == "config_fallback"
            and row.get("protocolTimeSource") == "time_ms_since_midnight"
            and row.get("fallbackReason") == "none"
        ]

        # The renderer writes azimuth/elevation/zenith/direction/frame before
        # positionSource, and writes valid/fallbackReason after it.  Recover
        # only the trusted prefix when the exact ProtocolIngress logger begins
        # at that boundary.  Never normalize the corrupted tail or accept a
        # generic suffix: independent M1 and Stage5 rows must also prove that
        # the requested angles reached the production radiance chain.
        source_field = "positionSource=controlled_config_solar_override"
        ingress_marker = source_field + "[ProtocolIngress]"

        def recoverable_ingress_prefix(line: str, row: dict[str, object]) -> bool:
            marker_index = line.find(ingress_marker)
            if marker_index < 0:
                return False
            trusted_end = marker_index + len(source_field)
            trusted_row = parse_fields(line[:trusted_end])
            foreign_suffix = line[trusted_end:]
            return (
                trusted_row.get("positionSource") == "controlled_config_solar_override"
                and solar_geometry_match(trusted_row)
                and re.match(r"^\[ProtocolIngress\]\s+transport=(?:udp|tcp)\b", foreign_suffix) is not None
                and row.get("type") == "realtime"
                and integer(row.get("accepted")) in (0, 1)
                and integer(row.get("duplicate")) in (0, 1)
            )

        interleaved_candidates = [
            row for line, row in solar_line_rows if recoverable_ingress_prefix(line, row)
        ]
        m1_compare_corroboration = [
            row for row in tagged_entries("M1 Compare", hwa_text)
            if row.get("band") == band
            and integer(row.get("valid")) == 1
            and row.get("fallbackReason") == "none"
            and number(row.get("solarZenithDeg")) is not None
            and abs(float(row["solarZenithDeg"]) - (90.0 - expected_elevation)) <= tolerance
        ]
        stage5_corroboration = [
            row for row in tagged_entries("Stage5 Radiance", hwa_text)
            if row.get("band") == band
            and number(row.get("sunAzimuth")) is not None
            and number(row.get("sunElevation")) is not None
            and azimuth_error_degrees(float(row["sunAzimuth"]), expected_azimuth) <= tolerance
            and abs(float(row["sunElevation"]) - expected_elevation) <= tolerance
        ]
        recovered_solar = (
            interleaved_candidates
            if m1_compare_corroboration and stage5_corroboration else []
        )
        matching_solar_count = len(complete_solar) + len(recovered_solar)
        observations["solarOverride"] = {
            "requestedAzimuthDeg": expected_azimuth,
            "requestedElevationDeg": expected_elevation,
            "matchedRows": matching_solar_count,
            "completeRows": len(complete_solar),
            "interleavedCandidates": len(interleaved_candidates),
            "recoveredInterleavedRows": len(recovered_solar),
            "unrecoverableRows": len(solar_line_rows) - len(complete_solar) - len(recovered_solar),
            "m1CompareCorroborationRows": len(m1_compare_corroboration),
            "stage5RadianceCorroborationRows": len(stage5_corroboration),
        }
        if matching_solar_count == 0:
            gate_errors.append(f"solar_override_runtime_angle_not_observed:{expected_azimuth}:{expected_elevation}")
        if boolean(summary.get("m1SolarOverrideEnable")) is not True:
            gate_errors.append("solar_override_summary_not_enabled")
        summary_azimuth = number(summary.get("m1SolarOverrideAzimuthDeg"))
        summary_elevation = number(summary.get("m1SolarOverrideElevationDeg"))
        if summary_azimuth is None or min(abs(summary_azimuth - expected_azimuth) % 360.0, 360.0 - abs(summary_azimuth - expected_azimuth) % 360.0) > tolerance:
            gate_errors.append("solar_override_summary_azimuth_mismatch")
        if summary_elevation is None or abs(summary_elevation - expected_elevation) > tolerance:
            gate_errors.append("solar_override_summary_elevation_mismatch")

    expected_sun_visibility = number(scenario.get("m1SunVisibility"))
    if expected_sun_visibility is not None:
        physics_rows = tagged_entries("M1 PhysicsConfig", hwa_text)
        matching_visibility = [
            row for row in physics_rows
            if number(row.get("sunVisibility")) is not None
            and abs(float(row["sunVisibility"]) - expected_sun_visibility) <= 1.0e-6
        ]
        observations["m1SunVisibility"] = {
            "requested": expected_sun_visibility,
            "matchedConfigRows": len(matching_visibility),
        }
        if not matching_visibility:
            gate_errors.append(f"m1_sun_visibility_runtime_not_observed:{expected_sun_visibility}")
        summary_visibility = number(summary.get("m1SunVisibility"))
        if summary_visibility is None or abs(summary_visibility - expected_sun_visibility) > 1.0e-6:
            gate_errors.append("m1_sun_visibility_summary_mismatch")
        comparison_rows = [
            row for row in tagged_entries("M1 Compare", hwa_text)
            if row.get("band") == band and integer(row.get("valid")) == 1
            and number(row.get("solarReflected")) is not None
        ]
        if expected_sun_visibility == 0.0:
            if not comparison_rows or any(float(row["solarReflected"]) > 1.0e-9 for row in comparison_rows):
                gate_errors.append("solar_off_has_nonzero_direct_reflection")
        elif expected_sun_visibility > 0.0 and not any(float(row["solarReflected"]) > 0.0 for row in comparison_rows):
            gate_errors.append("solar_on_has_no_positive_direct_reflection")

    active_expectation = scenario.get("activeExpectation")
    if active_expectation is not None:
        active_rows = [
            row for row in tagged_entries("L2 ActiveIlluminator", hwa_text)
            if row.get("sensorBand") == band and (integer(row.get("sourceSeq")) or 0) > 0
        ]
        observations["activeIlluminator"] = {
            "expectation": active_expectation,
            "candidateRows": len(active_rows),
            "summaryPositiveSamples": integer(summary.get("activePositiveSampleCount")),
            "summaryMaximum": number(summary.get("activeSensorRadianceMaxWm2SrUm")),
        }

        def zero(value: object) -> bool:
            parsed = number(value)
            return parsed is not None and abs(parsed) <= 1.0e-12

        if active_expectation == "off":
            matching_active = [
                row for row in active_rows
                if integer(row.get("protocolEnabled")) == 0
                and integer(row.get("activeContributionEnabled")) == 0
                and zero(row.get("activeSensorWm2SrUm"))
                and row.get("fallbackReason") == "protocol_illuminator_disabled"
            ]
        elif active_expectation == "in_band_positive":
            matching_active = [
                row for row in active_rows
                if integer(row.get("protocolEnabled")) == 1
                and integer(row.get("spectralOverlap")) == 1
                and (number(row.get("beamFactor")) or 0.0) > 0.0
                and (number(row.get("activeVisibility")) or 0.0) > 0.0
                and integer(row.get("activeContributionEnabled")) == 1
                and (number(row.get("activeSensorWm2SrUm")) or 0.0) > 0.0
                and row.get("fallbackReason") == "none"
            ]
        elif active_expectation == "outside_beam_zero":
            matching_active = [
                row for row in active_rows
                if integer(row.get("protocolEnabled")) == 1
                and integer(row.get("spectralOverlap")) == 1
                and zero(row.get("beamFactor"))
                and integer(row.get("activeContributionEnabled")) == 0
                and zero(row.get("activeSensorWm2SrUm"))
                and row.get("fallbackReason") == "outside_beam_cone"
            ]
        elif active_expectation == "out_of_band_zero":
            matching_active = [
                row for row in active_rows
                if integer(row.get("protocolEnabled")) == 1
                and integer(row.get("spectralOverlap")) == 0
                and integer(row.get("activeContributionEnabled")) == 0
                and zero(row.get("activeSensorWm2SrUm"))
                and row.get("fallbackReason") == "spectral_band_mismatch"
            ]
        else:
            matching_active = []
            gate_errors.append(f"unknown_active_expectation:{active_expectation}")
        if not matching_active:
            gate_errors.append(f"active_runtime_expectation_not_observed:{active_expectation}")
        positive_count = integer(summary.get("activePositiveSampleCount"))
        positive_maximum = number(summary.get("activeSensorRadianceMaxWm2SrUm"))
        if active_expectation == "in_band_positive":
            if positive_count is None or positive_count <= 0 or positive_maximum is None or positive_maximum <= 0.0:
                gate_errors.append("active_positive_summary_not_observed")
        elif positive_count != 0 or positive_maximum is None or abs(positive_maximum) > 1.0e-12:
            gate_errors.append("active_zero_summary_not_observed")

    return {
        "result": "PASS" if not gate_errors else "FAIL",
        "observations": observations,
        "errors": sorted(set(gate_errors)),
    }


def png_size(path: Path) -> tuple[int, int]:
    data = path.read_bytes()[:24]
    if len(data) < 24 or data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"not_png:{path}")
    return struct.unpack(">II", data[16:24])


def pfm_stats(path: Path) -> dict[str, object]:
    with path.open("rb") as stream:
        kind = stream.readline().strip()
        if kind not in (b"PF", b"Pf"):
            raise ValueError("invalid_pfm_magic")
        dims = stream.readline().strip()
        while dims.startswith(b"#"):
            dims = stream.readline().strip()
        width, height = map(int, dims.split())
        scale = float(stream.readline().strip())
        channels = 3 if kind == b"PF" else 1
        payload = stream.read()
    count = width * height * channels
    if len(payload) != count * 4:
        raise ValueError(f"pfm_payload_size:{len(payload)}!={count * 4}")
    endian = "<" if scale < 0 else ">"
    minimum = math.inf
    maximum = -math.inf
    total = 0.0
    finite = 0
    nonfinite = 0
    for (value,) in struct.iter_unpack(endian + "f", payload):
        if math.isfinite(value):
            minimum = min(minimum, value)
            maximum = max(maximum, value)
            total += value
            finite += 1
        else:
            nonfinite += 1
    return {
        "width": width, "height": height, "channels": channels,
        "scale": scale, "finiteValues": finite, "nonfiniteValues": nonfinite,
        "minimum": minimum if finite else None, "maximum": maximum if finite else None,
        "mean": total / finite if finite else None,
    }


def load_recording_index(recording_dir: Path) -> tuple[list[dict[str, object]], list[str]]:
    path = recording_dir / "frame_index.jsonl"
    if not path.is_file():
        return [], ["frame_index_missing"]
    rows: list[dict[str, object]] = []
    errors: list[str] = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), 1):
        if not line.strip():
            continue
        try:
            row = json.loads(line)
        except json.JSONDecodeError as exc:
            errors.append(f"frame_index_json:{line_number}:{exc.msg}")
            continue
        for key in ("sourceSeq", "frameSeq", "storageIndex", "mp4PtsUs"):
            if integer(row.get(key)) is None:
                errors.append(f"frame_index_integer_missing:{line_number}:{key}")
        rows.append(row)
    if not rows:
        errors.append("frame_index_empty")
        return rows, errors
    for key in ("sourceSeq", "frameSeq", "storageIndex"):
        values = [integer(row.get(key)) for row in rows]
        if None in values or len(set(values)) != len(values):
            errors.append(f"frame_index_not_unique:{key}")
            continue
        if any(b != a + 1 for a, b in zip(values, values[1:])):
            errors.append(f"frame_index_not_continuous:{key}")
    return rows, errors


def load_producer_annotation(recording_dir: Path, row: dict[str, object]) -> dict[str, object]:
    path = recording_dir / "producer_annotations.jsonl"
    offset = integer(row.get("annotationBodyOffset"))
    size = integer(row.get("annotationBodyBytes"))
    expected_hash = row.get("annotationBodySha256")
    if not path.is_file() or offset is None or size is None or offset < 0 or size <= 0:
        raise ValueError("producer_annotation_reference_invalid")
    with path.open("rb") as stream:
        stream.seek(offset)
        payload = stream.read(size)
    if len(payload) != size:
        raise ValueError(f"producer_annotation_short_read:{len(payload)}!={size}")
    payload_hash = hashlib.sha256(payload).hexdigest()
    if not isinstance(expected_hash, str) or payload_hash != expected_hash:
        raise ValueError("producer_annotation_hash_mismatch")
    annotation = json.loads(payload.decode("utf-8"))
    for key in ("sourceSeq", "frameSeq"):
        if integer(annotation.get(key)) != integer(row.get(key)):
            raise ValueError(f"producer_annotation_identity_mismatch:{key}")
    return annotation


def duration_boundary_values(
    index_rows: list[dict[str, object]],
    first_annotation: dict[str, object],
    last_annotation: dict[str, object],
    requested_duration_ms: int | None,
) -> dict[str, object]:
    """Keep encoded PTS duration separate from protocol simulation duration.

    ``producerPtsMs`` is synthesized from the ordinal of frames that reached the
    receiver recording.  It necessarily becomes shorter when an input is lost,
    so using it for the case-duration boundary duplicates the independent frame
    conservation/count gates.  The producer annotation carries the originating
    protocol ``simTimeMs`` and is the authoritative duration clock.
    """
    first_producer_pts_ms = integer(index_rows[0].get("producerPtsMs")) if index_rows else None
    last_producer_pts_ms = integer(index_rows[-1].get("producerPtsMs")) if index_rows else None
    encoded_timeline_duration_ms = (
        last_producer_pts_ms - first_producer_pts_ms
        if first_producer_pts_ms is not None and last_producer_pts_ms is not None
        else None
    )
    first_sim_time_ms = integer(first_annotation.get("simTimeMs"))
    last_sim_time_ms = integer(last_annotation.get("simTimeMs"))
    scenario_duration_ms = (
        last_sim_time_ms - first_sim_time_ms
        if first_sim_time_ms is not None and last_sim_time_ms is not None
        else None
    )
    duration_delta_ms = (
        scenario_duration_ms - requested_duration_ms
        if scenario_duration_ms is not None and requested_duration_ms is not None
        else None
    )
    return {
        "firstProducerPtsMs": first_producer_pts_ms,
        "lastProducerPtsMs": last_producer_pts_ms,
        "producerDurationMs": encoded_timeline_duration_ms,
        "encodedTimelineDurationMs": encoded_timeline_duration_ms,
        "firstProducerSimTimeMs": first_sim_time_ms,
        "lastProducerSimTimeMs": last_sim_time_ms,
        "scenarioDurationMs": scenario_duration_ms,
        "requestedDurationMs": requested_duration_ms,
        "durationDeltaMs": duration_delta_ms,
        "durationMetric": "producer_annotation.simTimeMs",
    }


def decode_exact(path: Path, output: Path, row: dict[str, object]) -> dict[str, object]:
    storage_index = integer(row.get("storageIndex"))
    if storage_index is None or storage_index <= 0:
        raise ValueError("invalid_storage_index")
    zero_based_frame = storage_index - 1
    subprocess.run([
        str(FFMPEG), "-v", "error", "-y", "-i", str(path),
        "-vf", f"select=eq(n\\,{zero_based_frame})", "-frames:v", "1",
        "-fps_mode", "vfr", str(output),
    ], check=True)
    width, height = png_size(output)
    return {
        "source": str(path), "sourceSha256": sha256(path),
        "sourceSeq": integer(row.get("sourceSeq")), "frameSeq": integer(row.get("frameSeq")),
        "storageIndex": storage_index, "mp4PtsUs": integer(row.get("mp4PtsUs")),
        "output": str(output), "outputSha256": sha256(output), "width": width,
        "height": height,
        "meaning": "exact storageIndex decode of requested sourceSeq from actual TCP receiver recording; no pixel synthesis",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("case_dir", type=Path)
    args = parser.parse_args()
    case_dir = args.case_dir.resolve()
    request_path = case_dir / "case_request.json"
    if not request_path.is_file():
        raise SystemExit(f"missing {request_path}")
    request = json.loads(request_path.read_text(encoding="utf-8-sig"))
    errors: list[str] = []
    evidence: dict[str, object] = {}
    requested_source_seq = integer(request.get("deterministicReplaySourceSeq"))
    if requested_source_seq is None or requested_source_seq <= 0:
        errors.append("deterministic_replay_source_seq_missing_or_invalid")
    recording_indexes: dict[str, list[dict[str, object]]] = {}
    selected_rows: dict[str, dict[str, object]] = {}
    selected_frame: dict[str, object] = {
        "requestedSourceSeq": requested_source_seq,
        "policy": "independent formal replays decoded at identical sourceSeq via frame_index storageIndex",
        "variants": {},
    }

    if not FFMPEG.is_file():
        errors.append("ffmpeg_missing")
    for variant, canonical in VARIANTS.items():
        recording_dir = case_dir / "variants" / variant / "receiver_recording"
        movie = recording_dir / "output.mp4"
        if not movie.is_file():
            errors.append(f"missing_tcp_recording:{variant}")
        rows, index_errors = load_recording_index(recording_dir)
        recording_indexes[variant] = rows
        errors.extend(f"recording_index:{variant}:{item}" for item in index_errors)
        matching_rows = [row for row in rows if integer(row.get("sourceSeq")) == requested_source_seq]
        if len(matching_rows) != 1:
            errors.append(f"deterministic_source_seq_match_count:{variant}:{len(matching_rows)}")
            continue
        row = matching_rows[0]
        selected_rows[variant] = row
        try:
            annotation = load_producer_annotation(recording_dir, row)
        except Exception as exc:
            errors.append(f"producer_annotation_invalid:{variant}:{exc}")
            annotation = {}
        selected_frame["variants"][variant] = {
            "sourceSeq": integer(row.get("sourceSeq")),
            "frameSeq": integer(row.get("frameSeq")),
            "storageIndex": integer(row.get("storageIndex")),
            "mp4PtsUs": integer(row.get("mp4PtsUs")),
            "producerPtsMs": integer(row.get("producerPtsMs")),
            "simTimeMs": integer(annotation.get("simTimeMs")),
            "annotationBodySha256": row.get("annotationBodySha256"),
        }
        if FFMPEG.is_file() and movie.is_file():
            try:
                evidence[variant] = decode_exact(movie, case_dir / canonical, row)
            except Exception as exc:  # evidence failure must remain visible
                errors.append(f"decode_failed:{variant}:{exc}")

    sim_times = [
        item.get("simTimeMs")
        for item in selected_frame["variants"].values()
        if isinstance(item, dict) and item.get("simTimeMs") is not None
    ]
    # Three independently started 60 Hz replays retain the exact sourceSeq but
    # their wall-clock-derived simulation timestamp can land anywhere inside
    # one nominal frame interval.  Reject drift beyond that single-frame bound;
    # do not demand a sub-frame 2 ms phase lock that the sender never promises.
    sim_time_tolerance_ms = 17
    sim_time_delta_ms = max(sim_times) - min(sim_times) if sim_times else None
    selected_frame["simTimeToleranceMs"] = sim_time_tolerance_ms
    selected_frame["simTimeMinMs"] = min(sim_times) if sim_times else None
    selected_frame["simTimeMaxMs"] = max(sim_times) if sim_times else None
    selected_frame["simTimeMaxDeltaMs"] = sim_time_delta_ms
    if (
        len(sim_times) != len(VARIANTS)
        or sim_time_delta_ms is None
        or sim_time_delta_ms > sim_time_tolerance_ms
    ):
        errors.append(
            f"deterministic_replay_sim_time_delta_exceeded:{sim_time_delta_ms}>{sim_time_tolerance_ms}"
        )

    agc_movie = case_dir / "variants" / "agc" / "receiver_recording" / "output.mp4"
    if FFMPEG.is_file() and agc_movie.is_file() and "agc" in selected_rows:
        try:
            evidence["received"] = decode_exact(agc_movie, case_dir / "received.png", selected_rows["agc"])
        except Exception as exc:
            errors.append(f"received_decode_failed:{exc}")

    fixed = case_dir / "variants" / "fixed"
    raw_candidates = sorted(
        path for path in fixed.rglob("*.pfm")
        if not path.name.endswith("_stats.pfm")
    )
    raw_meta: dict[str, object] = {}
    physical_line = ""
    physical_fields: dict[str, object] = {}
    physical_matches: list[tuple[str, dict[str, object]]] = []
    for log in fixed.rglob("hwa.out.log"):
        text = log.read_text(encoding="utf-8", errors="replace")
        matches = re.findall(r"^\[P6LinearCapture\].*$", text, flags=re.MULTILINE)
        for line in matches:
            fields = parse_fields(line)
            # The triggered-copy path emits an "armed" row and, only after a
            # verified RAM image plus successful PFM write, a physical artifact
            # row.  Bind evidence solely to the latter so arming is never
            # mistaken for a second capture or for proof of a written file.
            if (
                integer(fields.get("sourceSeq")) == requested_source_seq
                and fields.get("stage") == "pre_display"
                and integer(fields.get("physicalRadiance")) == 1
                and fields.get("file")
            ):
                physical_matches.append((line, fields))
    if len(physical_matches) != 1:
        errors.append(f"raw_capture_source_seq_match_count:{len(physical_matches)}")
    else:
        physical_line, physical_fields = physical_matches[0]
    if not raw_candidates:
        errors.append("raw_physical_float_missing")
    elif not physical_fields:
        errors.append("raw_physical_float_not_bound_to_capture_log")
    else:
        declared = Path(str(physical_fields.get("file", "")))
        matching_candidates = [
            path for path in raw_candidates
            if path.resolve() == declared.resolve()
        ]
        if len(matching_candidates) != 1:
            errors.append(f"raw_capture_file_match_count:{len(matching_candidates)}")
            source = None
        else:
            source = matching_candidates[0]
    if raw_candidates and physical_fields and source is not None:
        target = case_dir / "raw_radiance.pfm"
        shutil.copy2(source, target)
        try:
            raw_meta = pfm_stats(target)
        except Exception as exc:
            errors.append(f"raw_pfm_invalid:{exc}")
        is_physical = (
            integer(physical_fields.get("sourceSeq")) == requested_source_seq
            and
            "stage=pre_display" in physical_line
            and "domain=spectral_radiance" in physical_line
            and "unit=W/(m^2_sr_um)" in physical_line
            and "physicalRadiance=1" in physical_line
        )
        if not is_physical:
            errors.append("raw_pfm_lacks_physical_unit_proof")
        raw_meta.update({
            "path": str(target), "sha256": sha256(target),
            "source": str(source), "producerLogLine": physical_line,
            "stage": "pre_display", "domain": "spectral_radiance" if is_physical else "unproven",
            "unit": "W/(m^2 sr um)" if is_physical else "unproven",
            "bandQuantity": "response-weighted spectral-radiance band mean",
        })
        (case_dir / "raw_radiance.json").write_text(json.dumps(raw_meta, indent=2) + "\n", encoding="utf-8")

    component_rows = []
    frame_lines = []
    for log in sorted((case_dir / "variants").rglob("*.log")):
        for line_number, line in enumerate(log.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            tag_match = re.match(r"^\[([^]]+)\]", line)
            if not tag_match:
                continue
            tag = tag_match.group(1)
            if any(tag.startswith(prefix.rstrip("[")) for prefix in COMPONENT_TAGS):
                fields = parse_fields(line)
                component_rows.append({
                    "variant": log.parts[log.parts.index("variants") + 1] if "variants" in log.parts else "",
                    "source_log": str(log.relative_to(case_dir)), "line": line_number,
                    "tag": tag, "fields_json": json.dumps(fields, ensure_ascii=False, sort_keys=True),
                })
                if any(key in fields for key in ("sourceSeq", "frameSeq", "simTimeMs")):
                    frame_lines.append({"tag": tag, "sourceLog": str(log.relative_to(case_dir)), "line": line_number, **fields})
    component_path = case_dir / "physical_components.csv"
    with component_path.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=("variant", "source_log", "line", "tag", "fields_json"))
        writer.writeheader()
        writer.writerows(component_rows)
    if not component_rows:
        errors.append("physical_component_log_missing")
    if not frame_lines:
        errors.append("frame_identity_log_missing")

    factor = request.get("factor")
    parsed_component_fields = [json.loads(row["fields_json"]) for row in component_rows]
    if factor == "solar_azimuth_elevation" and not any(
        "solarAz" in fields and "solarEl" in fields for fields in parsed_component_fields
    ):
        errors.append("solar_azimuth_elevation_evidence_missing")
    if factor == "active_illumination" and not any(
        row["tag"].startswith("L2 ActiveIlluminator") for row in component_rows
    ):
        errors.append("active_illuminator_component_evidence_missing")
    if factor == "visibility":
        expected_km = request.get("scenario", {}).get("expectedVisibilityKm")
        observed_km = []
        for fields in parsed_component_fields:
            if "visibilityKm" in fields:
                observed_km.append(float(fields["visibilityKm"]))
            if "visibilityM" in fields:
                observed_km.append(float(fields["visibilityM"]) / 1000.0)
        if expected_km is not None and not any(abs(value - float(expected_km)) <= 1.0e-6 for value in observed_km):
            errors.append(f"visibility_value_not_observed:{expected_km}")

    scenario = request.get("scenario", {})
    if not isinstance(scenario, dict):
        scenario = {}
        errors.append("request_scenario_not_object")
    request_seconds = integer(request.get("seconds"))
    if request_seconds is None or request_seconds <= 0:
        errors.append("request_seconds_missing_or_invalid")
    scenario_seconds = integer(scenario.get("seconds"))
    if scenario_seconds is not None and scenario_seconds != request_seconds:
        errors.append(f"per_case_seconds_mismatch:{scenario_seconds}!={request_seconds}")
    expected_frame_count = request_seconds * 60 if request_seconds is not None and request_seconds > 0 else None
    frame_boundary_tolerance = 2
    duration_boundary_tolerance_ms = 34

    # Recompute the effective dynamic/static classification from the fixture.
    # This also makes the finalizer compatible with evidence captured before the
    # runner fixed PowerShell's surprising @($null).Count == 1 behavior.
    dynamic_evidence: dict[str, object] = {
        "declaredByRequest": bool(request.get("dynamicScenario")),
        "requestedByScenario": bool(scenario.get("dynamic")),
    }
    try:
        fixture_path = Path(str(request.get("fixture", ""))).resolve()
        fixture = json.loads(fixture_path.read_text(encoding="utf-8-sig"))
        telemetry_keyframes = fixture.get("SyntheticTelemetryKeyframes") or []
        illumination_steps = fixture.get("OrdinaryIlluminationEnableSteps") or []
        timelines = [rows for rows in (telemetry_keyframes, illumination_steps) if rows]
        effective_dynamic = bool(timelines) or bool(scenario.get("dynamic"))
        dynamic_evidence.update({
            "requested": effective_dynamic,
            "fixture": str(fixture_path),
            "telemetryKeyframes": len(telemetry_keyframes),
            "illuminationSteps": len(illumination_steps),
            "requestFlagCorrected": bool(request.get("dynamicScenario")) != effective_dynamic,
        })
        if effective_dynamic:
            for rows in timelines:
                states = {json.dumps(row[1:], sort_keys=True) for row in rows if isinstance(row, list) and len(row) > 1}
                if len(states) < 2:
                    errors.append("dynamic_fixture_has_no_state_change")
                last_time = max(number(row[0]) or 0.0 for row in rows if isinstance(row, list) and row)
                if request_seconds is None or last_time > request_seconds:
                    errors.append(f"dynamic_fixture_exceeds_case_seconds:{last_time}")
    except Exception as exc:
        errors.append(f"dynamic_fixture_invalid:{exc}")

    formal_summaries = {}
    formal_chain_gates = {}
    scenario_gates = {}
    duration_evidence = {}
    expected_protocol_band = integer(request.get("protocolBand"))
    if expected_protocol_band not in (0, 1, 2):
        errors.append("request_protocol_band_not_production_supported")
    for variant in VARIANTS:
        summary_path = case_dir / "variants" / variant / "formal_tcp_run" / "phase2a_sync60_save_summary.json"
        if not summary_path.is_file():
            errors.append(f"formal_tcp_summary_missing:{variant}")
            continue
        summary = json.loads(summary_path.read_text(encoding="utf-8-sig"))
        formal_summaries[variant] = summary
        expected_agc = variant != "fixed"
        expected_annotation = variant == "annotated"
        if str(summary.get("agcEnabled", "")).lower() != str(expected_agc).lower():
            errors.append(f"agc_state_mismatch:{variant}")
        if str(summary.get("annotationOverlayInSensorImage", "")).lower() != str(expected_annotation).lower():
            errors.append(f"annotation_state_mismatch:{variant}")
        if summary.get("decodeCodec", "unknown") == "unknown" or summary.get("activeCodec", "unknown") == "unknown":
            errors.append(f"tcp_codec_evidence_missing:{variant}")
        formal_dir = summary_path.parent
        hwa_text = "\n".join(
            path.read_text(encoding="utf-8-sig", errors="replace")
            for path in (formal_dir / "hwa.out.log", formal_dir / "hwa.err.log")
            if path.is_file()
        )
        video_text = "\n".join(
            path.read_text(encoding="utf-8-sig", errors="replace")
            for path in (formal_dir / "video.out.log", formal_dir / "video.err.log")
            if path.is_file()
        )
        stim_text = "\n".join(
            path.read_text(encoding="utf-8-sig", errors="replace")
            for path in (formal_dir / "stim.out.log", formal_dir / "stim.err.log")
            if path.is_file()
        )
        gate = validate_formal_chain(
            summary, hwa_text, video_text, stim_text,
            expected_protocol_band if expected_protocol_band is not None else -1,
        )
        formal_chain_gates[variant] = gate
        errors.extend(f"formal_chain_gate:{variant}:{item}" for item in gate["errors"])

        index_rows = recording_indexes.get(variant, [])
        summary_written = integer(summary.get("writtenFrames"))
        summary_mp4_frames = integer(summary.get("mp4Frames"))
        requested_duration_ms = request_seconds * 1000 if request_seconds is not None else None
        first_duration_annotation: dict[str, object] = {}
        last_duration_annotation: dict[str, object] = {}
        if index_rows:
            recording_dir = case_dir / "variants" / variant / "receiver_recording"
            try:
                first_duration_annotation = load_producer_annotation(recording_dir, index_rows[0])
                last_duration_annotation = (
                    first_duration_annotation
                    if len(index_rows) == 1
                    else load_producer_annotation(recording_dir, index_rows[-1])
                )
            except Exception as exc:
                errors.append(f"duration_annotation_invalid:{variant}:{exc}")
        duration_metrics = duration_boundary_values(
            index_rows,
            first_duration_annotation,
            last_duration_annotation,
            requested_duration_ms,
        )
        duration_delta_ms = duration_metrics["durationDeltaMs"]
        frame_count_delta = (
            len(index_rows) - expected_frame_count if expected_frame_count is not None else None
        )
        stop_total_ms = summary.get("stopTotalMs")
        duration_evidence[variant] = {
            "requestedSeconds": request_seconds,
            "expectedFramesAt60Hz": expected_frame_count,
            "indexedFrames": len(index_rows),
            "frameCountDelta": frame_count_delta,
            "frameBoundaryTolerance": frame_boundary_tolerance,
            "summaryWrittenFrames": summary_written,
            "summaryMp4Frames": summary_mp4_frames,
            **duration_metrics,
            "durationBoundaryToleranceMs": duration_boundary_tolerance_ms,
            # STOP response, output drain and receiver/file flush are separate
            # from per-frame output latency.  Preserve the complete wall time
            # (including values over 80 ms) instead of reclassifying it as a
            # frame-latency failure.  Completion/timeout gates remain strict.
            "stopTotalMs": stop_total_ms,
            "stopTotalOver80Ms": (
                isinstance(stop_total_ms, (int, float)) and stop_total_ms > 80.0
            ),
            "rendererStopCompleted": summary.get("hwaStopCompleted"),
            "receiverFlushCompleted": summary.get("videoFlushCompleted"),
            "outputDrainTimeout": summary.get("outputDrainTimeout"),
        }
        if len(index_rows) != summary_written or len(index_rows) != summary_mp4_frames:
            errors.append(f"recording_index_count_mismatch:{variant}:{len(index_rows)}:{summary_written}:{summary_mp4_frames}")
        if (
            expected_frame_count is not None
            and (frame_count_delta is None or abs(frame_count_delta) > frame_boundary_tolerance)
        ):
            errors.append(
                f"per_case_frame_boundary_exceeded:{variant}:{frame_count_delta}:"
                f"tolerance={frame_boundary_tolerance}"
            )
        if (
            requested_duration_ms is not None
            and (duration_delta_ms is None or abs(duration_delta_ms) > duration_boundary_tolerance_ms)
        ):
            errors.append(
                f"per_case_duration_boundary_exceeded:{variant}:{duration_delta_ms}ms:"
                f"tolerance={duration_boundary_tolerance_ms}ms"
            )

        scenario_gate = validate_scenario_expectations(
            scenario, hwa_text, summary, str(request.get("band", "")), expected_agc,
            requested_source_seq,
        )
        scenario_gates[variant] = scenario_gate
        errors.extend(f"scenario_gate:{variant}:{item}" for item in scenario_gate["errors"])

        perf_rows = tagged_entries("Perf", hwa_text)
        observed_agc_enabled = maximum_field(perf_rows, "agcEnabled")
        observed_agc_samples = maximum_field(perf_rows, "agcSampleCount")
        if expected_agc:
            if observed_agc_enabled != 1:
                errors.append(f"agc_not_effective:{variant}")
            if observed_agc_samples is None or observed_agc_samples <= 0:
                errors.append(f"agc_samples_missing:{variant}")
        elif observed_agc_enabled not in (0, None):
            errors.append(f"fixed_mapping_has_agc_enabled:{variant}")

        if not isinstance(stop_total_ms, (int, float)) or stop_total_ms < 0:
            errors.append(f"stop_total_missing_or_negative:{variant}:{stop_total_ms}")
        if summary.get("hwaStopCompleted") is not True:
            errors.append(f"renderer_stop_not_completed:{variant}")
        if summary.get("videoFlushCompleted") is not True:
            errors.append(f"receiver_flush_not_completed:{variant}")
        if summary.get("outputDrainTimeout") is not False:
            errors.append(f"output_drain_timeout:{variant}")

    frame_identity = {
        "schema": "hwasimir_p11_frame_identity_1",
        "evidence": frame_lines,
        "note": "Identity rows are copied from formal producer/renderer/receiver logs; no Latest remapping is introduced by this finalizer.",
    }
    (case_dir / "frame_identity.json").write_text(json.dumps(frame_identity, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    required_names = [
        "fixed_clean.png",
        "auto_clean.png",
        "annotated.png",
        "received.png",
        "raw_radiance.pfm",
        "raw_radiance.json",
    ]
    files = {}
    for name in required_names + ["physical_components.csv", "frame_identity.json", "case_request.json"]:
        path = case_dir / name
        if not path.is_file():
            errors.append(f"missing_required_artifact:{name}")
        else:
            files[name] = {"sha256": sha256(path), "bytes": path.stat().st_size}
            if path.suffix.lower() == ".png":
                width, height = png_size(path)
                files[name].update({"width": width, "height": height})
                if (width, height) != (800, 800):
                    errors.append(f"wrong_image_size:{name}:{width}x{height}")

    manifest = {
        "schema": "hwasimir_p11_windows_case_evidence_1",
        "result": "PASS" if not errors else "FAIL",
        "finalizer": {
            "path": str(Path(__file__).resolve()),
            "sha256": sha256(Path(__file__).resolve()),
        },
        "transport": "TCP",
        "formalPrograms": ["HwaSim_IR", "DataDrivenTestQT", "HwaSim_IR_VideoDisplay"],
        "request": request,
        "rawRadiance": raw_meta,
        "decodedEvidence": evidence,
        "selectedFrame": selected_frame,
        "dynamicEvidence": dynamic_evidence,
        "durationEvidence": duration_evidence,
        "formalTcpSummaries": formal_summaries,
        "formalChainGates": formal_chain_gates,
        "scenarioGates": scenario_gates,
        "componentRows": len(component_rows),
        "frameIdentityRows": len(frame_lines),
        "files": files,
        "errors": sorted(set(errors)),
        "prohibitions": {"ordinarySensorLab": False, "syntheticImageGeneration": False, "agcAsPhysicsProof": False},
    }
    (case_dir / "case.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"result": manifest["result"], "case": str(case_dir), "errors": manifest["errors"]}, ensure_ascii=False))
    return 0 if not errors else 2


if __name__ == "__main__":
    sys.exit(main())
