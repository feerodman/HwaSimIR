#!/usr/bin/env python3
"""Generate the deterministic CC0 P11 controlled IR sample rack.

The eight large panels intentionally expose ideal/reference properties through
material IDs 21..28.  Visible RGB is only a human orientation aid and is never
used to derive SWIR/MWIR optics or temperature.
"""

from __future__ import annotations

import csv
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "HwaSim_IR" / "Bin" / "Config" / "TargetLib" / "p11" / "controlled_samples"

SAMPLES = [
    {"id": 21, "key": "blackbody_250K", "label": "BLACKBODY-250K", "db": "BM_PAINT",
     "rgb": (0.05, 0.10, 0.34), "swir": (0.0, 1.0, 0.0), "mwir": (0.0, 1.0, 0.0), "temperature_k": 250.0,
     "basis": "Ideal blackbody reference assumption; epsilon=1 exactly in both bands."},
    {"id": 22, "key": "blackbody_300K", "label": "BLACKBODY-300K", "db": "BM_PAINT",
     "rgb": (0.08, 0.34, 0.62), "swir": (0.0, 1.0, 0.0), "mwir": (0.0, 1.0, 0.0), "temperature_k": 300.0,
     "basis": "Ideal blackbody reference assumption; epsilon=1 exactly in both bands."},
    {"id": 23, "key": "blackbody_350K", "label": "BLACKBODY-350K", "db": "BM_PAINT",
     "rgb": (0.12, 0.58, 0.45), "swir": (0.0, 1.0, 0.0), "mwir": (0.0, 1.0, 0.0), "temperature_k": 350.0,
     "basis": "Ideal blackbody reference assumption; epsilon=1 exactly in both bands."},
    {"id": 24, "key": "blackbody_475K", "label": "BLACKBODY-475K", "db": "BM_PAINT",
     "rgb": (0.86, 0.20, 0.08), "swir": (0.0, 1.0, 0.0), "mwir": (0.0, 1.0, 0.0), "temperature_k": 475.0,
     "basis": "Ideal blackbody reference assumption; epsilon=1 exactly in both bands."},
    {"id": 25, "key": "graybody_300K", "label": "GRAYBODY-E0.8-300K", "db": "BM_PAINT",
     "rgb": (0.42, 0.42, 0.42), "swir": (0.2, 0.8, 0.0), "mwir": (0.2, 0.8, 0.0), "temperature_k": 300.0,
     "basis": "Ideal opaque graybody reference assumption; rho=0.2 epsilon=0.8."},
    {"id": 26, "key": "reflector_300K", "label": "REFLECTOR-R0.8-300K", "db": "BM_METAL-IRON",
     "rgb": (0.78, 0.78, 0.82), "swir": (0.8, 0.2, 0.0), "mwir": (0.8, 0.2, 0.0), "temperature_k": 300.0,
     "basis": "Ideal opaque diffuse-reflector reference assumption; rho=0.8 epsilon=0.2."},
    {"id": 27, "key": "glass_300K", "label": "GLASS-SWIR-T0.85-300K", "db": "BM_GLASS",
     "rgb": (0.10, 0.66, 0.82), "swir": (0.08, 0.07, 0.85), "mwir": (0.05, 0.90, 0.05), "temperature_k": 300.0,
     "basis": "Generic uncoated-glass engineering sample; explicit SWIR transmission and MWIR emission, pending coupon data."},
    {"id": 28, "key": "cold_graybody_275K", "label": "GRAYBODY-E0.7-275K", "db": "BM_PAINT",
     "rgb": (0.32, 0.20, 0.62), "swir": (0.3, 0.7, 0.0), "mwir": (0.3, 0.7, 0.0), "temperature_k": 275.0,
     "basis": "Ideal opaque cold-graybody engineering reference; rho=0.3 epsilon=0.7."},
]
SAMPLE_BY_KEY = {sample["key"]: sample for sample in SAMPLES}


