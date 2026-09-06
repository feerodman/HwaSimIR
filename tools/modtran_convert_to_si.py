#!/usr/bin/env python3
"""Convert MODTRAN5 MODOUT2 native spectral densities to SI with the exact Jacobian."""

from __future__ import annotations

import argparse
import csv
import math
import re
from pathlib import Path


NUM = re.compile(r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[Ee][-+]?\d+)?")
RADIANCE_FIELDS = [
    ("wavenumber_cm1", 0, 9), ("tau_los", 9, 20), ("path_thermal_native", 20, 31),
    ("thermal_scatter_native", 31, 42), ("surface_emission_native", 42, 53),
    ("solar_scatter_native", 53, 64), ("single_scatter_native", 64, 75),
    ("ground_reflected_native", 75, 86), ("direct_reflected_native", 86, 97),
    ("total_radiance_native_diagnostic_only", 97, 108),
]


def native_cm1_to_si(value: float, wavelength_um: float) -> float:
    return value * 1.0e8 / (wavelength_um * wavelength_um)


def si_to_native_cm1(value: float, wavelength_um: float) -> float:
    return value * wavelength_um * wavelength_um / 1.0e8


def maybe_float(text: str) -> float | None:
    text = text.strip()
    return float(text) if text else None


def spectral_lines(path: Path, marker: str) -> tuple[list[str], int]:
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    for i, line in enumerate(lines):
        if marker in line.upper():
            return lines, i + 1
    raise ValueError(f"No {marker!r} table in {path}")


def parse_radiance(path: Path) -> list[dict[str, object]]:
    lines, start = spectral_lines(path, "TOT_TRANS")
    rows = []
    for line in lines[start:]:
        if not line.strip() or not re.match(r"^\s*\d", line):
            continue
        wn = maybe_float(line[0:9])
        if wn is None or wn < 100.0:
            continue
        wavelength = 10000.0 / wn
        row: dict[str, object] = {"wavenumber_cm1": wn, "wavelength_um": wavelength}
        for name, a, b in RADIANCE_FIELDS[1:]:
            value = maybe_float(line[a:b])
            row[name] = "" if value is None else value
            if name.endswith("_native") and value is not None:
                row[name.replace("_native", "_W_m2_sr_um")] = native_cm1_to_si(value, wavelength)
        # Never substitute TOTAL_RAD for a path component.
        # PTH_THRML already includes thermal multiple-scatter/reflected-ground
        # contributions when that option is enabled (PcModWin manual, p. 649).
        # Keep THRML_SCT as a diagnostic subset and never add it again here.
        row["path_radiance_excluding_surface_W_m2_sr_um"] = float(row.get("path_thermal_W_m2_sr_um") or 0.0) + float(row.get("solar_scatter_W_m2_sr_um") or 0.0)
        rows.append(row)
    return rows


def parse_solar(path: Path) -> list[dict[str, object]]:
    lines, start = spectral_lines(path, "SOL TR")
    rows = []
    for line in lines[start:]:
        values = [float(m.group(0)) for m in NUM.finditer(line)]
        if len(values) < 4 or values[0] < 100.0:
            continue
        wn, tau, transmitted, toa = values[:4]
        wavelength = 10000.0 / wn
        rows.append({
            "wavenumber_cm1": wn,
            "wavelength_um": wavelength,
            "tau_down": tau,
            "direct_solar_native": transmitted,
            "direct_solar_W_m2_um": native_cm1_to_si(transmitted, wavelength),
            "toa_solar_native_diagnostic": toa,
            "toa_solar_W_m2_um_diagnostic": native_cm1_to_si(toa, wavelength),
        })
    return rows


def parse_transmittance(path: Path) -> list[dict[str, object]]:
    lines, start = spectral_lines(path, "COMBIN")
    rows = []
    for line in lines[start:]:
        values = [float(m.group(0)) for m in NUM.finditer(line)]
        if len(values) < 2 or values[0] < 100.0:
            continue
        wn, tau = values[:2]
        rows.append({"wavenumber_cm1": wn, "wavelength_um": 10000.0 / wn, "tau_los": tau})
    return rows


