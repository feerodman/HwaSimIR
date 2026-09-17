#!/usr/bin/env python3
"""Build the deterministic, project-authored P11 civil panel-van source asset.

The generated OBJ is the canonical editable geometry.  Panda3D's obj2egg and
egg2bam tools are invoked by p11_civil_van_asset_check.ps1 so the native EGG
and BAM files are reproducible derived artifacts.
"""

from __future__ import annotations

import csv
import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "HwaSim_IR" / "Bin" / "Config" / "TargetLib" / "p11" / "civil_van"

MATERIALS = [
    {
        "id": 11,
        "key": "car_paint",
        "db": "BM_PAINT",
        "rgb": (0.68, 0.08, 0.055),
        "swir": (0.50, 0.50, 0.00),
        "mwir": (0.25, 0.75, 0.00),
        "temperature_k": 300.0,
        "thickness_m": 0.0008,
        "notes": "Painted steel outer panels; surface optics, not visible RGB.",
    },
    {
        "id": 12,
        "key": "metal",
        "db": "BM_METAL",
        "rgb": (0.38, 0.41, 0.45),
        "swir": (0.58, 0.42, 0.00),
        "mwir": (0.35, 0.65, 0.00),
        "temperature_k": 303.0,
        "thickness_m": 0.0030,
        "notes": "Unpainted bumper, grille and wheel-hub metal.",
    },
    {
        "id": 13,
        "key": "glass",
        "db": "BM_GLASS",
        "rgb": (0.055, 0.18, 0.27),
        "swir": (0.08, 0.07, 0.85),
        "mwir": (0.05, 0.90, 0.05),
        "temperature_k": 299.0,
        "thickness_m": 0.0050,
        "notes": "Generic vehicle glazing; transmissive SWIR assumption is explicit.",
    },
    {
        "id": 14,
        "key": "rubber",
        "db": "BM_RUBBER",
        "rgb": (0.035, 0.035, 0.035),
        "swir": (0.15, 0.85, 0.00),
        "mwir": (0.05, 0.95, 0.00),
        "temperature_k": 306.0,
        "thickness_m": 0.0350,
        "notes": "Tyre tread/sidewall engineering assumption pending coupon data.",
    },
    {
        "id": 15,
        "key": "engine_bay",
        "db": "BM_METAL-IRON",
        "rgb": (0.63, 0.22, 0.035),
        "swir": (0.40, 0.60, 0.00),
        "mwir": (0.20, 0.80, 0.00),
        "temperature_k": 303.0,
        "engine_on_temperature_k": 345.0,
        "thickness_m": 0.0040,
        "notes": "Localized engine/hood sample: 303 K engine-off nominal, 345 K only while engine-on; never applied to the whole vehicle.",
    },
    {
        "id": 16,
        "key": "exhaust_tailpipe",
        "db": "BM_METAL-IRON",
        "rgb": (0.78, 0.34, 0.04),
        "swir": (0.35, 0.65, 0.00),
        "mwir": (0.12, 0.88, 0.00),
        "temperature_k": 303.0,
        "engine_on_temperature_k": 475.0,
        "thickness_m": 0.0020,
        "notes": "Localized oxidized exhaust/tailpipe: 303 K engine-off nominal, 475 K only while engine-on.",
    },
]

MAT_BY_KEY = {m["key"]: m for m in MATERIALS}


