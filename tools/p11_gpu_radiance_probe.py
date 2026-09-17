#!/usr/bin/env ppython
"""Run the P11 Planck/formal-radiance equations on a real Panda OpenGL GPU."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
from pathlib import Path

from panda3d.core import (
    Camera, CardMaker, Filename, FrameBufferProperties, GraphicsOutput,
    NodePath, OrthographicLens, Shader, Texture, WindowProperties,
    loadPrcFileData,
)
from direct.showbase.ShowBase import ShowBase

H = 6.62607015e-34
C = 299792458.0
KB = 1.380649e-23
PI = math.pi
BANDS = {"SWIR": (1.1, 2.5), "MWIR": (3.0, 5.0)}


def planck(wavelength_um: float, temperature_k: float) -> float:
    wavelength_m = wavelength_um * 1e-6
    exponent = H * C / (wavelength_m * KB * temperature_k)
    return 2 * H * C * C / (wavelength_m**5 * math.expm1(exponent)) * 1e-6


def band_mean(band: str, temperature_k: float, intervals: int = 16384) -> float:
    low, high = BANDS[band]
    step = (high - low) / intervals
    total = planck(low, temperature_k) + planck(high, temperature_k)
    for index in range(1, intervals):
        total += (4 if index % 2 else 2) * planck(low + index * step, temperature_k)
    return total * step / 3 / (high - low)


def expected_sensor(band: str, tau: float = 0.7) -> float:
    body = 0.8 * band_mean(band, 300.0)
    reflected = 0.2 / PI * (2.0 * 0.5 + 0.5)
    return tau * (body + reflected) + 0.1 + 0.02 + 0.028


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    cases = [
        ("SWIR_planck_300K", "planck", "SWIR", 300.0, 0.0),
        ("SWIR_planck_900K", "planck", "SWIR", 900.0, 0.0),
        ("SWIR_planck_1200K", "planck", "SWIR", 1200.0, 0.0),
        ("MWIR_planck_300K", "planck", "MWIR", 300.0, 0.0),
        ("MWIR_planck_500K", "planck", "MWIR", 500.0, 0.0),
        ("MWIR_planck_900K", "planck", "MWIR", 900.0, 0.0),
        ("SWIR_formal_base", "formal", "SWIR", 300.0, 0.7),
        ("MWIR_formal_base", "formal", "MWIR", 300.0, 0.7),
        ("SWIR_tau_1e-8", "formal", "SWIR", 300.0, 1e-8),
        ("MWIR_tau_1e-8", "formal", "MWIR", 300.0, 1e-8),
    ]
    width = len(cases)

    loadPrcFileData("", "\n".join((
        "window-type offscreen", f"win-size {width} 1", "audio-library-name null",
        "framebuffer-srgb false", "sync-video false", "notify-level-glgsg warning",
    )))
    app = ShowBase(windowType="offscreen")
    texture = Texture("P11GpuRadianceRGB32F")
    texture.setup2dTexture(width, 1, Texture.TFloat, Texture.FRgb)
    props = FrameBufferProperties()
    props.setRgbColor(True)
    props.setFloatColor(True)
    props.setRgbaBits(32, 32, 32, 0)
    props.setDepthBits(0)
    buffer = app.win.makeTextureBuffer("P11GpuRadianceBuffer", width, 1, texture, False, props)
    if buffer is None:
        raise RuntimeError("RGB32F offscreen buffer unavailable")
    buffer.clearRenderTextures()
    buffer.addRenderTexture(texture, GraphicsOutput.RTMCopyRam)

    root = NodePath("P11GpuRadianceRoot")
    camera = Camera("P11GpuRadianceCamera")
    lens = OrthographicLens()
    lens.setFilmSize(2, 2)
    lens.setNearFar(-10, 10)
    camera.setLens(lens)
    camera_node = root.attachNewNode(camera)
    camera_node.setPos(0, -1, 0)
    region = buffer.makeDisplayRegion()
    region.setCamera(camera_node)
    region.setClearColorActive(True)
    region.setClearDepthActive(False)
    card_maker = CardMaker("P11GpuRadianceCard")
    card_maker.setFrame(-1, 1, -1, 1)
    card = root.attachNewNode(card_maker.generate())

    vertex = """#version 130