def parse_flux(path: Path, target_alt_km: float = 5.0) -> list[dict[str, object]]:
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    altitude_text = " ".join(lines[:35])
    altitudes = [float(x) for x in re.findall(r"([-+]?\d+(?:\.\d+)?)\s+KM", altitude_text)]
    if not altitudes:
        raise ValueError(f"No altitude list in spectral flux file {path}")
    try:
        level = min(range(len(altitudes)), key=lambda i: abs(altitudes[i] - target_alt_km))
    except ValueError as exc:
        raise ValueError("Empty altitude list") from exc
    if abs(altitudes[level] - target_alt_km) > 1e-6:
        raise ValueError(f"Requested altitude {target_alt_km} km not present; nearest is {altitudes[level]} km")
    start = next(i for i, line in enumerate(lines) if "------- -----------" in line) + 1
    rows = []
    i = start
    while i < len(lines):
        first = [float(m.group(0)) for m in NUM.finditer(lines[i])]
        if len(first) < 7:
            i += 1
            continue
        wavelength_nm = first[0]
        if wavelength_nm <= 0.0:
            i += 1
            continue
        values = first[1:]
        j = i + 1
        while len(values) < len(altitudes) * 3 and j < len(lines):
            more = [float(m.group(0)) for m in NUM.finditer(lines[j])]
            if len(more) != 6:
                break
            values.extend(more)
            j += 1
        if len(values) < len(altitudes) * 3:
            raise ValueError(f"Incomplete flux block at wavelength {wavelength_nm} nm")
        upward, downward, direct = values[level * 3:level * 3 + 3]
        # The .flx header states W cm^-2 / nm.  1 cm^-2=1e4 m^-2 and
        # one micrometre contains 1000 nanometres, hence the exact 1e7 factor.
        rows.append({
            "wavelength_nm": wavelength_nm,
            "wavelength_um": wavelength_nm / 1000.0,
            "altitude_km": altitudes[level],
            "upward_diffuse_native_W_cm2_nm": upward,
            "downward_diffuse_native_W_cm2_nm": downward,
            "direct_solar_native_W_cm2_nm": direct,
            "upward_diffuse_W_m2_um": upward * 1.0e7,
            "downward_diffuse_W_m2_um": downward * 1.0e7,
            "direct_solar_W_m2_um": direct * 1.0e7,
        })
        i = j
    return rows


def write_rows(path: Path, rows: list[dict[str, object]], source: Path, case_id: str) -> None:
    if not rows:
        raise ValueError(f"No spectral rows parsed from {source}")
    is_flux = "wavelength_nm" in rows[0]
    common = {
        "case_id": case_id,
        "source_file": str(source.resolve()),
        "raw_unit_radiance": "" if is_flux else "W/(cm^2 sr cm^-1)",
        "raw_unit_irradiance": "W/(cm^2 nm)" if is_flux else "W/(cm^2 cm^-1)",
        "si_unit_radiance": "W/(m^2 sr um)",
        "si_unit_irradiance": "W/(m^2 um)",
        "conversion_method": "value_si=value_native*1e7 (cm^-2/nm -> m^-2/um)" if is_flux else "value_si=value_native*1e8/(wavelength_um^2)",
    }
    final = [{**common, **r} for r in rows]
    fields = list(dict.fromkeys(k for row in final for k in row))
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(final)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--input", required=True, type=Path)
    ap.add_argument("--mode", required=True, choices=["transmittance", "radiance", "solar", "flux"])
    ap.add_argument("--output", required=True, type=Path)
    ap.add_argument("--case-id", default="")
    ap.add_argument("--target-alt-km", type=float, default=5.0,
                    help="Target altitude used to select the requested .flx level")
    args = ap.parse_args()
    if args.mode == "flux":
        rows = parse_flux(args.input, args.target_alt_km)
    else:
        parser = {"transmittance": parse_transmittance, "radiance": parse_radiance, "solar": parse_solar}[args.mode]
        rows = parser(args.input)
    for row in rows:
        for key, value in row.items():
            if isinstance(value, float) and not math.isfinite(value):
                raise ValueError(f"Non-finite {key} in {args.input}")
    write_rows(args.output, rows, args.input, args.case_id or args.input.stem)
    print(f"Converted {len(rows)} rows -> {args.output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
