"""Package code-validation binaries, unchanged P14 data and P20M evidence.

This is an overlay/checkpoint requiring existing P20 assets and dependencies.
It is deliberately not a full-domain physics release or standalone installer.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
TASK = ROOT / "logs/p20m/20261021_full_domain"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
out = args.output.resolve()
out.relative_to(ROOT / "logs/p20m")
out.mkdir(parents=True, exist_ok=False)
receipt = json.loads((TASK / "deployment/receipt.json").read_text(encoding="utf-8"))


def copy(source, target):
    destination = out / target
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


copy(ROOT / "HwaSim_IR/Bin/HwaSim_IR.exe", "program/windows/HwaSim_IR.exe")
copy(ROOT / "build-p20m-stim/release/DataDrivenTestQT.exe", "program/windows/DataDrivenTestQT.exe")
copy(ROOT / "build-p20m-stim/release/P20MDirectDds.exe", "program/windows/P20MDirectDds.exe")
copy(TASK / "aarch64_build/HwaSim_IR.aarch64", "program/aarch64/HwaSim_IR")
assert sha(out / "program/aarch64/HwaSim_IR") == receipt["build"]["elf_sha256"]
for name, expected in receipt["instances"]["coarse"]["data"].items():
    source = ROOT / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed" / name
    assert sha(source) == expected, "Local/board dataset differs: " + name
    copy(source, "unchanged_P14_data/" + name)

# Read exact candidate configuration, verify deployed manifest bytes and ELF.
# No remote file or deployment entry is changed.
env = os.environ.copy()
env.update(SSH_ASKPASS=str(ROOT / "tools/p5_ssh_askpass.cmd"),
           SSH_ASKPASS_REQUIRE="force", DISPLAY="p20m")
remote = """
import pathlib,hashlib,json,base64
result={}
for role in ('coarse','precise'):
 root=pathlib.Path(BASE)/role
 config=root/'Config'
 result[role]={}
 for name in ('HwaSimIRRuntime.ini','deployment_version.env','deployment_manifest.sha256','p20m_candidate.json'):
  result[role][name]=base64.b64encode((config/name).read_bytes()).decode()
 result[role]['elf_sha256']=hashlib.sha256((root/'HwaSim_IR').read_bytes()).hexdigest()
print(json.dumps(result))
""".replace("BASE", repr(str(Path(receipt["instances"]["coarse"]["root"]).parent).replace("\\", "/")))
data = json.loads(subprocess.check_output(
    ["ssh", "-o", "ConnectTimeout=10", "-o", "UpdateHostKeys=no", "root@192.168.1.116", "python3 -"],
    input=remote, text=True, encoding="utf-8", env=env))
import base64
for role, files in data.items():
    assert files.pop("elf_sha256") == receipt["build"]["elf_sha256"]
    for name, value in files.items():
        destination = out / "candidate_configuration" / role / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(base64.b64decode(value))
    assert sha(out / "candidate_configuration" / role / "HwaSimIRRuntime.ini") == receipt["instances"][role]["runtimeSha256"]
    assert sha(out / "candidate_configuration" / role / "deployment_manifest.sha256") == receipt["instances"][role]["metadata"]["ConfigManifestSha256"]

tracked = subprocess.check_output(["git", "diff", "--name-only", "-z"], cwd=ROOT).decode("utf-8").split("\0")
untracked = subprocess.check_output(["git", "ls-files", "--others", "--exclude-standard", "-z"], cwd=ROOT).decode("utf-8").split("\0")
for name in set(tracked + untracked):
    if not name or name.endswith("HwaSimIR_MODTRAN_FullDomain_Solution_and_Codex_Prompt_20261021.md"):
        continue  # User's reference document is preserved at its original location.
    if (ROOT / name).is_file():
        copy(ROOT / name, "source/" + name)
(out / "tracked_source_changes.patch").write_bytes(subprocess.check_output(["git", "diff", "--binary"], cwd=ROOT))
for name in ("receipt.json",):
    copy(TASK / "deployment" / name, "evidence/deployment/" + name)
for folder in ("20261021_full_domain", "20261021_geometry_repair", "20261021_solar_repair"):
    task = ROOT / "logs/p20m" / folder
    for name in ("case_manifest.json", "run_manifest.json", "pilot_qc.json", "engine_database_identity.json"):
        copy(task / name, "evidence/" + folder + "/" + name)
for name in ("customer_full_runs.json", "csv_query_regression.json", "official_development_summary.json",
             "board_runs_summary.json", "pilot_components_NOT_RUNTIME.csv", "physical_components_sampled.csv"):
    copy(TASK / "evidence" / name, "evidence/" + name)
manifest = dict(
    schema="HwaSimIR.P20M.CheckpointPackage.1", purpose="CODE_VALIDATION_OVERLAY_REQUIRES_P20_BASELINE",
    full_domain_release="NOT_ACCEPTED", data_release="UNCHANGED_P14",
    original_baseline="/userdata/HwaSimIR/candidates/P20_aarch64_40_prefix_prewarm",
    candidate=str(Path(receipt["instances"]["coarse"]["root"]).parent).replace("\\", "/"),
    actual_customer_software="NOT_RUN", calibration="NOT_VERIFIED_CALIBRATION",
    raw_evidence_retained_at=[str(ROOT / "logs/p20m" / name / "raw") for name in
                             ("20261021_full_domain", "20261021_geometry_repair", "20261021_solar_repair")],
    videos_retained_at=str(ROOT / "logs/p20/runs"),
    files=[dict(path=str(p.relative_to(out)).replace("\\", "/"), bytes=p.stat().st_size, sha256=sha(p))
           for p in sorted(out.rglob("*")) if p.is_file()])
(out / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
archive = out.with_suffix(".zip")
with zipfile.ZipFile(archive, "x", zipfile.ZIP_DEFLATED) as z:
    for p in sorted(out.rglob("*")):
        if p.is_file():
            z.write(p, str(p.relative_to(out)))
print(json.dumps(dict(archive=str(archive), sha256=sha(archive), files=len(manifest["files"]),
                      status="CHECKPOINT_ONLY_NOT_FULL_DOMAIN_RELEASE")))
