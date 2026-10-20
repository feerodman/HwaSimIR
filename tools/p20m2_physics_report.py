"""Verify immutable pilot evidence and report independent gates; never publish."""
import argparse
import collections
import csv
import json
from pathlib import Path
import re
import p20m_modtran as engine
from p20m2_geometry_qc import actual_geometry

def collect(root):
    rows, timings = [], collections.defaultdict(list)
    tasks = ("g1_v2", "g1_directional", "g1_vertical", "g1_clear")
    for task in tasks:
        base = root/task
        for case in engine.read_json(base/"case_manifest.json")["cases"]:
            raw = base/"raw"/case["case_id"]
            checkpoint = engine.read_json(raw/"checkpoint.json")
            if engine.digest(case["input_file"]) != case["input_sha256"]:
                raise ValueError("Input hash changed")
            for name, sha in checkpoint["output_sha256"].items():
                if engine.digest(raw/name) != sha:
                    raise ValueError("Output hash changed: "+str(raw/name))
            text = (raw/"MODOUT1.txt").read_text(errors="replace")
            geometry = actual_geometry(case, text)
            card2 = re.search(r"CARD 2\s+\*+\s*(.*)", text)
            actual_cloud = int(card2.group(1).split()[4]) if card2 else None
            result = dict(task=task, case=case, raw=str(raw),
                          raw_sha256=checkpoint["output_sha256"], geometry=geometry,
                          engine_seconds=checkpoint["elapsed_seconds"],
                          engine_attempt=checkpoint["attempt"],
                          release_eligible=False, source_qc="NOT_VERIFIED",
                          accuracy="NOT_VERIFIED", cell="NOT_DECLARED",
                          actual_cloud_model=actual_cloud,
                          template_consistency="FAIL_HIDDEN_CIRRUS" if actual_cloud == 18 else "CLOUD_BASELINE_MATCH",
                          actual_solar_source="LSUNFL=F; SUN01kurucz1997.dat; see source_audit.json",
                          prior_template_metadata_correction="2005 label was incorrect; immutable inputs/raw unchanged",
                          warnings=[line.strip() for line in text.splitlines()
                                    if re.search(r"WARNING|TRLAY:|RESET TO|IERROR|FTRANG", line)])
            try:
                qc = engine.qc_case(case, raw)
                result.update(spectrum="PASS", band_value=qc)
            except ValueError as exc:
                result.update(spectrum="FAIL", error=str(exc))
            timings[case["mode"]].append(checkpoint["elapsed_seconds"])
            rows.append(result)
    result = dict(schema="HwaSimIR.P20M2.DevelopmentPhysics.1",
                  status="G1_NOT_ACCEPTED_NO_RUNTIME_DATASET", case_count=len(rows),
                  spectrum_counts=dict(collections.Counter(r["spectrum"] for r in rows)),
                  geometry_counts=dict(collections.Counter(r["geometry"]["status"] for r in rows)),
                  seconds_by_mode={k:dict(count=len(v),mean=sum(v)/len(v),maximum=max(v)) for k,v in timings.items()},
                  raw_bytes=sum(p.stat().st_size for sub in tasks
                                for p in (root/sub/"raw").rglob("*") if p.is_file()),
                  full_domain_eta="NOT_ESTIMABLE_BEFORE_VALIDATED_CELL_DESIGN",
                  independent_holdout_count=0, release_eligible_cases=0,
                  cases=rows)
    engine.atomic_json(root/"g1_physics_report.json", result)
    # Preserve full precision for solver/source comparisons, including thermal-only.
    solar = []
    for row in rows:
        c = row["case"]
        if c["mode"] == "SpectralFlux" and row["spectrum"] == "PASS":
            q = row["band_value"]
            solar.append(dict(band=c["band"], altitude_km=c["target"], solar=c["solar"],
                              sza_deg=c["sza"], solver=c["solver"], case_id=c["case_id"],
                              qc_json=json.dumps(q, sort_keys=True),
                              warning_count=len(row["warnings"])))
    with (root/"solar_solver_comparison.csv").open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(solar[0]))
        w.writeheader(); w.writerows(solar)
    print(json.dumps({k:v for k,v in result.items() if k!="cases"}, indent=2))

if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--root", type=Path, required=True)
    a = p.parse_args()
    collect(a.root.resolve())
