"""Read every original customer row and preserve simulation time vs 60 Hz pacing."""
import argparse
import csv
import math
from pathlib import Path
import p20m_modtran as engine

def inventory(out):
    records, all_rows = [], []
    for name in ("circleDataAll.csv", "circleDataAll2.csv", "circleDataAll3.csv"):
        path = engine.ROOT/"1020测试内容"/name
        with path.open(encoding="utf-8-sig", newline="") as f:
            rows=list(csv.reader(f))
        if rows[0][0].strip().lower().startswith("time"):
            rows=rows[1:]
        rows=[r for r in rows if r]
        numeric=[[float(v) for v in row] for row in rows]
        if not all(len(r)>=54 and all(math.isfinite(v) for v in r) for r in numeric):
            raise ValueError("Malformed source: "+name)
        times=[r[0] for r in numeric]
        if not all(b>=a for a,b in zip(times,times[1:])):
            raise ValueError("Source time reverses: "+name)
        all_rows.append(rows)
        records.append(dict(source=str(path),sha256=engine.digest(path),rows=len(rows),
                            original_rows_preserved=True,
                            source_span_ms=times[-1]-times[0],
                            wall_deadline_span_ms=(len(rows)-1)*1000/60,
                            input_hz=60,video_fps=60,
                            view_invalid_rows=sum(r[53]==0 for r in numeric),
                            formal_new_data_query="NOT_RUN_NO_ACCEPTED_NEW_DATASET",
                            rendered_regression="NOT_RUN_NO_ACCEPTED_NEW_DATASET"))
    result=dict(schema="HwaSimIR.P20M2.ReplayInventory.1",cases=records,
                csv2_is_csv3_prefix=all_rows[1]==all_rows[2][:len(all_rows[1])],
                old_audit_csv2="319_ROWS_PREVIOUSLY_PASSED_NOT_RECLASSIFIED_AS_SAME_FAILURE",
                clock_contract="source UTC offsets preserved; 60 Hz wall pacing is slower replay for 10 ms source rows")
    engine.atomic_json(out,result)
    print([(r["rows"],r["source_span_ms"],r["wall_deadline_span_ms"]) for r in records])

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out",type=Path,required=True)
    inventory(p.parse_args().out)
