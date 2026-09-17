#!/usr/bin/env python3
"""Attach auditable material-region tags to the obj2egg-derived van EGG."""

from __future__ import annotations

import re
import sys
from pathlib import Path


REGIONS = (
    ("paint_", 11, "car_paint"),
    ("metal_", 12, "metal"),
    ("glass_", 13, "glass"),
    ("rubber_", 14, "rubber"),
    ("engine_bay", 15, "engine_bay"),
    ("exhaust_", 16, "exhaust_tailpipe"),
)


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: p11_finalize_civil_van_egg.py MODEL.egg", file=sys.stderr)
        return 2
    path = Path(sys.argv[1])
    text = path.read_text(encoding="utf-8")
    text = re.sub(
        r"(?s)<Comment>\s*\{.*?\}",
        '<Comment> { "Generated from p11_civil_van.obj by the audited P11 asset check." }',
        text,
        count=1,
    )
    tagged = 0

    def replace(match: re.Match[str]) -> str:
        nonlocal tagged
        indent, name = match.group(1), match.group(2)
        for prefix, material_id, region in REGIONS:
            if name == prefix or name.startswith(prefix):
                tagged += 1
                return (
                    f"{indent}<Group> {name} {{\n"
                    f"{indent}  <Tag> p11_material_id {{ {material_id} }}\n"
                    f"{indent}  <Tag> p11_material_region {{ {region} }}"
                )
        return match.group(0)

    text = re.sub(r"(?m)^(\s*)<Group>\s+([^\s{]+)\s+\{", replace, text)
    if tagged != 20:
        print(f"ERROR: expected 20 geometry groups, tagged {tagged}", file=sys.stderr)
        return 3
    path.write_text(text, encoding="utf-8")
    print(f"egg_tags={tagged}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
