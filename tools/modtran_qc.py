#!/usr/bin/env python3
"""Physics/unit QC for the audited MODTRAN SI chain and the existing candidate LUT."""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path

from modtran_convert_to_si import native_cm1_to_si, parse_radiance, parse_solar, parse_transmittance, si_to_native_cm1


H = 6.62607015e-34
C = 299792458.0
KB = 1.380649e-23


def planck_lambda_w_m2_sr_um(wavelength_um: float, temperature_k: float) -> float:
    wavelength_m = wavelength_um * 1e-6
    per_m = (2.0 * H * C * C) / (wavelength_m**5 * math.expm1(H * C / (wavelength_m * KB * temperature_k)))
    return per_m * 1e-6


def band_mean_planck(temperature_k: float, lo: float = 3.0, hi: float = 5.0, n: int = 2001) -> tuple[float, float]:
    dx = (hi - lo) / (n - 1)
    values = [planck_lambda_w_m2_sr_um(lo + i * dx, temperature_k) for i in range(n)]
    integral = dx * (0.5 * values[0] + sum(values[1:-1]) + 0.5 * values[-1])
    return integral / (hi - lo), integral


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as f:
        return list(csv.DictReader(f))


def spectral_mean(rows: list[dict[str, object]], column: str) -> float:
    samples = sorted((float(r["wavelength_um"]), float(r[column])) for r in rows if r.get(column) not in {None, ""})
    integral = sum((x1 - x0) * (y0 + y1) * 0.5 for (x0, y0), (x1, y1) in zip(samples, samples[1:]))
    return integral / (samples[-1][0] - samples[0][0])


def build_trends(raw_root: Path) -> list[dict[str, object]]:
    trends = []
    for meta in read_csv(raw_root / "qc_grid_manifest.csv"):
        case_dir = raw_root / "qc_grid" / meta["case_id"]
        output = case_dir / "MODOUT2.txt"
        mode = meta["mode"]
        row: dict[str, object] = dict(meta)
        if mode == "Transmittance":
            parsed = parse_transmittance(output)
            row["tau_mean"] = spectral_mean(parsed, "tau_los")
        elif mode == "ThermalRadiance":
            parsed = parse_radiance(output)
            row["path_mean_W_m2_sr_um"] = spectral_mean(parsed, "path_thermal_W_m2_sr_um")
        elif mode == "DirectSolarIrradiance":
            parsed = parse_solar(output)
            row["tau_down_mean"] = spectral_mean(parsed, "tau_down")
            row["direct_solar_mean_W_m2_um"] = spectral_mean(parsed, "direct_solar_W_m2_um")
        trends.append(row)
    return trends


