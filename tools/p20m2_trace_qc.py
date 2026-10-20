"""Strict JSONL evidence validation; schema success is not physical acceptance."""
import argparse
import json
import math
from pathlib import Path

FIELDS = set("schema runId sourceSeq generation simUtcMs simRelativeSec timeValid targetKey band datasetId cellId sourceCases cameraStatus overall error inputSnapshotId input components".split())
VALID = {"VALID", "ANALYTIC_BOUNDARY", "OCCLUDED_PHYSICAL_ZERO"}
STATUS = VALID | {"NOT_VERIFIED", "OUT_OF_DOMAIN", "NUMERICAL_FAILURE", "INVALID"}
UNITS = {"tau":"1", "pathThermal":"W/(m2 sr um)", "direct":"W/(m2 um)",
         "downward":"W/(m2 um)", "pathSolarScatter":"W/(m2 sr um)",
         "surfaceTemperature":"K", "sensorRadiance":"W/(m2 sr um)"}

def unique(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("Duplicate JSON key: "+key)
        result[key] = value
    return result

def finite(value):
    return type(value) in (int, float) and math.isfinite(value)

def validate(path, expected=None):
    seen, datasets, count, complete = set(), set(), 0, 0
    with Path(path).open(encoding="utf-8") as stream:
        for line in stream:
            count += 1
            if not line.endswith("\n"):
                raise ValueError("Truncated final record")
            row = json.loads(line, object_pairs_hook=unique,
                             parse_constant=lambda v: (_ for _ in ()).throw(ValueError(v)))
            if set(row) != FIELDS or row["schema"] != "HwaSimIR.PhysicalTrace.1":
                raise ValueError("Record schema mismatch")
            if type(row["sourceSeq"]) is not int or row["sourceSeq"] <= 0 or type(row["timeValid"]) is not bool:
                raise ValueError("Invalid sequence/time validity")
            if row["band"] not in {"SWIR", "MWIR", "NIR"}:
                raise ValueError("Invalid imaging band")
            if row["overall"] not in {"COMPLETE", "PARTIAL", "INVALID", "LEGACY_DIAGNOSTIC_NOT_PHYSICS_ACCEPTED"}:
                raise ValueError("Invalid overall state")
            for field in ("runId", "targetKey", "band", "datasetId", "cellId", "cameraStatus", "overall"):
                if not isinstance(row[field], str) or not row[field]:
                    raise ValueError("Missing string identity: "+field)
            for field in ("simUtcMs", "simRelativeSec"):
                if (row["timeValid"] and not finite(row[field])) or (not row["timeValid"] and row[field] is not None):
                    raise ValueError("Clock validity mismatch")
            key = tuple(row[k] for k in ("runId", "generation", "sourceSeq", "targetKey", "band"))
            if key in seen:
                raise ValueError("Duplicate physical record")
            seen.add(key)
            datasets.add(row["datasetId"])
            if not row["input"] or not all(finite(v) for v in row["input"].values()):
                raise ValueError("Missing/nonfinite input snapshot")
            if not row["components"]:
                raise ValueError("Missing components")
            for name, component in row["components"].items():
                if set(component) != {"value", "unit", "status"} or not component["unit"] or component["status"] not in STATUS:
                    raise ValueError("Component schema mismatch: "+name)
                if name not in UNITS or component["unit"] != UNITS[name]:
                    raise ValueError("Unknown component or unit: "+name)
                if component["status"] in VALID:
                    if not finite(component["value"]):
                        raise ValueError("Nonfinite valid component: "+name)
                    if component["status"] == "OCCLUDED_PHYSICAL_ZERO" and component["value"] != 0:
                        raise ValueError("Nonzero physical zero")
                    if name == "tau" and not 0 <= component["value"] <= 1:
                        raise ValueError("Invalid transmittance")
                elif component["value"] is not None:
                    raise ValueError("Invalid component has numeric stand-in: "+name)
            if row["overall"] == "COMPLETE":
                if not {"tau", "pathThermal", "direct", "downward", "pathSolarScatter", "surfaceTemperature", "sensorRadiance"}.issubset(row["components"]):
                    raise ValueError("Incomplete COMPLETE components")
                if not all(c["status"] in VALID for c in row["components"].values()) or not row["timeValid"]:
                    raise ValueError("Invalid COMPLETE state")
                complete += 1
    if count == 0 or (expected is not None and count != expected):
        raise ValueError("Missing records: expected %s, received %d" % (expected, count))
    return dict(schema="HwaSimIR.P20M2.TraceQC.1", schema_status="PASS", records=count,
                complete_records=complete, datasets=sorted(datasets), physical_acceptance="NOT_EVALUATED")

if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("path", type=Path)
    p.add_argument("--expected", type=int)
    p.add_argument("--out", type=Path, required=True)
    a = p.parse_args()
    try:
        result = validate(a.path, a.expected)
    except (ValueError, OSError) as error:
        result = dict(schema_status="FAIL", error=str(error))
    a.out.write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(result))
    raise SystemExit(0 if result["schema_status"] == "PASS" else 2)