class Mesh:
    def __init__(self) -> None:
        self.vertices: list[tuple[float, float, float]] = []
        self.uvs: list[tuple[float, float]] = []
        self.faces: list[tuple[str, str, list[int]]] = []

    def vertex(self, xyz: tuple[float, float, float], material: str) -> int:
        self.vertices.append(xyz)
        slot = SAMPLES.index(SAMPLE_BY_KEY[material])
        self.uvs.append(((slot + 0.5) / len(SAMPLES), 0.5))
        return len(self.vertices)

    def polygon(self, group: str, material: str, coords: list[tuple[float, float, float]]) -> None:
        self.faces.append((group, material, [self.vertex(point, material) for point in coords]))

    def box(self, group: str, material: str, xmin: float, xmax: float,
            ymin: float, ymax: float, zmin: float, zmax: float) -> None:
        p = {
            "000": (xmin, ymin, zmin), "100": (xmax, ymin, zmin),
            "110": (xmax, ymax, zmin), "010": (xmin, ymax, zmin),
            "001": (xmin, ymin, zmax), "101": (xmax, ymin, zmax),
            "111": (xmax, ymax, zmax), "011": (xmin, ymax, zmax),
        }
        for face in (["000", "010", "110", "100"], ["001", "101", "111", "011"],
                     ["000", "100", "101", "001"], ["010", "011", "111", "110"],
                     ["000", "001", "011", "010"], ["100", "110", "111", "101"]):
            self.polygon(group, material, [p[key] for key in face])


def build_mesh() -> Mesh:
    mesh = Mesh()
    panel_width, panel_height = 0.95, 0.90
    x_centers = (-1.68, -0.56, 0.56, 1.68)
    z_centers = (1.75, 0.65)
    for index, sample in enumerate(SAMPLES):
        row, column = divmod(index, 4)
        x, z = x_centers[column], z_centers[row]
        mesh.box(
            f"sample_{sample['id']}_{sample['key']}", sample["key"],
            x - panel_width / 2.0, x + panel_width / 2.0,
            -0.08, 0.08,
            z - panel_height / 2.0, z + panel_height / 2.0,
        )
    return mesh


def write_obj(mesh: Mesh) -> None:
    lines = [
        "# SPDX-License-Identifier: CC0-1.0",
        "# Project-authored P11 controlled IR sample rack; units are metres.",
        "mtllib p11_controlled_samples.mtl",
        "o P11_Controlled_IR_Samples",
    ]
    lines.extend(f"v {x:.8f} {y:.8f} {z:.8f}" for x, y, z in mesh.vertices)
    lines.extend(f"vt {u:.8f} {v:.8f}" for u, v in mesh.uvs)
    active = None
    for group, material, indices in mesh.faces:
        if (group, material) != active:
            lines.extend((f"g {group}", f"usemtl P11_{material}", "s off"))
            active = (group, material)
        lines.append("f " + " ".join(f"{idx}/{idx}" for idx in indices))
    (OUT / "p11_controlled_samples.obj").write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def write_mtl() -> None:
    lines = [
        "# SPDX-License-Identifier: CC0-1.0",
        "# Visible orientation colours only; never infer IR properties from Kd.",
    ]
    for sample in SAMPLES:
        r, g, b = sample["rgb"]
        lines.extend((f"newmtl P11_{sample['key']}", f"Kd {r:.6f} {g:.6f} {b:.6f}",
                      "Ka 0.080000 0.080000 0.080000", "Ks 0.040000 0.040000 0.040000",
                      "Ns 8.000000", "d 1.000000", "illum 2", ""))
    (OUT / "p11_controlled_samples.mtl").write_text("\n".join(lines), encoding="utf-8", newline="\n")


def write_atlases() -> None:
    visible = [tuple(round(channel * 255) for channel in sample["rgb"]) for sample in SAMPLES]
    ppm = ["P3", "# Visible false colours only; not IR data.", "8 1", "255",
           " ".join(f"{r} {g} {b}" for r, g, b in visible)]
    (OUT / "p11_controlled_samples_visible.ppm").write_text("\n".join(ppm) + "\n", encoding="ascii")
    pgm = ["P2", "# Exact material IDs 21..28; nearest filtering required.", "8 1", "255",
           " ".join(str(sample["id"]) for sample in SAMPLES)]
    (OUT / "p11_controlled_samples_material_id.pgm").write_text("\n".join(pgm) + "\n", encoding="ascii")


