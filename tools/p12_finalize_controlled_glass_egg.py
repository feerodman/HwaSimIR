#!/usr/bin/env python3
"""Attach exact material tags to the P12 controlled-glass EGG."""
from __future__ import annotations
import re
import sys
from pathlib import Path

EXPECTED = {"p12_glass_front": (31, "glass"), "p12_background": (32, "background")}

def main() -> int:
    path = Path(sys.argv[1])
    text = path.read_text(encoding="utf-8")
    tagged: set[str] = set()
    def repl(match: re.Match[str]) -> str:
        indent, name = match.group(1), match.group(2)
        if name not in EXPECTED: return match.group(0)
        material_id, region = EXPECTED[name]; tagged.add(name)
        return f"{indent}<Group> {name} {{\n{indent}  <Tag> p11_material_id {{ {material_id} }}\n{indent}  <Tag> p11_material_region {{ {region} }}"
    text = re.sub(r"(?m)^(\s*)<Group>\s+([^\s{]+)\s+\{", repl, text)
    if tagged != set(EXPECTED):
        print(f"missing={sorted(set(EXPECTED)-tagged)}", file=sys.stderr); return 3
    path.write_text(text, encoding="utf-8", newline="\n")
    print("egg_tags=2 ids=31,32")
    return 0

if __name__ == "__main__": raise SystemExit(main())
