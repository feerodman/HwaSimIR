"""Re-QC immutable P20M raw into a new P20M2 evidence directory; no engine calls."""
import argparse
import collections
import csv
import hashlib
import json
import math
from pathlib import Path
import re

import p20m_modtran as engine
from p20m2_geometry_qc import canonical_case, actual_geometry

ROOT = Path(__file__).resolve().parents[1]


def atmospheric_refraction_profile(tape6):
    block = tape6.split("ATMOSPHERIC PROFILES", 1)[1].split("BOUNDARY LAYER", 1)[0]
    rows = []
    for line in block.splitlines():
        fields = line.split()
        if len(fields) != 14 or not fields[0].isdigit():
            continue
        try:
            row = [float(v) for v in fields]
        except ValueError:
            continue
        if row[0] == len(rows)+1 and 100 <= row[3] <= 400 and 0 <= row[8] < .001:
            rows.append(dict(altitude_km=row[1], refractive_index=1+row[8]))
    if not rows or rows[0]["altitude_km"] != 0:
        raise ValueError("Missing actual refractivity profile")
    return rows


def occultation(case, tape6):
    """Bouguer invariant n*r*sin(apparent zenith), verified monotone n*r.

    Only classifies the legacy deck's APPARENT angle. Astronomical->apparent
    conversion is a separate gate, not silently assumed.
    """
    profile = atmospheric_refraction_profile(tape6)
    radius = float(re.search(r"([\d.]+)\s+RADIUS OF THE EARTH", tape6).group(1))
    levels = [(r["altitude_km"], r["refractive_index"]*(radius+r["altitude_km"])) for r in profile]
    if not all(b[1] > a[1] for a, b in zip(levels, levels[1:])):
        raise ValueError("Nonmonotone refractive invariant; ray tracing required")
    obs = next((x[1] for x in levels if abs(x[0]-case["target"]) < 1e-9), None)
    if obs is None:
        raise ValueError("Actual profile lacks exact observer layer")
    impact = obs*math.sin(math.radians(case["sza"]))
    margin = levels[0][1]-impact
    occluded = case["sza"] > 90 and margin > 1e-3
    return dict(status="OCCLUDED_PHYSICAL_ZERO" if occluded else "NOT_PROVEN_OCCLUDED",
                source="Bouguer_spherical_refractive_invariant_actual_MLS_profile",
                angle_semantics="legacy_apparent_zenith_at_target", radius_km=radius,
                impact_invariant_km=impact, surface_invariant_km=levels[0][1],
                blocking_margin_km=margin, profile=profile,
                astronomical_mapping="NOT_VERIFIED", component="direct_only")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    out.relative_to(ROOT/"logs/p20m2")
    out.mkdir(parents=True, exist_ok=True)
    reviews, failures = [], []
    for folder in ("20261021_full_domain", "20261021_geometry_repair", "20261021_solar_repair"):
        source = ROOT/"logs/p20m"/folder
        manifest = engine.read_json(source/"case_manifest.json")
        original = {x["case_id"]: x for x in engine.read_json(source/"pilot_qc.json")["cases"]}
        for legacy in manifest["cases"]:
            case = canonical_case(legacy)
            raw = source/"raw"/case["case_id"]
            record = original[case["case_id"]]
            checkpoint = engine.read_json(raw/"checkpoint.json")
            assert engine.digest(case["input_file"]) == case["input_sha256"]
            assert checkpoint["input_sha256"] == case["input_sha256"]
            for name, sha in checkpoint["output_sha256"].items():
                assert engine.digest(raw/name) == sha, "Changed raw: "+str(raw/name)
            tape6 = (raw/"MODOUT1.txt").read_text(encoding="utf-8", errors="replace")
            geometry = actual_geometry(case, tape6)
            review = dict(task=folder, case_id=case["case_id"], canonical_case=case,
                          original_status=record["status"], raw=str(raw),
                          raw_hashes=checkpoint["output_sha256"], geometry=geometry,
                          gates=dict(input="PASS_SCHEMA_AND_HASH", engine="RAW_IDENTITY_VERIFIED",
                                     geometry=geometry["status"], source="NOT_VERIFIED",
                                     cell="NOT_VERIFIED", accuracy="NOT_VERIFIED", release="NOT_ELIGIBLE"),
                          warnings=(raw/"warnings.txt").read_text(encoding="utf-8", errors="replace") if (raw/"warnings.txt").exists() else "")
            try:
                review["spectrum"] = engine.qc_case(case, raw)
                review["gates"]["spectrum"] = "PASS"
            except ValueError as exc:
                review["gates"]["spectrum"] = "FAIL"
                review["error"] = str(exc)
            if record["status"] == "FAIL":
                failure = dict(task=folder, original_case=case["case_id"],
                               original_error=record["error"], evidence=str(raw/"MODOUT1.txt"),
                               raw_sha256=engine.digest(raw/"MODOUT1.txt"), new_case_id="",
                               release_eligible=False)
                if "NaN" in record["error"]:
                    failure.update(classification="NEAR_FIELD_LAYER_ABSORBER_NAN",
                                   closure="OPEN_NEEDS_VALIDATED_TRANSPORT_METHOD",
                                   basis="actual refracted layer absorber output contains NaN; no zero substitution")
                elif case["mode"] == "DirectSolarIrradiance" and "IERROR" in record["error"]:
                    proof = occultation(case, tape6)
                    review["occultation_proof"] = proof
                    failure.update(classification=proof["status"],
                                   closure="CLOSED_DIRECT_LEGACY_APPARENT_ANGLE_ONLY" if proof["status"] == "OCCLUDED_PHYSICAL_ZERO" else "OPEN",
                                   basis="actual-profile Bouguer invariant; blocking margin %.6f km" % proof["blocking_margin_km"],
                                   new_case_id=hashlib.sha256(json.dumps(proof, sort_keys=True).encode()).hexdigest())
                else:
                    failure.update(classification="UNCLASSIFIED", closure="OPEN", basis="needs diagnosis")
                failures.append(failure)
            reviews.append(review)
    assert len(failures) == 52
    engine.atomic_json(out/"raw_review.json", dict(
        schema="HwaSimIR.P20M2.RawReview.1", engine_invocations=0,
        qc_sha256={name: engine.digest(ROOT/"tools"/name) for name in
                   ("p20m2_review.py", "p20m2_geometry_qc.py", "p20m_modtran.py", "modtran_convert_to_si.py", "modtran_flux.py")},
        count=len(reviews), original_spectrum_pass_count=sum(x["original_status"] == "PASS" for x in reviews),
        geometry_counts=dict(collections.Counter(x["geometry"]["status"] for x in reviews)), cases=reviews))
    engine.atomic_json(out/"failure_closure.json", failures)
    with (out/"failure_closure.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(failures[0]))
        writer.writeheader()
        writer.writerows(failures)
    print(json.dumps(dict(reviewed=len(reviews), original_failures=len(failures), engine_invocations=0,
                          closure=dict(collections.Counter(x["closure"] for x in failures)),
                          geometry=dict(collections.Counter(x["geometry"]["status"] for x in reviews)))))


if __name__ == "__main__":
    main()
