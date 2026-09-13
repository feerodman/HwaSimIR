"""Standalone visible-light, uniform-field teaching experiment; no renderer imports."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import platform

import numpy as np

H = 6.62607015e-34  # J s, exact SI definition
C = 299792458.0  # m/s, exact SI definition
VERSION = "ordinary-sensor-lab-1"


def number(value, name, low=0.0, strict=False):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{name}: finite number required")
    value = float(value)
    if not math.isfinite(value) or value < low or (strict and value == low):
        raise ValueError(f"{name}: invalid range")
    return value


def curve(data, role, bounds_nm=(400.0, 700.0)):
    """Return wavelength in nm; spectral irradiance always per nm, never per sr."""
    if data.get("source_type") != "synthetic" or not data.get("source"):
        raise ValueError(f"{role}: this experiment requires documented synthetic input")
    units = {"nm": 1.0, "um": 1000.0}
    if data.get("wavelength_unit") not in units:
        raise ValueError(f"{role}: wavelength unit must be nm or um")
    try:
        rows = np.array(data["samples"], dtype=float)
    except (ValueError, TypeError, KeyError) as exc:
        raise ValueError(f"{role}: numeric wavelength/value pairs required") from exc
    if rows.ndim != 2 or rows.shape[1] != 2 or len(rows) < 2:
        raise ValueError(f"{role}: at least two wavelength/value pairs required")
    if not np.isfinite(rows).all():
        raise ValueError(f"{role}: non-finite sample")
    x = rows[:, 0] * units[data["wavelength_unit"]]
    y = rows[:, 1].copy()
    if (np.diff(x) <= 0).any() or x[0] < bounds_nm[0] or x[-1] > bounds_nm[1]:
        raise ValueError(f"{role}: strictly increasing samples within {bounds_nm} nm required")
    if (y < 0).any():
        raise ValueError(f"{role}: negative sample")
    if role == "illumination":
        scale = {"W/m2/nm": 1.0, "W/m2/um": 0.001}
        if data.get("value_unit") not in scale:
            raise ValueError("illumination: spectral irradiance unit W/m2/nm or W/m2/um required")
        y *= scale[data["value_unit"]]
    else:
        scale = {"fraction": 1.0, "percent": 0.01}
        if data.get("value_unit") not in scale:
            raise ValueError(f"{role}: fraction or percent required")
        y *= scale[data["value_unit"]]
        if (y > 1).any():
            raise ValueError(f"{role}: response outside 0..1")
    return x, y


def integrate(curves, photon=False):
    """Piecewise linear functions; 3-point Gauss rule, exact through degree five.

    Integration domain is the full illumination support. No extrapolation or
    silent band truncation. Multiplication and wavelength weighting happen at
    quadrature nodes, not only at the original sample positions.
    """
    lo, hi = curves[0][0][[0, -1]]
    for x, _ in curves[1:]:
        if x[0] > lo or x[-1] < hi:
            raise ValueError("response does not cover the full illumination support")
    knots = np.unique(np.concatenate([x[(x >= lo) & (x <= hi)] for x, _ in curves]))
    a, b = knots[:-1], knots[1:]
    nodes = (a + b)[:, None] / 2 + (b - a)[:, None] / 2 * np.array([-math.sqrt(3/5), 0, math.sqrt(3/5)])
    value = np.ones_like(nodes)
    for x, y in curves:
        value *= np.interp(nodes, x, y)
    if photon:
        value *= nodes * 1e-9 / (H * C)
    return float(np.sum((b-a) / 2 * (value @ np.array([5/9, 8/9, 5/9]))))


def run(config, count=100000, seed=20260913):
    if config.get("schema") != VERSION:
        raise ValueError("unsupported schema")
    bands = {"synthetic_visible_photon_pixel": (400., 700.),
             "synthetic_nir_photon_pixel": (700., 1100.),
             "synthetic_mwir_photon_pixel": (3000., 5000.)}
    if config.get("model") not in bands:
        raise ValueError("only explicit synthetic photon pixel models are supported")
    bounds = bands[config["model"]]
    if not isinstance(count, int) or isinstance(count, bool) or not 2 <= count <= 1000000:
        raise ValueError("samples must be an integer in 2..1000000")
    if not isinstance(seed, int) or isinstance(seed, bool) or not 0 <= seed < 2**64:
        raise ValueError("seed must be an unsigned 64-bit integer")
    light, response = config["illumination"], config["response"]
    illumination = curve(light, "illumination", bounds)
    response_curve = curve(response, "response", bounds)
    factors = response.get("included_factors")
    if not isinstance(factors, list) or len(factors) != len(set(factors)):
        raise ValueError("response: explicit unique included_factors required")
    kind = response.get("kind")
    expected = {"absolute_qe": {"qe"}, "absolute_system_efficiency": {"qe", "optics"}}
    relative = kind in ("relative_energy", "relative_photon")
    if kind not in expected and not relative:
        raise ValueError("unsupported response kind")
    if not set(factors) <= {"qe", "optics"}:
        raise ValueError("unsupported included factor")
    if not relative and set(factors) != expected[kind]:
        raise ValueError("absolute response kind disagrees with included factors")
    optics = config.get("optics")
    if optics is not None and "optics" in factors:
        raise ValueError("optics already included in response")
    if config.get("qe") is not None:
        raise ValueError("a second QE factor is not supported")
    optical_path = "optics" in factors or optics is not None
    expected_plane = "before_optics_uniform_plane" if optical_path else "detector_plane"
    if light.get("reference_plane") != expected_plane:
        raise ValueError(f"illumination reference_plane must be {expected_plane}")
    curves = [illumination, response_curve]
    if optics is not None:
        curves.append(curve(optics, "optics", bounds))
    weighted_energy = integrate(curves)
    weighted_photons = integrate(curves, photon=True)
    result = {
        "schema": VERSION, "model": config["model"], "validation_level": "synthetic_model_only_not_device_calibration",
        "input_irradiance_W_m2": integrate([illumination]),
        "response_kind": kind, "included_factors": factors,
        "separate_optics_applied": optics is not None,
        "reference_plane": expected_plane,
        "integration": "piecewise_linear_3_point_Gauss_no_extrapolation",
        "wavelength_support_nm": illumination[0][[0, -1]].tolist(),
        "runtime": {"python": platform.python_version(), "numpy": np.__version__, "rng": "PCG64"},
    }
    if relative:
        result["relative_weighted_response"] = {
            "value": weighted_energy if kind == "relative_energy" else weighted_photons,
            "unit": "relative_weighted_W/m2" if kind == "relative_energy" else "relative_weighted_photons/m2/s",
        }
        result["charge_and_adc"] = "not_computed_relative_response_has_no_absolute_calibration"
        return result
    p = config["pixel"]
    if p.get("source_type") != "synthetic" or not p.get("source"):
        raise ValueError("pixel: synthetic parameter provenance required")
    required_units = {"pitch": "um", "exposure": "s", "dark_current": "electron/pixel/s",
                      "read_noise": "electron_rms", "full_well": "electron",
                      "conversion_gain": "DN/electron", "black_level": "DN"}
    values = {}
    for key, unit in required_units.items():
        actual_unit = p[key].get("unit")
        compatible = (key == "exposure" and actual_unit == "us") or (key == "conversion_gain" and actual_unit == "electron/DN")
        if actual_unit != unit and not compatible:
            raise ValueError(f"pixel.{key}: expected {unit}")
        values[key] = number(p[key]["value"], key, strict=key in ("pitch", "full_well", "conversion_gain"))
        if key == "exposure" and actual_unit == "us":
            values[key] *= 1e-6
        if key == "conversion_gain" and actual_unit == "electron/DN":
            values[key] = 1.0 / values[key]
    bits = p["adc_bits"]
    if not isinstance(bits, int) or isinstance(bits, bool) or not 1 <= bits <= 24:
        raise ValueError("adc_bits: integer 1..24 required")
    limit = 2**bits - 1
    if values["black_level"] > limit:
        raise ValueError("black level exceeds ADC range")
    area = (values["pitch"] * 1e-6)**2
    exposure = values["exposure"]
    photo = weighted_photons * area * exposure
    dark = values["dark_current"] * exposure
    if not math.isfinite(photo + dark) or photo + dark > 1e12:
        raise ValueError("charge exceeds numerical scope of teaching experiment")
    rng = np.random.Generator(np.random.PCG64(seed))
    # Charge saturation precedes electronic read noise; clipping ADC is separate.
    raw_charge = rng.poisson(photo, count) + rng.poisson(dark, count)
    stored_charge = np.minimum(raw_charge, values["full_well"])
    readout = stored_charge + rng.normal(0, values["read_noise"], count)
    analog_dn = readout * values["conversion_gain"] + values["black_level"]
    adc = np.clip(np.floor(analog_dn + 0.5), 0, limit).astype(np.int64)
    result.update({
        "parameters": p, "sample_count": count, "seed": seed,
        "pixel_area_m2": area,
        "input_exposure_J_m2": result["input_irradiance_W_m2"] * exposure,
        "input_energy_per_pixel_J": result["input_irradiance_W_m2"] * exposure * area,
        "mean_photoelectrons": photo, "mean_dark_electrons": dark,
        "expected_unsaturated_variance_electron2": photo + dark + values["read_noise"]**2,
        "measured_raw_charge_mean_electron": float(raw_charge.mean()),
        "measured_raw_charge_variance_electron2": float(raw_charge.var(ddof=1)),
        "measured_readout_mean_electron": float(readout.mean()),
        "measured_readout_variance_electron2": float(readout.var(ddof=1)),
        "full_well_fraction": float(np.mean(raw_charge >= values["full_well"])),
        "adc_clipping_fraction": float(np.mean((analog_dn < 0) | (analog_dn > limit))),
        "adc_mean_DN": float(adc.mean()), "adc_min_DN": int(adc.min()), "adc_max_DN": int(adc.max()),
        "adc_first_16_DN": adc[:16].tolist(),
        "rounding": "floor(DN+0.5)_then_clip",
    })
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("config", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--samples", type=int, default=100000)
    parser.add_argument("--seed", type=int, default=20260913)
    args = parser.parse_args()
    try:
        raw = args.config.read_bytes()
        result = run(json.loads(raw.decode("utf-8-sig")), args.samples, args.seed)
        result["config_sha256"] = hashlib.sha256(raw).hexdigest()
        result["implementation_sha256"] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2, allow_nan=False) + "\n", encoding="utf-8")
    except (ValueError, KeyError, TypeError, OSError) as exc:
        parser.exit(2, f"Invalid experiment: {exc}\n")
    print(f"Synthetic experiment written to {args.output}")


if __name__ == "__main__":
    main()