class Mesh:
    def __init__(self) -> None:
        self.vertices: list[tuple[float, float, float]] = []
        self.uvs: list[tuple[float, float]] = []
        self.faces: list[tuple[str, str, list[int]]] = []

    def vertex(self, xyz: tuple[float, float, float], material: str) -> int:
        self.vertices.append(xyz)
        slot = MATERIALS.index(MAT_BY_KEY[material])
        self.uvs.append(((slot + 0.5) / len(MATERIALS), 0.5))
        return len(self.vertices)

    def polygon(self, group: str, material: str, coords: list[tuple[float, float, float]]) -> None:
        self.faces.append((group, material, [self.vertex(p, material) for p in coords]))

    def box(
        self,
        group: str,
        material: str,
        xmin: float,
        xmax: float,
        ymin: float,
        ymax: float,
        zmin: float,
        zmax: float,
    ) -> None:
        p = {
            "000": (xmin, ymin, zmin), "100": (xmax, ymin, zmin),
            "110": (xmax, ymax, zmin), "010": (xmin, ymax, zmin),
            "001": (xmin, ymin, zmax), "101": (xmax, ymin, zmax),
            "111": (xmax, ymax, zmax), "011": (xmin, ymax, zmax),
        }
        for face in (
            ["000", "010", "110", "100"],
            ["001", "101", "111", "011"],
            ["000", "100", "101", "001"],
            ["010", "011", "111", "110"],
            ["000", "001", "011", "010"],
            ["100", "110", "111", "101"],
        ):
            self.polygon(group, material, [p[k] for k in face])

    def cylinder_x(
        self,
        group: str,
        material: str,
        x0: float,
        x1: float,
        y: float,
        z: float,
        radius: float,
        segments: int = 12,
    ) -> None:
        left = []
        right = []
        for i in range(segments):
            a = 2.0 * math.pi * i / segments
            yz = (y + radius * math.cos(a), z + radius * math.sin(a))
            left.append((x0, yz[0], yz[1]))
            right.append((x1, yz[0], yz[1]))
        for i in range(segments):
            j = (i + 1) % segments
            self.polygon(group, material, [left[i], left[j], right[j], right[i]])
        self.polygon(group, material, list(reversed(left)))
        self.polygon(group, material, right)

    def cylinder_y(
        self,
        group: str,
        material: str,
        y0: float,
        y1: float,
        x: float,
        z: float,
        radius: float,
        segments: int = 10,
    ) -> None:
        rear = []
        front = []
        for i in range(segments):
            a = 2.0 * math.pi * i / segments
            xz = (x + radius * math.cos(a), z + radius * math.sin(a))
            rear.append((xz[0], y0, xz[1]))
            front.append((xz[0], y1, xz[1]))
        for i in range(segments):
            j = (i + 1) % segments
            self.polygon(group, material, [rear[i], front[i], front[j], rear[j]])
        self.polygon(group, material, list(reversed(rear)))
        self.polygon(group, material, front)


def build_mesh() -> Mesh:
    mesh = Mesh()

    # Body axes: X width, Y length (front is +Y), Z up.  Dimensions are metres.
    mesh.box("paint_lower_body", "car_paint", -1.00, 1.00, -2.40, 2.40, 0.52, 1.43)
    mesh.box("paint_cargo_body", "car_paint", -0.94, 0.94, -2.20, 0.56, 1.42, 2.34)

    # Trapezoidal cab: the front upper edge is swept rearwards to form a windshield.
    cab = {
        "a": (-0.94, 0.55, 1.42), "b": (0.94, 0.55, 1.42),
        "c": (0.94, 2.16, 1.42), "d": (-0.94, 2.16, 1.42),
        "e": (-0.94, 0.55, 2.34), "f": (0.94, 0.55, 2.34),
        "g": (0.90, 1.66, 2.24), "h": (-0.90, 1.66, 2.24),
    }
    for keys in (["a", "d", "c", "b"], ["e", "f", "g", "h"],
                 ["a", "b", "f", "e"], ["d", "h", "g", "c"],
                 ["a", "e", "h", "d"], ["b", "c", "g", "f"]):
        mesh.polygon("paint_cab", "car_paint", [cab[k] for k in keys])

    # Glass is a distinct, slightly offset skin so it is unambiguous in close views.
    mesh.polygon("glass_windshield", "glass", [
        (-0.80, 2.225, 1.55), (-0.76, 1.785, 2.12),
        (0.76, 1.785, 2.12), (0.80, 2.225, 1.55),
    ])
    mesh.polygon("glass_left_window", "glass", [
        (-0.970, 0.76, 1.57), (-0.970, 0.76, 2.18),
        (-0.930, 1.58, 2.12), (-0.970, 1.58, 1.57),
    ])
    mesh.polygon("glass_right_window", "glass", [
        (0.970, 1.58, 1.57), (0.930, 1.58, 2.12),
        (0.970, 0.76, 2.18), (0.970, 0.76, 1.57),
    ])

    # Localized engine bay/hood patch, intentionally separate from the outer paint.
    mesh.box("engine_bay", "engine_bay", -0.76, 0.76, 1.78, 2.33, 1.445, 1.535)

    # Bumpers and grille stay separate from body paint.
    mesh.box("metal_front_bumper", "metal", -0.94, 0.94, 2.39, 2.51, 0.62, 0.86)
    mesh.box("metal_rear_bumper", "metal", -0.94, 0.94, -2.51, -2.39, 0.62, 0.84)
    mesh.box("metal_front_grille", "metal", -0.55, 0.55, 2.402, 2.515, 0.90, 1.20)

    # Four tyres plus exposed hub faces; wheels are deliberately low-poly.
    for side, x0, x1 in (("left", -1.13, -0.91), ("right", 0.91, 1.13)):
        for axle, y in (("rear", -1.55), ("front", 1.55)):
            mesh.cylinder_x(f"rubber_{side}_{axle}_tyre", "rubber", x0, x1, y, 0.55, 0.43)
            outer0, outer1 = ((-1.145, -1.132) if side == "left" else (1.132, 1.145))
            mesh.cylinder_x(f"metal_{side}_{axle}_hub", "metal", outer0, outer1, y, 0.55, 0.19)

    # Localized exhaust line and tailpipe exit.  It is not a global body heat source.
    mesh.cylinder_y("exhaust_tailpipe", "exhaust_tailpipe", -2.88, -2.12, 0.66, 0.56, 0.105)
    mesh.box("exhaust_muffler", "exhaust_tailpipe", 0.43, 0.86, -2.10, -1.48, 0.42, 0.68)
    return mesh