def monotonic(values: list[float], increasing: bool) -> bool:
    return all((b >= a - 1e-12) if increasing else (b <= a + 1e-12) for a, b in zip(values, values[1:]))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--standard-table", required=True, type=Path)
    ap.add_argument("--candidate", required=True, type=Path)
    ap.add_argument("--output", required=True, type=Path)
    ap.add_argument("--raw-root", type=Path)
    args = ap.parse_args()
    standard = read_csv(args.standard_table)[0]
    candidate = read_csv(args.candidate)
    max_rel = 0.0
    for wavelength in [3.0, 3.5, 4.0, 4.5, 5.0]:
        native = 1.23456789e-8
        back = si_to_native_cm1(native_cm1_to_si(native, wavelength), wavelength)
        max_rel = max(max_rel, abs(back - native) / native)

    lines = [
        "# MODTRAN MWIR SI QC",
        "",
        f"- raw_SI_roundtrip_max_relative_error: {max_rel:.3e}",
        f"- roundtrip_status: {'PASS' if max_rel < 1e-14 else 'FAIL'}",
        f"- standard_table: `{args.standard_table.resolve()}`",
        f"- existing_candidate_rows: {len(candidate)}",
        "",
        "## Planck 3-5 um reference",
        "",
        "| T (K) | mean spectral radiance W/(m^2 sr um) | integrated radiance W/(m^2 sr) |",
        "| ---: | ---: | ---: |",
    ]
    for t in [250, 300, 500, 1000]:
        mean, integral = band_mean_planck(t)
        lines.append(f"| {t} | {mean:.9g} | {integral:.9g} |")
    lines += [
        "",
        "## Standard-case values",
        "",
        f"- tau_los_mean: {float(standard['tau_los_mean']):.9g}",
        f"- tau_down_mean: {float(standard['tau_down_mean']):.9g}",
        f"- path_radiance_mean_W_m2_sr_um: {float(standard['path_radiance_mean_W_m2_sr_um']):.9g}",
        f"- direct_solar_irradiance_mean_W_m2_um: {float(standard['direct_solar_irradiance_mean_W_m2_um']):.9g}",
        f"- solar_scatter_mean_W_m2_sr_um: {float(standard['solar_scatter_mean_W_m2_sr_um']):.9g}",
        f"- downward_sky_diffuse_flux_mean_W_m2_um: {float(standard['downward_sky_diffuse_flux_mean_W_m2_um']):.9g}",
        "",
        "## Existing candidate comparison",
        "",
        "The existing candidate is compared as evidence only. Its solar column was built from MODOUT2 `SOLAR` (TOA), not `SOL TR` (transmitted at target), so it is not accepted as direct-target irradiance.",
    ]
    matches = [r for r in candidate if r.get("band") == "MWIR" and r.get("visibility_km") == "23" and r.get("observer_alt_km") == "10" and r.get("target_alt_km") == "5" and r.get("range_km") == "10"]
    lines.append(f"- matching_existing_rows: {len(matches)}")
    if matches:
        r = matches[0]
        for k in ["tau_up_band", "tau_down_band", "path_radiance_band_W_m2_sr_um", "solar_irradiance_band_W_m2_um"]:
            lines.append(f"- existing_{k}: {r.get(k, '')}")
    if args.raw_root:
        trends = build_trends(args.raw_root)
        trend_path = args.output.parent / "trend_qc.csv"
        fields = list(dict.fromkeys(k for row in trends for k in row))
        with trend_path.open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fields)
            writer.writeheader()
            writer.writerows(trends)
        def selected(mode: str, key: str, values: list[float], metric: str, required: dict[str, float] | None = None) -> list[float]:
            found = []
            for v in values:
                match = next(r for r in trends if r["mode"] == mode and float(r[key]) == v and all(float(r[k]) == x for k, x in (required or {}).items()))
                found.append(float(match[metric]))
            return found
        range_values = [1.0, 10.0, 30.0, 50.0]
        vis_values = [2.0, 5.0, 10.0, 23.0, 50.0]
        sza_values = [0.0, 30.0, 45.0, 60.0, 75.0]
        range_tau = selected("Transmittance", "range_km", range_values, "tau_mean", {"visibility_km": 23.0, "observer_alt_km": 5.0})
        range_path = selected("ThermalRadiance", "range_km", range_values, "path_mean_W_m2_sr_um", {"visibility_km": 23.0, "observer_alt_km": 5.0})
        vis_tau = selected("Transmittance", "visibility_km", vis_values, "tau_mean", {"range_km": 10.0, "observer_alt_km": 1.0})
        vis_path = selected("ThermalRadiance", "visibility_km", vis_values, "path_mean_W_m2_sr_um", {"range_km": 10.0, "observer_alt_km": 1.0})
        sza_solar = selected("DirectSolarIrradiance", "solar_zenith_deg", sza_values, "direct_solar_mean_W_m2_um")
        checks = [
            ("range tau non-increasing", monotonic(range_tau, False)),
            ("range path non-decreasing", monotonic(range_path, True)),
            ("visibility tau non-decreasing", monotonic(vis_tau, True)),
            ("visibility path non-increasing", monotonic(vis_path, False)),
            ("solar-zenith direct irradiance non-increasing", monotonic(sza_solar, False)),
        ]
        lines += ["", "## Trend QC", "", f"- trend_table: `{trend_path.resolve()}`"]
        for name, passed in checks:
            lines.append(f"- {name}: {'PASS' if passed else 'FAIL'}")
        lines += ["", "| axis | values | metric values |", "| --- | --- | --- |",
                  f"| range_km | {range_values} | tau={range_tau}; path={range_path} |",
                  f"| visibility_km | {vis_values} | tau={vis_tau}; path={vis_path} |",
                  f"| solar_zenith_deg | {sza_values} | direct solar={sza_solar} |"]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(args.output.resolve())
    return 0 if max_rel < 1e-14 else 2


if __name__ == "__main__":
    raise SystemExit(main())
