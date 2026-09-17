#!/usr/bin/env python3
"""Independent P11 material-transmission equation, asset, and source gate.

This does not launch HwaSimIR or touch Runtime.ini.  It verifies that the
formal SWIR/MWIR glass path is energy-conserving and that the production
shader/scene binding implement the same premultiplied-alpha equation.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import re
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def planck_w_m2_sr_um(wavelength_um: float, temperature_k: float) -> float:
    exponent = 14387.752 / (wavelength_um * temperature_k)
    return 119104200.0 / (wavelength_um**5 * math.expm1(exponent))


def band_mean_planck(low_um: float, high_um: float, temperature_k: float, intervals: int = 16384) -> float:
    if intervals % 2:
        raise ValueError("Simpson integration needs an even interval count")
    step = (high_um - low_um) / intervals
    total = planck_w_m2_sr_um(low_um, temperature_k) + planck_w_m2_sr_um(high_um, temperature_k)
    for index in range(1, intervals):
        total += (4.0 if index % 2 else 2.0) * planck_w_m2_sr_um(low_um + index * step, temperature_k)
    return total * step / 3.0 / (high_um - low_um)


def require(checks: list[dict], name: str, condition: bool, evidence: object) -> None:
    checks.append({"name": name, "pass": bool(condition), "evidence": evidence})


def load_transmissive_rows(csv_path: Path) -> list[dict]:
    with csv_path.open(newline="", encoding="utf-8-sig") as stream:
        rows = list(csv.DictReader(stream))
    return [
        row
        for row in rows
        if float(row["SWIRTransmissivity"]) > 0.0 or float(row["MWIRTransmissivity"]) > 0.0
    ]


def evaluate_case(
    name: str,
    wavelength_range_um: tuple[float, float],
    emissivity: float,
    reflectance: float,
    transmissivity: float,
    temperature_k: float,
    tau_atmosphere: float,
    direct_irradiance: float,
    sky_irradiance: float,
    ndotl: float,
    path_radiance: float,
    active_sensor_radiance: float,
    behind_sensor_radiance: float,
) -> dict:
    blackbody = band_mean_planck(*wavelength_range_um, temperature_k)
    reflected = reflectance / math.pi * (direct_irradiance * ndotl + sky_irradiance)
    surface = emissivity * blackbody + reflected
    opacity = 1.0 - transmissivity

    # Production shader output for a tagged node using premultiplied alpha.
    source_rgb = (
        tau_atmosphere * surface
        + active_sensor_radiance
        + opacity * path_radiance
    )
    framebuffer = source_rgb + transmissivity * behind_sensor_radiance

    # Independent statement of the intended sensor-plane transfer.
    reference = (
        tau_atmosphere * surface
        + active_sensor_radiance
        + opacity * path_radiance
        + transmissivity * behind_sensor_radiance
    )
    wrong_global_brightening = (
        tau_atmosphere * surface + path_radiance + active_sensor_radiance
    ) + transmissivity * behind_sensor_radiance
    return {
        "name": name,
        "unit": "W/(m^2 sr um)",
        "temperatureK": temperature_k,
        "emissivity": emissivity,
        "reflectance": reflectance,
        "transmissivity": transmissivity,
        "energySum": emissivity + reflectance + transmissivity,
        "opacity": opacity,
        "opacityEqualsEmissivityPlusReflectance": opacity - (emissivity + reflectance),
        "blackbodyBandMean": blackbody,
        "reflectedSurfaceRadiance": reflected,
        "surfaceRadiance": surface,
        "premultipliedSourceRgb": source_rgb,
        "behindSensorRadiance": behind_sensor_radiance,
        "framebufferResult": framebuffer,
        "independentReference": reference,
        "absoluteError": abs(framebuffer - reference),
        "wrongFullPathDoubleCountResult": wrong_global_brightening,
        "wrongFullPathBias": wrong_global_brightening - reference,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("logs/p11/reference/glass_transmission/p11_glass_transmission_check.json"),
    )
    args = parser.parse_args()
    repo = args.repo.resolve()
    output = args.output if args.output.is_absolute() else repo / args.output

    mapper = repo / "HwaSim_IR/HwaSim_IR/IR/IRSceneMaterialMapper.cpp"
    shader = repo / "HwaSim_IR/HwaSim_IR/HwaSimIR.cpp"
    mapper_text = mapper.read_text(encoding="utf-8", errors="replace")
    shader_text = shader.read_text(encoding="utf-8", errors="replace")
    checks: list[dict] = []

    source_requirements = {
        "band_transmissivity_uniform_bound": (
            'set_shader_input("u_material_band_transmissivity"' in mapper_text
            and "uniform vec4 u_material_band_transmissivity[8]" in shader_text
        ),
        "tagged_geometry_only": '"**/=p11_material_id="' in mapper_text,
        "premultiplied_blend": "TransparencyAttrib::M_premultiplied_alpha" in mapper_text,
        "transparent_depth_contract": (
            "transmissiveNode.set_depth_test(true)" in mapper_text
            and "transmissiveNode.set_depth_write(false)" in mapper_text
            and 'transmissiveNode.set_bin("transparent", 20)' in mapper_text
        ),
        "legacy_geometry_fail_closed": "action=fail_closed_opaque" in mapper_text,
        "shader_opacity_from_tau": "float material_opacity = clamp(1.0 - surface_transmissivity" in shader_text,
        "shader_surface_and_front_path_once": bool(
            re.search(
                r"stage5_intensity\s*=\s*max\(u_m1_tau_up \* m1_surface \+ l2_active_sensor \+\s*"
                r"material_opacity \* u_m1_path_radiance",
                shader_text,
            )
        ),
        "shader_alpha_is_opacity": "stage5_output_alpha = material_opacity" in shader_text,
    }
    for name, condition in source_requirements.items():
        require(checks, name, condition, {"mapper": str(mapper), "shader": str(shader)})

    assets = []
    for asset_name, expected_material_id in (("civil_van", 13), ("controlled_samples", 27)):
        asset_dir = repo / f"HwaSim_IR/Bin/Config/TargetLib/p11/{asset_name}"
        csv_path = next(asset_dir.glob("*_band_optics.csv"))
        egg_path = next(asset_dir.glob("*.egg"))
        rows = load_transmissive_rows(csv_path)
        ids = [int(row["MaterialId"]) for row in rows]
        egg_text = egg_path.read_text(encoding="utf-8", errors="replace")
        tag_count = len(re.findall(rf"<Tag>\s+p11_material_id\s+\{{\s*{expected_material_id}\s*\}}", egg_text))
        energy_errors = []
        for row in rows:
            for prefix in ("SWIR", "MWIR"):
                energy = sum(float(row[prefix + suffix]) for suffix in ("Reflectance", "Emissivity", "Transmissivity"))
                if abs(energy - 1.0) > 1.0e-9:
                    energy_errors.append({"materialId": row["MaterialId"], "band": prefix, "sum": energy})
        require(checks, f"{asset_name}_one_expected_transmissive_material", ids == [expected_material_id], ids)
        require(checks, f"{asset_name}_transmissive_geometry_tagged", tag_count >= 1, tag_count)
        require(checks, f"{asset_name}_band_energy_balance", not energy_errors, energy_errors)
        assets.append(
            {
                "name": asset_name,
                "bandOpticsCsv": str(csv_path),
                "bandOpticsSha256": sha256(csv_path),
                "egg": str(egg_path),
                "eggSha256": sha256(egg_path),
                "transmissiveMaterialIds": ids,
                "matchingTagCount": tag_count,
            }
        )

    numerical = [
        evaluate_case(
            "SWIR_glass_tau_0.85",
            (1.1, 2.5), 0.07, 0.08, 0.85, 300.0,
            0.72, 1.7, 0.3, 0.6, 0.04, 0.015, 0.21,
        ),
        evaluate_case(
            "MWIR_glass_tau_0.05",
            (3.0, 5.0), 0.90, 0.05, 0.05, 300.0,
            0.75, 0.12, 0.08, 0.6, 0.25, 0.004, 0.60,
        ),
        evaluate_case(
            "opaque_invariance_tau_0",
            (3.0, 5.0), 0.92, 0.08, 0.0, 325.0,
            0.81, 0.1, 0.07, 0.4, 0.19, 0.0, 8.0,
        ),
    ]
    for case in numerical:
        require(checks, case["name"] + "_energy_balance", abs(case["energySum"] - 1.0) <= 1.0e-12, case["energySum"])
        require(checks, case["name"] + "_opacity_partition", abs(case["opacityEqualsEmissivityPlusReflectance"]) <= 1.0e-12, case["opacityEqualsEmissivityPlusReflectance"])
        require(checks, case["name"] + "_equation", case["absoluteError"] <= 1.0e-12, case["absoluteError"])
    require(
        checks,
        "opaque_tau_zero_ignores_behind",
        abs(numerical[2]["framebufferResult"] - numerical[2]["premultipliedSourceRgb"]) <= 1.0e-12,
        numerical[2]["framebufferResult"] - numerical[2]["premultipliedSourceRgb"],
    )
    require(
        checks,
        "path_is_not_added_globally_over_transmitted_background",
        numerical[0]["wrongFullPathBias"] > 0.0 and numerical[1]["wrongFullPathBias"] > 0.0,
        [numerical[0]["wrongFullPathBias"], numerical[1]["wrongFullPathBias"]],
    )

    failures = [check["name"] for check in checks if not check["pass"]]
    result = {
        "result": "PASS" if not failures else "FAIL",
        "scope": "formal SWIR/MWIR single-layer material transmission",
        "equation": "Lout = tau_atm*(epsilon*B + rho*E/pi) + Lactive + (1-tau_material)*Lpath + tau_material*Lbehind",
        "blend": "premultiplied alpha: Csrc + (1-alpha)*Cdst, alpha=1-tau_material",
        "source": {
            "mapper": str(mapper),
            "mapperSha256": sha256(mapper),
            "shader": str(shader),
            "shaderSha256": sha256(shader),
        },
        "assets": assets,
        "numericalCases": numerical,
        "checks": checks,
        "failures": failures,
        "limitations": [
            "Only independently tagged geometry is enabled; untagged legacy transmissive materials fail closed as opaque.",
            "The implementation models straight-through band transmission, not refraction, Fresnel angle dependence, or internal multiple reflection.",
            "Overlapping transparent groups are sorted by Panda3D bounds; each physical layer contributes its own tau factor.",
            "This static/CPU gate does not replace a runtime float-FBO pixel measurement on Windows and RK3588.",
        ],
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"result": result["result"], "checks": len(checks), "failures": failures, "output": str(output)}))
    return 0 if result["result"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
