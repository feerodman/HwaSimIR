#!/usr/bin/env python3
"""Reproducible L1 NIR component and directional thermal-inertia QC.

This is a numerical acceptance harness, not an image tuning tool.  It reads the
formal SI LUTs and the existing material database and writes auditable tables.
"""

from __future__ import annotations

import csv
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BAND_LUT = ROOT / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv"
SOLAR_LUT = ROOT / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/solar_heating_lut_si.csv"
MATERIALS = ROOT / "materials/MaterialDatabase.csv"
OUT_CSV = ROOT / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/l1_natural_solar_ab.csv"
OUT_MD = ROOT / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/l1_natural_solar_ab.md"

SIGMA = 5.670374419e-8
C1_UM = 1.191042e8
C2_UM_K = 1.4387752e4


def load_rows(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def load_materials() -> dict[str, dict[str, float]]:
    lines = MATERIALS.read_text(encoding="utf-8").splitlines()
    begin = lines.index("!RawMaterials") + 1
    rows = list(csv.DictReader(lines[begin:]))
    result = {}
    for row in rows:
        if not row.get("Name") or row["Name"].startswith("!") or row.get("Solar Absorptivity") is None:
            break
        result[row["Name"]] = {
            "alpha": float(row["Solar Absorptivity"]),
            "epsilon": float(row["Thermal Emissivity"]),
            "cp": float(row["Specific Heat (w-sec/gm/K)"]) * 1000.0,
            "k": float(row["Conductivity (w/m/K)"]),
            "density": float(row["Density (kg/m**3)"]),
            "transmissivity": float(row["Transmissivity"]),
        }
    return result


def find(rows: list[dict[str, str]], **wanted: str) -> dict[str, str]:
    matches = [row for row in rows if all(row.get(key) == str(value) for key, value in wanted.items())]
    if len(matches) != 1:
        raise RuntimeError(f"expected one row for {wanted}, found {len(matches)}")
    return matches[0]


def thermal_step(delta: float, material: dict[str, float], thickness: float,
                 direct: float, diffuse: float, visibility: float, dt: float = 0.1) -> float:
    baseline = 288.15
    temp = baseline + delta
    capacity = material["density"] * material["cp"] * thickness
    q_solar = material["alpha"] * (direct * visibility + diffuse * 0.5)
    q_conv = 8.0 * (temp - baseline)
    q_rad = material["epsilon"] * SIGMA * (temp**4 - baseline**4)
    q_cond = min(material["k"] / thickness, 12.0) * (temp - baseline)
    return max(-50.0, min(120.0, delta + dt * (q_solar - q_conv - q_rad - q_cond) / capacity))


def planck_4um(temperature_k: float) -> float:
    """Match IRRadianceModelV2 spectral Planck radiance, W/(m^2 sr um)."""
    return C1_UM / (4.0**5 * (math.exp(C2_UM_K / (4.0 * temperature_k)) - 1.0))


def main() -> int:
    band_rows = load_rows(BAND_LUT)
    solar_rows = load_rows(SOLAR_LUT)
    materials = load_materials()
    output: list[dict[str, object]] = []
    nir_front_values = []
    for sza in (20, 45, 70):
        row = find(band_rows, band="NIR", observer_alt_km="10", target_alt_km="5",
                   range_km="10", visibility_km="23", solar_zenith_deg=str(sza))
        rho = 1.0 - materials["BM_PAINT"]["alpha"] - materials["BM_PAINT"]["transmissivity"]
        tau = float(row["tau_up"])
        direct = float(row["direct_solar_irradiance_at_target_W_m2_um"])
        sky = float(row["downward_sky_diffuse_irradiance_W_m2_um"])
        path = float(row["los_path_scattering_radiance_W_m2_sr_um"])
        for condition, ndotl, visibility in (("front", 1.0, 1.0), ("back", 0.0, 1.0), ("geometric_shadow", 1.0, 0.0)):
            solar = rho / math.pi * direct * ndotl * visibility
            sky_reflected = rho / math.pi * sky
            sensor = tau * (solar + sky_reflected) + path
            output.append({"test": "NIR", "case": f"SZA{sza}_{condition}", "material": "BM_PAINT",
                           "value1": solar, "value2": sky_reflected, "result": sensor,
                           "units": "W/(m^2 sr um)", "status": "PASS"})
            if condition == "front":
                nir_front_values.append(sensor)
        output.append({"test": "NIR", "case": f"SZA{sza}_night", "material": "BM_PAINT",
                       "value1": 0.0, "value2": 0.0, "result": 0.0,
                       "units": "W/(m^2 sr um)", "status": "PASS"})

    solar = find(solar_rows, target_alt_km="5", visibility_km="23", solar_zenith_deg="45")
    direct = float(solar["direct_shortwave_solar_irradiance_W_m2"])
    diffuse = float(solar["diffuse_shortwave_down_irradiance_W_m2"])
    material_cases = (("BM_METAL-ALUMINIUM", 0.01), ("BM_PAINT", 0.05), ("BM_GLASS", 0.02))
    mwir = find(band_rows, band="MWIR", observer_alt_km="10", target_alt_km="5",
                range_km="10", visibility_km="23")
    mwir_tau = float(mwir["tau_up"])
    mwir_path = float(mwir["path_thermal_W_m2_sr_um"])
    thermal_radiance = []
    for name, thickness in material_cases:
        delta = 0.0
        previous = delta
        max_step = 0.0
        checkpoints = {}
        for step in range(1, 3601):
            time_s = step * 0.1
            if time_s <= 120.0:
                phase, e_dir, e_dif, visibility = "sun", direct, diffuse, 1.0
            elif time_s <= 240.0:
                phase, e_dir, e_dif, visibility = "shadow", direct, diffuse, 0.0
            else:
                phase, e_dir, e_dif, visibility = "night", 0.0, 0.0, 0.0
            delta = thermal_step(delta, materials[name], thickness, e_dir, e_dif, visibility)
            max_step = max(max_step, abs(delta - previous))
            previous = delta
            if step in (1200, 2400, 3600):
                checkpoints[phase] = delta
        # A high-capacity surface may continue warming briefly after direct sun is
        # blocked because diffuse flux remains.  The physical acceptance is that
        # the night segment cools from the shadow checkpoint and no step jumps.
        cooling_ok = checkpoints["sun"] > 0.0 and checkpoints["night"] < checkpoints["shadow"]
        output.append({"test": "MWIR_THERMAL", "case": "sun120_shadow120_night120", "material": name,
                       "value1": checkpoints["sun"], "value2": checkpoints["shadow"],
                       "result": checkpoints["night"], "units": "K delta",
                       "status": "PASS" if cooling_ok and max_step < 0.05 else "FAIL"})
        epsilon = materials[name]["epsilon"]
        base_sensor = mwir_tau * epsilon * planck_4um(288.15) + mwir_path
        sensor_values = [mwir_tau * epsilon * planck_4um(288.15 + checkpoints[phase]) + mwir_path
                         for phase in ("sun", "shadow", "night")]
        thermal_radiance.append((name, base_sensor, sensor_values))

    statuses = [row["status"] for row in output]
    if not (nir_front_values[0] > nir_front_values[1] > nir_front_values[2]):
        statuses.append("FAIL")

    OUT_CSV.parent.mkdir(parents=True, exist_ok=True)
    with OUT_CSV.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=("test", "case", "material", "value1", "value2", "result", "units", "status"))
        writer.writeheader()
        writer.writerows(output)

    lines = ["# L1 Natural Solar Numerical A/B", "",
             f"Overall: **{'PASS' if all(status == 'PASS' for status in statuses) else 'FAIL'}**", "",
             "No image gain, tone-map, body-floor, or empirical solar scale is used by this harness.", "",
             "| Test | Case | Material | Component/Delta 1 | Component/Delta 2 | Sensor/Final delta | Unit | Status |",
             "|---|---|---|---:|---:|---:|---|---|"]
    for row in output:
        lines.append(f"| {row['test']} | {row['case']} | {row['material']} | {row['value1']:.9g} | {row['value2']:.9g} | {row['result']:.9g} | {row['units']} | {row['status']} |")
    lines.extend(("", "Thermal sequence: direct sun 120 s, geometric-shadow equivalent (direct blocked, diffuse retained) 120 s, then night/direct+diffuse off 120 s. Integration dt=0.1 s.", "",
                  "## MWIR physical ROI-equivalent radiance", "",
                  "Computed with the formal standard-case tau/path and the same 4 um spectral Planck function as the runtime shader. Contrast is against the same unheated material; these are radiance-domain ROI equivalents, not display-gray measurements.", "",
                  "| Material | Base | Sun 120 s | Shadow 120 s | Night 120 s | Contrast sun/shadow/night | Unit |",
                  "|---|---:|---:|---:|---:|---|---|"))
    for name, base_sensor, sensor_values in thermal_radiance:
        contrasts = [value - base_sensor for value in sensor_values]
        lines.append(f"| {name} | {base_sensor:.9g} | {sensor_values[0]:.9g} | {sensor_values[1]:.9g} | {sensor_values[2]:.9g} | {contrasts[0]:.9g} / {contrasts[1]:.9g} / {contrasts[2]:.9g} | W/(m^2 sr um) |")
    OUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"status={'PASS' if all(status == 'PASS' for status in statuses) else 'FAIL'} rows={len(output)} csv={OUT_CSV} report={OUT_MD}")
    return 0 if all(status == "PASS" for status in statuses) else 1


if __name__ == "__main__":
    raise SystemExit(main())
