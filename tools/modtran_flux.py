"""Read only actual MODTRAN flux levels; never extrapolate an altitude."""
from __future__ import annotations

import math
import re
from pathlib import Path
import modtran_convert_to_si as convert


def flux_altitudes(path: Path) -> list[float]:
    header = " ".join(path.read_text(encoding="utf-8", errors="replace").splitlines()[:35])
    levels = [float(v) for v in re.findall(r"([-+]?\d+(?:\.\d+)?)\s+KM", header)]
    if not levels:
        raise ValueError(f"No flux altitude levels in {path}")
    return levels


def parse_flux_at_target(path: Path, target_alt_km: float,
                         required_components=("upward", "downward", "direct")):
    if not math.isfinite(target_alt_km):
        raise ValueError("Non-finite target altitude")
    levels = sorted(set(flux_altitudes(path)))
    exact = next((v for v in levels if abs(v - target_alt_km) <= 1e-9), None)
    if exact is not None:
        low_level = high_level = exact
    else:
        lower = [v for v in levels if v < target_alt_km]
        upper = [v for v in levels if v > target_alt_km]
        if not lower or not upper:
            raise ValueError(f"Target altitude {target_alt_km} outside real flux levels {levels} in {path}")
        low_level, high_level = max(lower), min(upper)
    low = convert.parse_flux(path, low_level, required_components)
    high = low if exact is not None else convert.parse_flux(path, high_level, required_components)
    if not low or len(low) != len(high):
        raise ValueError(f"Empty or mismatched flux spectra in {path}")
    fraction = 0.0 if exact is not None else (target_alt_km-low_level)/(high_level-low_level)
    method = "exact_real_flux_level" if exact is not None else "linear_between_adjacent_real_flux_levels"
    fields = ("upward_diffuse_native_W_cm2_nm", "downward_diffuse_native_W_cm2_nm",
              "direct_solar_native_W_cm2_nm", "upward_diffuse_W_m2_um",
              "downward_diffuse_W_m2_um", "direct_solar_W_m2_um")
    rows = []
    for a, b in zip(low, high):
        if abs(float(a["wavelength_um"])-float(b["wavelength_um"])) > 1e-12:
            raise ValueError(f"Flux wavelength mismatch in {path}")
        item = dict(a)
        for field in fields:
            if a[field] == "" or b[field] == "":
                item[field] = ""
                continue
            av, bv = float(a[field]), float(b[field])
            if not math.isfinite(av) or not math.isfinite(bv):
                raise ValueError(f"Non-finite flux {field} in {path}")
            item[field] = av + (bv-av)*fraction
        for component in ("upward", "downward", "direct"):
            flag = component+"_valid"
            item[flag] = bool(a.get(flag, True) and b.get(flag, True))
        item.update(altitude_km=target_alt_km, vertical_interpolation=method)
        rows.append(item)
    return rows, dict(targetAltKm=target_alt_km, lowLevelKm=low_level,
                      highLevelKm=high_level, fraction=fraction, method=method)
