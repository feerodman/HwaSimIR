#!/usr/bin/env python3
"""Audit one controlled P11 blackbody surface across CPU, shader and GPU readback.

The input must be a finalized ``controlled_sample_solar_off`` Windows case.
Only ideal opaque blackbody panels (material IDs 21..24) are used here.  Their
zero reflectance removes normal/illumination ambiguity, so the selected pixel
must satisfy exactly

    Lsensor = tau * epsilon * band_mean_Planck(T) + Lpath

in the independent CPU reference, the production fragment-shader equation and
the Stage6 pre-display floating-point GPU readback.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import re
import statistics
import sys
from pathlib import Path

import numpy as np


H = 6.62607015e-34
C = 299792458.0
KB = 1.380649e-23
BANDS = {"SWIR": (1.1, 2.5, 10), "MWIR": (3.0, 5.0, 4)}
BLACKBODY_IDS = {21, 22, 23, 24}
CPU_SHADER_REL_TOL = 0.01
GPU_SHADER_REL_TOL = 0.02


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def planck(wavelength_um: float, temperature_k: float) -> float:
    wavelength_m = wavelength_um * 1.0e-6
    exponent = H * C / (wavelength_m * KB * temperature_k)
    return (2.0 * H * C * C) / (wavelength_m**5 * math.expm1(exponent)) * 1.0e-6


def simpson_mean(band: str, temperature_k: float, intervals: int) -> float:
    lo_um, hi_um, _ = BANDS[band]
    if intervals <= 0 or intervals % 2:
        raise ValueError("Simpson interval count must be positive and even")
    step = (hi_um - lo_um) / intervals
    total = planck(lo_um, temperature_k) + planck(hi_um, temperature_k)
    for index in range(1, intervals):
        total += (4.0 if index % 2 else 2.0) * planck(lo_um + index * step, temperature_k)
    return total * step / 3.0 / (hi_um - lo_um)


def rel_error(actual: float, expected: float) -> float:
    return abs(actual - expected) / max(abs(expected), 1.0e-12)


def parse_fields(line: str) -> dict[str, str]:
    return {match.group(1): match.group(2) for match in re.finditer(r"([A-Za-z][A-Za-z0-9_]*)=([^\s]+)", line)}


def first_tag(log_path: Path, tag: str) -> tuple[int, str, dict[str, str]]:
    for line_number, line in enumerate(log_path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        if line.startswith(tag):
            return line_number, line, parse_fields(line)
    raise RuntimeError(f"missing runtime evidence {tag} in {log_path}")


def read_pfm(path: Path) -> tuple[np.ndarray, float]:
    with path.open("rb") as stream:
        magic = stream.readline().strip()
        if magic not in (b"PF", b"Pf"):
            raise RuntimeError(f"unsupported PFM magic {magic!r}")
        width, height = map(int, stream.readline().split())
        scale = float(stream.readline())
        channels = 3 if magic == b"PF" else 1
        dtype = "<f4" if scale < 0.0 else ">f4"
        values = np.fromfile(stream, dtype=dtype)
    expected = width * height * channels
    if values.size != expected:
        raise RuntimeError(f"PFM size mismatch: {values.size} != {expected}")
    # P6LinearCapture writes rows in displayed top-to-bottom order.  Do not
    # apply the generic PFM bottom-up convention here; annotation coordinates
    # and the runtime image therefore address the same row directly.
    return values.reshape((height, width, channels)), scale


def annotation_for_source_seq(path: Path, source_seq: int) -> dict[str, object]:
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        row = json.loads(line)
        if int(row.get("sourceSeq", -1)) == source_seq:
            return row
    raise RuntimeError(f"sourceSeq={source_seq} absent from {path}")


def parse_obj_centers(path: Path) -> dict[int, tuple[float, float, float]]:
    vertices: list[tuple[float, float, float]] = [(0.0, 0.0, 0.0)]
    groups: dict[int, set[int]] = {}
    active_id: int | None = None
    for line in path.read_text(encoding="utf-8", errors="strict").splitlines():
        if line.startswith("v "):
            _, x, y, z = line.split()[:4]
            vertices.append((float(x), float(y), float(z)))
        elif line.startswith("g sample_"):
            match = re.match(r"g sample_(\d+)_", line)
            active_id = int(match.group(1)) if match else None
            if active_id is not None:
                groups.setdefault(active_id, set())
        elif line.startswith("f ") and active_id is not None:
            for token in line.split()[1:]:
                groups[active_id].add(int(token.split("/", 1)[0]))
    centers: dict[int, tuple[float, float, float]] = {}
    for material_id, indices in groups.items():
        points = [vertices[index] for index in indices]
        centers[material_id] = tuple((min(p[axis] for p in points) + max(p[axis] for p in points)) / 2.0 for axis in range(3))
    return centers


def source_line(path: Path, needle: str) -> int:
    for line_number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        if needle in line:
            return line_number
    raise RuntimeError(f"source needle not found: {needle}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("case_dir", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    case_dir = args.case_dir.resolve()
    output = (args.output or case_dir / "cpu_shader_pixel_check.json").resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    request = json.loads((case_dir / "case_request.json").read_text(encoding="utf-8-sig"))
    case_summary = json.loads((case_dir / "case.json").read_text(encoding="utf-8-sig"))
    band = str(request["band"]).upper()
    if band not in BANDS:
        raise RuntimeError(f"unsupported band {band}")
    scenario = request["scenario"]
    if request.get("caseId") != "controlled_sample_solar_off":
        raise RuntimeError("this check requires controlled_sample_solar_off")
    if bool(scenario.get("naturalSolarEnable")) or float(scenario.get("m1SunVisibility", 1.0)) != 0.0:
        raise RuntimeError("solar-off fixture required to eliminate surface-temperature ambiguity")

    fixed_dir = case_dir / "variants" / "fixed"
    log_path = fixed_dir / "formal_tcp_run" / "hwa.out.log"
    m1_line_number, m1_line, m1 = first_tag(log_path, "[M1 Compare]")
    component_line_number, component_line, component = first_tag(log_path, "[Stage5 RadianceComponents]")
    if m1.get("finalOutput") != "M1" or m1.get("valid") != "1":
        raise RuntimeError("formal M1 runtime is not the image-producing valid route")
    tau = float(m1["tauUp"])
    path_thermal = float(m1["pathThermal"])
    path_scattering = float(m1["pathScattering"])
    path_radiance = path_thermal + path_scattering

    selected_seq = int(case_summary["selectedFrame"]["requestedSourceSeq"])
    annotation_path = fixed_dir / "receiver_recording" / "producer_annotations.jsonl"
    annotation = annotation_for_source_seq(annotation_path, selected_seq)
    target = annotation["targets"][0]
    xs = [int(point["x"]) for point in target["bboxCorners"]]
    ys = [int(point["y"]) for point in target["bboxCorners"]]
    bbox = {"left": min(xs), "right": max(xs), "top": min(ys), "bottom": max(ys)}

    raw_path = case_dir / "raw_radiance.pfm"
    raw, pfm_scale = read_pfm(raw_path)
    repo = Path(__file__).resolve().parents[1]
    asset_dir = repo / "HwaSim_IR" / "Bin" / "Config" / "TargetLib" / "p11" / "controlled_samples"
    manifest_path = asset_dir / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    centers = parse_obj_centers(asset_dir / "p11_controlled_samples.obj")
    bounds = manifest["nominalBoundsM"]
    xmin, xmax = map(float, bounds["x"])
    zmin, zmax = map(float, bounds["z"])

    panels = []
    errors: list[str] = []
    for panel in manifest["panels"]:
        material_id = int(panel["id"])
        if material_id not in BLACKBODY_IDS:
            continue
        center_x, _, center_z = centers[material_id]
        # The manifest documents that a camera on +Y sees each row in reverse
        # screen order.  The fixture's 180-degree target heading presents that
        # face to the south-side sensor used by the evidence runner.
        x_fraction = (xmax - center_x) / (xmax - xmin)
        y_fraction = (zmax - center_z) / (zmax - zmin)
        pixel_x = int(round(bbox["left"] + x_fraction * (bbox["right"] - bbox["left"])))
        pixel_y = int(round(bbox["top"] + y_fraction * (bbox["bottom"] - bbox["top"])))
        patch = raw[max(0, pixel_y - 2):pixel_y + 3, max(0, pixel_x - 2):pixel_x + 3, :]
        channel_medians = [float(np.median(patch[:, :, channel])) for channel in range(patch.shape[2])]
        gpu_value = float(statistics.median(channel_medians))

        temperature_k = float(panel["temperature_k"])
        reflectance, emissivity, transmissivity = map(float, panel[band.lower()])
        if abs(reflectance) > 1.0e-12 or abs(emissivity - 1.0) > 1.0e-12 or abs(transmissivity) > 1.0e-12:
            errors.append(f"material_{material_id}_is_not_ideal_blackbody")
        cpu_planck = simpson_mean(band, temperature_k, 16384)
        shader_intervals = BANDS[band][2]
        shader_planck = simpson_mean(band, temperature_k, shader_intervals)
        cpu_sensor = tau * emissivity * cpu_planck + path_radiance
        shader_sensor = tau * emissivity * shader_planck + path_radiance
        cpu_shader_error = rel_error(shader_sensor, cpu_sensor)
        gpu_shader_error = rel_error(gpu_value, shader_sensor)
        if cpu_shader_error > CPU_SHADER_REL_TOL:
            errors.append(f"material_{material_id}_cpu_shader_relative_error={cpu_shader_error:.9g}")
        if gpu_shader_error > GPU_SHADER_REL_TOL:
            errors.append(f"material_{material_id}_gpu_shader_relative_error={gpu_shader_error:.9g}")
        panels.append({
            "materialId": material_id,
            "label": panel["label"],
            "temperatureK": temperature_k,
            "emissivity": emissivity,
            "reflectance": reflectance,
            "transmissivity": transmissivity,
            "selectedPixel": {"x": pixel_x, "y": pixel_y, "patch": "5x5 median"},
            "gpuChannelMediansWm2SrUm": channel_medians,
            "cpuReferencePlanckBandMeanWm2SrUm": cpu_planck,
            "shaderSimpsonPlanckBandMeanWm2SrUm": shader_planck,
            "cpuReferenceSensorWm2SrUm": cpu_sensor,
            "shaderEquationSensorWm2SrUm": shader_sensor,
            "gpuRawPixelWm2SrUm": gpu_value,
            "cpuVsShaderRelativeError": cpu_shader_error,
            "gpuVsShaderRelativeError": gpu_shader_error,
            "pass": cpu_shader_error <= CPU_SHADER_REL_TOL and gpu_shader_error <= GPU_SHADER_REL_TOL,
        })

    shader_source = repo / "HwaSim_IR" / "HwaSim_IR" / "HwaSimIR.cpp"
    result = {
        "schema": "hwasimir_p11_cpu_shader_pixel_check_1",
        "result": "PASS" if not errors and len(panels) == 4 else "FAIL",
        "validationLevel": "controlled_formula_vs_production_shader_equation_vs_runtime_gpu_readback",
        "caseDirectory": str(case_dir),
        "band": band,
        "sourceSeq": selected_seq,
        "radianceUnit": "W/(m^2 sr um)",
        "bandQuantity": "response-weighted spectral-radiance band mean",
        "uniformEvidence": {
            "runtimeLog": str(log_path),
            "runtimeLogSha256": sha256(log_path),
            "m1CompareLine": m1_line_number,
            "componentLine": component_line_number,
            "u_m1_physics_runtime_en": 1,
            "u_m1_raw_si_output": 1,
            "u_m1_tau_up": tau,
            "u_m1_path_radiance": path_radiance,
            "pathThermalWm2SrUm": path_thermal,
            "pathScatteringWm2SrUm": path_scattering,
            "activeSensorWm2SrUm": float(m1.get("activeSensor", "0")),
            "finalOutput": m1["finalOutput"],
            "formalRuntimeAffectsImage": int(float(component.get("formalRuntimeAffectsImage", "0"))),
        },
        "shaderSource": {
            "path": str(shader_source),
            "sha256": sha256(shader_source),
            "sensorEquationLine": source_line(shader_source, "float m1_sensor = u_m1_tau_up * m1_surface + u_m1_path_radiance + l2_active_sensor;"),
            "tauUniformSetLine": source_line(shader_source, '"u_m1_tau_up"'),
            "pathUniformSetLine": source_line(shader_source, '"u_m1_path_radiance"'),
            "swirIntegration": "10-interval composite Simpson, 1.1-2.5 um",
            "mwirIntegration": "4-interval composite Simpson, 3.0-5.0 um",
        },
        "gpuReadback": {
            "path": str(raw_path),
            "sha256": sha256(raw_path),
            "width": int(raw.shape[1]),
            "height": int(raw.shape[0]),
            "pfmScale": pfm_scale,
            "stage": case_summary["rawRadiance"]["stage"],
            "domain": case_summary["rawRadiance"]["domain"],
            "annotation": str(annotation_path),
            "annotationSha256": sha256(annotation_path),
            "bbox": bbox,
        },
        "thresholds": {
            "cpuVsShaderRelative": CPU_SHADER_REL_TOL,
            "gpuVsShaderRelative": GPU_SHADER_REL_TOL,
        },
        "panels": panels,
        "errors": errors,
        "limitations": [
            "This three-way check intentionally uses rho=0 ideal blackbodies; per-pixel reflected solar/sky is validated separately.",
            "The glass panel is excluded from this opaque-surface pixel selector; production background transmission is validated by p11_glass_transmission_check.py and the independent WGL RGBA32F p11_glass_gpu_probe.py.",
            "These are controlled formula/implementation results, not calibration against a measured real target or sensor.",
        ],
        "sourceLines": {"m1Compare": m1_line, "components": component_line},
    }
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    csv_path = output.with_suffix(".csv")
    with csv_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=[
            "materialId", "label", "temperatureK", "pixelX", "pixelY",
            "cpuReferenceSensorWm2SrUm", "shaderEquationSensorWm2SrUm",
            "gpuRawPixelWm2SrUm", "cpuVsShaderRelativeError", "gpuVsShaderRelativeError", "pass",
        ])
        writer.writeheader()
        for row in panels:
            writer.writerow({
                "materialId": row["materialId"], "label": row["label"], "temperatureK": row["temperatureK"],
                "pixelX": row["selectedPixel"]["x"], "pixelY": row["selectedPixel"]["y"],
                "cpuReferenceSensorWm2SrUm": row["cpuReferenceSensorWm2SrUm"],
                "shaderEquationSensorWm2SrUm": row["shaderEquationSensorWm2SrUm"],
                "gpuRawPixelWm2SrUm": row["gpuRawPixelWm2SrUm"],
                "cpuVsShaderRelativeError": row["cpuVsShaderRelativeError"],
                "gpuVsShaderRelativeError": row["gpuVsShaderRelativeError"], "pass": row["pass"],
            })
    print(json.dumps({"result": result["result"], "output": str(output), "csv": str(csv_path), "errors": errors}))
    return 0 if result["result"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
