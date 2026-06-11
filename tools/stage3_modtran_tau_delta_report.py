#!/usr/bin/env python3
import argparse
import csv
import math
import re
from pathlib import Path


PAIR_RE = re.compile(r"([A-Za-z_]+)=([^ ]+)")


def parse_debug_line(line):
    if "MODTRAN Tau Debug" not in line:
        return None
    values = {match.group(1): match.group(2) for match in PAIR_RE.finditer(line)}
    if not values:
        return None
    return values


def parse_float(value):
    if value is None or value == "" or value == "NA":
        return None
    try:
        number = float(value)
    except ValueError:
        return None
    return number if math.isfinite(number) else None


def read_debug_rows(log_dir):
    rows = []
    for path in sorted(log_dir.glob("HwaSimIR-modtran-tau-*.out.log")):
        with path.open("r", encoding="utf-8", errors="replace") as handle:
            for line_no, line in enumerate(handle, 1):
                values = parse_debug_line(line)
                if not values:
                    continue
                old_tau = parse_float(values.get("old_tau"))
                new_tau = parse_float(values.get("new_tau"))
                diff = parse_float(values.get("diff"))
                ratio = None
                if old_tau is not None and new_tau is not None and abs(old_tau) > 1.0e-12:
                    ratio = new_tau / old_tau
                warnings = []
                if diff is not None and abs(diff) > 0.5:
                    warnings.append("WARNING_LARGE_DIFF")
                if ratio is not None and abs(ratio) > 10.0:
                    warnings.append("WARNING_LARGE_RATIO")
                rows.append({
                    "log_file": str(path),
                    "line": line_no,
                    "source": values.get("source", ""),
                    "band": values.get("band", ""),
                    "obs_km": values.get("obs_km", ""),
                    "target_km": values.get("target_km", ""),
                    "range_km": values.get("range_km", ""),
                    "visibility_km": values.get("visibility_km", ""),
                    "solar_zenith_deg": values.get("solar_zenith_deg", ""),
                    "tau_up": values.get("tau_up", ""),
                    "tau_down": values.get("tau_down", ""),
                    "old_tau": values.get("old_tau", ""),
                    "new_tau": values.get("new_tau", ""),
                    "diff": values.get("diff", ""),
                    "ratio": "" if ratio is None else f"{ratio:.10g}",
                    "interpolation": values.get("interpolation", ""),
                    "fallback_state": values.get("fallback_state", ""),
                    "fallback_input": values.get("fallback_input", ""),
                    "active": values.get("active", ""),
                    "return_source": values.get("return_source", ""),
                    "fallback": values.get("fallback", ""),
                    "warning": ";".join(warnings),
                })
    return rows


def read_band_lut_stats(path):
    stats = {
        "rows": 0,
        "bands": set(),
        "min_tau": None,
        "max_tau": None,
    }
    if not path.exists():
        return stats
    with path.open("r", newline="", encoding="utf-8-sig") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            stats["rows"] += 1
            if row.get("band"):
                stats["bands"].add(row["band"])
            for name in ("tau_up_band", "tau_down_band"):
                value = parse_float(row.get(name))
                if value is None:
                    continue
                stats["min_tau"] = value if stats["min_tau"] is None else min(stats["min_tau"], value)
                stats["max_tau"] = value if stats["max_tau"] is None else max(stats["max_tau"], value)
    return stats


