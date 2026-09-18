#!/usr/bin/env python3
"""Build the additive P12 two-layer controlled-glass DDS fixture sources."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "HwaSim_IR" / "Bin" / "Config" / "TargetLib" / "p12" / "controlled_glass"


def box(group: str, material: str, bounds: tuple[float, float, float, float, float, float],
        vertices: list[tuple[float, float, float]], faces: list[tuple[str, str, list[int]]]) -> None:
    xmin, xmax, ymin, ymax, zmin, zmax = bounds
    p = {
        "000": (xmin, ymin, zmin), "100": (xmax, ymin, zmin),
        "110": (xmax, ymax, zmin), "010": (xmin, ymax, zmin),
        "001": (xmin, ymin, zmax), "101": (xmax, ymin, zmax),
        "111": (xmax, ymax, zmax), "011": (xmin, ymax, zmax),
    }
    for keys in (["000", "010", "110", "100"], ["001", "101", "111", "011"],
                 ["000", "100", "101", "001"], ["010", "011", "111", "110"],
                 ["000", "001", "011", "010"], ["100", "110", "111", "101"]):
        indices = []
        for key in keys:
            vertices.append(p[key]); indices.append(len(vertices))
        faces.append((group, material, indices))


def material_xml(background: str) -> str:
    if background == "bright":
        swir, mwir, temperature = (1.0, 0.0, 0.0), (0.0, 1.0, 0.0), 475.0
    else:
        swir, mwir, temperature = (0.0, 1.0, 0.0), (0.0, 1.0, 0.0), 250.0
    return f"""<Composite_Material_Table>
  <Composite_Material index=\"31\" IsVisibleInClassification=\"True\" IsVisibleInPaint=\"True\">
    <Name>P12-CONTROLLED-GLASS</Name>
    <Primary_Substrate><Material><Name>BM_GLASS</Name><Weight>100</Weight></Material><Thickness>0.0100</Thickness></Primary_Substrate>
    <SWIRReflectance>0.0800</SWIRReflectance><SWIREmissivity>0.0700</SWIREmissivity><SWIRTransmissivity>0.8500</SWIRTransmissivity>
    <MWIRReflectance>0.0500</MWIRReflectance><MWIREmissivity>0.9000</MWIREmissivity><MWIRTransmissivity>0.0500</MWIRTransmissivity>
    <NominalTemperatureK>300.0</NominalTemperatureK>
  </Composite_Material>
  <Composite_Material index=\"32\" IsVisibleInClassification=\"True\" IsVisibleInPaint=\"True\">
    <Name>P12-CONTROLLED-BACKGROUND-{background.upper()}</Name>
    <Primary_Substrate><Material><Name>BM_PAINT</Name><Weight>100</Weight></Material><Thickness>0.0200</Thickness></Primary_Substrate>
    <SWIRReflectance>{swir[0]:.4f}</SWIRReflectance><SWIREmissivity>{swir[1]:.4f}</SWIREmissivity><SWIRTransmissivity>{swir[2]:.4f}</SWIRTransmissivity>
    <MWIRReflectance>{mwir[0]:.4f}</MWIRReflectance><MWIREmissivity>{mwir[1]:.4f}</MWIREmissivity><MWIRTransmissivity>{mwir[2]:.4f}</MWIRTransmissivity>
    <NominalTemperatureK>{temperature:.1f}</NominalTemperatureK>
  </Composite_Material>
