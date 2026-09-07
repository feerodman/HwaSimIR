#!/usr/bin/env python3
"""Verify deployable material resources and warn on Config/repo-root drift."""

from __future__ import annotations

import hashlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PAIRS = (
    (ROOT / "HwaSim_IR/Bin/Config/Materials/MaterialDatabase.csv", ROOT / "materials/MaterialDatabase.csv"),
    (ROOT / "HwaSim_IR/Bin/Config/Materials/MaterialBandOptics.csv", ROOT / "materials/MaterialBandOptics.csv"),
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    failed = False
    for config_path, root_path in PAIRS:
        if not config_path.is_file():
            print(f"[MaterialDeployment][ERROR] missing Config resource: {config_path}")
            failed = True
            continue
        if not root_path.is_file():
            print(f"[MaterialDeployment][WARN] repo-root development fallback missing: {root_path}")
            continue
        config_hash = sha256(config_path)
        root_hash = sha256(root_path)
        status = "PASS" if config_hash == root_hash else "WARN"
        print(
            f"[MaterialDeployment][{status}] resource={config_path.name} "
            f"config={config_path} repoRoot={root_path} hashAlgorithm=SHA256 "
            f"configHash={config_hash} repoRootHash={root_hash}"
        )
        if config_hash != root_hash:
            failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
