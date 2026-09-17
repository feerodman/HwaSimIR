#!/usr/bin/env python3
"""Generate the isolated P11 SWIR 1.0 km altitude-layer MODTRAN cards."""

from __future__ import annotations

import sys
from pathlib import Path


TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
import p11_modtran_swir_grid as grid  # noqa: E402


def main() -> int:
    # Reuse the already-audited card layouts and axes, changing only the
    # observer/target/flux target altitude. The output stays isolated.
    grid.ALTITUDE_KM = 1.0
    if len(sys.argv) == 1:
        sys.argv.extend(["--output-root", "logs/p11/modtran/swir_alt1_grid"])
    return grid.main()


if __name__ == "__main__":
    raise SystemExit(main())
