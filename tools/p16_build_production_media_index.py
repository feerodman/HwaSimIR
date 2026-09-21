#!/usr/bin/env python3
"""Build the P16 production-renderer media/evidence index.

The index is deliberately descriptive.  Controlled renderer fixtures and an
immutable-original replay are not relabelled as user ordinary-entry evidence,
and independent graphics sample results remain outside this index.
"""

from __future__ import annotations

import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RUN_ROOT = ROOT / "logs" / "p16" / "production_effects" / "runs"
OUT = ROOT / "logs" / "p16" / "production_effects" / "media_index.json"


def read_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8-sig") as stream:
        return json.load(stream)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def file_record(path: Path | None) -> dict | None:
    if path is None or not path.is_file():
        return None
    return {
        "path": str(path.resolve()),
        "bytes": path.stat().st_size,
        "sha256": sha256(path),
    }


def classification(case_name: str) -> tuple[str, bool, bool]:
    if "original" in case_name:
        return "immutable_original_replay_with_diagnostic_camera", False, True
    return "controlled_production_renderer_fixture", False, False


OBSERVATIONS = {
    "p16_final_mwir_plume_on_effects7": (
        "Hot core is visible with the actual priority-100 sprite path; signed raw "
        "comparison retains the surrounding cold-smoke absorption."
    ),
    "p16_final_mwir_plume_off_effects7": "Same-timeline generic-nozzle Off baseline.",
    "p16_final_mwir_rain_effects7": (
        "Moving Weather-texture streaks are present; fixed-mapping still frames are "
        "low contrast, while signed raw comparison resolves the streaks."
    ),
    "p16_final_mwir_snow_effects7": "Moving cold snow absorption is present in MWIR.",
    "p16_final_swir_snow_effects7": "Moving cold snow flecks are directly discernible in SWIR.",
    "p16_final_swir_clear_effects7": "Same-timeline SWIR clear baseline.",
    "p16_final_mwir_cloud_original_effects7": (
        "Unmodified 1.txt replay loads cloud_scattered.png and reaches two selected "
        "visible cloud volumes; fixed-mapping contrast remains subtle."
    ),
    "p16_final_mwir_clear_original_effects7": "Unmodified 1.txt clear baseline.",
}


RAW_COMPARISONS = [
    ("plume_on_minus_off", "raw_qc/plume_mwir_effects7/seq900_signed_raw_qc.json"),
    ("mwir_rain_minus_clear", "raw_qc/mwir_rain_effects7/seq900_signed_raw_qc.json"),
    ("mwir_snow_minus_clear", "raw_qc/mwir_snow_effects7/seq900_signed_raw_qc.json"),
    ("swir_snow_minus_clear", "raw_qc/swir_snow_effects7/seq900_signed_raw_qc.json"),
    ("mwir_cloudy_minus_clear_original", "raw_qc/mwir_cloud_original_effects7/seq900_signed_raw_qc.json"),
]


def case_entry(case_dir: Path) -> dict:
    result = read_json(case_dir / "case_result.json")
    probe = read_json(case_dir / "media_probe.json")
    stream = probe["streams"][0]
    case_name = result["name"]
    evidence_class, ordinary_entry, immutable_original = classification(case_name)

    mp4 = Path(result["products"]["mp4"])
    recording_status = Path(result["products"]["recordingStatus"])
    frame_index = Path(result["products"]["frameIndex"])
    products = {
        "mp4": file_record(mp4),
        "receivedH264": file_record(case_dir / "received.h264"),
        "contactSheet": file_record(case_dir / "contact_6x.png"),
        "fixedMappingFrame900": file_record(case_dir / "frame_0900.png"),
        "mediaProbe": file_record(case_dir / "media_probe.json"),
        "recordingStatus": file_record(recording_status),
        "frameIdentityIndex": file_record(frame_index),
        "boardLog": file_record(case_dir / "board.log"),
    }
    products = {key: value for key, value in products.items() if value is not None}

    decoded_frames = int(stream["nb_read_frames"])
    expected_frames = int(result["inputRows"])
    advertised_hash = result["products"]["mp4Sha256"].lower()
    actual_hash = products["mp4"]["sha256"].lower()
    preflight = {
        item.split("=", 1)[0]: item.split("=", 1)[1]
        for item in result.get("boardPreflight", [])
        if "=" in item
    }

    if "plume_on" in case_name:
        switch_events = [{"sourceSeq": 1, "event": "genericHeatSource=On"}]
    elif "plume_off" in case_name:
        switch_events = [{"sourceSeq": 1, "event": "genericHeatSource=Off"}]
    else:
        switch_events = [{"sourceSeq": 1, "event": f"weatherProfile={result['weather']}"}]

    return {
        "caseId": case_name,
        "evidenceClass": evidence_class,
        "fromUserOrdinaryEntry": ordinary_entry,
        "immutableOriginalInput": immutable_original,
        "band": result["band"],
        "weather": result["weather"],
        "inputMode": result["inputMode"],
        "input": result["input"],
        "inputSha256": result["inputSha256"],
        "targetTypeProtocolCode": result["targetTypeProtocolCode"],
        "resolution": result["resolution"],
        "materialView": result["materialView"],
        "ddsOnly": result["ddsOnly"],
        "identity": result["identity"],
        "programIdentity": {
            "boardElfSha256": preflight.get("ElfSha256"),
            "boardBuildId": preflight.get("BuildId"),
            "runtimeConfigSha256": preflight.get("RuntimeConfigSha256"),
            "configManifestSha256": preflight.get("ConfigManifestSha256"),
            "formalLutSha256": preflight.get("FormalLutSha256"),
            "coverageManifestSha256": preflight.get("CoverageManifestSha256"),
            "senderExeSha256": result["senderExeSha256"],
            "receiverExeSha256": result["receiverExeSha256"],
        },
        "timeBase": {
            "source": "accepted DDS source timestamp",
            "simulation": "source time in ms; particle phase is generation-relative",
            "videoPts": "receiver packet arrival/encoder PTS recorded in frameIdentityIndex",
            "reportedAverageFrameRate": stream["avg_frame_rate"],
            "durationSeconds": float(stream["duration"]),
        },
        "switchEvents": switch_events,
        "decode": {
            "codec": stream["codec_name"],
            "width": int(stream["width"]),
            "height": int(stream["height"]),
            "decodedFrames": decoded_frames,
            "expectedAcceptedRows": expected_frames,
            "frameCountMatches": decoded_frames == expected_frames,
            "mp4HashMatchesCaseReceipt": actual_hash == advertised_hash,
            "decodable": decoded_frames > 0 and decoded_frames == expected_frames,
        },
        "observation": OBSERVATIONS[case_name],
        "products": products,
    }