def summarize(rows):
    summary = {}
    for row in rows:
        band = row["band"] or "UNKNOWN"
        item = summary.setdefault(band, {
            "count": 0,
            "warnings": 0,
            "return_sources": {},
            "fallback_states": {},
            "min_old": None,
            "max_old": None,
            "min_new": None,
            "max_new": None,
            "min_diff": None,
            "max_diff": None,
            "max_abs_ratio": None,
        })
        item["count"] += 1
        if row["warning"]:
            item["warnings"] += 1
        item["return_sources"][row["return_source"]] = item["return_sources"].get(row["return_source"], 0) + 1
        item["fallback_states"][row["fallback_state"]] = item["fallback_states"].get(row["fallback_state"], 0) + 1
        for key, field in (("min_old", "old_tau"), ("max_old", "old_tau"), ("min_new", "new_tau"), ("max_new", "new_tau"), ("min_diff", "diff"), ("max_diff", "diff")):
            value = parse_float(row.get(field))
            if value is None:
                continue
            if key.startswith("min"):
                item[key] = value if item[key] is None else min(item[key], value)
            else:
                item[key] = value if item[key] is None else max(item[key], value)
        ratio = parse_float(row.get("ratio"))
        if ratio is not None:
            abs_ratio = abs(ratio)
            item["max_abs_ratio"] = abs_ratio if item["max_abs_ratio"] is None else max(item["max_abs_ratio"], abs_ratio)
    return summary


def format_map(values):
    if not values:
        return "none"
    return ", ".join(f"{key or 'empty'}={value}" for key, value in sorted(values.items()))


def write_markdown(path, rows, band_stats, summary):
    warning_count = sum(1 for row in rows if row["warning"])
    lines = [
        "# Stage 3 MODTRAN Tau Delta Report",
        "",
        "Scope: runtime debug log comparison only. This report does not change C++ behavior, shader uniforms, or MODTRAN data.",
        "",
        "## Inputs",
        f"- Debug log rows: {len(rows)}",
        f"- Warning rows: {warning_count}",
        f"- band_lut.csv rows: {band_stats['rows']}",
        f"- band_lut.csv bands: {', '.join(sorted(band_stats['bands'])) if band_stats['bands'] else 'none'}",
        f"- band_lut.csv tau range: {band_stats['min_tau']} to {band_stats['max_tau']}",
        "",
        "## Band Summary",
        "",
        "| band | rows | warnings | old_tau min/max | new_tau min/max | diff min/max | max_abs_ratio | return_source | fallback_state |",
        "| --- | ---: | ---: | --- | --- | --- | ---: | --- | --- |",
    ]
    for band, item in sorted(summary.items()):
        lines.append(
            f"| {band} | {item['count']} | {item['warnings']} | "
            f"{item['min_old']} / {item['max_old']} | "
            f"{item['min_new']} / {item['max_new']} | "
            f"{item['min_diff']} / {item['max_diff']} | "
            f"{item['max_abs_ratio']} | "
            f"{format_map(item['return_sources'])} | "
            f"{format_map(item['fallback_states'])} |"
        )
    lines.extend([
        "",
        "## Warning Rule",
        "",
        "- `WARNING_LARGE_DIFF`: `abs(new_tau - old_tau) > 0.5`.",
        "- `WARNING_LARGE_RATIO`: `abs(new_tau / old_tau) > 10` when `old_tau` is nonzero.",
        "- These are warnings for controlled stage 3 tau-active preparation, not fatal failures.",
        "",
    ])
    path.write_text("\n".join(lines), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--log-dir", default="logs/stage3")
    parser.add_argument("--band-lut", default="HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut.csv")
    parser.add_argument("--out-csv", default="logs/stage3_modtran_tau_delta_report.csv")
    parser.add_argument("--out-md", default="logs/stage3_modtran_tau_delta_report.md")
    args = parser.parse_args()

    log_dir = Path(args.log_dir)
    rows = read_debug_rows(log_dir)
    band_stats = read_band_lut_stats(Path(args.band_lut))
    summary = summarize(rows)

    out_csv = Path(args.out_csv)
    out_md = Path(args.out_md)
    out_csv.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = [
        "log_file", "line", "source", "band", "obs_km", "target_km", "range_km",
        "visibility_km", "solar_zenith_deg", "tau_up", "tau_down", "old_tau",
        "new_tau", "diff", "ratio", "interpolation", "fallback_state",
        "fallback_input", "active", "return_source", "fallback", "warning",
    ]
    with out_csv.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)
    write_markdown(out_md, rows, band_stats, summary)
    print(f"Wrote {out_csv} rows={len(rows)}")
    print(f"Wrote {out_md}")


if __name__ == "__main__":
    main()
