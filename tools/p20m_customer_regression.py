"""Run all three immutable customer CSVs through an independent DDS sender.

Transport success is never interpreted as full-domain physics acceptance.
Requires the separately built/deployed P20M code-validation candidate.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import re

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/"logs/p20m/20261021_full_domain/evidence"
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--prefix", default="p20m_20261021_direct_full",
                    help="Choose a fresh prefix to preserve earlier run evidence")
args = parser.parse_args()
if not re.fullmatch(r"p20m_[A-Za-z0-9_]+", args.prefix):
    raise SystemExit("Invalid isolated run prefix")
records = []
for file, count in (("circleDataAll.csv",3940), ("circleDataAll2.csv",319), ("circleDataAll3.csv",2902)):
    name = args.prefix+"_"+Path(file).stem
    result = ROOT/"logs/p20/runs"/name/"result.json"
    command = [sys.executable, str(ROOT/"tools/p20_board_run.py"),
        "--name",name,"--frames",str(count),"--source","1020测试内容/"+file,
        "--receipt","logs/p20m/20261021_full_domain/deployment/receipt.json",
        "--tmp-board-logs","--control-domain","150","--video-domain","1","--customer-domain","1",
        "--use-deployed-domains","--use-deployed-qos","--band","2","--precise-band","0",
        "--fps","40","--input-hz","100","--utc-date","2026-10-21","--utc-hour","3.5",
        "--stim-directory","build-p20m-stim/release",
        "--direct-dds-sender","build-p20m-stim/release/P20MDirectDds.exe"]
    if result.exists():
        raise SystemExit("Existing regression evidence; inspect before selecting a new run name: "+str(result))
    print("RUN",name,flush=True)
    with (OUT/(name+"_console.log")).open("w",encoding="utf-8") as log:
        p=subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT,timeout=600)
    record=dict(source=file,source_sha256=hashlib.sha256((ROOT/"1020测试内容"/file).read_bytes()).hexdigest(),
                frames_per_route=count,command=command,exit_code=p.returncode,
                result=str(result),physical_acceptance="NOT_ACCEPTED_P14_DOMAIN")
    if result.exists():
        data=json.loads(result.read_text())
        record.update(transport_status=data["status"],system_policy_restored=data.get("systemPolicyRestored"))
    records.append(record)
    checkpoint = "customer_full_runs.json" if args.prefix == "p20m_20261021_direct_full" else args.prefix+".json"
    tmp=OUT/(checkpoint+".tmp")
    tmp.write_text(json.dumps(records,indent=2),encoding="utf-8")
    tmp.replace(OUT/checkpoint)
    print("END",name,record.get("transport_status","NO_RECEIPT"),flush=True)
    if p.returncode:
        raise SystemExit("Stop on transport failure; checkpoint saved, production data unchanged.")