def comparison_entry(name: str, relative_path: str) -> dict:
    path = ROOT / "logs" / "p16" / "production_effects" / relative_path
    report = read_json(path)
    entry = {
        "comparisonId": name,
        "report": file_record(path),
        "status": report["status"],
        "mode": report["mode"],
        "sequence": report["sequence"],
        "comparison": report["comparison"],
        "acceptanceBasis": report["acceptanceBasis"],
        "roiSignedMean": report["roi"]["signedMean"],
        "roiPositiveSamples": report["roi"]["positiveSamples"],
        "roiNegativeSamples": report["roi"]["negativeSamples"],
        "visual": report["visual"],
    }
    if name == "plume_on_minus_off":
        entry["coreRoi"] = {
            "signedMean": report["coreRoi"]["signedMean"],
            "signedSum": report["coreRoi"]["signedSum"],
            "maximum": report["coreRoi"]["maximum"],
            "positiveToNegativeMagnitudeRatio": report["coreRoi"]["positiveToNegativeMagnitudeRatio"],
        }
    return entry


def main() -> int:
    case_dirs = sorted(
        path.parent
        for path in RUN_ROOT.glob("p16_final_*_effects7/case_result.json")
    )
    cases = [case_entry(case_dir) for case_dir in case_dirs]
    comparisons = [comparison_entry(name, rel) for name, rel in RAW_COMPARISONS]
    index = {
        "schema": "HwaSimIR.P16.ProductionEffectsMediaIndex.1",
        "createdUtc": datetime.now(timezone.utc).isoformat(),
        "scope": (
            "Actual HwaSim_IR RK3588 production renderer and external production resources; "
            "controlled fixtures and diagnostic cameras are explicitly classified."
        ),
        "calibrationStatus": "NOT_VERIFIED_CALIBRATION",
        "claims": {
            "allMp4DecodableWithExpectedFrames": all(
                case["decode"]["decodable"] for case in cases
            ),
            "allCasesUseSameElf": len(
                {case["programIdentity"]["boardElfSha256"] for case in cases}
            ) == 1,
            "userOrdinaryEntryAcceptance": False,
            "independentSampleClosesProductionIssues": False,
        },
        "cases": cases,
        "signedRawComparisons": comparisons,
        "modelSamplingComparison": file_record(
            ROOT / "logs" / "p16" / "production_effects" / "model_flicker" / "comparison.json"
        ),
        "readOnlyHistoricalFlickerAudit": file_record(
            ROOT / "logs" / "p16" / "flicker_audit" / "p16_flicker_audit.json"
        ),
        "independentSampleIndex": file_record(
            ROOT / "logs" / "p16" / "independent_sample" / "media_index.json"
        ),
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(index, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(f"wrote {OUT}")
    print(f"cases={len(cases)} comparisons={len(comparisons)}")
    print(f"allMp4DecodableWithExpectedFrames={index['claims']['allMp4DecodableWithExpectedFrames']}")
    print(f"allCasesUseSameElf={index['claims']['allCasesUseSameElf']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
