#!/usr/bin/env python3
"""Update the three P12-touched Config hashes in a downloaded board manifest."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path


TARGETS = (
    "Annotation/annotation_profiles.json",
    "Atmosphere/MODTRAN/processed/band_lut_si.csv",
    "TargetLib/Targets.json",
)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--config-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    before = args.before.resolve(); config_root = args.config_root.resolve(); output = args.output.resolve()
    raw = before.read_bytes()
    newline = b"\r\n" if b"\r\n" in raw[:4096] else b"\n"
    lines = raw.splitlines()
    replacement_hashes = {}
    for target in TARGETS:
        source = config_root / Path(target)
        if not source.is_file():
            raise ValueError(f"replacement source missing: {source}")
        replacement_hashes[target] = digest(source)
    changed = []
    result = []
    pattern = re.compile(rb"^([0-9a-fA-F]{64})([ \t]+)(.+)$")
    for index, line in enumerate(lines, 1):
        match = pattern.match(line)
        target = match.group(3).decode("utf-8") if match else ""
        if match and target in replacement_hashes:
            new_hash = replacement_hashes[target]
            changed.append({"line": index, "oldSha256": match.group(1).decode("ascii").lower(),
                            "newSha256": new_hash, "path": target})
            line = new_hash.encode("ascii") + match.group(2) + match.group(3)
        result.append(line)
    if len(changed) != len(TARGETS) or {item["path"] for item in changed} != set(TARGETS):
        raise ValueError(f"expected exactly {len(TARGETS)} manifest targets, got {changed}")
    candidate = newline.join(result) + (newline if raw.endswith((b"\n", b"\r")) else b"")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(candidate)
    # Prove every non-target byte-line is unchanged.
    after_lines = candidate.splitlines()
    changed_line_numbers = {item["line"] for item in changed}
    other_unchanged = all(a == b for idx, (a, b) in enumerate(zip(lines, after_lines), 1)
                          if idx not in changed_line_numbers)
    report = {
        "schema": "HwaSimIR.P12.BoardManifestDelta.2", "targets": list(TARGETS),
        "beforeManifestSha256": digest(before), "afterManifestSha256": digest(output),
        "replacementHashes": replacement_hashes, "lineCount": len(lines), "changedLines": changed,
        "allOtherLinesByteIdentical": other_unchanged,
    }
    if len(lines) != len(after_lines) or not other_unchanged:
        raise ValueError("non-target manifest content changed")
    args.report.resolve().write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
