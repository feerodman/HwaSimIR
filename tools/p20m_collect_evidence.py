"""Collect executed P20M evidence without promoting pilot data to a release."""
import csv
import hashlib
import json
from pathlib import Path
import re
import statistics

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "logs/p20m/20261021_full_domain/evidence"
OUT.mkdir(parents=True, exist_ok=True)


def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def save(name, value):
    path = OUT / name
    temp = path.with_suffix(path.suffix + ".tmp")
    temp.write_text(json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False), encoding="utf-8")
    temp.replace(path)


official = []
components = []
for folder in ("20261021_full_domain", "20261021_geometry_repair", "20261021_solar_repair"):
    task = ROOT / "logs/p20m" / folder
    qc = read(task / "pilot_qc.json")
    cases = {x["case_id"]: x for x in read(task / "case_manifest.json")["cases"]}
    official.append(dict(
        task=folder, counts=qc["counts"], component_cases=qc["component_count"],
        engine_wall_seconds=sum(x.get("elapsed_seconds", 0) for x in qc["cases"]),
        raw_evidence_bytes=sum(p.stat().st_size for p in (task / "raw").rglob("*") if p.is_file()),
        timing_by_mode=qc["seconds_by_mode"],
        qc_sha256=hashlib.sha256((task / "pilot_qc.json").read_bytes()).hexdigest(),
        continuous_domain="NOT_VERIFIED", independent_holdout_states=0))
    for result in qc["cases"]:
        if result["status"] != "PASS":
            continue
        case = cases[result["case_id"]]
        measurement = result["qc"]
        components.append(dict(
            task=folder, case_id=case["case_id"], band=case["band"], mode=case["mode"],
            component=measurement["component"], value=measurement["value"],
            response=measurement["response"], response_lo_um=measurement["response_um"][0],
            response_hi_um=measurement["response_um"][1],
            observer_km=case.get("observer", case.get("requested_observer", "")),
            target_km=case.get("target", ""), range_km=case.get("range", case.get("requested_range", "")),
            RH=case.get("rh", ""), visibility_km=case.get("visibility", ""),
            SZA_deg=case.get("sza", ""), relative_azimuth_deg=case.get("relative_azimuth", ""),
            input_sha256=case["input_sha256"], status=measurement["status"],
            runtime_eligible=False))
save("official_development_summary.json", official)
with (OUT / "pilot_components_NOT_RUNTIME.csv").open("w", newline="", encoding="utf-8") as f:
    writer = csv.DictWriter(f, fieldnames=list(components[0]))
    writer.writeheader()
    writer.writerows(components)

runs = []
physical = []
for task in sorted((ROOT / "logs/p20/runs").glob("p20m_20261021_*")):
    if not (task / "result.json").exists():
        continue
    result = read(task / "result.json")
    run = dict(name=task.name, result_sha256=hashlib.sha256((task / "result.json").read_bytes()).hexdigest(),
               transport_status=result["status"], input_hz=result.get("inputPacketHz"),
               configured_video_fps=result.get("configuredVideoFps"),
               policy_restored=result.get("systemPolicyRestored"), streams={})
    perf = read(task / "performance.json") if (task / "performance.json").exists() else {}
    run["performance_status"] = perf.get("performanceStatus", "NOT_ANALYZED")
    for role in ("coarse", "precise"):
        if role not in result:
            continue
        rx = result[role]
        board = task / role / "board/board.log"
        text = board.read_text(encoding="utf-8", errors="replace") if board.exists() else ""
        p = perf.get("streams", {}).get(role, {})
        stream = dict(render_fps=p.get("renderFps"), queue_peak=p.get("maximumPendingInputFrames"),
                      latency_ms=p.get("windowsStimulusToDecodedImageMs"),
                      conservation=p.get("queueConservation"),
                      decoded=rx.get("decodedFrames"), saved=rx.get("savedFramesVerified"),
                      utc_evidence=[x for x in text.splitlines() if "[M1 SolarPosition]" in x][:2],
                      invalid_reference_evidence=[x for x in text.splitlines() if "reject_reference_no_id_alias" in x],
                      m1_records_sampled=True, physical_acceptance="NOT_ACCEPTED_UNCHANGED_P14")
        run["streams"][role] = stream
        for line in text.splitlines():
            if "[M1 Compare]" not in line:
                continue
            values = dict(re.findall(r"(\w+)=([^\s]+)", line))
            values.update(run=task.name, role=role, evidence="rate_limited_runtime_diagnostics",
                          missing_values_policy="no_imputation", physical_acceptance="NOT_ASSESSED")
            physical.append(values)
    runs.append(run)
save("board_runs_summary.json", runs)
if physical:
    keys = sorted({key for row in physical for key in row})
    with (OUT / "physical_components_sampled.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=keys)
        writer.writeheader()
        writer.writerows(physical)
print(json.dumps(dict(official_component_cases=sum(x["component_cases"] for x in official),
                      pilot_spectrum_pass_rows=len(components), board_runs=len(runs),
                      sampled_physical_records=len(physical), release="NOT_ACCEPTED")))
