#!/usr/bin/env python3
"""Attach exact material-ID tags to the generated P11 sample-rack EGG."""

from __future__ import annotations

import re
import sys
from pathlib import Path


EXPECTED = {
    "sample_21_blackbody_250K": (21, "blackbody_250K"),
    "sample_22_blackbody_300K": (22, "blackbody_300K"),
    "sample_23_blackbody_350K": (23, "blackbody_350K"),
    "sample_24_blackbody_475K": (24, "blackbody_475K"),
    "sample_25_graybody_300K": (25, "graybody_300K"),
    "sample_26_reflector_300K": (26, "reflector_300K"),
    "sample_27_glass_300K": (27, "glass_300K"),
    "sample_28_cold_graybody_275K": (28, "cold_graybody_275K"),
}


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: p11_finalize_controlled_samples_egg.py MODEL.egg", file=sys.stderr)
        return 2
    path = Path(sys.argv[1])
    text = path.read_text(encoding="utf-8")
    text = re.sub(
        r"(?s)<Comment>\s*\{.*?\}",
        '<Comment> { "Generated from p11_controlled_samples.obj by the audited P11 asset check." }',
        text,
        count=1,
    )
    tagged: set[str] = set()

    def replace(match: re.Match[str]) -> str:
        indent, name = match.group(1), match.group(2)
        if name not in EXPECTED:
            return match.group(0)
        material_id, region = EXPECTED[name]
        tagged.add(name)
        return (
            f"{indent}<Group> {name} {{\n"
            f"{indent}  <Tag> p11_material_id {{ {material_id} }}\n"
            f"{indent}  <Tag> p11_material_region {{ {region} }}"
        )

    text = re.sub(r"(?m)^(\s*)<Group>\s+([^\s{]+)\s+\{", replace, text)
    if tagged != set(EXPECTED):
        missing = sorted(set(EXPECTED) - tagged)
        print(f"ERROR: expected 8 sample groups; missing={missing} tagged={len(tagged)}", file=sys.stderr)
        return 3
    path.write_text(text, encoding="utf-8")
    print(f"egg_tags={len(tagged)} ids=21..28")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
