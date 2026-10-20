"""Read-only P20 MODTRAN deployment and supplied-log inventory.

SSH authentication is supplied by the caller. Remote files and processes are
never changed. Evidence is written beneath logs/p20/1020_modtran_audit.
"""
import collections
import concurrent.futures
import csv
import hashlib
import json
import pathlib
import re
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / "logs/p20/1020_modtran_audit"
OUT.mkdir(parents=True, exist_ok=True)
CANDIDATE = "/userdata/HwaSimIR/candidates/P20_aarch64_40_prefix_prewarm"
SCAN = r'''
import pathlib,hashlib,json
def scan(root):
    root=pathlib.Path(root)
    result={}
    for p in sorted(root.rglob('*')):
        if not p.is_file(): continue
        rel=p.relative_to(root).as_posix()
        result[rel]={'bytes':p.stat().st_size}
        if rel in ('processed/band_lut_si.csv','processed/solar_heating_lut_si.csv','processed/p14_coverage_manifest.json'):
            result[rel]['sha256']=hashlib.sha256(p.read_bytes()).hexdigest()
    return result
'''

def remote():
    code = SCAN + "\nresult={}\n"
    code += f"base=pathlib.Path({CANDIDATE!r})\n"
    code += """
for role in ('coarse','precise'):
    b=base/role
    result[role]={'inventory':scan(b/'Config/Atmosphere/MODTRAN'),
                  'runtime':(b/'Config/HwaSimIRRuntime.ini').read_text(),
                  'version':(b/'Config/deployment_version.env').read_text(),
                  'elfSha256':hashlib.sha256((b/'HwaSim_IR').read_bytes()).hexdigest()}
print(json.dumps(result))
"""
    p = subprocess.run(
        ["ssh", "-o", "UpdateHostKeys=no", "-o", "ConnectTimeout=10",
         "-o", "NumberOfPasswordPrompts=1", "root@192.168.1.116", "python3 -"],
        input=code, capture_output=True, text=True, encoding="utf-8", timeout=180)
    if p.returncode:
        raise RuntimeError(p.stderr)
    return json.loads(p.stdout)

def local():
    scope = {}
    exec(SCAN, scope)
    return scope["scan"](ROOT / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN")

with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    remote_future = pool.submit(remote)
    local_inventory = local()
    board = remote_future.result()

(OUT / "local_inventory.json").write_text(json.dumps(local_inventory, indent=2), encoding="utf-8")
(OUT / "board_inventory.json").write_text(json.dumps(board, indent=2), encoding="utf-8")
summary = {"comparison": {}, "logs": {}, "lut": {}}
for role, data in board.items():
    inventory = data["inventory"]
    summary["comparison"][role] = {
        "localFiles": len(local_inventory), "boardFiles": len(inventory),
        "localBytes": sum(x["bytes"] for x in local_inventory.values()),
        "boardBytes": sum(x["bytes"] for x in inventory.values()),
        "onlyLocal": sorted(local_inventory.keys()-inventory.keys()),
        "onlyBoard": sorted(inventory.keys()-local_inventory.keys()),
        "sizeDifferences": [k for k in local_inventory.keys() & inventory.keys()
                            if local_inventory[k]["bytes"] != inventory[k]["bytes"]],
        "runtimeDataHashes": {k: {"local": v["sha256"], "board": inventory.get(k, {}).get("sha256")}
                              for k, v in local_inventory.items() if "sha256" in v},
        "hashScope": "Three named runtime data/identity files; others compared by name and size."
    }
for path in sorted((ROOT / "1020测试内容").glob("*/*.log")):
    counts = collections.Counter()
    samples = {}
    failures = collections.Counter()
    for n, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        for tag in ("[M1 FormalFailClosed]", "[L1 SolarHeatingLut]", "[M1 Compare]",
                    "[M1 MWIR EnvironmentRadiance]", "[FormalReferenceCommit]",
                    "[CameraControl][WARN]", "[M1 Formal CloudRadiance]"):
            if tag in line:
                counts[tag] += 1
                samples.setdefault(tag, [])
                if len(samples[tag]) < 3:
                    samples[tag].append({"line": n, "text": line})
        if "[M1 FormalFailClosed]" in line:
            reason = re.search(r"fallbackReason=(\S+).*fallbackAxis=(\S+)", line)
            if reason:
                failures["/".join(reason.groups())] += 1
    summary["logs"][str(path.relative_to(ROOT))] = {
        "counts": dict(counts), "failureReasons": dict(failures), "firstSamples": samples}

for name in ("band_lut_si.csv", "solar_heating_lut_si.csv"):
    p = ROOT / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed" / name
    rows = list(csv.DictReader(p.open(encoding="utf-8-sig", newline="")))
    groups = collections.defaultdict(list)
    rejected = collections.Counter()
    required = ["tau_up", "direct_solar_irradiance_at_target_W_m2_um",
                "downward_sky_diffuse_irradiance_W_m2_um", "los_path_scattering_radiance_W_m2_sr_um"]
    for row in rows:
        if name == "band_lut_si.csv":
            fields = required + (["path_thermal_W_m2_sr_um"] if row["band"] in ("SWIR", "MWIR") else [])
            if any(not row[k] for k in fields):
                rejected[row["band"]] += 1
                continue
        groups[(row["band"], row["humidity_profile"])].append(row)
    summary["lut"][name] = {"csvRows": len(rows), "missingComponentRows": dict(rejected), "groups": {}}
    for key, rr in groups.items():
        axes = {}
        for axis in ("observer_alt_km", "target_alt_km", "range_km", "visibility_km", "solar_zenith_deg"):
            values = sorted({float(row[axis]) for row in rr if row.get(axis)})
            if values:
                axes[axis] = values
        summary["lut"][name]["groups"]["/".join(key)] = {"rows": len(rr), "axisValues": axes}
(OUT / "audit_summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps({k: v for k, v in summary.items() if k != "logs"}, ensure_ascii=False, indent=2))
print("Log counts:", json.dumps({k: v["counts"] for k, v in summary["logs"].items()}))
