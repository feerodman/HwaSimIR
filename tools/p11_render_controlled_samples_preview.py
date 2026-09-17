#!/usr/bin/env python3
"""Render deterministic front/oblique previews of the P11 sample rack."""

from __future__ import annotations

import json
import sys
from pathlib import Path

from panda3d.core import Filename, LColor, TexturePool, loadPrcFileData


loadPrcFileData("", "window-type offscreen")
loadPrcFileData("", "win-size 1100 720")
loadPrcFileData("", "framebuffer-srgb true")
loadPrcFileData("", "sync-video false")
loadPrcFileData("", "notify-level warning")

from direct.showbase.ShowBase import ShowBase  # noqa: E402


VISIBLE = {
    21: (0.05, 0.10, 0.34, 1.0), 22: (0.08, 0.34, 0.62, 1.0),
    23: (0.12, 0.58, 0.45, 1.0), 24: (0.86, 0.20, 0.08, 1.0),
    25: (0.42, 0.42, 0.42, 1.0), 26: (0.78, 0.78, 0.82, 1.0),
    27: (0.10, 0.66, 0.82, 1.0), 28: (0.32, 0.20, 0.62, 1.0),
}


def groups(model):
    result = {}
    for material_id in range(21, 29):
        matches = list(model.findAllMatches(f"**/sample_{material_id}_*"))
        if len(matches) != 1:
            raise RuntimeError(f"expected one group for material ID {material_id}; got {len(matches)}")
        result[material_id] = matches[0]
    return result


def render_frames(app: ShowBase, count: int = 4) -> None:
    for _ in range(count):
        app.graphicsEngine.renderFrame()


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    asset_dir = root / "HwaSim_IR" / "Bin" / "Config" / "TargetLib" / "p11" / "controlled_samples"
    out_dir = root / "logs" / "p11" / "work" / "controlled_samples"
    out_dir.mkdir(parents=True, exist_ok=True)

    app = ShowBase(windowType="offscreen")
    app.setBackgroundColor(0.035, 0.040, 0.050, 1.0)
    egg_path = asset_dir / "p11_controlled_samples.egg"
    bam_path = asset_dir / "p11_controlled_samples.bam"
    model = app.loader.loadModel(Filename.fromOsSpecific(str(egg_path)))
    bam_model = app.loader.loadModel(Filename.fromOsSpecific(str(bam_path)))
    if model.isEmpty() or bam_model.isEmpty():
        print("ERROR: EGG/BAM load failed", file=sys.stderr)
        return 2
    model.reparentTo(app.render)
    model.setLightOff(1)
    tagged = groups(model)

    visible_texture = TexturePool.loadTexture(Filename.fromOsSpecific(
        str(asset_dir / "p11_controlled_samples_visible.ppm")))
    id_texture = TexturePool.loadTexture(Filename.fromOsSpecific(
        str(asset_dir / "p11_controlled_samples_material_id.pgm")))
    if visible_texture is None or id_texture is None:
        print("ERROR: sample atlases failed to load", file=sys.stderr)
        return 3

    for material_id, node in tagged.items():
        node.setColor(LColor(*VISIBLE[material_id]))
    app.camera.setPos(0.0, 7.2, 1.20)
    app.camera.lookAt(0.0, 0.0, 1.20)
    app.camLens.setFov(38.0)
    app.camLens.setNearFar(0.1, 100.0)
    render_frames(app)
    front_path = out_dir / "p11_controlled_samples_front_preview.png"
    if not app.win.saveScreenshot(Filename.fromOsSpecific(str(front_path))):
        return 4

    # High-contrast partition preview is explicitly a visualization of IDs,
    # not the actual normalized 8-bit material-ID texture values.
    for material_id, node in tagged.items():
        level = 0.12 + 0.82 * (material_id - 21) / 7.0
        node.setColor(LColor(level, level, level, 1.0))
    app.camera.setPos(4.8, 7.4, 3.0)
    app.camera.lookAt(0.0, 0.0, 1.20)
    app.camLens.setFov(42.0)
    render_frames(app, 3)
    partition_path = out_dir / "p11_controlled_samples_partition_preview.png"
    if not app.win.saveScreenshot(Filename.fromOsSpecific(str(partition_path))):
        return 5

    bounds = model.getTightBounds()
    report = {
        "result": "PASS", "egg": str(egg_path), "bam": str(bam_path),
        "frontPreview": str(front_path), "partitionPreview": str(partition_path),
        "bounds": [[float(value) for value in point] for point in bounds] if bounds else None,
        "materialGroups": len(tagged), "materialIds": list(range(21, 29)),
        "visibleAtlasLoad": "PASS", "materialIdAtlasLoad": "PASS",
        "previewPolicy": "visible colours and high-contrast partition image are orientation aids only",
    }
    report_path = out_dir / "preview_validation.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, separators=(",", ":")))
    app.destroy()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
