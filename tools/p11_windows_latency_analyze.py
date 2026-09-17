#!/usr/bin/env python3
"""Join P11 producer and receiver ledgers and report measured same-host latency.

The Windows evidence runner records producer timestamps in
``producer_annotations.jsonl`` and receiver timestamps in
``frame_index.jsonl``.  Both are system-clock nanoseconds on the same host.
This tool joins them by the immutable source sequence and keeps per-frame
values so frames over the P11 80 ms reporting threshold cannot be hidden by
an average.
"""

from __future__ import annotations

import argparse
import csv
import json
import math
from pathlib import Path
from typing import Any, Iterable


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8-sig") as stream:
        for line_number, line in enumerate(stream, 1):
            text = line.strip()
            if not text:
                continue
            try:
                value = json.loads(text)
            except json.JSONDecodeError as exc:
                raise ValueError(f"{path}:{line_number}: invalid JSON: {exc}") from exc
            if not isinstance(value, dict):
                raise ValueError(f"{path}:{line_number}: expected a JSON object")
            rows.append(value)
    return rows


def as_int(row: dict[str, Any], key: str) -> int:
    value = row.get(key)
    if value is None or value == "":
        raise ValueError(f"missing {key}")
    return int(value)


def percentile(values: Iterable[float], p: float) -> float | None:
    ordered = sorted(values)
    if not ordered:
        return None
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * p
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    weight = position - lower
    return ordered[lower] * (1.0 - weight) + ordered[upper] * weight


def stats(values: list[float]) -> dict[str, float | int | None]:
    return {
        "count": len(values),
        "minMs": min(values) if values else None,
        "meanMs": sum(values) / len(values) if values else None,
        "p50Ms": percentile(values, 0.50),
        "p95Ms": percentile(values, 0.95),
        "p99Ms": percentile(values, 0.99),
        "maxMs": max(values) if values else None,
    }


def unique_by_source(rows: list[dict[str, Any]], label: str) -> dict[int, dict[str, Any]]:
    result: dict[int, dict[str, Any]] = {}
    for row in rows:
        source_seq = as_int(row, "sourceSeq")
        if source_seq in result:
            raise ValueError(f"duplicate {label} sourceSeq={source_seq}")
        result[source_seq] = row
    return result


def find_receiver_dir(path: Path) -> Path:
    if (path / "producer_annotations.jsonl").is_file() and (path / "frame_index.jsonl").is_file():
        return path
    candidate = path / "receiver_recording"
    if (candidate / "producer_annotations.jsonl").is_file() and (candidate / "frame_index.jsonl").is_file():
        return candidate
    raise FileNotFoundError(f"receiver ledgers not found under {path}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("evidence_dir", type=Path)
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--threshold-ms", type=float, default=80.0)
    args = parser.parse_args()

    receiver_dir = find_receiver_dir(args.evidence_dir.resolve())
    output_dir = (args.output_dir or receiver_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    producer_path = receiver_dir / "producer_annotations.jsonl"
    receiver_path = receiver_dir / "frame_index.jsonl"
    producer = unique_by_source(read_jsonl(producer_path), "producer")
    receiver = unique_by_source(read_jsonl(receiver_path), "receiver")
    shared = sorted(set(producer) & set(receiver))

    per_frame: list[dict[str, Any]] = []
    invalid: list[dict[str, Any]] = []
    for source_seq in shared:
        p = producer[source_seq]
        r = receiver[source_seq]
        try:
            input_ns = as_int(p, "udpReceiveTimeNs")
            send_ns = as_int(p, "tcpSendTimeNs")
            receive_ns = as_int(r, "receiveTimeNs")
            display_ns = as_int(r, "displayTimeNs")
        except ValueError as exc:
            invalid.append({"sourceSeq": source_seq, "reason": str(exc)})
            continue

        intervals = {
            "producerInputToSendMs": (send_ns - input_ns) / 1_000_000.0,
            "transportMs": (receive_ns - send_ns) / 1_000_000.0,
            "receiverToDisplayMs": (display_ns - receive_ns) / 1_000_000.0,
            "inputToDisplayMs": (display_ns - input_ns) / 1_000_000.0,
        }
        if any(value < 0.0 or value > 60_000.0 for value in intervals.values()):
            invalid.append({"sourceSeq": source_seq, "reason": "clock interval outside [0,60000] ms", **intervals})
            continue
        per_frame.append(
            {
                "sourceSeq": source_seq,
                "frameSeq": as_int(r, "frameSeq"),
                "association": str(r.get("association", "")),
                **{key: round(value, 6) for key, value in intervals.items()},
                "overThreshold": intervals["inputToDisplayMs"] > args.threshold_ms,
            }
        )

    columns = [
        "sourceSeq",
        "frameSeq",
        "association",
        "producerInputToSendMs",
        "transportMs",
        "receiverToDisplayMs",
        "inputToDisplayMs",
        "overThreshold",
    ]
    with (output_dir / "p11_per_frame_latency.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=columns)
        writer.writeheader()
        writer.writerows(per_frame)

    over = [row for row in per_frame if row["overThreshold"]]
    with (output_dir / "p11_over_80ms_frames.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=columns)
        writer.writeheader()
        writer.writerows(over)

    metrics = [
        "producerInputToSendMs",
        "transportMs",
        "receiverToDisplayMs",
        "inputToDisplayMs",
    ]
    producer_seq = sorted(producer)
    receiver_seq = sorted(receiver)
    summary: dict[str, Any] = {
        "result": "PASS" if shared and not invalid and len(shared) == len(producer) == len(receiver) else "FAIL",
        "measurementScope": "same Windows host application clocks; UDP ingress to GUI display submission",
        "joinKey": "sourceSeq",
        "associationValues": sorted({str(row.get("association", "")) for row in receiver.values()}),
        "producerRows": len(producer),
        "receiverRows": len(receiver),
        "joinedRows": len(shared),
        "validLatencyRows": len(per_frame),
        "invalidRows": invalid,
        "producerOnlySourceSeq": sorted(set(producer) - set(receiver)),
        "receiverOnlySourceSeq": sorted(set(receiver) - set(producer)),
        "producerSourceSeqContinuous": producer_seq == list(range(producer_seq[0], producer_seq[-1] + 1)) if producer_seq else False,
        "receiverSourceSeqContinuous": receiver_seq == list(range(receiver_seq[0], receiver_seq[-1] + 1)) if receiver_seq else False,
        "thresholdMs": args.threshold_ms,
        "overThresholdCount": len(over),
        "overThresholdPercent": (100.0 * len(over) / len(per_frame)) if per_frame else None,
        "firstOverThresholdSourceSeq": over[0]["sourceSeq"] if over else None,
        "lastOverThresholdSourceSeq": over[-1]["sourceSeq"] if over else None,
        "metrics": {metric: stats([float(row[metric]) for row in per_frame]) for metric in metrics},
        "inputs": {
            "producerAnnotations": str(producer_path),
            "frameIndex": str(receiver_path),
        },
    }
    summary_path = output_dir / "p11_latency_summary.json"
    summary_path.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, ensure_ascii=False, separators=(",", ":")))
    return 0 if summary["result"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
