"""Aggregate actual execution evidence. Missing physical gates never become PASS."""
import argparse
import collections
from pathlib import Path
import subprocess
import p20m_modtran as engine

def collect(root):
    tasks=("g1_v2","g1_directional","g1_vertical","g1_clear","g1_nearfield",
           "g1_nearfield_p1","g1_nearfield_p1_swir")
    counts=collections.Counter(); inputs=set(); executed=0; rows=[]
    for task in tasks:
        base=root/task
        manifest=engine.read_json(base/"case_manifest.json")
        for case in manifest["cases"]:
            raw=base/"raw"/case["case_id"]
            cp=engine.read_json(raw/"checkpoint.json")
            if cp["input_sha256"]!=case["input_sha256"] or engine.digest(case["input_file"])!=case["input_sha256"]:
                raise ValueError("Input identity mismatch")
            for name,sha in cp["output_sha256"].items():
                if engine.digest(raw/name)!=sha: raise ValueError("Raw identity mismatch")
            counts[cp["status"]]+=1;inputs.add(case["input_sha256"]);executed+=cp["attempt"]
            rows.append(dict(task=task,case_id=case["case_id"],input_sha256=case["input_sha256"],
                             checkpoint=str(raw/"checkpoint.json"),status=cp["status"]))
    builds={}
    for name,path in (
        ("windows_renderer",engine.ROOT/"build-p20m2-renderer/bin/HwaSim_IR.exe"),
        ("windows_stim",engine.ROOT/"build-p20m2-stim/release/DataDrivenTestQT.exe"),
        ("windows_direct_sender",engine.ROOT/"build-p20m2-stim/release/P20MDirectDds.exe"),
        ("aarch64_renderer",root/"build/aarch64_geometry_cache/HwaSim_IR.aarch64")):
        builds[name]=dict(status="BUILT" if path.is_file() else "NOT_BUILT",
                          path=str(path),sha256=engine.digest(path) if path.is_file() else None)
    receipt=engine.read_json(root/"build/aarch64_geometry_cache/build_receipt.json")
    if receipt["elf_sha256"]!=builds["aarch64_renderer"]["sha256"]:
        raise ValueError("ELF receipt mismatch")
    source_count=0
    for line in (root/"build/aarch64_geometry_cache/source_manifest.sha256").read_text(encoding="utf-8").splitlines():
        sha,name=line.split("  ",1)
        path=engine.ROOT/name if name.startswith(("DDS/","Shared/")) else engine.ROOT/"HwaSim_IR/HwaSim_IR"/name
        if engine.digest(path)!=sha: raise ValueError("Source changed since build: "+name)
        source_count+=1
    builds["source_receipt"]=dict(status="MATCH_CURRENT_SOURCES",files=source_count,
                                  manifest_sha256=engine.digest(root/"build/aarch64_geometry_cache/source_manifest.sha256"))
    windows_log=root/"build/windows_geometry_cache.log"
    qt_log=root/"build/windows_isolated.log"
    for log in (windows_log,qt_log):
        raw=log.read_bytes()
        text=raw.decode("utf-16" if raw[:2] in (b"\xff\xfe",b"\xfe\xff") else "utf-8",errors="replace")
        if "Stage 0 build completed." not in text:
            raise ValueError("Windows build completion missing: "+str(log))
    builds["windows_logs"]={str(p):engine.digest(p) for p in (windows_log,qt_log)}
    engine.atomic_json(root/"build/build_receipt.json",builds)
    g0=engine.read_json(root/"g0/raw_review.json")
    closure=engine.read_json(root/"g0/failure_closure.json")
    result=dict(schema="HwaSimIR.P20M2.Acceptance.1",run_id=root.name,overall="INCOMPLETE_NOT_DELIVERED",
                git_head=subprocess.check_output(["git","rev-parse","HEAD"],cwd=engine.ROOT,text=True).strip(),
                original_raw_reviewed=g0["count"],original_spectrum_pass_reviewed=g0["original_spectrum_pass_count"],
                original_failures=dict(collections.Counter(x["closure"] for x in closure)),
                new_case_runs=len(rows),unique_input_cards=len(inputs),engine_attempts=executed,
                spectrum_checkpoint_counts=dict(counts),builds=builds,
                gates=dict(
                    data_generation="DEVELOPMENT_CASES_EXECUTED_NOT_FULL_PRODUCTION",
                    production_dataset_identity="NOT_CREATED",
                    continuous_domain="NOT_IMPLEMENTED",
                    independent_accuracy="NOT_RUN_NO_FROZEN_COMPONENT_BUDGET_OR_VALIDATED_CELLS",
                    physical_composition="NOT_VERIFIED",
                    simulation_clock="UNIT_TESTED_WINDOWS_AND_BOARD_NOT_END_TO_END",
                    camera_contract="NOT_RUN_END_TO_END",
                    physical_trace="THREADED_SCHEMA_TESTED_NOT_RENDERED_FRAME_EVIDENCE",
                    windows="BUILD_PASS_PHYSICAL_RENDER_NOT_VERIFIED",
                    board_dual="NOT_RUN_NO_ACCEPTED_NEW_DATASET",
                    candidate_deployment="NOT_PERFORMED_NO_ACCEPTED_NEW_DATASET",
                    customer_csv="ALL_ROWS_INVENTORIED_FORMAL_NEW_DATA_REGRESSION_NOT_RUN",
                    transport="NOT_RUN_THIS_PHASE",
                    performance_60fps="NOT_RUN_NO_COMPLETE_NEW_PHYSICAL_PATH",
                    customer_software="NOT_RUN",
                    calibration="NOT_VERIFIED_CALIBRATION"),
                performance_contract=dict(input_hz=60,video_fps=60,maximum_latency_ms=80,
                                          test_100hz=False,achieved_fps=None),
                issues=[
                    "Local spectral near-field extension has unresolved radiance error; higher spectral resolution does not establish acceptance",
                    "Hidden ICLD=18 flux cloud was found and removed from the new common gas/aerosol template; twilight validation remains incomplete",
                    "Actual default solar source is Kurucz 1997; old experiment metadata mislabeled 2005, corrected by raw TOA comparison",
                    "A/B/C/D/E production schema, effective cells, independent background and shader integration remain implementation work",
                    "No claim that MODTRAN5 has no possible solution; current tested methods do not complete the required physical gates"],
                evidence_cases=rows)
    engine.atomic_json(root/"acceptance.json",result)
    engine.atomic_json(root/"coverage_manifest.json",dict(
        schema="HwaSimIR.P20M2.DevelopmentCoverage.1",status="NOT_RUNTIME_DATASET",
        requested=dict(observer_altitude_km=[3,20],target_altitude_km=[3,20],range_km=[0,50],sza_deg=[0,180]),
        accepted_cells=[],continuous_coverage_proved=False,release_eligible=False))
    print({k:v for k,v in result.items() if k not in ("evidence_cases","builds")})
    return 2

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--root",type=Path,required=True)
    raise SystemExit(collect(p.parse_args().root.resolve()))