def write_obj(mesh: Mesh) -> None:
    path = OUT / "p11_civil_van.obj"
    lines = [
        "# SPDX-License-Identifier: CC0-1.0",
        "# Project-authored P11 low-poly civil panel van; units are metres.",
        "mtllib p11_civil_van.mtl",
        "o P11_Civil_Van",
    ]
    lines.extend(f"v {x:.8f} {y:.8f} {z:.8f}" for x, y, z in mesh.vertices)
    lines.extend(f"vt {u:.8f} {v:.8f}" for u, v in mesh.uvs)
    active = None
    for group, material, indices in mesh.faces:
        state = (group, material)
        if state != active:
            lines.extend((f"g {group}", f"usemtl P11_{material}", "s off"))
            active = state
        refs = " ".join(f"{i}/{i}" for i in indices)
        lines.append(f"f {refs}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def write_mtl() -> None:
    lines = [
        "# SPDX-License-Identifier: CC0-1.0",
        "# Visible preview colours only; never use these RGB values as IR optics.",
    ]
    for material in MATERIALS:
        r, g, b = material["rgb"]
        lines.extend((
            f"newmtl P11_{material['key']}",
            f"Kd {r:.6f} {g:.6f} {b:.6f}",
            "Ka 0.080000 0.080000 0.080000",
            "Ks 0.120000 0.120000 0.120000",
            "Ns 16.000000",
            "d 1.000000",
            "illum 2",
            "",
        ))
    (OUT / "p11_civil_van.mtl").write_text("\n".join(lines), encoding="utf-8", newline="\n")


def write_atlases() -> None:
    visible = [tuple(round(c * 255) for c in material["rgb"]) for material in MATERIALS]
    ppm = ["P3", "# Visible preview colours only; not IR material data.", f"{len(visible)} 1", "255"]
    ppm.append(" ".join(f"{r} {g} {b}" for r, g, b in visible))
    (OUT / "p11_civil_van_visible.ppm").write_text("\n".join(ppm) + "\n", encoding="ascii")

    pgm = ["P2", "# Exact 8-bit material IDs; load with nearest filtering.", f"{len(MATERIALS)} 1", "255"]
    pgm.append(" ".join(str(material["id"]) for material in MATERIALS))
    (OUT / "p11_civil_van_material_id.pgm").write_text("\n".join(pgm) + "\n", encoding="ascii")


def write_material_csv() -> None:
    path = OUT / "p11_civil_van_band_optics.csv"
    fields = [
        "MaterialId", "Region", "MaterialDatabaseName", "SWIRReflectance",
        "SWIREmissivity", "SWIRTransmissivity", "MWIRReflectance",
        "MWIREmissivity", "MWIRTransmissivity", "NominalTemperatureK",
        "EngineOnTemperatureK",
        "EffectiveThicknessM", "Source", "Notes",
    ]
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        for material in MATERIALS:
            writer.writerow({
                "MaterialId": material["id"],
                "Region": material["key"],
                "MaterialDatabaseName": material["db"],
                "SWIRReflectance": f"{material['swir'][0]:.4f}",
                "SWIREmissivity": f"{material['swir'][1]:.4f}",
                "SWIRTransmissivity": f"{material['swir'][2]:.4f}",
                "MWIRReflectance": f"{material['mwir'][0]:.4f}",
                "MWIREmissivity": f"{material['mwir'][1]:.4f}",
                "MWIRTransmissivity": f"{material['mwir'][2]:.4f}",
                "NominalTemperatureK": f"{material['temperature_k']:.1f}",
                "EngineOnTemperatureK": (
                    f"{material['engine_on_temperature_k']:.1f}"
                    if "engine_on_temperature_k" in material else ""
                ),
                "EffectiveThicknessM": f"{material['thickness_m']:.4f}",
                "Source": "P11_model_engineering_assumption",
                "Notes": material["notes"],
            })


def write_material_xml() -> None:
    lines = ["<Composite_Material_Table>"]
    for material in MATERIALS:
        swir_reflectance, swir_emissivity, swir_transmissivity = material["swir"]
        mwir_reflectance, mwir_emissivity, mwir_transmissivity = material["mwir"]
        lines.extend((
            f"  <Composite_Material index=\"{material['id']}\" IsVisibleInClassification=\"True\" IsVisibleInPaint=\"True\">",
            f"    <Name>P11-{material['key'].upper().replace('_', '-')}</Name>",
            "    <Primary_Substrate>",
            "      <Material>",
            f"        <Name>{material['db']}</Name>",
            "        <Weight>100</Weight>",
            "      </Material>",
            f"      <Thickness>{material['thickness_m']:.4f}</Thickness>",
            "    </Primary_Substrate>",
            f"    <SWIRReflectance>{swir_reflectance:.4f}</SWIRReflectance>",
            f"    <SWIREmissivity>{swir_emissivity:.4f}</SWIREmissivity>",
            f"    <SWIRTransmissivity>{swir_transmissivity:.4f}</SWIRTransmissivity>",
            f"    <MWIRReflectance>{mwir_reflectance:.4f}</MWIRReflectance>",
            f"    <MWIREmissivity>{mwir_emissivity:.4f}</MWIREmissivity>",
            f"    <MWIRTransmissivity>{mwir_transmissivity:.4f}</MWIRTransmissivity>",
            f"    <NominalTemperatureK>{material['temperature_k']:.1f}</NominalTemperatureK>",
        ))
        if "engine_on_temperature_k" in material:
            lines.append(f"    <EngineOnTemperatureK>{material['engine_on_temperature_k']:.1f}</EngineOnTemperatureK>")
        lines.extend((
            "  </Composite_Material>",
        ))
    lines.append("</Composite_Material_Table>")
    (OUT / "p11_civil_van_material_id.pgm.xml").write_text(
        "\n".join(lines) + "\n", encoding="utf-8", newline="\n"
    )


def write_manifest(mesh: Mesh) -> None:
    manifest = {
        "schema": 1,
        "asset": "P11 Civil Panel Van",
        "origin": "Project-authored deterministic geometry; no GMC geometry, texture, or conversion output used.",
        "license": "CC0-1.0",
        "coordinateSystem": "Z-up, X width, Y length, front +Y",
        "units": "metres",
        "nominalBoundsM": {"x": [-1.145, 1.145], "y": [-2.88, 2.515], "z": [0.12, 2.34]},
        "geometry": {"vertices": len(mesh.vertices), "polygons": len(mesh.faces), "materials": len(MATERIALS)},
        "canonicalSource": "p11_civil_van.obj",
        "derived": ["p11_civil_van.egg", "p11_civil_van.bam"],
        "visibleTexture": "p11_civil_van_visible.ppm",
        "materialIdTexture": "p11_civil_van_material_id.pgm",
        "materialMap": "p11_civil_van_material_id.pgm.xml",
        "bandOptics": "p11_civil_van_band_optics.csv",
        "materialPolicy": "Visible RGB is presentation-only. SWIR/MWIR optics are explicit engineering assumptions and are not inferred from RGB.",
        "materials": MATERIALS,
    }
    (OUT / "manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n"
    )


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
    print(f"vertices={len(mesh.vertices)} polygons={len(mesh.faces)} materials={len(MATERIALS)}")


if __name__ == "__main__":
    main()