uniform mat4 p3d_ModelViewProjectionMatrix;
in vec4 p3d_Vertex;
void main(){gl_Position=p3d_ModelViewProjectionMatrix*p3d_Vertex;}
"""
    fragment = """#version 130
out vec4 fragColor;
float planck(float wavelength_um,float temperature_K){
    float exponent_value=clamp(14387.752/(wavelength_um*temperature_K),0.000001,80.0);
    return 119104200.0/(pow(wavelength_um,5.0)*max(exp(exponent_value)-1.0,0.000001));
}
float swir(float t){
    float s=planck(1.1,t)+planck(2.5,t);
    for(int i=1;i<10;++i){float w=(i-(i/2)*2==0)?2.0:4.0;s+=w*planck(1.1+.14*float(i),t);}
    return s/30.0;
}
float mwir(float t){return (planck(3.0,t)+4.0*planck(3.5,t)+2.0*planck(4.0,t)+4.0*planck(4.5,t)+planck(5.0,t))/12.0;}
float formal(float body,float tau){
    float reflected=.2/3.14159265*(2.0*.5+.5);
    return tau*(.8*body+reflected)+.1+.02+.028;
}
void main(){
    int i=int(floor(gl_FragCoord.x));float value=0.0;
    if(i==0)value=swir(300.0);else if(i==1)value=swir(900.0);else if(i==2)value=swir(1200.0);
    else if(i==3)value=mwir(300.0);else if(i==4)value=mwir(500.0);else if(i==5)value=mwir(900.0);
    else if(i==6)value=formal(swir(300.0),.7);else if(i==7)value=formal(mwir(300.0),.7);
    else if(i==8)value=formal(swir(300.0),.00000001);else if(i==9)value=formal(mwir(300.0),.00000001);
    fragColor=vec4(value,value,value,1.0);
}
"""
    shader = Shader.make(Shader.SLGLSL, vertex, fragment)
    if shader is None:
        raise RuntimeError("P11 GPU shader compilation failed")
    card.setShader(shader, 1)
    card.setDepthTest(False)
    card.setDepthWrite(False)
    for _ in range(4):
        app.graphicsEngine.renderFrame()
    app.graphicsEngine.extractTextureData(texture, buffer.getGsg())
    raw = bytes(texture.getRamImageAs("RGB"))
    if len(raw) != width * 3 * 4:
        raise RuntimeError(f"unexpected float RAM image length {len(raw)}")
    values = struct.unpack("<" + "f" * (width * 3), raw)

    samples = []
    for index, (name, kind, band, temperature, tau) in enumerate(cases):
        gpu = float(values[index * 3])
        cpu = band_mean(band, temperature) if kind == "planck" else expected_sensor(band, tau)
        samples.append({
            "name": name, "kind": kind, "band": band, "temperature_K": temperature,
            "tau": tau, "gpu_W_m2_sr_um": gpu, "cpu_W_m2_sr_um": cpu,
            "relative_error": abs(gpu - cpu) / max(abs(cpu), 1e-12),
        })
    fb = buffer.getFbProperties()
    result = {
        "schema": "HwaSimIR.P11.GpuRadianceProbe.v1",
        "status": "PASS" if all(row["relative_error"] <= 0.02 for row in samples) else "FAIL",
        "backend": {
            "pipe": app.pipe.getType().getName(), "gsg": buffer.getGsg().getType().getName(),
            "float_color": bool(fb.getFloatColor()),
            "rgb_bits": [fb.getRedBits(), fb.getGreenBits(), fb.getBlueBits()],
        },
        "radiance_unit": "W/(m^2 sr um)", "samples": samples,
    }
    output = args.output_dir / "gpu_probe.json"
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    (args.output_dir / "shader.frag.glsl").write_text(fragment, encoding="utf-8")
    result["shader_sha256"] = sha256(args.output_dir / "shader.frag.glsl")
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": result["status"], "backend": result["backend"],
                      "max_relative_error": max(row["relative_error"] for row in samples)}))
    app.destroy()
    return 0 if result["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
