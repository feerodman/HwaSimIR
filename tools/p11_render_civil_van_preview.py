#!/usr/bin/env python3
"""Render a deterministic offscreen preview of the P11 civil van asset."""

from __future__ import annotations

import json
import sys
from pathlib import Path

from panda3d.core import (
    AmbientLight,
    CardMaker,
    DirectionalLight,
    Filename,
    LColor,
    TexturePool,
    TransparencyAttrib,
    loadPrcFileData,
)


loadPrcFileData("", "window-type offscreen")
loadPrcFileData("", "win-size 1000 760")
loadPrcFileData("", "framebuffer-srgb true")
loadPrcFileData("", "sync-video false")
loadPrcFileData("", "notify-level warning")

from direct.showbase.ShowBase import ShowBase  # noqa: E402


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    asset_dir = root / "HwaSim_IR" / "Bin" / "Config" / "TargetLib" / "p11" / "civil_van"
    out_dir = root / "logs" / "p11" / "work" / "civil_model"
    out_dir.mkdir(parents=True, exist_ok=True)

    app = ShowBase(windowType="offscreen")
    app.setBackgroundColor(0.055, 0.065, 0.080, 1.0)
    model_path = asset_dir / "p11_civil_van.egg"
    model = app.loader.loadModel(Filename.fromOsSpecific(str(model_path)))
    if model.isEmpty():
        print(f"ERROR: failed to load {model_path}", file=sys.stderr)
        return 2
    bam_path = asset_dir / "p11_civil_van.bam"
    bam_model = app.loader.loadModel(Filename.fromOsSpecific(str(bam_path)))
    if bam_model.isEmpty():
        print(f"ERROR: failed to load {bam_path}", file=sys.stderr)
        return 3
    model.reparentTo(app.render)

    visible_texture = TexturePool.loadTexture(
        Filename.fromOsSpecific(str(asset_dir / "p11_civil_van_visible.ppm"))
    )
    if visible_texture is None:
        print("ERROR: visible atlas failed to load", file=sys.stderr)
        return 4
    visible_texture.setMinfilter(visible_texture.FT_nearest)
    visible_texture.setMagfilter(visible_texture.FT_nearest)
    material_id_texture = TexturePool.loadTexture(
        Filename.fromOsSpecific(str(asset_dir / "p11_civil_van_material_id.pgm"))
    )
    if material_id_texture is None:
        print("ERROR: material-ID atlas failed to load", file=sys.stderr)
        return 5

    # Use inherited group colours for the human-facing preview.  The two atlases
    # are still loaded above to verify Panda compatibility, but the one-pixel
    # material-ID cells must not be minified as visible colour in this preview.
    colours = {
        "paint_*": (0.68, 0.08, 0.055, 1.0),
        "metal_*": (0.38, 0.41, 0.45, 1.0),
        "glass_*": (0.055, 0.18, 0.27, 0.88),
        "rubber_*": (0.035, 0.035, 0.035, 1.0),
        "engine_bay": (0.63, 0.22, 0.035, 1.0),
        "exhaust_*": (0.78, 0.34, 0.04, 1.0),
    }
    coloured_groups = 0
    for pattern, colour in colours.items():
        for node in model.findAllMatches(f"**/{pattern}"):
            node.setColor(*colour)
            if pattern == "glass_*":
                node.setTransparency(TransparencyAttrib.MAlpha)
                node.setDepthOffset(2)
            coloured_groups += 1
    if coloured_groups != 20:
        print(f"ERROR: expected 20 tagged geometry groups, found {coloured_groups}", file=sys.stderr)
        return 6

    app.camera.setPos(6.8, 8.7, 4.2)
    app.camera.lookAt(0.0, 0.05, 1.15)
    app.camLens.setFov(34.0)
    app.camLens.setNearFar(0.1, 100.0)

    ambient = AmbientLight("ambient")
    ambient.setColor(LColor(0.30, 0.32, 0.36, 1.0))
    app.render.setLight(app.render.attachNewNode(ambient))
    key = DirectionalLight("key")
    key.setColor(LColor(1.15, 1.05, 0.88, 1.0))
    key_node = app.render.attachNewNode(key)
    key_node.setHpr(-42.0, -48.0, 0.0)
    app.render.setLight(key_node)
    fill = DirectionalLight("fill")
    fill.setColor(LColor(0.30, 0.42, 0.62, 1.0))
    fill_node = app.render.attachNewNode(fill)
    fill_node.setHpr(138.0, -22.0, 0.0)
    app.render.setLight(fill_node)

    ground_maker = CardMaker("ground")
    ground_maker.setFrame(-6.0, 6.0, -6.0, 6.0)
    ground = app.render.attachNewNode(ground_maker.generate())
    ground.setP(-90.0)
    ground.setZ(0.105)
    ground.setColor(0.16, 0.18, 0.20, 1.0)

    for _ in range(4):
        app.graphicsEngine.renderFrame()
    preview = out_dir / "p11_civil_van_close_preview.png"
    if not app.win.saveScreenshot(Filename.fromOsSpecific(str(preview))):
        print(f"ERROR: screenshot failed: {preview}", file=sys.stderr)
        return 7

    app.camera.setPos(6.5, -8.5, 3.6)
    app.camera.lookAt(0.0, -0.15, 1.05)
    for _ in range(3):
        app.graphicsEngine.renderFrame()
    rear_preview = out_dir / "p11_civil_van_rear_preview.png"
    if not app.win.saveScreenshot(Filename.fromOsSpecific(str(rear_preview))):
        print(f"ERROR: screenshot failed: {rear_preview}", file=sys.stderr)
        return 8

    bounds = model.getTightBounds()
    stats = {
        "result": "PASS",
        "model": str(model_path),
        "preview": str(preview),
        "rearPreview": str(rear_preview),
        "bounds": [[float(v) for v in point] for point in bounds] if bounds else None,
        "materialGroups": coloured_groups,
        "bamLoad": "PASS",
        "visibleAtlasLoad": "PASS",
        "materialIdAtlasLoad": "PASS",
    }
    (out_dir / "preview_validation.json").write_text(
        json.dumps(stats, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(stats, separators=(",", ":")))
    app.destroy()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
