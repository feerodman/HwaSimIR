#!/usr/bin/env python3
"""Audit P11 SI-domain preservation and the public SWIR/MWIR display windows."""

from __future__ import annotations

import argparse
import configparser
import csv
import hashlib
import json
import math
from pathlib import Path

H = 6.62607015e-34
C = 299792458.0
KB = 1.380649e-23
PI = math.pi
BANDS = {"SWIR": (1.1, 2.5), "MWIR": (3.0, 5.0)}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def planck(wavelength_um: float, temperature_k: float) -> float:
    wavelength_m = wavelength_um * 1.0e-6
    exponent = H * C / (wavelength_m * KB * temperature_k)
    return 2.0 * H * C * C / wavelength_m**5 / math.expm1(exponent) * 1.0e-6


def band_mean(band: str, temperature_k: float, intervals: int = 16384) -> float:
    low, high = BANDS[band]
    step = (high - low) / intervals
    value = planck(low, temperature_k) + planck(high, temperature_k)
    for index in range(1, intervals):
        value += (4.0 if index % 2 else 2.0) * planck(low + index * step, temperature_k)
    return value * step / 3.0 / (high - low)


def numeric_values(rows: list[dict[str, str]], field: str) -> list[float]:
    return [float(row[field]) for row in rows if row.get(field, "").strip()]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path, default=Path("logs/p11/display_window/current.json"))
    args = parser.parse_args()
    root = args.root.resolve()
    output = args.output if args.output.is_absolute() else root / args.output

    ini_path = root / "HwaSim_IR/Bin/Config/HwaSimIRRuntime.ini"
    lut_path = root / "HwaSim_IR/Bin/Config/Atmosphere/MODTRAN/processed/band_lut_si.csv"
    manifest_path = root / "HwaSim_IR/Bin/Config/TargetLib/p11/civil_van/manifest.json"
    source_path = root / "HwaSim_IR/HwaSim_IR/HwaSimIR.cpp"

    config = configparser.ConfigParser(interpolation=None)
    config.optionxform = str
    with ini_path.open(encoding="utf-8-sig") as stream:
        config.read_file(stream)
    windows = {
        "SWIR": (float(config["M1RadianceDisplay"]["SWIRRadianceMinWm2SrUm"]),
                 float(config["M1RadianceDisplay"]["SWIRRadianceMaxWm2SrUm"])),
        "MWIR": (float(config["M1RadianceDisplay"]["MWIRRadianceMinWm2SrUm"]),
                 float(config["M1RadianceDisplay"]["MWIRRadianceMaxWm2SrUm"])),
    }
    with lut_path.open(newline="", encoding="utf-8-sig") as stream:
        lut_rows = list(csv.DictReader(stream))
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    source = source_path.read_text(encoding="utf-8")

    details: dict[str, object] = {}
    checks: list[dict[str, object]] = []

    def check(name: str, passed: bool, detail: object) -> None:
        checks.append({"name": name, "pass": bool(passed), "detail": detail})

    for band in ("SWIR", "MWIR"):
        rows = [row for row in lut_rows if row.get("band") == band]
        if not rows:
            raise RuntimeError(f"No {band} rows in {lut_path}")
        direct = max(numeric_values(rows, "direct_solar_irradiance_at_target_W_m2_um"), default=0.0)
        sky = max(numeric_values(rows, "downward_sky_diffuse_irradiance_W_m2_um"), default=0.0)
        path_thermal = max(numeric_values(rows, "path_thermal_W_m2_sr_um"), default=0.0)
        path_scattering = max(numeric_values(rows, "los_path_scattering_radiance_W_m2_sr_um"), default=0.0)
        optics_key = band.lower()
        opaque_materials = [m for m in manifest["materials"] if float(m[optics_key][2]) == 0.0]
        max_reflectance = max(float(m[optics_key][0]) for m in opaque_materials)
        max_solid_emission = max(
            float(m[optics_key][1]) * band_mean(band, float(m.get("engine_on_temperature_k", m["temperature_k"])))
            for m in opaque_materials
        )
        active_upper = max_reflectance / PI * (1.0 / (BANDS[band][1] - BANDS[band][0]))
        envelope = (max_solid_emission + max_reflectance / PI * (direct + sky) +
                    active_upper + path_thermal + path_scattering)
        details[band] = {
            "window_W_m2_sr_um": windows[band],
            "audited_lut_row_count": len(rows),
            "maximum_direct_irradiance_W_m2_um": direct,
            "maximum_sky_irradiance_W_m2_um": sky,
            "maximum_opaque_reflectance": max_reflectance,
            "maximum_solid_emission_W_m2_sr_um": max_solid_emission,
            "one_W_m2_band_integrated_active_upper_W_m2_sr_um": active_upper,
            "conservative_civil_target_envelope_W_m2_sr_um": envelope,
            "window_headroom_ratio": windows[band][1] / envelope if envelope > 0.0 else None,
        }
        check(f"{band}_window_contains_audited_civil_envelope",
              windows[band][0] <= 0.0 and envelope < windows[band][1], details[band])

    tailpipe_mwir = 0.88 * band_mean("MWIR", 475.0)
    details["former_MWIR_2p5_window"] = {
        "tailpipe_emission_W_m2_sr_um": tailpipe_mwir,
        "former_normalized_before_clamp": tailpipe_mwir / 2.5,
        "p11_normalized_before_clamp": tailpipe_mwir / windows["MWIR"][1],
    }
    check("former_2p5_window_clipped_475K_tailpipe", tailpipe_mwir / 2.5 > 1.0,
          details["former_MWIR_2p5_window"])
    check("p11_64_window_preserves_475K_tailpipe", 0.0 < tailpipe_mwir / windows["MWIR"][1] < 1.0,
          details["former_MWIR_2p5_window"])

    source_guards = {
        "formal_texture_is_float": "formalSiDomainRequested ? Texture::T_float" in source,
        "formal_fbo_requests_32_bits": "formalSiDomainRequested ? 32 : 16" in source,
        "formal_shader_outputs_unmapped_sensor_radiance": "u_m1_raw_si_output == 1" in source and "max(m1_sensor, 0.0)" in source,
        "mapping_is_in_final_stage": "1.0 / physicalSpan" in source and "u_stage6_raw_to_display_scale" in source,
        "runtime_fails_non_32_bit_formal_buffer": "failure=formal_si_requires_rgba32f" in source,
        "formal_buffer_uses_mali_renderable_rgba32f": (
            'formalSiDomainRequested ? Texture::F_rgba : Texture::F_rgb' in source
            and 'actualRawFb.get_alpha_bits() < 32' in source
        ),
    }
    details["source_guards"] = source_guards
    for name, passed in source_guards.items():
        check(name, passed, str(passed))
    check("declared_windows_are_p11_values", windows == {"SWIR": (0.0, 40.0), "MWIR": (0.0, 64.0)}, windows)

    result = {
        "schema": "HwaSimIR.P11.DisplayWindowAudit.v1",
        "status": "PASS" if all(item["pass"] for item in checks) else "FAIL",
        "radiance_unit": "W/(m^2 sr um)",
        "mapping_semantics": "response-weighted rectangular-band mean; SI RGB32F before fixed display/AGC",
        "inputs": {
            str(path.relative_to(root)).replace("\\", "/"): sha256(path)
            for path in (ini_path, lut_path, manifest_path, source_path)
        },
        "details": details,
        "checks": checks,
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"P11 display-window audit {result['status']}: {sum(i['pass'] for i in checks)}/{len(checks)} checks")
    print(f"output={output}")
    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