def write_material_csv() -> None:
    fields = ["MaterialId", "Region", "Label", "MaterialDatabaseName", "SWIRReflectance",
              "SWIREmissivity", "SWIRTransmissivity", "MWIRReflectance", "MWIREmissivity",
              "MWIRTransmissivity", "NominalTemperatureK", "EffectiveThicknessM", "Source", "Notes"]
    with (OUT / "p11_controlled_samples_band_optics.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for sample in SAMPLES:
            writer.writerow({
                "MaterialId": sample["id"], "Region": sample["key"], "Label": sample["label"],
                "MaterialDatabaseName": sample["db"],
                "SWIRReflectance": f"{sample['swir'][0]:.4f}",
                "SWIREmissivity": f"{sample['swir'][1]:.4f}",
                "SWIRTransmissivity": f"{sample['swir'][2]:.4f}",
                "MWIRReflectance": f"{sample['mwir'][0]:.4f}",
                "MWIREmissivity": f"{sample['mwir'][1]:.4f}",
                "MWIRTransmissivity": f"{sample['mwir'][2]:.4f}",
                "NominalTemperatureK": f"{sample['temperature_k']:.1f}",
                "EffectiveThicknessM": "0.0100", "Source": "P11_controlled_ideal_or_engineering_assumption",
                "Notes": sample["basis"],
            })


def write_material_xml() -> None:
    lines = ["<Composite_Material_Table>"]
    for sample in SAMPLES:
        sr, se, st = sample["swir"]
        mr, me, mt = sample["mwir"]
        lines.extend((
            f"  <Composite_Material index=\"{sample['id']}\" IsVisibleInClassification=\"True\" IsVisibleInPaint=\"True\">",
            f"    <Name>P11-{sample['label']}</Name>", "    <Primary_Substrate>", "      <Material>",
            f"        <Name>{sample['db']}</Name>", "        <Weight>100</Weight>", "      </Material>",
            "      <Thickness>0.0100</Thickness>", "    </Primary_Substrate>",
            f"    <SWIRReflectance>{sr:.4f}</SWIRReflectance>",
            f"    <SWIREmissivity>{se:.4f}</SWIREmissivity>",
            f"    <SWIRTransmissivity>{st:.4f}</SWIRTransmissivity>",
            f"    <MWIRReflectance>{mr:.4f}</MWIRReflectance>",
            f"    <MWIREmissivity>{me:.4f}</MWIREmissivity>",
            f"    <MWIRTransmissivity>{mt:.4f}</MWIRTransmissivity>",
            f"    <NominalTemperatureK>{sample['temperature_k']:.1f}</NominalTemperatureK>",
            "  </Composite_Material>",
        ))
    lines.append("</Composite_Material_Table>")
    (OUT / "p11_controlled_samples_material_id.pgm.xml").write_text(
        "\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def write_manifest(mesh: Mesh) -> None:
    panels = []
    for index, sample in enumerate(SAMPLES):
        row, column = divmod(index, 4)
        panels.append({**sample, "row": row, "column": column})
    manifest = {
        "schema": 1, "asset": "P11 Controlled IR Samples", "protocolTargetType": "0x66",
        "platform": "Resv2", "origin": "Project-authored deterministic geometry and reference values.",
        "license": "CC0-1.0", "coordinateSystem": "Z-up, front +Y", "units": "metres",
        "nominalBoundsM": {"x": [-2.155, 2.155], "y": [-0.08, 0.08], "z": [0.20, 2.20]},
        "geometry": {"vertices": len(mesh.vertices), "polygons": len(mesh.faces), "materials": 8},
        "canonicalSource": "p11_controlled_samples.obj",
        "derived": ["p11_controlled_samples.egg", "p11_controlled_samples.bam"],
        "visibleTexture": "p11_controlled_samples_visible.ppm",
        "materialIdTexture": "p11_controlled_samples_material_id.pgm",
        "materialMap": "p11_controlled_samples_material_id.pgm.xml",
        "bandOptics": "p11_controlled_samples_band_optics.csv",
        "layout": "in increasing world X: top row IDs 21..24 and bottom row IDs 25..28; a camera on +Y sees each row in reverse screen order",
        "materialPolicy": "All SWIR/MWIR values are explicit ideal or engineering assumptions; visible RGB is orientation-only.",
        "panels": panels,
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    mesh = build_mesh()
    write_obj(mesh)
    write_mtl()
    write_atlases()
    write_material_csv()
    write_material_xml()
    write_manifest(mesh)
    print(f"generated={OUT}")
    print(f"vertices={len(mesh.vertices)} polygons={len(mesh.faces)} materials={len(SAMPLES)}")


if __name__ == "__main__":
    main()