</Composite_Material_Table>
"""


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    vertices: list[tuple[float, float, float]] = []
    faces: list[tuple[str, str, list[int]]] = []
    # With the protocol fixture yaw=180 and a camera south of the target,
    # local +Y is the camera side.  The glass is therefore physically in
    # front of the larger background plate.
    box("p12_glass_front", "glass", (-0.65, 0.65, 0.18, 0.22, 0.35, 1.65), vertices, faces)
    box("p12_background", "background", (-0.85, 0.85, -0.42, -0.38, 0.15, 1.85), vertices, faces)
    lines = ["# SPDX-License-Identifier: CC0-1.0", "mtllib p12_controlled_glass.mtl", "o P12_Controlled_Glass_Rig"]
    lines.extend(f"v {x:.8f} {y:.8f} {z:.8f}" for x, y, z in vertices)
    active = None
    for group, material, indices in faces:
        if (group, material) != active:
            lines.extend((f"g {group}", f"usemtl P12_{material}", "s off")); active = (group, material)
        uv = "1" if material == "glass" else "2"
        lines.append("f " + " ".join(f"{idx}/{uv}" for idx in indices))
    # Atlas samples are centered at u=.25 and .75.
    lines.insert(4 + len(vertices), "vt 0.25000000 0.50000000")
    lines.insert(5 + len(vertices), "vt 0.75000000 0.50000000")
    (OUT / "p12_controlled_glass.obj").write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    (OUT / "p12_controlled_glass.mtl").write_text(
        "newmtl P12_glass\nKd 0.10 0.65 0.85\nillum 2\n\nnewmtl P12_background\nKd 0.90 0.90 0.90\nillum 2\n",
        encoding="utf-8", newline="\n")
    (OUT / "p12_controlled_glass_visible.ppm").write_text("P3\n2 1\n255\n26 166 217 230 230 230\n", encoding="ascii", newline="\n")
    (OUT / "p12_controlled_glass_material_id.pgm").write_text("P2\n2 1\n255\n31 32\n", encoding="ascii", newline="\n")
    for background in ("bright", "dark"):
        (OUT / f"p12_controlled_glass_{background}.xml").write_text(material_xml(background), encoding="utf-8", newline="\n")
    readme = """# P12 controlled glass rig

This additive CC0 engineering fixture has one independently tagged glass slab
in front of one larger, opaque background plate. It is selected only by the
explicit `P12ControlledGlassRig=1` diagnostic switch; ordinary 0x66 runs retain
the P11 sample rack. `P12ControlledGlassBackground=Bright|Dark` selects an
energy-balanced known background, and `P12ControlledGlassTransmission=On|Blocked`
selects the production single-pass composite or a declared non-physical
transmission ablation. The glass values are engineering assumptions, not coupon
or sensor calibration. No refraction, Fresnel term, or multiple reflection is
claimed.
"""
    (OUT / "README.md").write_text(readme, encoding="utf-8", newline="\n")
    manifest = {
        "schema": "hwasimir.p12.controlled-glass-rig.v1", "license": "CC0-1.0",
        "protocolTargetType": "0x66 diagnostic selection only", "units": "metres",
        "geometry": {"glassMaterialId": 31, "backgroundMaterialId": 32, "glassLocalY": [0.18, 0.22], "backgroundLocalY": [-0.42, -0.38]},
        "glass": {"temperatureK": 300.0, "SWIR": {"rho": .08, "epsilon": .07, "tau": .85}, "MWIR": {"rho": .05, "epsilon": .90, "tau": .05}, "status": "engineering_assumption_NOT_VERIFIED_CALIBRATION"},
        "backgrounds": {
            "Bright": {"temperatureK": 475.0, "SWIR": {"rho": 1.0, "epsilon": 0.0, "tau": 0.0}, "MWIR": {"rho": 0.0, "epsilon": 1.0, "tau": 0.0}},
            "Dark": {"temperatureK": 250.0, "SWIR": {"rho": 0.0, "epsilon": 1.0, "tau": 0.0}, "MWIR": {"rho": 0.0, "epsilon": 1.0, "tau": 0.0}},
        },
        "model": "single straight-through premultiplied-alpha composite; no refraction/Fresnel/multiple reflection",
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({"result": "PASS", "output": str(OUT), "vertices": len(vertices), "polygons": len(faces), "sourceSha256": hashlib.sha256((OUT / "p12_controlled_glass.obj").read_bytes()).hexdigest()}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
