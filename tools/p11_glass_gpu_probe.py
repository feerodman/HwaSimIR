#!/usr/bin/env ppython
"""Validate P11 premultiplied material transmission on a real Panda OpenGL FBO."""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path

from panda3d.core import (
    Camera,
    CardMaker,
    Filename,
    FrameBufferProperties,
    GraphicsOutput,
    NodePath,
    OrthographicLens,
    Shader,
    Texture,
    TransparencyAttrib,
    loadPrcFileData,
)
from direct.showbase.ShowBase import ShowBase


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    repo = args.repo.resolve()
    output_dir = args.output_dir if args.output_dir.is_absolute() else repo / args.output_dir
    output_dir.mkdir(parents=True, exist_ok=True)

    # source RGB is already the sensor-plane surface/active/front-path term.
    cases = [
        {"name": "SWIR_tau_0.85", "source": 0.0375, "tau": 0.85, "behind": 0.21},
        {"name": "MWIR_tau_0.05", "source": 7.125, "tau": 0.05, "behind": 0.60},
        {"name": "opaque_tau_0", "source": 3.25, "tau": 0.0, "behind": 19.0},
    ]
    width = len(cases)
    loadPrcFileData(
        "",
        "\n".join(
            (
                "window-type offscreen",
                f"win-size {width} 1",
                "audio-library-name null",
                "framebuffer-srgb false",
                "sync-video false",
                "notify-level-glgsg warning",
            )
        ),
    )
    app = ShowBase(windowType="offscreen")

    # Verify that tags used by the production mapper survive Egg-to-BAM.
    bam_tags = {}
    for asset, material_id in (("civil_van", 13), ("controlled_samples", 27)):
        bam = repo / f"HwaSim_IR/Bin/Config/TargetLib/p11/{asset}/p11_{asset}.bam"
        model = app.loader.loadModel(Filename.fromOsSpecific(str(bam)))
        if model is None:
            raise RuntimeError(f"cannot load {bam}")
        matches = model.findAllMatches(f"**/=p11_material_id={material_id}")
        bam_tags[asset] = {
            "bam": str(bam),
            "materialId": material_id,
            "matchCount": matches.getNumPaths(),
            "nodes": [matches.getPath(index).getName() for index in range(matches.getNumPaths())],
        }
        model.removeNode()

    texture = Texture("P11GlassTransmissionRGBA32F")
    texture.setup2dTexture(width, 1, Texture.TFloat, Texture.FRgba)
    props = FrameBufferProperties()
    props.setRgbColor(True)
    props.setFloatColor(True)
    props.setRgbaBits(32, 32, 32, 32)
    props.setDepthBits(0)
    buffer = app.win.makeTextureBuffer("P11GlassTransmissionBuffer", width, 1, texture, False, props)
    if buffer is None:
        raise RuntimeError("RGBA32F offscreen buffer unavailable")
    buffer.clearRenderTextures()
    buffer.addRenderTexture(texture, GraphicsOutput.RTMCopyRam)

    root = NodePath("P11GlassTransmissionRoot")
    camera = Camera("P11GlassTransmissionCamera")
    lens = OrthographicLens()
    lens.setFilmSize(2, 2)
    lens.setNearFar(-10, 10)
    camera.setLens(lens)
    camera_node = root.attachNewNode(camera)
    camera_node.setPos(0, -1, 0)
    region = buffer.makeDisplayRegion()
    region.setCamera(camera_node)
    region.setClearColorActive(True)
    region.setClearColor((0, 0, 0, 0))
    region.setClearDepthActive(False)

    maker = CardMaker("P11GlassTransmissionCard")
    maker.setFrame(-1, 1, -1, 1)
    background = root.attachNewNode(maker.generate())
    foreground = root.attachNewNode(maker.generate())
    vertex = """#version 130
uniform mat4 p3d_ModelViewProjectionMatrix;
in vec4 p3d_Vertex;
void main(){gl_Position=p3d_ModelViewProjectionMatrix*p3d_Vertex;}
"""
    background_fragment = """#version 130
out vec4 fragColor;
void main(){int i=int(floor(gl_FragCoord.x));float v=i==0?.21:(i==1?.60:19.0);fragColor=vec4(v,v,v,1.0);}
"""
    foreground_fragment = """#version 130
out vec4 fragColor;
void main(){int i=int(floor(gl_FragCoord.x));float v=i==0?.0375:(i==1?7.125:3.25);float a=i==0?.15:(i==1?.95:1.0);fragColor=vec4(v,v,v,a);}
"""
    background.setShader(Shader.make(Shader.SLGLSL, vertex, background_fragment), 1)
    foreground.setShader(Shader.make(Shader.SLGLSL, vertex, foreground_fragment), 1)
    background.setDepthTest(False)
    background.setDepthWrite(False)
    # Panda's default bin order is opaque before transparent; the integer only
    # orders objects *within* one bin and is not a global bin priority.
    background.setBin("opaque", 0)
    foreground.setDepthTest(False)
    foreground.setDepthWrite(False)
    foreground.setTransparency(TransparencyAttrib.MPremultipliedAlpha)
    foreground.setBin("transparent", 20)

    for _ in range(4):
        app.graphicsEngine.renderFrame()
    app.graphicsEngine.extractTextureData(texture, buffer.getGsg())
    raw = bytes(texture.getRamImageAs("RGBA"))
    if len(raw) != width * 4 * 4:
        raise RuntimeError(f"unexpected float RAM image length {len(raw)}")
    values = struct.unpack("<" + "f" * (width * 4), raw)
    samples = []
    for index, case in enumerate(cases):
        gpu = float(values[index * 4])
        expected = case["source"] + case["tau"] * case["behind"]
        absolute_error = abs(gpu - expected)
        samples.append(
            {
                **case,
                "opacity": 1.0 - case["tau"],
                "gpu": gpu,
                "expected": expected,
                "absoluteError": absolute_error,
                "pass": absolute_error <= max(2.0e-5, abs(expected) * 2.0e-6),
            }
        )
    fb = buffer.getFbProperties()
    status = "PASS" if all(row["pass"] for row in samples) and all(
        value["matchCount"] >= 1 for value in bam_tags.values()
    ) else "FAIL"
    result = {
        "schema": "HwaSimIR.P11.GlassTransmissionGpuProbe.v1",
        "status": status,
        "equation": "Cout=Csrc+tau_material*Cbehind",
        "blend": "TransparencyAttrib.MPremultipliedAlpha",
        "backend": {
            "pipe": app.pipe.getType().getName(),
            "gsg": buffer.getGsg().getType().getName(),
            "floatColor": bool(fb.getFloatColor()),
            "rgbaBits": [fb.getRedBits(), fb.getGreenBits(), fb.getBlueBits(), fb.getAlphaBits()],
        },
        "bamTags": bam_tags,
        "samples": samples,
    }
    output = output_dir / "p11_glass_gpu_probe.json"
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": status, "backend": result["backend"], "output": str(output)}))
    app.destroy()
    return 0 if status == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
