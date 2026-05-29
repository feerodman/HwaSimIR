"""Shared helpers for MODTRAN production diagnostics."""

from __future__ import annotations

import csv
import hashlib
from pathlib import Path
from typing import Dict, Iterable, List, Sequence


PRODUCTION_VISIBILITIES = [2.0, 5.0, 10.0, 23.0, 50.0]
VISIBILITY_SENSITIVITY_EPS = 1e-5


def parse_float(value: object) -> float | None:
    if value is None or value == "":
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def format_float(value: float | None) -> str:
    if value is None:
        return ""
    return f"{value:.10g}"


def read_csv(path: Path) -> List[Dict[str, str]]:
    if not path.exists() or path.stat().st_size == 0:
        return []
    with path.open(newline="", encoding="utf-8-sig") as handle:
        return list(csv.DictReader(handle))


def write_csv(path: Path, rows: Sequence[Dict[str, object]], fieldnames: Sequence[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def upsert_marked_section(path: Path, marker: str, section: str) -> None:
    begin = f"<!-- BEGIN {marker} -->"
    end = f"<!-- END {marker} -->"
    text = path.read_text(encoding="utf-8", errors="replace") if path.exists() else "# MODTRAN LUT QC Report\n"
    block = f"\n{begin}\n{section.rstrip()}\n{end}\n"
    if begin in text and end in text:
        before = text.split(begin, 1)[0]
        after = text.split(end, 1)[1]
        path.write_text(before.rstrip() + block + after.lstrip(), encoding="utf-8")
    else:
        path.write_text(text.rstrip() + "\n" + block, encoding="utf-8")


def sha256_short(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8", errors="replace")).hexdigest()[:16]


def read_text_if_exists(path: Path) -> str:
    if not path.exists() or not path.is_file():
        return ""
    return path.read_text(encoding="utf-8", errors="replace")


def project_path(root: Path, value: str) -> Path:
    path = Path(value)
    if path.is_absolute():
        return path
    return root / value


def min_max_mean(values: Iterable[float]) -> tuple[float | None, float | None, float | None]:
    clean = list(values)
    if not clean:
        return None, None, None
    return min(clean), max(clean), sum(clean) / len(clean)


def rectangular_average(points: Sequence[tuple[float, float]]) -> float | None:
    if not points:
        return None
    ordered = sorted(points, key=lambda item: item[0])
    if len(ordered) == 1:
        return ordered[0][1]
    width = ordered[-1][0] - ordered[0][0]
    if width <= 0:
        return sum(value for _, value in ordered) / len(ordered)
    area = 0.0
    for (wavelength_a, value_a), (wavelength_b, value_b) in zip(ordered, ordered[1:]):
        area += 0.5 * (value_a + value_b) * (wavelength_b - wavelength_a)
    return area / width

